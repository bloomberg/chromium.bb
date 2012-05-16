// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "chrome/browser/automation/automation_window_tracker.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

void TestingAutomationProvider::WindowGetViewBounds(int handle,
                                                    int view_id,
                                                    bool screen_coordinates,
                                                    bool* success,
                                                    gfx::Rect* bounds) {
  *success = false;

  if (window_tracker_->ContainsHandle(handle)) {
    gfx::NativeWindow window = window_tracker_->GetResource(handle);
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    if (widget) {
      views::View* root_view = widget->GetRootView();
      views::View* view = root_view->GetViewByID(view_id);
      if (view) {
        *success = true;
        gfx::Point point;
        if (screen_coordinates)
          views::View::ConvertPointToScreen(view, &point);
        else
          views::View::ConvertPointToView(view, root_view, &point);
        *bounds = view->GetContentsBounds();
        bounds->set_origin(point);
      }
    }
  }
}
