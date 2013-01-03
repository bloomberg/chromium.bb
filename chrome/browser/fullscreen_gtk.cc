// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/rect.h"

namespace {

// TODO (jianli): Merge with ui::EnumerateTopLevelWindows.
void EnumerateAllChildWindows(ui::EnumerateWindowsDelegate* delegate,
                              XID window) {
  std::vector<XID> windows;

  if (!ui::GetXWindowStack(window, &windows)) {
    // Window Manager doesn't support _NET_CLIENT_LIST_STACKING, so fall back
    // to old school enumeration of all X windows.
    XID root, parent, *children;
    unsigned int num_children;
    int status = XQueryTree(ui::GetXDisplay(), window, &root, &parent,
                            &children, &num_children);
    if (status) {
      for (long i = static_cast<long>(num_children) - 1; i >= 0; i--)
        windows.push_back(children[i]);
      XFree(children);
    }
  }

  std::vector<XID>::iterator iter;
  for (iter = windows.begin(); iter != windows.end(); iter++) {
    if (delegate->ShouldStopIterating(*iter))
      return;
  }
}

// To find the top-most window:
// 1) Enumerate all top-level windows from the top to the bottom.
// 2) For each window:
//    2.1) If it is hidden, continue the iteration.
//    2.2) If it is managed by the Window Manager (has a WM_STATE property).
//         Return this window as the top-most window.
//    2.3) Enumerate all its child windows. If there is a child window that is
//         managed by the Window Manager (has a WM_STATE property). Return this
//         child window as the top-most window.
//    2.4) Otherwise, continue the iteration.

class WindowManagerWindowFinder : public ui::EnumerateWindowsDelegate {
 public:
  WindowManagerWindowFinder() : window_(None) { }

  XID window() const { return window_; }

 protected:
  virtual bool ShouldStopIterating(XID window) {
    if (ui::PropertyExists(window, "WM_STATE")) {
      window_ = window;
      return true;
    }
    return false;
  }

 private:
  XID window_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerWindowFinder);
};

class TopMostWindowFinder : public ui::EnumerateWindowsDelegate {
 public:
  TopMostWindowFinder()
      : top_most_window_(None) {}

  XID top_most_window() const { return top_most_window_; }

 protected:
   virtual bool ShouldStopIterating(XID window) {
     if (!ui::IsWindowVisible(window))
       return false;
     if (ui::PropertyExists(window, "WM_STATE")) {
      top_most_window_ = window;
      return true;
    }
     WindowManagerWindowFinder child_finder;
     EnumerateAllChildWindows(&child_finder, window);
     XID child_window = child_finder.window();
     if (child_window == None)
       return false;
     top_most_window_ = child_window;
     return true;
   }

 private:
  XID top_most_window_;

  DISALLOW_COPY_AND_ASSIGN(TopMostWindowFinder);
};

bool IsTopMostWindowFullScreen() {
  // Find the topmost window.
  TopMostWindowFinder finder;
  EnumerateAllChildWindows(&finder, ui::GetX11RootWindow());
  XID window = finder.top_most_window();
  if (window == None)
    return false;

  // Make sure it is not the desktop window.
  static Atom desktop_atom = gdk_x11_get_xatom_by_name_for_display(
      gdk_display_get_default(), "_NET_WM_WINDOW_TYPE_DESKTOP");

  std::vector<Atom> atom_properties;
  if (ui::GetAtomArrayProperty(window,
                               "_NET_WM_WINDOW_TYPE",
                               &atom_properties) &&
      std::find(atom_properties.begin(), atom_properties.end(), desktop_atom)
          != atom_properties.end())
    return false;

  // If it is a GDK window, check it using gdk function.
  GdkWindow* gwindow = gdk_window_lookup(window);
  if (gwindow && window != GDK_ROOT_WINDOW())
    return gdk_window_get_state(gwindow) == GDK_WINDOW_STATE_FULLSCREEN;

  // Otherwise, do the check via xlib function.
  return ui::IsX11WindowFullScreen(window);
}

}

bool IsFullScreenMode() {
  gdk_error_trap_push();
  bool result = IsTopMostWindowFullScreen();
  bool got_error = gdk_error_trap_pop();
  return result && !got_error;
}
