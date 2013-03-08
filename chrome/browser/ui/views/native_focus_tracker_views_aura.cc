// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_focus_tracker_views.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura/window.h"

// static
Browser* NativeFocusTrackerViews::GetBrowserForNativeView(
    gfx::NativeView view) {
  while (view) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForNativeWindow(view);
    if (browser_view)
      return browser_view->browser();
    view = view->parent();
  }
  return NULL;
}
