/*
    This file is part of sxmlc.

    sxmlc is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    sxmlc is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with sxmlc.  If not, see <http://www.gnu.org/licenses/>.

	Copyright 2010 - Matthieu Labas
*/
#pragma warning(disable : 4996)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "sxmlc.h"

/*
 Struct defining "special" tags such as "<? ?>" or "<![CDATA[ ]]/>".
 These tags are considered having a start and an end with some data in between that will
 be stored in the 'tag' member of an XMLNode.
 The 'tag_type' member is a constant that is associated to such tag.
 All 'len_*' members are basically the "strlen()" of 'start' and 'end' members.
 */
typedef struct _Tag {
	int tag_type;
	char* start;
	int len_start;
	char* end;
	int len_end;
} _TAG;

typedef struct _SpecialTag {
	_TAG *tags;
	int n_tags;
} SPECIAL_TAG;

/*
 List of "special" tags handled by sxmlc.
 NB the "<!DOCTYPE" tag has a special handling because its 'end' changes according
 to its content.
 */
static _TAG _spec[] = {
		{ TAG_INSTR, "<?", 2, "?>", 2 },
		{ TAG_COMMENT, "<!--", 4, "-->", 3 },
		{ TAG_CDATA, "<![CDATA[", 9, "]]/>", 4 }
};
static int NB_SPECIAL_TAGS = sizeof(_spec) / sizeof(_TAG); /* Auto computation of number of special tags */

/*
 User-registered tags.
 */
static SPECIAL_TAG _user_tags = { NULL, 0 };

int XML_register_user_tag(int tag_type, char* start, char* end)
{
	_TAG* p;
	int i, n, le;

	if (tag_type < TAG_USER) return false;

	if (start == NULL || end == NULL || *start != '<') return false;

	le = strlen(end);
	if (end[le-1] != '>') return false;

	i = _user_tags.n_tags;
	n = i + 1;
	p = (_TAG*)realloc(_user_tags.tags, n * sizeof(_TAG));
	if (p == NULL) return false;

	p[i].tag_type = tag_type;
	p[i].start = start;
	p[i].end = end;
	p[i].len_start = strlen(start);
	p[i].len_end = le;
	_user_tags.tags = p;
	_user_tags.n_tags = n;

	return true;
}

/* --- XMLNode methods --- */

/*
 Add 'node' to given '*array' of '*len_array' elements.
 '*len_array' is the number of elements in '*array' after its reallocation.
 Return false for memory error.
 */
static int _add_node(XMLNode*** array, int* len_array, const XMLNode* node)
{
	XMLNode** pt = (XMLNode**)realloc(*array, (*len_array+1) * sizeof(XMLNode*));
	
	if (pt == NULL) return -1;
	
	pt[*len_array] = (XMLNode*)node;
	*array = pt;
	(*len_array)++;
	
	return *len_array - 1;
}

void XMLNode_init(XMLNode* node)
{
	if (node == NULL) return;
	
	node->tag = NULL;
	node->text = NULL;
	
	node->attributes = NULL;
	node->n_attributes = 0;
	
	node->children = NULL;
	node->n_children = 0;
	
	node->tag_type = TAG_NONE;
	node->active = true;
}

XMLNode* XMLNode_alloc(int n)
{
	int i;
	XMLNode* p;
	
	if (n <= 0) return NULL;
	
	p = (XMLNode*)malloc(n * sizeof(XMLNode));
	if (p == NULL) return NULL;

	for (i = 0; i < n; i++)
		XMLNode_init(&p[i]);
	
	return p;
}

void XMLNode_free(XMLNode* node)
{
	int i;

	if (node == NULL) return;
	
	if (node->attributes != NULL) {
		for (i = 0; i < node->n_attributes; i++) {
			if (node->attributes[i].name != NULL) free(node->attributes[i].name);
			if (node->attributes[i].value != NULL) free(node->attributes[i].value);
		}
		free(node->attributes);
		node->attributes = NULL;
	}
	node->n_attributes = 0;
	
	if (node->children != NULL) {
		for (i = 0; i < node->n_children; i++)
			if (node->children[i] != NULL) {
				XMLNode_free(node->children[i]);
			}
		free(node->children);
		node->children = NULL;
	}
	node->n_children = 0;
	
	if (node->tag != NULL) {
		free(node->tag);
		node->tag = NULL;
	}
	if (node->text != NULL) {
		free(node->text);
		node->text = NULL;
	}
	
	node->tag_type = TAG_NONE;
}

