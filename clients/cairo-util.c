/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2012 Intel Corporation
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <cairo.h>
#include "cairo-util.h"

#include <jpeglib.h>
#include <png.h>

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

void
surface_flush_device(cairo_surface_t *surface)
{
	cairo_device_t *device;

	device = cairo_surface_get_device(surface);
	if (device)
		cairo_device_flush(device);
}

void
blur_surface(cairo_surface_t *surface, int margin)
{
	int32_t width, height, stride, x, y, z, w;
	uint8_t *src, *dst;
	uint32_t *s, *d, a, p;
	int i, j, k, size, half;
	uint32_t kernel[49];
	double f;

	size = ARRAY_LENGTH(kernel);
	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);
	stride = cairo_image_surface_get_stride(surface);
	src = cairo_image_surface_get_data(surface);

	dst = malloc(height * stride);

	half = size / 2;
	a = 0;
	for (i = 0; i < size; i++) {
		f = (i - half);
		kernel[i] = exp(- f * f / ARRAY_LENGTH(kernel)) * 10000;
		a += kernel[i];
	}

	for (i = 0; i < height; i++) {
		s = (uint32_t *) (src + i * stride);
		d = (uint32_t *) (dst + i * stride);
		for (j = 0; j < width; j++) {
			if (margin < j && j < width - margin) {
				d[j] = s[j];
				continue;
			}

			x = 0;
			y = 0;
			z = 0;
			w = 0;
			for (k = 0; k < size; k++) {
				if (j - half + k < 0 || j - half + k >= width)
					continue;
				p = s[j - half + k];

				x += (p >> 24) * kernel[k];
				y += ((p >> 16) & 0xff) * kernel[k];
				z += ((p >> 8) & 0xff) * kernel[k];
				w += (p & 0xff) * kernel[k];
			}
			d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
		}
	}

	for (i = 0; i < height; i++) {
		s = (uint32_t *) (dst + i * stride);
		d = (uint32_t *) (src + i * stride);
		for (j = 0; j < width; j++) {
			if (margin <= i && i < height - margin) {
				d[j] = s[j];
				continue;
			}

			x = 0;
			y = 0;
			z = 0;
			w = 0;
			for (k = 0; k < size; k++) {
				if (i - half + k < 0 || i - half + k >= height)
					continue;
				s = (uint32_t *) (dst + (i - half + k) * stride);
				p = s[j];

				x += (p >> 24) * kernel[k];
				y += ((p >> 16) & 0xff) * kernel[k];
				z += ((p >> 8) & 0xff) * kernel[k];
				w += (p & 0xff) * kernel[k];
			}
			d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
		}
	}

	free(dst);
	cairo_surface_mark_dirty(surface);
}

void
tile_mask(cairo_t *cr, cairo_surface_t *surface,
	  int x, int y, int width, int height, int margin, int top_margin)
{
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;
	int i, fx, fy, vmargin;

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	pattern = cairo_pattern_create_for_surface (surface);

	for (i = 0; i < 4; i++) {
		fx = i & 1;
		fy = i >> 1;

		cairo_matrix_init_translate(&matrix,
					    -x + fx * (128 - width),
					    -y + fy * (128 - height));
		cairo_pattern_set_matrix(pattern, &matrix);

		if (fy)
			vmargin = margin;
		else
			vmargin = top_margin;

		cairo_reset_clip(cr);
		cairo_rectangle(cr,
				x + fx * (width - margin),
				y + fy * (height - vmargin),
				margin, vmargin);
		cairo_clip (cr);
		cairo_mask(cr, pattern);
	}

	/* Top strecth */
	cairo_matrix_init_translate(&matrix, 64, 0);
	cairo_matrix_scale(&matrix, 64.0 / (width - 2 * margin), 1);
	cairo_matrix_translate(&matrix, -x - width / 2, -y);
	cairo_pattern_set_matrix(pattern, &matrix);
	cairo_rectangle(cr, x + margin, y, width - 2 * margin, margin);

	cairo_reset_clip(cr);
	cairo_rectangle(cr,
			x + margin,
			y,
			width - 2 * margin, margin);
	cairo_clip (cr);
	cairo_mask(cr, pattern);

	/* Bottom strecth */
	cairo_matrix_translate(&matrix, 0, -height + 128);
	cairo_pattern_set_matrix(pattern, &matrix);

	cairo_reset_clip(cr);
	cairo_rectangle(cr, x + margin, y + height - margin,
			width - 2 * margin, margin);
	cairo_clip (cr);
	cairo_mask(cr, pattern);

	/* Left strecth */
	cairo_matrix_init_translate(&matrix, 0, 64);
	cairo_matrix_scale(&matrix, 1, 64.0 / (height - 2 * margin));
	cairo_matrix_translate(&matrix, -x, -y - height / 2);
	cairo_pattern_set_matrix(pattern, &matrix);
	cairo_reset_clip(cr);
	cairo_rectangle(cr, x, y + margin, margin, height - 2 * margin);
	cairo_clip (cr);
	cairo_mask(cr, pattern);

	/* Right strecth */
	cairo_matrix_translate(&matrix, -width + 128, 0);
	cairo_pattern_set_matrix(pattern, &matrix);
	cairo_rectangle(cr, x + width - margin, y + margin,
			margin, height - 2 * margin);
	cairo_reset_clip(cr);
	cairo_clip (cr);
	cairo_mask(cr, pattern);

	cairo_pattern_destroy(pattern);
	cairo_reset_clip(cr);
}

