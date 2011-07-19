// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CAIRO_CACHED_SURFACE_H_
#define CHROME_BROWSER_UI_GTK_CAIRO_CACHED_SURFACE_H_
#pragma once

typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

// A helper class that takes a GdkPixbuf* and renders it to the screen. Unlike
// gdk_cairo_set_source_pixbuf(), CairoCachedSurface assumes that the pixbuf is
// immutable after UsePixbuf() is called and can be sent to the X server
// once. From then on, that cached version is used so we don't upload the same
// image each and every time we expose.
//
// Most cached surfaces are owned by the GtkThemeService, which associates
// them with a certain XDisplay. Some users of surfaces (CustomDrawButtonBase,
// for example) own their surfaces instead since they interact with the
// ResourceBundle instead of the GtkThemeService.
class CairoCachedSurface {
 public:
  CairoCachedSurface();
  ~CairoCachedSurface();

  // Whether this CairoCachedSurface owns a GdkPixbuf.
  bool valid() const {
    return pixbuf_;
  }

  // The dimensions of the underlying pixbuf/surface. (or -1 if invalid.)
  int Width() const;
  int Height() const;

  // Sets the pixbuf that we pass to cairo. Calling UsePixbuf() only derefs the
  // current pixbuf and surface (if they exist). Actually transfering data to
  // the X server occurs at SetSource() time. Calling UsePixbuf() should only
  // be done once as it clears cached data from the X server.
  void UsePixbuf(GdkPixbuf* pixbuf);

  // Sets our pixbuf as the active surface starting at (x, y), uploading it in
  // case we don't have an X backed surface cached.
  void SetSource(cairo_t* cr, int x, int y);

  // Raw access to the pixbuf. May be NULL. Used for a few gdk operations
  // regarding window shaping.
  GdkPixbuf* pixbuf() { return pixbuf_; }

 private:
  // The source pixbuf.
  GdkPixbuf* pixbuf_;

  // Our cached surface. This should be a xlib surface so the data lives on the
  // server instead of on the client.
  cairo_surface_t* surface_;
};

#endif  // CHROME_BROWSER_UI_GTK_CAIRO_CACHED_SURFACE_H_
