// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/nine_box.h"

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/cairo_cached_surface.h"
#include "ui/gfx/image/image.h"
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
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  GdkPixbuf* pixbuf = rb.GetNativeImageNamed(image).ToGdkPixbuf();
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
  GtkAllocation allocation;
  gtk_widget_get_allocation(dst, &allocation);
  int w = allocation.width;
  int h = allocation.height;

  // In case the corners and edges don't all have the same width/height, we draw
  // the center first, and extend it out in all directions to the edges of the
  // images with the smallest widths/heights.  This way there will be no
  // unpainted areas, though some corners or edges might overlap the center.
  int i0w = images_[0] ? images_[0]->Width() : 0;
  int i2w = images_[2] ? images_[2]->Width() : 0;
  int i3w = images_[3] ? images_[3]->Width() : 0;
  int i5w = images_[5] ? images_[5]->Width() : 0;
  int i6w = images_[6] ? images_[6]->Width() : 0;
  int i8w = images_[8] ? images_[8]->Width() : 0;
  int i4x = std::min(std::min(i0w, i3w), i6w);
  int i4w = w - i4x - std::min(std::min(i2w, i5w), i8w);
  int i0h = images_[0] ? images_[0]->Height() : 0;
  int i1h = images_[1] ? images_[1]->Height() : 0;
  int i2h = images_[2] ? images_[2]->Height() : 0;
  int i6h = images_[6] ? images_[6]->Height() : 0;
  int i7h = images_[7] ? images_[7]->Height() : 0;
  int i8h = images_[8] ? images_[8]->Height() : 0;
  int i4y = std::min(std::min(i0h, i1h), i2h);
  int i4h = h - i4y - std::min(std::min(i6h, i7h), i8h);

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(gtk_widget_get_window(dst)));
  // For widgets that have their own window, the allocation (x,y) coordinates
  // are GdkWindow relative. For other widgets, the coordinates are relative
  // to their container.
  if (!gtk_widget_get_has_window(dst)) {
    // Transform our cairo from window to widget coordinates.
    cairo_translate(cr, allocation.x, allocation.y);
  }

  if (base::i18n::IsRTL()) {
    cairo_translate(cr, w, 0.0f);
    cairo_scale(cr, -1.0f, 1.0f);
  }

  // Top row, center image is horizontally tiled.
  DrawImage(cr, dst, images_[0], 0, 0, opacity);
  TileImage(cr, dst, images_[1], i0w, 0, w - i0w - i2w, i1h, opacity);
  DrawImage(cr, dst, images_[2], w - i2w, 0, opacity);

  // Center row, all images are vertically tiled, center is horizontally tiled.
  TileImage(cr, dst, images_[3], 0, i0h, i3w, h - i0h - i6h, opacity);
  TileImage(cr, dst, images_[4], i4x, i4y, i4w, i4h, opacity);
  TileImage(cr, dst, images_[5],  w - i5w, i2h, i5w, h - i2h - i8h, opacity);

  // Bottom row, center image is horizontally tiled.
  DrawImage(cr, dst, images_[6], 0, h - i6h, opacity);
  TileImage(cr, dst, images_[7], i6w, h - i7h, w - i6w - i8w, i7h, opacity);
  DrawImage(cr, dst, images_[8], w - i8w, h - i8h, opacity);

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
