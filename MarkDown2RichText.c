// Convert Markdown to RTF  © 2022 by Thomas Führinger, released under the terms of the GPL
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif

static char* path;
static int top_of_page = 1;

static char*
get_line(char** input)
{
	char* pos = *input;
	size_t length = strlen(pos);

	if (length == 0)
		return NULL;

	char* line = NULL;
	char* eol = strchr(pos, '\n');

	if (!eol) {
		line = (char*)malloc(length + 1);
		memcpy(line, pos, length);
		line[length] = 0;
		*input = pos + length;
	}
	else {
		length = eol - pos;
		line = (char*)malloc(length + 1);
		memcpy(line, pos, length);
		if (line[length - 1] == '\r')  // CRLF line terminator
			line[length - 1] = 0;
		else
			line[length] = 0;

		*input = pos + length + 1;
	}

	return line;
}

static char* rtf;
static size_t buffer_size;
static size_t buffer_left;

static void
append_buffer(const char* str)
{
	size_t length = strlen(str);
	if (buffer_left < length) {
		buffer_left += buffer_size;
		buffer_size = buffer_size * 2 + length;
		rtf = realloc(rtf, buffer_size);
	}
	strcat(rtf, str);
	buffer_left -= strlen(str);
}

#ifdef _WIN32
LPWSTR toW(const char* strTextUTF8);
#endif

static char hex[] = "00";
const static char* hex_digits = "0123456789ABCDEF";
static void
append_image(const char* file_name)
{
	FILE* file;
	char full_path[1024];
#ifdef _WIN32
	char full_path_[1024];
	sprintf(full_path, "%s\\%s", path, file_name);
	LPWSTR path_w = toW(full_path);
	file = _wfopen(path_w, L"rb");
	free(path_w);
#else
	sprintf(full_path, "%s/%s", path, file_name);
	file = fopen(full_path, "rb");
#endif

	if (file == NULL)
		return;
	int c;
	append_buffer("\\par\\qc{\\pict\\pngblip\\picscalex100\\picscaley100\\picscaled1 ");

	while ((c = getc(file)) != EOF) {
		*hex = hex_digits[c >> 4 & 0xF];
		*(hex + 1) = hex_digits[c & 0xF];
		append_buffer(hex);
	}
	append_buffer("\n}\\par\\ql\n");

	fclose(file);
}

static int bold_state = 0;
static int italic_state = 0;

static void
append_buffer_line(char* line)
{
	char* pos = line;
	while (*pos != 0)
	{
		if ((strncmp(pos, "*", 1) == 0 && strncmp(pos + 1, "*", 1) != 0) || (strncmp(pos, "_", 1) == 0 && strncmp(pos + 1, "_", 1) != 0)) {
			*pos = 0;
			append_buffer(line);
			if (italic_state) {
				append_buffer("\\i0 ");
				italic_state = 0;
			}
			else {
				append_buffer("\\i1 ");
				italic_state = 1;
			}
			pos += 1;
			line = pos;
		}
		else if ((strncmp(pos, "**", 2) == 0 && strncmp(pos + 2, "*", 1) != 0) || (strncmp(pos, "__", 2) == 0 && strncmp(pos + 2, "_", 1) != 0)) {
			*pos = 0;
			append_buffer(line);
			if (bold_state) {
				append_buffer("\\b0 ");
				bold_state = 0;
			}
			else {
				append_buffer("\\b1 ");
				bold_state = 1;
			}
			pos += 2;
			line = pos;
		}
		else if (strncmp(pos, "![", 2) == 0) {
			char* middle = strstr(pos + 2, "](") + 2;
			if (middle) {
				char* end = strstr(middle + 2, ")");
				*end = 0;
				append_image(middle);

				pos = end + 1;
				line = pos;
			}
		}
		else if (strncmp(pos, "[", 1) == 0) {
			char* middle = strstr(pos + 1, "](") + 2;
			if (middle) {
				char* end = strstr(middle + 2, ")");
				*end = 0;
				*(middle - 2) = 0;
				//MessageBoxA(0, middle, pos+1, 0);
				append_buffer("\\ul {\\field{\\*\\fldinst {HYPERLINK \"");
				append_buffer(middle);
				append_buffer("\" }}{\\fldrslt {");
				append_buffer(pos + 1);
				append_buffer("}}}\\ul0");

				pos = end + 1;
				line = pos;
			}
		}
		pos += 1;
	}
	append_buffer(line);
}

char*
markdown2rtf(const char* md, const char* img_path)
{
	buffer_size = strlen(md) * 4;
	buffer_left = buffer_size - 1;
	rtf = malloc(buffer_size);
	if (rtf == NULL)
		return "";
	rtf[0] = 0;
	char* pos = md;
	char* line;
	path = img_path;

	append_buffer("{\\rtf\\ansi\\f0\\fnil \\sl300 {\\fonttbl {\\f0 Arial;}}\n{\\colortbl;\\red5\\green10\\blue221;}\\fs22\n");

	while (line = get_line(&pos))
	{
		if (strncmp(line, "# ", 2) == 0) {
			if (top_of_page) {
				append_buffer("{\\fs32\\sb0\\sa100\\b1 ");
				top_of_page = 0;
			}
			else
				append_buffer("{\\par\\fs32\\sb100\\sa100\\b1 ");
			append_buffer(line + 2);
			append_buffer("\\par\\pard}\n");
		}
		else if (strncmp(line, "## ", 3) == 0) {
			append_buffer("{\\par\\fs26\\sb170\\sa100\\b1 ");
			append_buffer(line + 3);
			append_buffer("\\par\\pard}\n");
		}
		else if (strncmp(line, "### ", 4) == 0) {
			append_buffer("{\\par\\pard\\fs24\\sb150\\sa50\\b1 ");
			append_buffer(line + 4);
			append_buffer("\\par}\n");
		}
		else if (strncmp(line, "#### ", 5) == 0) {
			append_buffer("{\\par\\pard\\fs23\\sb120\\sa50\\b1 ");
			append_buffer(line + 5);
			append_buffer("\\par}\n");
		}
		else if (strncmp(line, "##### ", 6) == 0) {
			append_buffer("{\\par\\pard\\fs22\\sb100\\sa50\\b1 ");
			append_buffer(line + 6);
			append_buffer("\\par}\n");
		}
		else {
			int len = strlen(line);
			if (strncmp(line + len - 2, "  ", 2) == 0) {
				line[len - 2] = 0;
				append_buffer_line(line);
				append_buffer("\\par\n");
			}
			else
				append_buffer_line(line);
			append_buffer("\n");
		}
		free(line);
	}

	append_buffer("}\n\0");
	top_of_page = 1;
	return rtf;
}
