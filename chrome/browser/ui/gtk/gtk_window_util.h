// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GTK_WINDOW_UTIL_H_
#define CHROME_BROWSER_UI_GTK_GTK_WINDOW_UTIL_H_

#include <gtk/gtk.h>
#include "ui/gfx/rect.h"

namespace content {
class WebContents;
}

namespace gtk_window_util {

// Performs Cut/Copy/Paste operation on the |window|'s |web_contents|.
void DoCut(GtkWindow* window, content::WebContents* web_contents);
void DoCopy(GtkWindow* window, content::WebContents* web_contents);
void DoPaste(GtkWindow* window, content::WebContents* web_contents);

// Ubuntu patches their version of GTK+ to that there is always a
// gripper in the bottom right corner of the window. We always need to
// disable this feature since we can't communicate this to WebKit easily.
void DisableResizeGrip(GtkWindow* window);

// Returns the resize cursor corresponding to the window |edge|.
GdkCursorType GdkWindowEdgeToGdkCursorType(GdkWindowEdge edge);

// Returns |true| if the window bounds match the monitor size.
bool BoundsMatchMonitorSize(GtkWindow* window, gfx::Rect bounds);

bool HandleTitleBarLeftMousePress(GtkWindow* window,
                                  const gfx::Rect& bounds,
                                  GdkEventButton* event);

// Request the underlying window to unmaximize.  Also tries to work around
// a window manager "feature" that can prevent this in some edge cases.
void UnMaximize(GtkWindow* window,
                const gfx::Rect& bounds,
                const gfx::Rect& restored_bounds);

}  // namespace gtk_window_util

#endif  // CHROME_BROWSER_UI_GTK_GTK_WINDOW_UTIL_H_