void
tile_source(cairo_t *cr, cairo_surface_t *surface,
	    int x, int y, int width, int height, int margin, int top_margin)
{
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;
	int i, fx, fy, vmargin;

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	pattern = cairo_pattern_create_for_surface (surface);
	cairo_set_source(cr, pattern);
	cairo_pattern_destroy(pattern);

	for (i = 0; i < 4; i++) {
		fx = i & 1;
		fy = i >> 1;

		cairo_matrix_init_translate(&matrix,
					    -x + fx * (128 - width),
					    -y + fy * (128 - height));
		cairo_pattern_set_matrix(pattern, &matrix);

		if (fy)
			vmargin = margin;
		else
			vmargin = top_margin;

		cairo_rectangle(cr,
				x + fx * (width - margin),
				y + fy * (height - vmargin),
				margin, vmargin);
		cairo_fill(cr);
	}

	/* Top strecth */
	cairo_matrix_init_translate(&matrix, 64, 0);
	cairo_matrix_scale(&matrix, 64.0 / (width - 2 * margin), 1);
	cairo_matrix_translate(&matrix, -x - width / 2, -y);
	cairo_pattern_set_matrix(pattern, &matrix);
	cairo_rectangle(cr, x + margin, y, width - 2 * margin, top_margin);
	cairo_fill(cr);

	/* Bottom strecth */
	cairo_matrix_translate(&matrix, 0, -height + 128);
	cairo_pattern_set_matrix(pattern, &matrix);
	cairo_rectangle(cr, x + margin, y + height - margin,
			width - 2 * margin, margin);
	cairo_fill(cr);

	/* Left strecth */
	cairo_matrix_init_translate(&matrix, 0, 64);
	cairo_matrix_scale(&matrix, 1, 64.0 / (height - margin - top_margin));
	cairo_matrix_translate(&matrix, -x, -y - height / 2);
	cairo_pattern_set_matrix(pattern, &matrix);
	cairo_rectangle(cr, x, y + top_margin,
			margin, height - margin - top_margin);
	cairo_fill(cr);

	/* Right strecth */
	cairo_matrix_translate(&matrix, -width + 128, 0);
	cairo_pattern_set_matrix(pattern, &matrix);
	cairo_rectangle(cr, x + width - margin, y + top_margin,
			margin, height - margin - top_margin);
	cairo_fill(cr);
}

void
rounded_rect(cairo_t *cr, int x0, int y0, int x1, int y1, int radius)
{
	cairo_move_to(cr, x0, y0 + radius);
	cairo_arc(cr, x0 + radius, y0 + radius, radius, M_PI, 3 * M_PI / 2);
	cairo_line_to(cr, x1 - radius, y0);
	cairo_arc(cr, x1 - radius, y0 + radius, radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to(cr, x1, y1 - radius);
	cairo_arc(cr, x1 - radius, y1 - radius, radius, 0, M_PI / 2);
	cairo_line_to(cr, x0 + radius, y1);
	cairo_arc(cr, x0 + radius, y1 - radius, radius, M_PI / 2, M_PI);
	cairo_close_path(cr);
}

static void
swizzle_row(JSAMPLE *row, JDIMENSION width)
{
	JSAMPLE *s;
	uint32_t *d;

	s = row + (width - 1) * 3;
	d = (uint32_t *) (row + (width - 1) * 4);
	while (s >= row) {
		*d = 0xff000000 | (s[0] << 16) | (s[1] << 8) | (s[2] << 0);
		s -= 3;
		d--;
	}
}

static void
error_exit(j_common_ptr cinfo)
{
	longjmp(cinfo->client_data, 1);
}

static cairo_surface_t *
load_jpeg(FILE *fp)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	int stride, i, first;
	JSAMPLE *data, *rows[4];
	jmp_buf env;

	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = error_exit;
	cinfo.client_data = env;
	if (setjmp(env))
		return NULL;

	jpeg_create_decompress(&cinfo);

	jpeg_stdio_src(&cinfo, fp);

	jpeg_read_header(&cinfo, TRUE);

	cinfo.out_color_space = JCS_RGB;
	jpeg_start_decompress(&cinfo);

	stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,
					       cinfo.output_width);
	data = malloc(stride * cinfo.output_height);
	if (data == NULL) {
		fprintf(stderr, "couldn't allocate image data\n");
		return NULL;
	}

	while (cinfo.output_scanline < cinfo.output_height) {
		first = cinfo.output_scanline;
		for (i = 0; i < ARRAY_LENGTH(rows); i++)
			rows[i] = data + (first + i) * stride;

		jpeg_read_scanlines(&cinfo, rows, ARRAY_LENGTH(rows));
		for (i = 0; first + i < cinfo.output_scanline; i++)
			swizzle_row(rows[i], cinfo.output_width);
	}

	jpeg_finish_decompress(&cinfo);

	jpeg_destroy_decompress(&cinfo);

	return cairo_image_surface_create_for_data (data,
						    CAIRO_FORMAT_RGB24,
						    cinfo.output_width,
						    cinfo.output_height,
						    stride);
}

