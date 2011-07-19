// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/cairo_cached_surface.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/logging.h"

CairoCachedSurface::CairoCachedSurface() : pixbuf_(NULL), surface_(NULL) {
}

CairoCachedSurface::~CairoCachedSurface() {
  if (surface_)
    cairo_surface_destroy(surface_);

  if (pixbuf_)
    g_object_unref(pixbuf_);
}

int CairoCachedSurface::Width() const {
  return pixbuf_ ? gdk_pixbuf_get_width(pixbuf_) : -1;
}

int CairoCachedSurface::Height() const {
  return pixbuf_ ? gdk_pixbuf_get_height(pixbuf_) : -1;
}

void CairoCachedSurface::UsePixbuf(GdkPixbuf* pixbuf) {
  if (surface_) {
    cairo_surface_destroy(surface_);
    surface_ = NULL;
  }

  if (pixbuf)
    g_object_ref(pixbuf);

  if (pixbuf_)
    g_object_unref(pixbuf_);

  pixbuf_ = pixbuf;
}

void CairoCachedSurface::SetSource(cairo_t* cr, int x, int y) {
  DCHECK(pixbuf_);
  DCHECK(cr);

  if (!surface_) {
    // First time here since last UsePixbuf call. Generate the surface.
    cairo_surface_t* target = cairo_get_target(cr);
    surface_ = cairo_surface_create_similar(
        target,
        CAIRO_CONTENT_COLOR_ALPHA,
        gdk_pixbuf_get_width(pixbuf_),
        gdk_pixbuf_get_height(pixbuf_));

    DCHECK(surface_);
#if !defined(NDEBUG)
    int surface_type = cairo_surface_get_type(surface_);
    DCHECK(surface_type == CAIRO_SURFACE_TYPE_XLIB ||
           surface_type == CAIRO_SURFACE_TYPE_XCB ||
           surface_type == CAIRO_SURFACE_TYPE_IMAGE);
#endif

    cairo_t* copy_cr = cairo_create(surface_);
    gdk_cairo_set_source_pixbuf(copy_cr, pixbuf_, 0, 0);
    cairo_paint(copy_cr);
    cairo_destroy(copy_cr);
  }

  cairo_set_source_surface(cr, surface_, x, y);
}
