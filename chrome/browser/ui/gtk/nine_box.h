// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_NINE_BOX_H_
#define CHROME_BROWSER_UI_GTK_NINE_BOX_H_
#pragma once

#include <gtk/gtk.h>

namespace gfx {
class Image;
}

// A NineBox manages a set of source images representing a 3x3 grid, where
// non-corner images can be tiled to make a larger image.  It's used to
// use bitmaps for constructing image-based resizable widgets like buttons.
//
// If you want just a vertical image that stretches in height but is fixed
// in width, only pass in images for the left column (leave others NULL).
// Similarly, for a horizontal image that stretches in width but is fixed in
// height, only pass in images for the top row.
class NineBox {
 public:
  // Construct a NineBox with nine images.  Images are specified using resource
  // ids that will be passed to the resource bundle.  Use 0 for no image.
  NineBox(int top_left, int top, int top_right, int left, int center, int right,
          int bottom_left, int bottom, int bottom_right);

  // Construct a NineBox from a single image and insets indicating the sizes
  // of the edges and corners.
  NineBox(int image, int top_margin, int bottom_margin, int left_margin,
          int right_margin);
  ~NineBox();

  // Render the NineBox to |dst|.
  // The images will be tiled to fit into the widget.
  void RenderToWidget(GtkWidget* dst) const;

  // As above, but rendered partially transparent.
  void RenderToWidgetWithOpacity(GtkWidget* dst, double opacity) const;

  // Set the shape of |widget| to match that of the ninebox. Note that |widget|
  // must have its own window and be allocated. Also, currently only the top
  // three images are used.
  // TODO(estade): extend this function to use all 9 images (if it's ever
  // needed).
  void ContourWidget(GtkWidget* widget) const;

 private:
  gfx::Image* images_[9];
  bool unref_images_on_destroy_;
};

#endif  // CHROME_BROWSER_UI_GTK_NINE_BOX_H_
