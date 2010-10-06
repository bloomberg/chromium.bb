// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/gtk_native_view_id_manager.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <X11/Xlib.h>

#include "base/logging.h"
#include "base/rand_util.h"
#include "gfx/rect.h"

// -----------------------------------------------------------------------------
// Bounce functions for GTK to callback into a C++ object...
namespace {

void OnRealize(gfx::NativeView widget, void* arg) {
  GtkNativeViewManager* manager = reinterpret_cast<GtkNativeViewManager*>(arg);
  manager->OnRealize(widget);
}

void OnUnrealize(gfx::NativeView widget, void *arg) {
  GtkNativeViewManager* manager = reinterpret_cast<GtkNativeViewManager*>(arg);
  manager->OnUnrealize(widget);
}

void OnDestroy(GtkObject* obj, void* arg) {
  GtkNativeViewManager* manager = reinterpret_cast<GtkNativeViewManager*>(arg);
  manager->OnDestroy(reinterpret_cast<GtkWidget*>(obj));
}

void OnSizeAllocate(gfx::NativeView widget,
                    GtkAllocation* alloc,
                    void* arg) {
  GtkNativeViewManager* manager = reinterpret_cast<GtkNativeViewManager*>(arg);
  manager->OnSizeAllocate(widget, alloc);
}

XID CreateOverlay(XID underlying) {
  XID overlay = 0;
  Display* dpy = gdk_x11_get_default_xdisplay();

  // Prevent interference with reparenting/mapping of overlay
  XSetWindowAttributes attr;
  attr.override_redirect = True;

  if (underlying) {
    XID root;
    int x, y;
    unsigned int width, height;
    unsigned int border_width;
    unsigned int depth;
    XGetGeometry(
        dpy, underlying, &root, &x, &y,
        &width, &height, &border_width, &depth);
    overlay = XCreateWindow(
        dpy,                      // display
        underlying,               // parent
        0, 0, width, height,      // x, y, width, height
        0,                        // border width
        CopyFromParent,           // depth
        InputOutput,              // class
        CopyFromParent,           // visual
        CWOverrideRedirect,       // value mask
        &attr);                   // value attributes
    XMapWindow(dpy, overlay);
  } else {
    overlay = XCreateWindow(
        dpy,                                 // display
        gdk_x11_get_default_root_xwindow(),  // parent
        0, 0, 1, 1,                          // x, y, width, height
        0,                                   // border width
        CopyFromParent,                      // depth
        InputOutput,                         // class
        CopyFromParent,                      // visual
        CWOverrideRedirect,                  // value mask
        &attr);                              // value attributes
  }
  XSync(dpy, False);
  return overlay;
}

}

// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Public functions...

GtkNativeViewManager::GtkNativeViewManager() {
}

GtkNativeViewManager::~GtkNativeViewManager() {
}

gfx::NativeViewId GtkNativeViewManager::GetIdForWidget(gfx::NativeView widget) {
  // This is just for unit tests:
  if (!widget)
    return 0;

  AutoLock locked(lock_);

  std::map<gfx::NativeView, gfx::NativeViewId>::const_iterator i =
    native_view_to_id_.find(widget);

  if (i != native_view_to_id_.end())
    return i->second;

  gfx::NativeViewId new_id =
      static_cast<gfx::NativeViewId>(base::RandUint64());
  while (id_to_info_.find(new_id) != id_to_info_.end())
    new_id = static_cast<gfx::NativeViewId>(base::RandUint64());

  NativeViewInfo info;
  if (GTK_WIDGET_REALIZED(widget)) {
    GdkWindow *gdk_window = widget->window;
    CHECK(gdk_window);
    info.x_window_id = GDK_WINDOW_XID(gdk_window);
  }
  native_view_to_id_[widget] = new_id;
  id_to_info_[new_id] = info;

  g_signal_connect(widget, "realize", G_CALLBACK(::OnRealize), this);
  g_signal_connect(widget, "unrealize", G_CALLBACK(::OnUnrealize), this);
  g_signal_connect(widget, "destroy", G_CALLBACK(::OnDestroy), this);
  g_signal_connect(
      widget, "size-allocate", G_CALLBACK(::OnSizeAllocate), this);

  return new_id;
}

