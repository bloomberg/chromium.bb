// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "chrome/browser/ui/touch/frame/touch_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

namespace browser {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view) {
  if (browser_view->IsBrowserTypePopup()) {
    // TODO(anicolao): implement popups for touch
    NOTIMPLEMENTED();
    return NULL;
  } else {
    return new TouchBrowserFrameView(frame, browser_view);
  }
}

}  // browser