int XMLNode_copy(XMLNode* dst, const XMLNode* src, int copy_children)
{
	int i;
	
	if (dst == NULL) return false;
	
	XMLNode_free(dst); /* 'dst' is freed first */
	
	/* NULL 'src' resets 'dst' */
	if (src == NULL) return true;
	
	/* Tag */
	if (src->tag != NULL) {
		dst->tag = (char*)malloc(strlen(src->tag)+1);
		if (dst->tag == NULL) goto copy_err;
		strcpy(dst->tag, src->tag);
	}

	/* Text */
	if (dst->text != NULL) {
		dst->text = (char*)malloc(strlen(src->text)+1);
		if (dst->text == NULL) goto copy_err;
		strcpy(dst->text, src->text);
	}

	/* Attributes */
	if (src->n_attributes > 0) {
		dst->attributes = (XMLAttribute*)malloc(src->n_attributes * sizeof(XMLAttribute));
		if (dst->attributes== NULL) goto copy_err;
		dst->n_attributes = src->n_attributes;
		for (i = 0; i < src->n_attributes; i++) {
			dst->attributes[i].name = (char*)malloc(strlen(src->attributes[i].name)+1);
			dst->attributes[i].value = (char*)malloc(strlen(src->attributes[i].value)+1);
			if (dst->attributes[i].name == NULL || dst->attributes[i].value == NULL) goto copy_err;
		
			strcpy(dst->attributes[i].name, src->attributes[i].name);
			strcpy(dst->attributes[i].value, src->attributes[i].value);
			dst->attributes[i].active = src->attributes[i].active;
		}
	}

	dst->tag_type = src->tag_type;
	dst->father = src->father;
	dst->user = src->user;
	dst->active = src->active;
	
	/* Copy children if required */
	if (copy_children) {
		dst->children = (XMLNode**)malloc(src->n_children * sizeof(XMLNode*));
		if (dst->children == NULL) goto copy_err;
		dst->n_children = src->n_children;
		for (i = 0; i < src->n_children; i++) {
			if (!XMLNode_copy(dst->children[i], src->children[i], true)) goto copy_err;
		}
	}
	
	return true;
	
copy_err:
	XMLNode_free(dst);
	
	return false;
}

void XMLNode_set_active(XMLNode* node, int active)
{
	if (node == NULL) return;

	node->active = active;
}

int XMLNode_set_tag(XMLNode* node, const char* tag)
{
	if (node == NULL || tag == NULL) return false;
	
	if (node->tag != NULL) free(node->tag);
	node->tag = strdup(tag);
	if (node->tag == NULL) return false;
	
	return true;
}

int XMLNode_set_comment(XMLNode* node, const char* comment)
{
	if (!XMLNode_set_tag(node, comment)) return false;
	
	node->tag_type = TAG_COMMENT;
	
	return true;
}

int XMLNode_set_attribute(XMLNode* node, const char* attr_name, const char* attr_value)
{
	XMLAttribute* pt;
	int i;
	
	if (node == NULL || attr_name == NULL || attr_name[0] == 0) return -1;
	
	i = XMLNode_search_attribute(node, attr_name, 0);
	if (i >= 0) {
		pt = node->attributes;
		if (pt[i].value != NULL) free(pt[i].value);
		pt[i].value = (char*)malloc(strlen(attr_value)+1);
		if (pt[i].value != NULL) strcpy(pt[i].value, attr_value);
	}
	else {
		i = node->n_attributes;
		pt = (XMLAttribute*)realloc(node->attributes, (i+1) * sizeof(XMLAttribute));
		if (pt == NULL) return 0;

		pt[i].name = (char*)malloc(strlen(attr_name)+1);
		pt[i].value = (char*)malloc(strlen(attr_value)+1);
		if (pt[i].name != NULL && pt[i].value != NULL) {
			strcpy(pt[i].name, attr_name);
			strcpy(pt[i].value, attr_value);
			node->attributes = pt;
			node->n_attributes = i + 1;
		}
		else {
			node->attributes = (XMLAttribute*)realloc(pt, i * sizeof(XMLAttribute)); /* Frees memory, cannot fail hopefully! */
			return -1;
		}
	}

	return node->n_attributes;
}

