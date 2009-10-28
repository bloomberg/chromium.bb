// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "base/gfx/point.h"
#include "views/view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

void AutomationProvider::WindowGetViewBounds(int handle, int view_id,
                                             bool screen_coordinates,
                                             bool* success,
                                             gfx::Rect* bounds) {
  *success = false;

  if (window_tracker_->ContainsHandle(handle)) {
    gfx::NativeWindow window = window_tracker_->GetResource(handle);
    views::RootView* root_view = views::Widget::FindRootView(window);
    if (root_view) {
      views::View* view = root_view->GetViewByID(view_id);
      if (view) {
        *success = true;
        gfx::Point point;
        if (screen_coordinates)
          views::View::ConvertPointToScreen(view, &point);
        else
          views::View::ConvertPointToView(view, root_view, &point);
        *bounds = view->GetLocalBounds(false);
        bounds->set_origin(point);
      }
    }
  }
}

