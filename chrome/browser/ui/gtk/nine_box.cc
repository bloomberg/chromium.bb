// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/nine_box.h"

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/cairo_cached_surface.h"
#include "ui/gfx/point.h"

namespace {

// Draw pixbuf |src| into |dst| at position (x, y).
void DrawImage(cairo_t* cr, GtkWidget* widget, gfx::Image* src,
                int x, int y, double alpha) {
  if (!src)
    return;

  src->ToCairo()->SetSource(cr, widget, x, y);
  cairo_paint_with_alpha(cr, alpha);
}

// Tile pixbuf |src| across |cr| at |x|, |y| for |width| and |height|.
void TileImage(cairo_t* cr, GtkWidget* widget, gfx::Image* src,
               int x, int y, int width, int height, double alpha) {
  if (!src)
    return;

  if (alpha == 1.0) {
    src->ToCairo()->SetSource(cr, widget, x, y);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr, x, y, width, height);
    cairo_fill(cr);
  } else {
    // Since there is no easy way to apply a mask to a fill operation, we create
    // a secondary surface and tile into that, then paint it with |alpha|.
    cairo_surface_t* target = cairo_get_target(cr);
    cairo_surface_t* surface = cairo_surface_create_similar(
        target, CAIRO_CONTENT_COLOR_ALPHA, width, height);
    cairo_t* tiled = cairo_create(surface);
    src->ToCairo()->SetSource(tiled, widget, 0, 0);
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
    : unref_images_on_destroy_(false) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  images_[0] = top_left ? &rb.GetNativeImageNamed(top_left) : NULL;
  images_[1] = top ? &rb.GetNativeImageNamed(top) : NULL;
  images_[2] = top_right ? &rb.GetNativeImageNamed(top_right) : NULL;
  images_[3] = left ? &rb.GetNativeImageNamed(left) : NULL;
  images_[4] = center ? &rb.GetNativeImageNamed(center) : NULL;
  images_[5] = right ? &rb.GetNativeImageNamed(right) : NULL;
  images_[6] = bottom_left ? &rb.GetNativeImageNamed(bottom_left) : NULL;
  images_[7] = bottom ? &rb.GetNativeImageNamed(bottom) : NULL;
  images_[8] = bottom_right ? &rb.GetNativeImageNamed(bottom_right) : NULL;
}

NineBox::NineBox(int image, int top_margin, int bottom_margin, int left_margin,
                 int right_margin)
    : unref_images_on_destroy_(true) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* pixbuf = rb.GetNativeImageNamed(image);
  int width = gdk_pixbuf_get_width(pixbuf);
  int height = gdk_pixbuf_get_height(pixbuf);
  int inset_width = left_margin + right_margin;
  int inset_height = top_margin + bottom_margin;

  images_[0] = new gfx::Image(gdk_pixbuf_new_subpixbuf(
      pixbuf, 0, 0, left_margin, top_margin));
  images_[1] = new gfx::Image(gdk_pixbuf_new_subpixbuf(
      pixbuf, left_margin, 0, width - inset_width, top_margin));
  images_[2] = new gfx::Image(gdk_pixbuf_new_subpixbuf(
      pixbuf, width - right_margin, 0, right_margin, top_margin));
  images_[3] = new gfx::Image(gdk_pixbuf_new_subpixbuf(
      pixbuf, 0, top_margin, left_margin, height - inset_height));
  images_[4] = new gfx::Image(gdk_pixbuf_new_subpixbuf(
      pixbuf, left_margin, top_margin, width - inset_width,
      height - inset_height));
  images_[5] = new gfx::Image(gdk_pixbuf_new_subpixbuf(
      pixbuf, width - right_margin, top_margin, right_margin,
      height - inset_height));
  images_[6] = new gfx::Image(gdk_pixbuf_new_subpixbuf(
      pixbuf, 0, height - bottom_margin, left_margin, bottom_margin));
  images_[7] = new gfx::Image(gdk_pixbuf_new_subpixbuf(
      pixbuf, left_margin, height - bottom_margin,
      width - inset_width, bottom_margin));
  images_[8] = new gfx::Image(gdk_pixbuf_new_subpixbuf(
      pixbuf, width - right_margin, height - bottom_margin,
      right_margin, bottom_margin));
}

NineBox::~NineBox() {
  if (unref_images_on_destroy_) {
    for (int i = 0; i < 9; i++) {
      delete images_[i];
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
  int x1 = gdk_pixbuf_get_width(images_[0]->ToGdkPixbuf());
  int y1 = gdk_pixbuf_get_height(images_[0]->ToGdkPixbuf());
  int x2 = images_[2] ? dst_width - gdk_pixbuf_get_width(
      images_[2]->ToGdkPixbuf()) : x1;
  int y2 = images_[6] ? dst_height - gdk_pixbuf_get_height(
      images_[6]->ToGdkPixbuf()) : y1;
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
  DrawImage(cr, dst, images_[0], 0, 0, opacity);
  TileImage(cr, dst, images_[1], x1, 0, x2 - x1, y1, opacity);
  DrawImage(cr, dst, images_[2], x2, 0, opacity);

  // Center row, all images are vertically tiled, center is horizontally tiled.
  TileImage(cr, dst, images_[3], 0, y1, x1, y2 - y1, opacity);
  TileImage(cr, dst, images_[4], x1, y1, x2 - x1, y2 - y1, opacity);
  TileImage(cr, dst, images_[5], x2, y1, dst_width - x2, y2 - y1, opacity);

  // Bottom row, center image is horizontally tiled.
  DrawImage(cr, dst, images_[6], 0, y2, opacity);
  TileImage(cr, dst, images_[7], x1, y2, x2 - x1, dst_height - y2, opacity);
  DrawImage(cr, dst, images_[8], x2, y2, opacity);

  cairo_destroy(cr);
}

void NineBox::ContourWidget(GtkWidget* widget) const {
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  int width = allocation.width;
  int height = allocation.height;
  int x1 = gdk_pixbuf_get_width(images_[0]->ToGdkPixbuf());
  int x2 = width - gdk_pixbuf_get_width(images_[2]->ToGdkPixbuf());

  // TODO(erg): Far in the future, when we're doing the real gtk3 porting, this
  // all needs to be switchted to pure cairo operations.

  // Paint the left and right sides.
  GdkBitmap* mask = gdk_pixmap_new(NULL, width, height, 1);
  gdk_pixbuf_render_threshold_alpha(images_[0]->ToGdkPixbuf(), mask,
                                    0, 0,
                                    0, 0, -1, -1,
                                    1);
  gdk_pixbuf_render_threshold_alpha(images_[2]->ToGdkPixbuf(), mask,
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