int XMLNode_search_attribute(const XMLNode* node, const char* attr_name, int isearch)
{
	int i;
	
	if (node == NULL || attr_name == NULL || attr_name[0] == 0 || isearch < 0 || isearch > node->n_attributes) return -1;
	
	for (i = isearch; i < node->n_attributes; i++)
		if (node->attributes[i].active && !strcmp(node->attributes[i].name, attr_name)) return i;
	
	return -1;
}

int XMLNode_remove_attribute(XMLNode* node, int i_attr)
{
	if (node == NULL || i_attr < 0 || i_attr >= node->n_attributes) return -1;
	
	/* Free attribute fields first */
	if (node->attributes[i_attr].name != NULL) free(node->attributes[i_attr].name);
	if (node->attributes[i_attr].value != NULL) free(node->attributes[i_attr].value);
	
	memmove(&node->attributes[i_attr], &node->attributes[i_attr+1], (node->n_attributes - i_attr - 1) * sizeof(XMLAttribute));
	node->attributes = (XMLAttribute*)realloc(node->attributes, --(node->n_attributes) * sizeof(XMLAttribute)); /* Frees memory */
	
	return node->n_attributes;
}

int XMLNode_set_text(XMLNode* node, const char* text)
{
	if (node == NULL) return false;

	if (text == NULL) { /* We want to remove it => free node text */
		if (node->text != NULL) {
			free(node->text);
			node->text = NULL;
		}

		return true;
	}

	/* No text is defined yet => allocate it */
	if (node->text == NULL) {
		node->text = (char*)malloc(strlen(text)+1);
		if (node->text == NULL) return false;
	}
	else {
		char* p = (char*)realloc(node->text, strlen(text)+1);
		if (p == NULL) return false;
		node->text = p;
	}

	strcpy(node->text, text);

	return true;
}

int XMLNode_add_child(XMLNode* node, const XMLNode* child)
{
	int n;
	
	if (node == NULL || child == NULL) return false;
	
	n = node->n_children;
	return _add_node(&node->children, &node->n_children, child) >= 0 ? true : false;
}

int XMLNode_search_child(const XMLNode* node, const char* tag, int isearch)
{
	int i;
	
	if (node == NULL || tag == NULL || tag[0] == 0 || isearch < 0 || isearch > node->n_children) return -1;
	
	for (i = isearch; i < node->n_children; i++)
		if (node->children[i]->active && !strcmp(node->children[i]->tag, tag)) return i;
	
	return -1;
}

int XMLNode_remove_child(XMLNode* node, int ichild)
{
	if (node == NULL || ichild < 0 || ichild >= node->n_children) return -1;
	
	/* Free node first */
	XMLNode_free(node->children[ichild]);
	
	memmove(&node->children[ichild], &node->children[ichild+1], (node->n_children - ichild - 1) * sizeof(XMLNode*));
	node->children = (XMLNode**)realloc(node->children, --(node->n_children) * sizeof(XMLNode*)); /* Frees memory */
	
	return node->n_children;
}

int XMLNode_equal(const XMLNode* node1, const XMLNode* node2)
{
	int i, j;

	if (node1 == node2) return true;

	if (node1 == NULL || node2 == NULL) return false;

	if (strcmp(node1->tag, node2->tag)) return false;

	/* Test all attributes from 'node1' */
	for (i = 0; i < node1->n_attributes; i++) {
		if (!node1->attributes[i].active) continue;
		j = XMLNode_search_attribute(node2, node1->attributes[i].name, 0);
		if (j < 0) return false;
		if (strcmp(node1->attributes[i].name, node2->attributes[j].name)) return false;
	}

	/* Test other attributes from 'node2' that might not be in 'node1' */
	for (i = 0; i < node2->n_attributes; i++) {
		if (!node2->attributes[i].active) continue;
		j = XMLNode_search_attribute(node1, node2->attributes[i].name, 0);
		if (j < 0) return false;
		if (strcmp(node2->attributes[i].name, node1->attributes[j].name)) return false;
	}

	return true;
}

