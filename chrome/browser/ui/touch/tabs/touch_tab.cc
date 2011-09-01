// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/tabs/touch_tab.h"

////////////////////////////////////////////////////////////////////////////////
// TouchTab, public:

TouchTab::TouchTab(TabController* controller)
    : Tab(controller) {
}

TouchTab::~TouchTab() {
}

// We only show the close button for the active tab.
bool TouchTab::ShouldShowCloseBox() const {
  return IsCloseable() && IsActive();
}
