// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_extender.h"

#include "chrome/browser/views/frame/browser_view.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, public:

BrowserExtender::BrowserExtender(BrowserView* browser_view)
    : browser_view_(browser_view),
      can_close_(true) {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, protected:

views::Window* BrowserExtender::GetBrowserWindow() {
  return browser_view_->frame()->GetWindow();
}