XMLNode* XMLNode_next_sibling(const XMLNode* node)
{
	int i;
	XMLNode* father;

	if (node == NULL || node->father == NULL) return NULL;

	father = node->father;
	for (i = 0; i < father->n_children && father->children[i] != node; i++) ;
	i++; /* father->children[i] is now 'node' next sibling */

	return i < father->n_children ? father->children[i] : NULL;
}

static XMLNode* _XMLNode_next(const XMLNode* node, int in_children)
{
	XMLNode* node2;

	if (node == NULL) return NULL;

	/* Check first child */
	if (in_children && node->n_children > 0) return node->children[0];

	/* Check next sibling */
	if ((node2 = XMLNode_next_sibling(node)) != NULL) return node2;

	/* Check next uncle */
	return _XMLNode_next(node->father, false);
}

XMLNode* XMLNode_next(const XMLNode* node)
{
	return _XMLNode_next(node, true);
}

/* --- XMLDoc methods --- */

void XMLDoc_init(XMLDoc* doc)
{
	doc->nodes = NULL;
	doc->n_nodes = 0;
	doc->i_root = -1;
	doc->init_value = XMLDOC_INIT_DONE;
}

int XMLDoc_free(XMLDoc* doc)
{
	int i;
	
	if (doc->init_value != XMLDOC_INIT_DONE) return false;

	for (i = 0; i < doc->n_nodes; i++)
		XMLNode_free(doc->nodes[i]);
	free(doc->nodes);
	XMLDoc_init(doc);

	return true;
}

int XMLDoc_set_root(XMLDoc* doc, int i_root)
{
	if (doc == NULL || doc->init_value != XMLDOC_INIT_DONE || i_root < 0 || i_root >= doc->n_nodes) return false;
	
	doc->i_root = i_root;
	
	return true;
}

int XMLDoc_add_node(XMLDoc* doc, XMLNode* node, int tag_type)
{
	if (doc == NULL || node == NULL || doc->init_value != XMLDOC_INIT_DONE) return false;
	
	if (tag_type == TAG_NONE) tag_type = node->tag_type;
	node->tag_type = tag_type;
	if (_add_node(&doc->nodes, &doc->n_nodes, node) >= 0) {
		if (tag_type == TAG_FATHER) doc->i_root = doc->n_nodes - 1;
		return doc->n_nodes;
	}
	else
		return -1;
}

/*
 Helper functions to print formatting before a new tag.
 Returns the new number of characters in the line.
 */
static int count_new_char_line(const char* str, int nb_char_tab, int cur_sz_line)
{
	for (; *str; str++) {
		if (*str == '\n') cur_sz_line = 0;
		else if (*str == '\t') cur_sz_line += nb_char_tab;
		else cur_sz_line++;
	}
	
	return cur_sz_line;
}
static int _print_formatting(FILE* f, const char* tag_sep, const char* child_sep, int nb_char_tab, int depth, int cur_sz_line)
{
	int i;
	
	if (tag_sep) {
		fprintf(f, tag_sep);
		cur_sz_line = count_new_char_line(tag_sep, nb_char_tab, cur_sz_line);
	}
	if (child_sep) {
		for (i = 0; i < depth; i++) {
			fprintf(f, child_sep);
			cur_sz_line = count_new_char_line(child_sep, nb_char_tab, cur_sz_line);
		}
	}
	
	return cur_sz_line;
}

