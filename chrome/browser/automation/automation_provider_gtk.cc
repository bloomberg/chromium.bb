// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "chrome/browser/gtk/view_id_util.h"

void AutomationProvider::SetWindowBounds(int handle, const gfx::Rect& bounds,
                                         bool* success) {
  *success = false;
  if (window_tracker_->ContainsHandle(handle)) {
    GtkWindow* window = window_tracker_->GetResource(handle);
    gtk_window_move(window, bounds.x(), bounds.height());
    gtk_window_resize(window, bounds.width(), bounds.height());
    *success = true;
  }
}

void AutomationProvider::SetWindowVisible(int handle, bool visible,
                                          bool* result) {
  *result = false;
  if (window_tracker_->ContainsHandle(handle)) {
    GtkWindow* window = window_tracker_->GetResource(handle);
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

  if (window_tracker_->ContainsHandle(handle)) {
    gfx::NativeWindow window = window_tracker_->GetResource(handle);
    GtkWidget* widget = ViewIDUtil::GetWidget(GTK_WIDGET(window),
                                              static_cast<ViewID>(view_id));
    if (!widget)
      return;
    *success = true;
    *bounds = gfx::Rect(0, 0,
                        widget->allocation.width, widget->allocation.height);
    gint x, y;
    if (screen_coordinates) {
      gdk_window_get_origin(widget->window, &x, &y);
      if (GTK_WIDGET_NO_WINDOW(widget)) {
        x += widget->allocation.x;
        y += widget->allocation.y;
      }
    } else {
      gtk_widget_translate_coordinates(widget, GTK_WIDGET(window),
                                       0, 0, &x, &y);
    }
    bounds->set_origin(gfx::Point(x, y));
  }
}
