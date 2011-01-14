// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ROUNDED_WINDOW_H_
#define CHROME_BROWSER_UI_GTK_ROUNDED_WINDOW_H_
#pragma once

#include <gtk/gtk.h>

namespace gtk_util {

// Symbolic names for arguments to |rounded_edges| in ActAsRoundedWindow().
enum RoundedBorders {
  ROUNDED_NONE = 0,
  ROUNDED_BOTTOM_LEFT = 1 << 0,
  ROUNDED_TOP_LEFT = 1 << 1,
  ROUNDED_TOP_RIGHT = 1 << 2,
  ROUNDED_TOP = (1 << 1) | (1 << 2),
  ROUNDED_BOTTOM_RIGHT = 1 << 3,
  ROUNDED_ALL = 0xF
};

// Symbolic names for arguments to |drawn_borders| in ActAsRoundedWindow().
enum BorderEdge {
  BORDER_NONE = 0,
  BORDER_LEFT = 1 << 0,
  BORDER_TOP = 1 << 1,
  BORDER_RIGHT = 1 << 2,
  BORDER_BOTTOM = 1 << 3,
  BORDER_ALL = 0xF
};

// Sets up the passed in widget that has its own GdkWindow with an expose
// handler that forces the window shape into roundness. Caller should not set
// an "expose-event" handler on |widget|; if caller needs to do custom
// rendering, use SetRoundedWindowExposeFunction() instead. |rounded_edges|
// control which corners are rounded. |drawn_borders| border control which
// sides have a visible border drawn in |color|.
void ActAsRoundedWindow(
    GtkWidget* widget, const GdkColor& color, int corner_size,
    int rounded_edges, int drawn_borders);

// Undoes most of the actions of ActAsRoundedWindow().
void StopActingAsRoundedWindow(GtkWidget* widget);

// Returns true if the window is rounded.
bool IsActingAsRoundedWindow(GtkWidget* widget);

// Sets edge and border properties on a widget that has already been configured
// with ActAsRoundedWindow().
void SetRoundedWindowEdgesAndBorders(GtkWidget* widget,
                                     int corner_size,
                                     int rounded_edges,
                                     int drawn_borders);

// Sets the color of the border on a widget that has already been configured
// with ActAsRoundedWindow().
void SetRoundedWindowBorderColor(GtkWidget* widget, GdkColor color);


}  // namespace gtk_util

#endif  // CHROME_BROWSER_UI_GTK_ROUNDED_WINDOW_H_