void XMLNode_print(const XMLNode* node, FILE* f, const char* tag_sep, const char* child_sep, int sz_line, int nb_char_tab, int depth)
{
	static int cur_sz_line = 0; /* How many characters are on the current line */
	int i;
	
	if (node == NULL || f == NULL || !node->active) return;
	
	if (nb_char_tab <= 0) nb_char_tab = 1;
	
	/* Print formatting */
	cur_sz_line = _print_formatting(f, tag_sep, child_sep, nb_char_tab, depth, cur_sz_line);
	
	if (node->tag_type == TAG_COMMENT) {
		fprintf(f, "<!--%s-->", node->tag);
		/*cur_sz_line += strlen(node->tag) + 7;*/
		return;
	}
	else if (node->tag_type == TAG_INSTR) {
		fprintf(f, "<?%s?>", node->tag);
		/*cur_sz_line += strlen(node->tag) + 4;*/
		return;
	}
	else if (node->tag_type == TAG_CDATA) {
		fprintf(f, "<![CDATA[%s]]/>", node->tag);
		/*cur_sz_line += strlen(node->tag) + 13;*/
		return;
	}
	else if (node->tag_type == TAG_DOCTYPE) {
		fprintf(f, "<!DOCTYPE%s%s>", node->tag, strchr(node->tag, '[') != NULL ? "]" : "");
		/*cur_sz_line += strlen(node->tag) + 10/11;*/
		return;
	}
	else {
		/* Print tag name */
		fprintf(f, "<%s", node->tag);
		cur_sz_line += strlen(node->tag) + 1;
	}
	
	/* Print attributes */
	for (i = 0; i < node->n_attributes; i++) {
		if (!node->attributes[i].active) continue;
		cur_sz_line += strlen(node->attributes[i].name) + strlen(node->attributes[i].value) + 3;
		if (sz_line > 0 && cur_sz_line > sz_line) {
			cur_sz_line = _print_formatting(f, tag_sep, child_sep, nb_char_tab, depth, cur_sz_line);
			/* Add extra separator, as if new line was a child of the previous one */
			if (child_sep) {
				fprintf(f, child_sep);
				cur_sz_line = count_new_char_line(child_sep, nb_char_tab, cur_sz_line);
			}
		}
		/* Attribute name */
		fprintf(f, " %s=", node->attributes[i].name);
		
		/* Attribute value */
		fputc('"', f);
		cur_sz_line += fprintHTML(f, node->attributes[i].value);
		fputc('"', f);
	}
	
	/* End the tag if there are no children and no text */
	if (node->n_children == 0 && (node->text == NULL || node->text[0] == 0)) {
		fprintf(f, "/>");
		/*cur_sz_line += 2;*/
		return;
	}
	else {
		fputc('>', f);
	}
	if (node->text != NULL && node->text[0]) fprintHTML(f, node->text);
	cur_sz_line++;
	
	/* Recursively print children */
	for (i = 0; i < node->n_children; i++)
		XMLNode_print(node->children[i], f, tag_sep, child_sep, sz_line, nb_char_tab, depth+1);
	
	/* Print tag end after children */
		/* Print formatting */
	if (node->n_children > 0)
		cur_sz_line = _print_formatting(f, tag_sep, child_sep, nb_char_tab, depth, cur_sz_line);
	fprintf(f, "</%s>", node->tag);
	/*cur_sz_line += strlen(node->tag) + 3;*/
}

void XMLDoc_print(const XMLDoc* doc, FILE* f, const char* tag_sep, const char* child_sep, int sz_line, int nb_char_tab)
{
	int i;
	
	if (doc == NULL || f == NULL || doc->init_value != XMLDOC_INIT_DONE) return;
	
	for (i = 0; i < doc->n_nodes; i++)
		XMLNode_print(doc->nodes[i], f, tag_sep, child_sep, sz_line, nb_char_tab, 0);
}

/* --- */

int XML_parse_attribute(const char* str, XMLAttribute* xmlattr)
{
	const char *p;
	int i, n0, n1, remQ = 0;
	int ret = 1;
	
	if (str == NULL || xmlattr == NULL) return 0;
	
	/* Search for the '=' */
	/* 'n0' is where the attribute name stops, 'n1' is where the attribute value starts */
	for (n0 = 0; str[n0] && str[n0] != '=' && !isspace(str[n0]); n0++) ; /* Search for '=' or a space */
	for (n1 = n0; str[n1] && isspace(str[n1]); n1++) ; /* Search for something not a space */
	if (str[n1] != '=') return 0; /* '=' not found: malformed string */
	for (n1++; str[n1] && isspace(str[n1]); n1++) ; /* Search for something not a space */
	if (str[n1] == '"') remQ = 1; /* Remove quotes */
	
	xmlattr->name = (char*)malloc(n0+1);
	xmlattr->value = (char*)malloc(strlen(str) - n1 - remQ);
	xmlattr->active = true;
	if (xmlattr->name != NULL && xmlattr->value != NULL) {
		/* Copy name */
		for (i = 0; i < n0; i++) xmlattr->name[i] = str[i];
		xmlattr->name[i] = 0;
		str_unescape(xmlattr->name[i]);
		/* Copy value (p starts after the quote (if any) and stops at the end of 'str'
		  (skipping the quote if any, hence the '*(p+remQ)') */
		for (i = 0, p = str + n1 + remQ; *(p+remQ); i++, p++)
			xmlattr->value[i] = *p;
		xmlattr->value[i] = 0;
		html2str(str_unescape(xmlattr->value)); /* Convert HTML escape sequences */
		if (remQ && *p != '"') ret = 2; /* Quote at the beginning but not at the end */
	}
	else ret = 0;
	
	if (ret == 0) {
		if (xmlattr->name != NULL) free(xmlattr->name);
		if (xmlattr->value != NULL) free(xmlattr->value);
	}
	
	return ret;
}

