// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/view_id_util.h"

void TestingAutomationProvider::ActivateWindow(int handle) {
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::IsWindowMaximized(int handle,
                                                  bool* is_maximized,
                                                  bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::TerminateSession(int handle, bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

#if !defined(TOOLKIT_VIEWS)
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
    *bounds = gfx::Rect(widget->allocation.width, widget->allocation.height);
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
#endif

void TestingAutomationProvider::GetWindowBounds(int handle,
                                                gfx::Rect* bounds,
                                                bool* result) {
  *result = false;
  NOTIMPLEMENTED();
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

void TestingAutomationProvider::SetWindowVisible(int handle,
                                                 bool visible,
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

void TestingAutomationProvider::GetWindowTitle(int handle, string16* text) {
  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  const gchar* title = gtk_window_get_title(window);
  text->assign(UTF8ToUTF16(title));
}
