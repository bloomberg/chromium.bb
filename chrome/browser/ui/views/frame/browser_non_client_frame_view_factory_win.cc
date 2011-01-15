// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "chrome/browser/ui/views/frame/app_panel_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

namespace browser {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view) {
  if (browser_view->IsBrowserTypePanel())
    return new AppPanelBrowserFrameView(frame, browser_view);
  else
    return new OpaqueBrowserFrameView(frame, browser_view);
}

}  // browser