static int _parse_special_tag(const char* str, int len, _TAG* tag, XMLNode* node)
{
	if (strncmp(str, tag->start, tag->len_start)) return TAG_NONE;

	if (strncmp(str + len - tag->len_end, tag->end, tag->len_end)) return TAG_PARTIAL; /* There probably is a '>' inside the tag */

	node->tag = (char*)malloc(len - tag->len_start - tag->len_end + 1);
	if (node->tag == NULL) return TAG_NONE;
	strncpy(node->tag, str + tag->len_start, len - tag->len_start - tag->len_end);
	node->tag[len - tag->len_start - tag->len_end] = 0;
	node->tag_type = tag->tag_type;

	return node->tag_type;
}

/*
 Reads a string that is supposed to be an xml tag like '<tag (attribName="attribValue")* [/]>' or '</tag>'.
 Fills the 'xmlnode' structure with the tag name and its attributes.
 Returns 0 if an error occurred (malformed 'str' or memory). 'TAG_*' when string is recognized.
 */
int XML_parse_1string(char* str, XMLNode* xmlnode)
{
	char *p, c;
	XMLAttribute* pt;
	int n, nn, len, tag_end = 0;
	
	if (str == NULL || xmlnode == NULL) return 0;
	len = strlen(str);
	
	/* Check for malformed string */
	if (str[0] != '<' || str[len-1] != '>') return 0;

	for (nn = 0; nn < NB_SPECIAL_TAGS; nn++) {
		n = _parse_special_tag(str, len, &_spec[nn], xmlnode);
		switch (n) {
			case TAG_ERROR:	return TAG_NONE;	/* Error => exit */
			case TAG_NONE:	break;				/* Nothing found => do nothing */
			default:		return n;			/* Tag found => return it */
		}
	}

	/* "<!DOCTYPE" requires a special handling because it can end with "]>" instead of ">" if a '[' is found inside */
	if (str[1] == '!') {
		/* DOCTYPE */
		if (!strncmp(str, "<!DOCTYPE", 9)) {
			for (n = 9; str[n] && str[n] != '['; n++) ; /* Look for a '[' inside the DOCTYPE, which would mean that we should be looking for a "]>" tag end */
			nn = 0;
			if (str[n]) { /* '[' was found */
				if (strncmp(str+len-2, "]>", 2)) return TAG_PARTIAL; /* There probably is a '>' inside the DOCTYPE */
				nn = 1;
			}
			xmlnode->tag = (char*)malloc(len-9-nn); /* 'len' - "<!DOCTYPE" and ">" + '\0' */
			if (xmlnode->tag == NULL) return 0;
			strncpy(xmlnode->tag, str+9, len-10-nn);
			xmlnode->tag[len-10-nn] = 0;
			xmlnode->tag_type = TAG_DOCTYPE;

			return TAG_DOCTYPE;
		}
	}
	
	/* Test user tags */
	for (nn = 0; nn < _user_tags.n_tags; nn++) {
		n = _parse_special_tag(str, len, &_user_tags.tags[nn], xmlnode);
		switch (n) {
			case TAG_ERROR:	return TAG_NONE;	/* Error => exit */
			case TAG_NONE:	break;				/* Nothing found => do nothing */
			default:		return n;			/* Tag found => return it */
		}
	}

	if (str[1] == '/') tag_end = 1;
	
	/* tag starts at index 1 (or 2 if tag end) and ends at the first space or '/>' */
	for (n = 1 + tag_end; str[n] && str[n] != '>' && str[n] != '/' && !isspace(str[n]); n++) ;
	xmlnode->tag = (char*)malloc(n - tag_end);
	if (xmlnode->tag == NULL) return 0;
	strncpy(xmlnode->tag, str+1+tag_end, n-1-tag_end);
	xmlnode->tag[n-1-tag_end] = 0;
	if (tag_end) {
		xmlnode->tag_type = TAG_END;
		return TAG_END;
	}
	
	/* Here, 'n' is the position of the first space after tag name */
	while (n < len) {
		/* Skips spaces */
		while (isspace(str[n])) n++;
		
		/* Check for XML end ('>' or '/>') */
		if (str[n] == '>') { /* Tag with children */
			xmlnode->tag_type = TAG_FATHER;
			return TAG_FATHER;
		}
		if (!strcmp(str+n, "/>")) { /* Tag without children */
			xmlnode->tag_type = TAG_SELF;
			return TAG_SELF;
		}
		
		/* New attribute found */
		p = strchr(str+n, '=');
		if (p == NULL) goto parse_err;
		pt = (XMLAttribute*)realloc(xmlnode->attributes, ++(xmlnode->n_attributes) * sizeof(XMLAttribute));
		if (pt == NULL) {
			xmlnode->n_attributes--; /* Reallocation failed */
			goto parse_err;
		}
		xmlnode->attributes = pt;
		while (*p && isspace(*++p)) ; /* Skip spaces */
		if (*p == '"') { /* Attribute value starts with a '"', look for next one, ignoring protected ones with '\"' */
			for (nn = p-str+1; str[nn] && str[nn] != '"'; nn++) {
				if (str[nn] == '\\') nn++;
			}
			nn++;
		}
		else { /* Attribute value stops at first space or end of XML string */
			for (nn = p-str+1; str[nn] && !isspace(str[nn]) && str[nn] != '/' && str[nn] != '>'; nn++) ; /* Go to the end of the attribute value */
		}
		
		/* Here 'str[nn]' is either a '"' or a space */
		/* the attribute definition ('attrName="attr val"') is between 'str[n]' and 'str[nn]' */
		c = str[nn]; /* Backup character */
		str[nn] = 0; /* End string to call 'parse_XML_attribute' */
		if (!XML_parse_attribute(str+n, &xmlnode->attributes[xmlnode->n_attributes - 1])) goto parse_err;
		str[nn] = c;
		
		n = nn;
	}
	
	fprintf(stderr, "\nWE SHOULD NOT BE HERE!\n[%s]\n\n", str);
	
parse_err:
	XMLNode_free(xmlnode);
	return 0;
}

