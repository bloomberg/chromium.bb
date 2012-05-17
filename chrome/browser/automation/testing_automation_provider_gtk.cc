// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "ui/base/gtk/gtk_screen_util.h"

void TestingAutomationProvider::TerminateSession(int handle, bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::WindowGetViewBounds(int handle,
                                                    int view_id,
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

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    *bounds = gfx::Rect(allocation.width, allocation.height);
    gint x, y;
    if (screen_coordinates) {
      gfx::Point point = ui::GetWidgetScreenPosition(widget);
      x = point.x();
      y = point.y();
    } else {
      gtk_widget_translate_coordinates(widget, GTK_WIDGET(window),
                                       0, 0, &x, &y);
    }
    bounds->set_origin(gfx::Point(x, y));
  }
}

void TestingAutomationProvider::SetWindowBounds(int handle,
                                                const gfx::Rect& bounds,
                                                bool* success) {
  *success = false;
  GtkWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    gtk_window_move(window, bounds.x(), bounds.height());
    gtk_window_resize(window, bounds.width(), bounds.height());
    *success = true;
  }
}
