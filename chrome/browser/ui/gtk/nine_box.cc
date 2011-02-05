// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/nine_box.h"

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/point.h"

namespace {

// Draw pixbuf |src| into |dst| at position (x, y).
void DrawPixbuf(cairo_t* cr, GdkPixbuf* src, int x, int y, double alpha) {
  gdk_cairo_set_source_pixbuf(cr, src, x, y);
  cairo_paint_with_alpha(cr, alpha);
}

// Tile pixbuf |src| across |cr| at |x|, |y| for |width| and |height|.
void TileImage(cairo_t* cr, GdkPixbuf* src,
               int x, int y, int width, int height, double alpha) {
  if (alpha == 1.0) {
    gdk_cairo_set_source_pixbuf(cr, src, x, y);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr, x, y, width, height);
    cairo_fill(cr);
  } else {
    // Since there is no easy way to apply a mask to a fill operation, we create
    // a secondary surface and tile into that, then paint it with |alpha|.
    cairo_surface_t* surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* tiled = cairo_create(surface);
    gdk_cairo_set_source_pixbuf(tiled, src, 0, 0);
    cairo_pattern_set_extend(cairo_get_source(tiled), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(tiled, 0, 0, width, height);
    cairo_fill(tiled);

    cairo_set_source_surface(cr, surface, x, y);
    cairo_paint_with_alpha(cr, alpha);

    cairo_destroy(tiled);
    cairo_surface_destroy(surface);
  }
}

}  // namespace

NineBox::NineBox(int top_left, int top, int top_right, int left, int center,
                 int right, int bottom_left, int bottom, int bottom_right)
    : unref_pixbufs_on_destroy_(false) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  images_[0] = top_left ? rb.GetPixbufNamed(top_left) : NULL;
  images_[1] = top ? rb.GetPixbufNamed(top) : NULL;
  images_[2] = top_right ? rb.GetPixbufNamed(top_right) : NULL;
  images_[3] = left ? rb.GetPixbufNamed(left) : NULL;
  images_[4] = center ? rb.GetPixbufNamed(center) : NULL;
  images_[5] = right ? rb.GetPixbufNamed(right) : NULL;
  images_[6] = bottom_left ? rb.GetPixbufNamed(bottom_left) : NULL;
  images_[7] = bottom ? rb.GetPixbufNamed(bottom) : NULL;
  images_[8] = bottom_right ? rb.GetPixbufNamed(bottom_right) : NULL;
}

NineBox::NineBox(int image, int top_margin, int bottom_margin, int left_margin,
                 int right_margin)
    : unref_pixbufs_on_destroy_(true) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* pixbuf = rb.GetPixbufNamed(image);
  int width = gdk_pixbuf_get_width(pixbuf);
  int height = gdk_pixbuf_get_height(pixbuf);
  int inset_width = left_margin + right_margin;
  int inset_height = top_margin + bottom_margin;

  images_[0] = gdk_pixbuf_new_subpixbuf(pixbuf, 0, 0, left_margin, top_margin);
  images_[1] = gdk_pixbuf_new_subpixbuf(pixbuf, left_margin, 0,
                                        width - inset_width, top_margin);
  images_[2] = gdk_pixbuf_new_subpixbuf(pixbuf, width - right_margin, 0,
                                        right_margin, top_margin);
  images_[3] = gdk_pixbuf_new_subpixbuf(pixbuf, 0, top_margin,
                                        left_margin, height - inset_height);
  images_[4] = gdk_pixbuf_new_subpixbuf(pixbuf, left_margin, top_margin,
                                        width - inset_width,
                                        height - inset_height);
  images_[5] = gdk_pixbuf_new_subpixbuf(pixbuf, width - right_margin,
                                        top_margin, right_margin,
                                        height - inset_height);
  images_[6] = gdk_pixbuf_new_subpixbuf(pixbuf, 0, height - bottom_margin,
                                        left_margin, bottom_margin);
  images_[7] = gdk_pixbuf_new_subpixbuf(pixbuf, left_margin,
                                        height - bottom_margin,
                                        width - inset_width, bottom_margin);
  images_[8] = gdk_pixbuf_new_subpixbuf(pixbuf, width - right_margin,
                                        height - bottom_margin,
                                        right_margin, bottom_margin);
}

NineBox::~NineBox() {
  if (unref_pixbufs_on_destroy_) {
    for (int i = 0; i < 9; i++) {
      g_object_unref(images_[i]);
    }
  }
}

void NineBox::RenderToWidget(GtkWidget* dst) const {
  RenderToWidgetWithOpacity(dst, 1.0);
}

