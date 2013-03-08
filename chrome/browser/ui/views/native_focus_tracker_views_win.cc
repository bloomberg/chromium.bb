// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_focus_tracker_views.h"

#include "chrome/browser/ui/views/frame/browser_view.h"

// static
Browser* NativeFocusTrackerViews::GetBrowserForNativeView(
    gfx::NativeView view) {
  HWND root = GetAncestor(view, GA_ROOT);
  BrowserView* browser_view =
      root ? BrowserView::GetBrowserViewForNativeWindow(root) : NULL;
  return browser_view ? browser_view->browser() : NULL;
}
