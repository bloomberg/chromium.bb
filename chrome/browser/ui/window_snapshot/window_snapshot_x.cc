// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_snapshot/window_snapshot.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "base/logging.h"
#include "gfx/rect.h"
#include "ui/base/x/x11_util.h"

namespace browser {

static cairo_status_t SnapshotCallback(
    void *closure, const unsigned char *data, unsigned int length) {
  std::vector<unsigned char>* png_representation =
      static_cast<std::vector<unsigned char>*>(closure);

  size_t old_size = png_representation->size();
  png_representation->resize(old_size + length);
  memcpy(&(*png_representation)[old_size], data, length);
  return CAIRO_STATUS_SUCCESS;
}

gfx::Rect GrabWindowSnapshot(gfx::NativeWindow gtk_window,
                             std::vector<unsigned char>* png_representation) {
  GdkWindow* gdk_window = GTK_WIDGET(gtk_window)->window;
  Display* display = GDK_WINDOW_XDISPLAY(gdk_window);
  XID win = GDK_WINDOW_XID(gdk_window);
  XWindowAttributes attr;
  if (XGetWindowAttributes(display, win, &attr) == 0) {
    LOG(ERROR) << "Couldn't get window attributes";
    return gfx::Rect();
  }
  XImage* image = XGetImage(
      display, win, 0, 0, attr.width, attr.height, AllPlanes, ZPixmap);
  if (!image) {
    LOG(ERROR) << "Couldn't get image";
    return gfx::Rect();
  }
  if (image->depth != 24) {
    LOG(ERROR)<< "Unsupported image depth " << image->depth;
    return gfx::Rect();
  }
  cairo_surface_t* surface =
      cairo_image_surface_create_for_data(
          reinterpret_cast<unsigned char*>(image->data),
          CAIRO_FORMAT_RGB24,
          image->width,
          image->height,
          image->bytes_per_line);

  if (!surface) {
    LOG(ERROR) << "Unable to create Cairo surface from XImage data";
    return gfx::Rect();
  }
  cairo_surface_write_to_png_stream(
      surface, SnapshotCallback, png_representation);
  cairo_surface_destroy(surface);

  return gfx::Rect(image->width, image->height);
}

}  // namespace browser
