// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TABSTRIP_ORIGIN_PROVIDER_H_
#define CHROME_BROWSER_UI_GTK_TABSTRIP_ORIGIN_PROVIDER_H_
#pragma once

namespace gfx{
class Point;
}

// Abstract interface that provides an offset of a widget. Many pieces of the
// UI don't need the full BrowserWindowGtk, but just need information about
// it's position relative to the tabstrip to draw correctly. This interface
// exists to make it easier to test piece by piece.
class TabstripOriginProvider {
 public:
  virtual ~TabstripOriginProvider() { }

  // Return the origin of the tab strip in coordinates relative to where we
  // start drawing the background theme image. This is the x coordinate of
  // the origin of the GdkWindow of widget(), but the y coordinate of the origin
  // of widget() itself.
  // Used to help other widgets draw their background relative to the tabstrip.
  // Should only be called after both the tabstrip and |widget| have been
  // allocated.
  virtual gfx::Point GetTabStripOriginForWidget(GtkWidget* widget) = 0;
};

#endif  // CHROME_BROWSER_UI_GTK_TABSTRIP_ORIGIN_PROVIDER_H_
