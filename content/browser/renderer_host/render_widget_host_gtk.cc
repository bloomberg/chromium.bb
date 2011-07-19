// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "base/synchronization/lock.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/x11/WebScreenInfoFactory.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#include "ui/gfx/rect.h"

using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;

// We get a null |view| passed into this function please see
// http://crbug.com/9060 for more details.
void RenderWidgetHost::OnMsgGetScreenInfo(gfx::NativeViewId view,
                                          WebScreenInfo* results) {
  GtkWidget* widget = NULL;
  GdkWindow* gdk_window = NULL;
  if (GtkNativeViewManager::GetInstance()->GetNativeViewForId(
      &widget, view) && widget) {
    gdk_window = widget->window;
  } else {
    GdkDisplay* display = gdk_display_get_default();
    gdk_window = gdk_display_get_default_group(display);
  }
  if (!gdk_window)
    return;
  GdkScreen* screen = gdk_drawable_get_screen(gdk_window);
  *results = WebScreenInfoFactory::screenInfo(
      gdk_x11_drawable_get_xdisplay(gdk_window),
      gdk_x11_screen_get_screen_number(screen));

  // TODO(tony): We should move this code into WebScreenInfoFactory.
  int monitor_number = gdk_screen_get_monitor_at_window(screen, gdk_window);
  GdkRectangle monitor_rect;
  gdk_screen_get_monitor_geometry(screen, monitor_number, &monitor_rect);
  results->rect = WebKit::WebRect(monitor_rect.x, monitor_rect.y,
                                  monitor_rect.width, monitor_rect.height);
  // TODO(tony): Should we query _NET_WORKAREA to get the workarea?
  results->availableRect = results->rect;
}

// We get null window_ids passed into this function please see
// http://crbug.com/9060 for more details.
void RenderWidgetHost::OnMsgGetWindowRect(gfx::NativeViewId window_id,
                                          gfx::Rect* results) {
  GtkWidget* widget = NULL;
  if (!GtkNativeViewManager::GetInstance()->GetNativeViewForId(
      &widget, window_id) || !widget) {
    return;
  }
  GdkWindow* gdk_window = widget->window;
  if (!gdk_window)
    return;
  GdkRectangle window_rect;
  gdk_window_get_origin(gdk_window, &window_rect.x, &window_rect.y);
  gdk_drawable_get_size(gdk_window, &window_rect.width, &window_rect.height);
  *results = window_rect;
}

// We get null window_ids passed into this function please see
// http://crbug.com/9060 for more details.
void RenderWidgetHost::OnMsgGetRootWindowRect(gfx::NativeViewId window_id,
                                              gfx::Rect* results) {
  GtkWidget* widget = NULL;
  if (!GtkNativeViewManager::GetInstance()->GetNativeViewForId(
      &widget, window_id) || !widget) {
    return;
  }
  GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
  if (!toplevel)
    return;

  GdkRectangle frame_extents;
  GdkWindow* gdk_window = toplevel->window;
  if (!gdk_window)
    return;
  gdk_window_get_frame_extents(gdk_window, &frame_extents);
  int width = 0;
  int height = 0;
  gdk_drawable_get_size(gdk_window, &width, &height);
  // Although we return a rect, this is actually two pairs of data: The
  // position of the top left corner of the window and the size of the window
  // not including the window decorations.
  *results = gfx::Rect(frame_extents.x, frame_extents.y, width, height);
}

void RenderWidgetHost::OnMsgCreatePluginContainer(gfx::PluginWindowHandle id) {
  // TODO(piman): view_ can only be NULL with delayed view creation in
  // extensions (see ExtensionHost::CreateRenderViewSoon). Figure out how to
  // support plugins in that case.
  if (view_) {
    view_->CreatePluginContainer(id);
  } else {
    deferred_plugin_handles_.push_back(id);
  }
}

void RenderWidgetHost::OnMsgDestroyPluginContainer(gfx::PluginWindowHandle id) {
  if (view_) {
    view_->DestroyPluginContainer(id);
  } else {
    for (int i = 0;
         i < static_cast<int>(deferred_plugin_handles_.size());
         i++) {
      if (deferred_plugin_handles_[i] == id) {
        deferred_plugin_handles_.erase(deferred_plugin_handles_.begin() + i);
        i--;
      }
    }
  }
}