void NineBox::RenderToWidgetWithOpacity(GtkWidget* dst, double opacity) const {
  int dst_width = dst->allocation.width;
  int dst_height = dst->allocation.height;

  // The upper-left and lower-right corners of the center square in the
  // rendering of the ninebox.
  int x1 = gdk_pixbuf_get_width(images_[0]);
  int y1 = gdk_pixbuf_get_height(images_[0]);
  int x2 = images_[2] ? dst_width - gdk_pixbuf_get_width(images_[2]) : x1;
  int y2 = images_[6] ? dst_height - gdk_pixbuf_get_height(images_[6]) : y1;
  // Paint nothing if there's not enough room.
  if (x2 < x1 || y2 < y1)
    return;

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(dst->window));
  // For widgets that have their own window, the allocation (x,y) coordinates
  // are GdkWindow relative. For other widgets, the coordinates are relative
  // to their container.
  if (GTK_WIDGET_NO_WINDOW(dst)) {
    // Transform our cairo from window to widget coordinates.
    cairo_translate(cr, dst->allocation.x, dst->allocation.y);
  }

  if (base::i18n::IsRTL()) {
    cairo_translate(cr, dst_width, 0.0f);
    cairo_scale(cr, -1.0f, 1.0f);
  }

  // Top row, center image is horizontally tiled.
  if (images_[0])
    DrawPixbuf(cr, images_[0], 0, 0, opacity);
  if (images_[1])
    TileImage(cr, images_[1], x1, 0, x2 - x1, y1, opacity);
  if (images_[2])
    DrawPixbuf(cr, images_[2], x2, 0, opacity);

  // Center row, all images are vertically tiled, center is horizontally tiled.
  if (images_[3])
    TileImage(cr, images_[3], 0, y1, x1, y2 - y1, opacity);
  if (images_[4])
    TileImage(cr, images_[4], x1, y1, x2 - x1, y2 - y1, opacity);
  if (images_[5])
    TileImage(cr, images_[5], x2, y1, dst_width - x2, y2 - y1, opacity);

  // Bottom row, center image is horizontally tiled.
  if (images_[6])
    DrawPixbuf(cr, images_[6], 0, y2, opacity);
  if (images_[7])
    TileImage(cr, images_[7], x1, y2, x2 - x1, dst_height - y2, opacity);
  if (images_[8])
    DrawPixbuf(cr, images_[8], x2, y2, opacity);

  cairo_destroy(cr);
}

void NineBox::RenderTopCenterStrip(cairo_t* cr, int x, int y,
                                   int width) const {
  const int height = gdk_pixbuf_get_height(images_[1]);
  TileImage(cr, images_[1], x, y, width, height, 1.0);
}

void NineBox::ChangeWhiteToTransparent() {
  for (int image_idx = 0; image_idx < 9; ++image_idx) {
    GdkPixbuf* pixbuf = images_[image_idx];
    if (!pixbuf)
      continue;

    if (!gdk_pixbuf_get_has_alpha(pixbuf))
      continue;

    guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);

    if (width * 4 > rowstride) {
      NOTREACHED();
      continue;
    }

    for (int i = 0; i < height; ++i) {
      for (int j = 0; j < width; ++j) {
         guchar* pixel = &pixels[i * rowstride + j * 4];
         if (pixel[0] == 0xff && pixel[1] == 0xff && pixel[2] == 0xff) {
           pixel[3] = 0;
         }
      }
    }
  }
}

void NineBox::ContourWidget(GtkWidget* widget) const {
  int width = widget->allocation.width;
  int height = widget->allocation.height;
  int x1 = gdk_pixbuf_get_width(images_[0]);
  int x2 = width - gdk_pixbuf_get_width(images_[2]);

  // Paint the left and right sides.
  GdkBitmap* mask = gdk_pixmap_new(NULL, width, height, 1);
  gdk_pixbuf_render_threshold_alpha(images_[0], mask,
                                    0, 0,
                                    0, 0, -1, -1,
                                    1);
  gdk_pixbuf_render_threshold_alpha(images_[2], mask,
                                    0, 0,
                                    x2, 0, -1, -1,
                                    1);

  // Assume no transparency in the middle rectangle.
  cairo_t* cr = gdk_cairo_create(mask);
  cairo_rectangle(cr, x1, 0, x2 - x1, height);
  cairo_fill(cr);
  cairo_destroy(cr);

  // Mask the widget's window's shape.
  if (!base::i18n::IsRTL()) {
    gtk_widget_shape_combine_mask(widget, mask, 0, 0);
  } else {
    GdkBitmap* flipped_mask = gdk_pixmap_new(NULL, width, height, 1);
    cairo_t* flipped_cr = gdk_cairo_create(flipped_mask);

    // Clear the target bitmap.
    cairo_set_operator(flipped_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(flipped_cr);

    // Apply flipping transformation.
    cairo_translate(flipped_cr, width, 0.0f);
    cairo_scale(flipped_cr, -1.0f, 1.0f);

    // Paint the source bitmap onto the target.
    cairo_set_operator(flipped_cr, CAIRO_OPERATOR_SOURCE);
    gdk_cairo_set_source_pixmap(flipped_cr, mask, 0, 0);
    cairo_paint(flipped_cr);
    cairo_destroy(flipped_cr);

    // Mask the widget.
    gtk_widget_shape_combine_mask(widget, flipped_mask, 0, 0);
    g_object_unref(flipped_mask);
  }

  g_object_unref(mask);
}