bool GtkNativeViewManager::GetXIDForId(XID* output, gfx::NativeViewId id) {
  AutoLock locked(lock_);

  std::map<gfx::NativeViewId, NativeViewInfo>::const_iterator i =
    id_to_info_.find(id);

  if (i == id_to_info_.end())
    return false;

  *output = i->second.x_window_id;
  return true;
}

bool GtkNativeViewManager::GetPermanentXIDForId(XID* output,
                                                gfx::NativeViewId id) {
  AutoLock locked(lock_);

  std::map<gfx::NativeViewId, NativeViewInfo>::iterator i =
    id_to_info_.find(id);

  if (i == id_to_info_.end())
    return false;

  if (!i->second.permanent_window_id) {
    i->second.permanent_window_id = CreateOverlay(i->second.x_window_id);
  }

  *output = i->second.permanent_window_id;
  return true;
}

// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Private functions...

gfx::NativeViewId GtkNativeViewManager::GetWidgetId(gfx::NativeView widget) {
  lock_.AssertAcquired();

  std::map<gfx::NativeView, gfx::NativeViewId>::const_iterator i =
    native_view_to_id_.find(widget);

  CHECK(i != native_view_to_id_.end());
  return i->second;
}

void GtkNativeViewManager::OnRealize(gfx::NativeView widget) {
  AutoLock locked(lock_);

  const gfx::NativeViewId id = GetWidgetId(widget);
  std::map<gfx::NativeViewId, NativeViewInfo>::iterator i =
    id_to_info_.find(id);

  CHECK(i != id_to_info_.end());
  CHECK(widget->window);

  i->second.x_window_id = GDK_WINDOW_XID(widget->window);

  if (i->second.permanent_window_id) {
    Display* dpy = gdk_x11_get_default_xdisplay();
    XReparentWindow(
        dpy, i->second.permanent_window_id, i->second.x_window_id, 0, 0);
    XMapWindow(dpy, i->second.permanent_window_id);
    XSync(dpy, False);
  }
}

void GtkNativeViewManager::OnUnrealize(gfx::NativeView widget) {
  AutoLock unrealize_locked(unrealize_lock_);
  AutoLock locked(lock_);

  const gfx::NativeViewId id = GetWidgetId(widget);
  std::map<gfx::NativeViewId, NativeViewInfo>::iterator i =
    id_to_info_.find(id);

  CHECK(i != id_to_info_.end());

  i->second.x_window_id = 0;
  if (i->second.permanent_window_id) {
    Display* dpy = gdk_x11_get_default_xdisplay();
    XUnmapWindow(dpy, i->second.permanent_window_id);
    XReparentWindow(dpy, i->second.permanent_window_id,
                    gdk_x11_get_default_root_xwindow(), 0, 0);
    XSync(dpy, False);
  }
}

void GtkNativeViewManager::OnDestroy(gfx::NativeView widget) {
  AutoLock locked(lock_);

  std::map<gfx::NativeView, gfx::NativeViewId>::iterator i =
    native_view_to_id_.find(widget);
  CHECK(i != native_view_to_id_.end());

  std::map<gfx::NativeViewId, NativeViewInfo>::iterator j =
    id_to_info_.find(i->second);
  CHECK(j != id_to_info_.end());

  if (j->second.permanent_window_id) {
    XDestroyWindow(gdk_x11_get_default_xdisplay(),
                   j->second.permanent_window_id);
  }

  native_view_to_id_.erase(i);
  id_to_info_.erase(j);
}

void GtkNativeViewManager::OnSizeAllocate(
    gfx::NativeView widget, GtkAllocation *alloc) {
  AutoLock locked(lock_);

  const gfx::NativeViewId id = GetWidgetId(widget);
  std::map<gfx::NativeViewId, NativeViewInfo>::iterator i =
    id_to_info_.find(id);

  if (i->second.permanent_window_id) {
    XResizeWindow(gdk_x11_get_default_xdisplay(),
                  i->second.permanent_window_id,
                  alloc->width, alloc->height);
  }
}

// -----------------------------------------------------------------------------