int XMLDoc_parse_file_SAX(const char* filename, const SAX_Callbacks* sax, void* user)
{
	FILE* f;
	char *line, *txt_end, *p;
	XMLNode node;
	int ret, exit, nline, sz, n0, ncr, tag_type;

	if (sax == NULL || filename == NULL || filename[0] == 0) return false;

	f = fopen(filename, "rt");
	if (f == NULL) return false;
	ret = true;
	exit = false;
	nline = 1; /* Line counter, starts at 1 */
	sz = 0; /* 'line' buffer size */
	XMLNode_init(&node);
	while ((n0 = read_line_alloc(f, &line, &sz, 0, 0, '>', true, '\n', &ncr)) != 0) {
		XMLNode_free(&node);
		for (p = line; *p && isspace(*p); p++) ; /* Checks if text is only spaces */
		if (*p == 0) break;
		nline += ncr;
/*printf("%4d: %s\n", nline, line);*/

		/* Get text for 'father' (i.e. what is before '<') */
		txt_end = strchr(line, '<');
		if (txt_end == NULL) { /* Missing tag start */
			fprintf(stderr, "%s:%d: ERROR: Unexpected tag end '>', without matching '<'!\n", filename, nline);
			ret = false;
			break;
		}
		/* First part of 'line' (before '<') is to be added to 'father->text' */
		*txt_end = 0; /* Makes 'line' be the text for 'father' */
		if (*line != 0 && (sax->new_text != NULL || sax->all_event != NULL)) {
			if (sax->new_text != NULL) {
				exit = !sax->new_text(str_unescape(line), user);
				if (exit) break;
			}
			if (sax->all_event != NULL) {
				exit = !sax->all_event(XML_EVENT_TEXT, NULL, line, NULL, user);
				if (exit) break;
			}
		}
		*txt_end = '<'; /* Restores tag start */
		tag_type = XML_parse_1string(txt_end, &node);
		if (tag_type == 0) {
			p = strchr(txt_end, '\n');
			if (p != NULL) *p = 0;
			fprintf(stderr, "%s:%d: SYNTAX ERROR (%s%s).\n", filename, nline, txt_end, p == NULL ? "" : "...");
			ret = false;
			break;
		}
		else if (tag_type == TAG_END && (sax->end_node != NULL || sax->all_event != NULL)) {
			if (sax->end_node != NULL) {
				exit = !sax->end_node(&node, user);
				if (exit) break;
			}
			if (sax->all_event != NULL) {
				exit = !sax->all_event(XML_EVENT_END, &node, NULL, NULL, user);
				if (exit) break;
			}
		}
		else { /* Add 'node' to 'father' children */
			/* If the line looks like a comment (or CDATA) but is not properly finished, loop until we find the end. */
			while (tag_type == TAG_PARTIAL) {
				n0 = read_line_alloc(f, &line, &sz, n0, 0, '>', true, '\n', &ncr); /* Go on reading the file from current position until next '>' */
				if (!n0) {
					ret = false;
					break;
				}
				nline += ncr;
				tag_type = XML_parse_1string(txt_end, &node);
			}
			if (ret == false) break;
			if (sax->start_node != NULL) exit = !sax->start_node(&node, user);
			if (exit) break;
			if (sax->all_event != NULL) exit = !sax->all_event(XML_EVENT_START, &node, NULL, NULL, user);
			if (exit) break;
			if (node.tag_type != TAG_FATHER && (sax->end_node != NULL || sax->all_event != NULL)) {
				if (sax->end_node != NULL) {
					exit = !sax->end_node(&node, user);
					if (exit) break;
				}
				if (sax->all_event != NULL) {
					exit = !sax->all_event(XML_EVENT_END, &node, NULL, NULL, user);
					if (exit) break;
				}
			}
		}
		if (exit == true || ret == false || feof(f)) break;
	}
	free(line);
	fclose(f);
	XMLNode_free(&node);

	return ret;
}

