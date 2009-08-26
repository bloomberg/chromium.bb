// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/view_id_util.h"
#include "chrome/common/gtk_util.h"

void AutomationProvider::SetWindowBounds(int handle, const gfx::Rect& bounds,
                                         bool* success) {
  *success = false;
  GtkWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    gtk_window_move(window, bounds.x(), bounds.height());
    gtk_window_resize(window, bounds.width(), bounds.height());
    *success = true;
  }
}

void AutomationProvider::SetWindowVisible(int handle, bool visible,
                                          bool* result) {
  *result = false;
  GtkWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    if (visible) {
      gtk_window_present(window);
    } else {
      gtk_widget_hide(GTK_WIDGET(window));
    }
    *result = true;
  }
}

void AutomationProvider::WindowGetViewBounds(int handle, int view_id,
                                             bool screen_coordinates,
                                             bool* success,
                                             gfx::Rect* bounds) {
  *success = false;

  GtkWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    GtkWidget* widget = ViewIDUtil::GetWidget(GTK_WIDGET(window),
                                              static_cast<ViewID>(view_id));
    if (!widget)
      return;
    *success = true;
    *bounds = gfx::Rect(0, 0,
                        widget->allocation.width, widget->allocation.height);
    gint x, y;
    if (screen_coordinates) {
      gfx::Point point = gtk_util::GetWidgetScreenPosition(widget);
      x = point.x();
      y = point.y();
    } else {
      gtk_widget_translate_coordinates(widget, GTK_WIDGET(window),
                                       0, 0, &x, &y);
    }
    bounds->set_origin(gfx::Point(x, y));
  }
}

void AutomationProvider::ActivateWindow(int handle) { NOTIMPLEMENTED(); }

void AutomationProvider::GetFocusedViewID(int handle, int* view_id) {
  NOTIMPLEMENTED();
}

void AutomationProvider::PrintAsync(int tab_handle) {
  NOTIMPLEMENTED();
}

void AutomationProvider::SetInitialFocus(const IPC::Message& message,
                                         int handle, bool reverse) {
  NOTIMPLEMENTED();
}

void AutomationProvider::GetBookmarkBarVisibility(int handle, bool* visible,
                                                  bool* animating) {
  *visible = false;
  *animating = false;
  NOTIMPLEMENTED();
}
