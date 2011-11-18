/*
 * Copyright © 2011 Collabora, Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "wscreensaver-glue.h"

double frand(double f)
{
	double r = random();
	return r * f / (double)RAND_MAX;
}

void clear_gl_error(void)
{
	while (glGetError() != GL_NO_ERROR)
		;
}

void check_gl_error(const char *msg)
{
	const char *emsg;
	int err = glGetError();

	switch (err)
	{
	case GL_NO_ERROR:
		return;

	#define ERR(tok) case tok: emsg = #tok; break;
	ERR(GL_INVALID_ENUM)
	ERR(GL_INVALID_VALUE)
	ERR(GL_INVALID_OPERATION)
	ERR(GL_STACK_OVERFLOW)
	ERR(GL_STACK_UNDERFLOW)
	ERR(GL_OUT_OF_MEMORY)
	#undef ERR

	default:
		fprintf(stderr, "%s: %s: unknown GL error 0x%04x\n",
			progname, msg, err);
		exit(1);
	}

	fprintf(stderr, "%s: %s: GL error %s\n", progname, msg, emsg);
	exit(1);
}

static void
read_xpm_color(uint32_t *ctable, const char *line)
{
	unsigned char key;
	char cstr[10];
	char *end;
	uint32_t value;

	if (sscanf(line, "%1c c %9s", &key, cstr) < 2) {
		fprintf(stderr, "%s: error in XPM color definition '%s'\n",
			progname, line);
		return;
	}

	value = strtol(&cstr[1], &end, 16);

	if (strcmp(cstr, "None") == 0)
		ctable[key] = 0x00ffffff;
	else if (cstr[0] != '#' || !(cstr[1] != '\0' && *end == '\0')) {
		fprintf(stderr, "%s: error interpreting XPM color '%s'\n",
			progname, cstr);
		return;
	}

	ctable[key] = value | 0xff000000;
}

static void
read_xpm_row(char *data, const char *line, uint32_t *ctable, int width)
{
	uint32_t *pixel = (uint32_t *)data;
	uint8_t *p = (uint8_t *)line;
	int i;

	for (i = 0; i < width; ++i)
		pixel[i] = ctable[p[i]];
}

XImage *xpm_to_ximage(char **xpm_data)
{
	XImage *xi;
	int colors;
	int cpp;
	int i;
	uint32_t ctable[256] = { 0 };

	xi = malloc(sizeof *xi);
	if (!xi)
		return NULL;
	xi->data = NULL;

	if (sscanf(xpm_data[0], "%d %d %d %d", &xi->width,
					&xi->height, &colors, &cpp) < 4)
		goto errout;

	if (xi->width < 1 || xi->height < 1 || cpp != 1)
		goto errout;

	xi->bytes_per_line = xi->width * sizeof(uint32_t);
	xi->data = malloc(xi->height * xi->bytes_per_line);
	if (!xi->data)
		goto errout;

	for (i = 0; i < colors; ++i)
		read_xpm_color(ctable, xpm_data[i + 1]);

	for (i = 0; i < xi->height; ++i)
		read_xpm_row(xi->data + i * xi->bytes_per_line,
			     xpm_data[i + colors + 1], ctable, xi->width);

	return xi;

errout:
	fprintf(stderr, "%s: error processing XPM data.\n", progname);
	XDestroyImage(xi);
	return NULL;
}

void XDestroyImage(XImage *xi)
{
	free(xi->data);
	free(xi);
}