int DOMXMLDoc_node_start(const XMLNode* node, DOM_through_SAX* dom)
{
	XMLNode* new_node = XMLNode_alloc(1);

	if (new_node == NULL || !XMLNode_copy(new_node, node, false)) return false;

	if (dom->current == NULL) {
		int i = _add_node(&dom->doc->nodes, &dom->doc->n_nodes, new_node);

		if (i < 0) return false;
		if (dom->doc->i_root < 0 && node->tag_type == TAG_FATHER) dom->doc->i_root = i;
	}
	else {
		if (_add_node(&dom->current->children, &dom->current->n_children, new_node) < 0) return false;
	}

	new_node->father = dom->current;
	dom->current = new_node;

	return true;
}

int DOMXMLDoc_node_end(const XMLNode* node, DOM_through_SAX* dom)
{
	if (dom->current == NULL) return false;

	dom->current = dom->current->father;

	return true;
}

int DOMXMLDoc_node_text(const char* text, DOM_through_SAX* dom)
{
	char* p = (char*)text;

	while(*p && isspace(*p++)) ;
	if (*p == 0) return true; /* Only spaces */

	if (dom->current == NULL || (dom->current->text = strdup(text)) == NULL) return false;

	return true;
}

int XMLDoc_parse_file_DOM(const char* filename, XMLDoc* doc)
{
	struct _DOM_through_SAX dom;
	SAX_Callbacks sax;

	if (doc == NULL || filename == NULL || filename[0] == 0 || doc->init_value != XMLDOC_INIT_DONE) return false;

	strncpy(doc->filename, filename, sizeof(doc->filename));

	dom.doc = doc;
	dom.current = NULL;
	sax.start_node = (int(*)(const XMLNode*,void*))DOMXMLDoc_node_start;
	sax.end_node = (int(*)(const XMLNode*,void*))DOMXMLDoc_node_end;
	sax.new_text = (int(*)(const char*,void*))DOMXMLDoc_node_text;
	sax.all_event = NULL;

	if (!XMLDoc_parse_file_SAX(filename, &sax, &dom)) {
		XMLDoc_free(doc);

		return false;
	}

	return true;
}

int XMLDoc_parse_file(const char* filename, XMLDoc* doc)
{
	return XMLDoc_parse_file_DOM(filename, doc);
}
