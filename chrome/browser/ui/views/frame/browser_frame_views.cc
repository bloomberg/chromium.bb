// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_views.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameViews, public:

BrowserFrameViews::BrowserFrameViews(BrowserFrame* browser_frame,
                                     BrowserView* browser_view)
    : views::NativeWidgetViews(browser_frame),
      browser_view_(browser_view),
      browser_frame_(browser_frame) {
}

BrowserFrameViews::~BrowserFrameViews() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameViews, NativeBrowserFrame implementation:

views::NativeWidget* BrowserFrameViews::AsNativeWidget() {
  return this;
}

const views::NativeWidget* BrowserFrameViews::AsNativeWidget() const {
  return this;
}

int BrowserFrameViews::GetMinimizeButtonOffset() const {
  return 0;
}

void BrowserFrameViews::TabStripDisplayModeChanged() {
}