static inline int
multiply_alpha(int alpha, int color)
{
    int temp = (alpha * color) + 0x80;

    return ((temp + (temp >> 8)) >> 8);
}

static void
premultiply_data(png_structp   png,
		 png_row_infop row_info,
		 png_bytep     data)
{
    unsigned int i;
    png_bytep p;

    for (i = 0, p = data; i < row_info->rowbytes; i += 4, p += 4) {
	png_byte  alpha = p[3];
	uint32_t w;

	if (alpha == 0) {
		w = 0;
	} else {
		png_byte red   = p[0];
		png_byte green = p[1];
		png_byte blue  = p[2];

		if (alpha != 0xff) {
			red   = multiply_alpha(alpha, red);
			green = multiply_alpha(alpha, green);
			blue  = multiply_alpha(alpha, blue);
		}
		w = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
	}

	* (uint32_t *) p = w;
    }
}

static void
read_func(png_structp png, png_bytep data, png_size_t size)
{
	FILE *fp = png_get_io_ptr(png);

	if (fread(data, 1, size, fp) < 0)
		png_error(png, NULL);
}

static void
png_error_callback(png_structp png, png_const_charp error_msg)
{
    longjmp (png_jmpbuf (png), 1);
}

static cairo_surface_t *
load_png(FILE *fp)
{
	png_struct *png;
	png_info *info;
	png_byte *data = NULL;
	png_byte **row_pointers = NULL;
	png_uint_32 width, height;
	int depth, color_type, interlace, stride;
	unsigned int i;

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
				     png_error_callback, NULL);
	if (!png)
		return NULL;

	info = png_create_info_struct(png);
	if (!info) {
		png_destroy_read_struct(&png, &info, NULL);
		return NULL;
	}

	if (setjmp(png_jmpbuf(png))) {
		if (data)
			free(data);
		if (row_pointers)
			free(row_pointers);
		png_destroy_read_struct(&png, &info, NULL);
		return NULL;
	}

	png_set_read_fn(png, fp, read_func);
	png_read_info(png, info);
	png_get_IHDR(png, info,
		     &width, &height, &depth,
		     &color_type, &interlace, NULL, NULL);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);

	if (color_type == PNG_COLOR_TYPE_GRAY)
		png_set_expand_gray_1_2_4_to_8(png);

	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	if (depth == 16)
		png_set_strip_16(png);

	if (depth < 8)
		png_set_packing(png);

	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);

	if (interlace != PNG_INTERLACE_NONE)
		png_set_interlace_handling(png);

	png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	png_set_read_user_transform_fn(png, premultiply_data);
	png_read_update_info(png, info);
	png_get_IHDR(png, info,
		     &width, &height, &depth,
		     &color_type, &interlace, NULL, NULL);


	stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);
	data = malloc(stride * height);
	if (!data) {
		png_destroy_read_struct(&png, &info, NULL);
		return NULL;
	}

	row_pointers = malloc(height * sizeof row_pointers[0]);
	if (row_pointers == NULL) {
		free(data);
		png_destroy_read_struct(&png, &info, NULL);
		return NULL;
	}

	for (i = 0; i < height; i++)
		row_pointers[i] = &data[i * stride];

	png_read_image(png, row_pointers);
	png_read_end(png, info);

	free(row_pointers);
	png_destroy_read_struct(&png, &info, NULL);

	return cairo_image_surface_create_for_data (data,
						    CAIRO_FORMAT_RGB24,
						    width, height, stride);
}

cairo_surface_t *
load_image(const char *filename)
{
	cairo_surface_t *surface;
	static const unsigned char png_header[] = { 0x89, 'P', 'N', 'G' };
	static const unsigned char jpeg_header[] = { 0xff, 0xd8 };
	unsigned char header[4];
	FILE *fp;

	fp = fopen(filename, "rb");
	if (fp == NULL)
		return NULL;

	fread(header, sizeof header, 1, fp);
	rewind(fp);
	if (memcmp(header, png_header, sizeof png_header) == 0)
		surface = load_png(fp);
	else if (memcmp(header, jpeg_header, sizeof jpeg_header) == 0)
		surface = load_jpeg(fp);
	else {
		fprintf(stderr, "unrecognized file header for %s: "
			"0x%02x 0x%02x 0x%02x 0x%02x\n",
			filename, header[0], header[1], header[2], header[3]);
		surface = NULL;
	}

	fclose(fp);

	return surface;
}
