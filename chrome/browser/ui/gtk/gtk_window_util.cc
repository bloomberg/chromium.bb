// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gtk_window_util.h"

#include <dlfcn.h>
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderWidgetHost;
using content::WebContents;

namespace gtk_window_util {

// Performs Cut/Copy/Paste operation on the |window|.
// If the current render view is focused, then just call the specified |method|
// against the current render view host, otherwise emit the specified |signal|
// against the focused widget.
// TODO(suzhe): This approach does not work for plugins.
void DoCutCopyPaste(GtkWindow* window,
                    WebContents* web_contents,
                    void (RenderWidgetHost::*method)(),
                    const char* signal) {
  GtkWidget* widget = gtk_window_get_focus(window);
  if (widget == NULL)
    return;  // Do nothing if no focused widget.

  if (web_contents && widget == web_contents->GetContentNativeView()) {
    (web_contents->GetRenderViewHost()->*method)();
  } else {
    guint id;
    if ((id = g_signal_lookup(signal, G_OBJECT_TYPE(widget))) != 0)
      g_signal_emit(widget, id, 0);
  }
}

void DoCut(GtkWindow* window, WebContents* web_contents) {
  DoCutCopyPaste(window, web_contents,
                 &RenderWidgetHost::Cut, "cut-clipboard");
}

void DoCopy(GtkWindow* window, WebContents* web_contents) {
  DoCutCopyPaste(window, web_contents,
                 &RenderWidgetHost::Copy, "copy-clipboard");
}

void DoPaste(GtkWindow* window, WebContents* web_contents) {
  DoCutCopyPaste(window, web_contents,
                 &RenderWidgetHost::Paste, "paste-clipboard");
}

// Ubuntu patches their version of GTK+ so that there is always a
// gripper in the bottom right corner of the window. We dynamically
// look up this symbol because it's a non-standard Ubuntu extension to
// GTK+. We always need to disable this feature since we can't
// communicate this to WebKit easily.
typedef void (*gtk_window_set_has_resize_grip_func)(GtkWindow*, gboolean);
gtk_window_set_has_resize_grip_func gtk_window_set_has_resize_grip_sym;

void DisableResizeGrip(GtkWindow* window) {
  static bool resize_grip_looked_up = false;
  if (!resize_grip_looked_up) {
    resize_grip_looked_up = true;
    gtk_window_set_has_resize_grip_sym =
        reinterpret_cast<gtk_window_set_has_resize_grip_func>(
            dlsym(NULL, "gtk_window_set_has_resize_grip"));
  }
  if (gtk_window_set_has_resize_grip_sym)
    gtk_window_set_has_resize_grip_sym(window, FALSE);
}

GdkCursorType GdkWindowEdgeToGdkCursorType(GdkWindowEdge edge) {
  switch (edge) {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      return GDK_TOP_LEFT_CORNER;
    case GDK_WINDOW_EDGE_NORTH:
      return GDK_TOP_SIDE;
    case GDK_WINDOW_EDGE_NORTH_EAST:
      return GDK_TOP_RIGHT_CORNER;
    case GDK_WINDOW_EDGE_WEST:
      return GDK_LEFT_SIDE;
    case GDK_WINDOW_EDGE_EAST:
      return GDK_RIGHT_SIDE;
    case GDK_WINDOW_EDGE_SOUTH_WEST:
      return GDK_BOTTOM_LEFT_CORNER;
    case GDK_WINDOW_EDGE_SOUTH:
      return GDK_BOTTOM_SIDE;
    case GDK_WINDOW_EDGE_SOUTH_EAST:
      return GDK_BOTTOM_RIGHT_CORNER;
    default:
      NOTREACHED();
  }
  return GDK_LAST_CURSOR;
}

}  // namespace gtk_window_util
