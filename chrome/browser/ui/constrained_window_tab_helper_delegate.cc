// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/constrained_window_tab_helper_delegate.h"

void ConstrainedWindowTabHelperDelegate::WillShowConstrainedWindow(
    TabContents* source) {
}

bool ConstrainedWindowTabHelperDelegate::ShouldFocusConstrainedWindow() {
  return true;
}

void ConstrainedWindowTabHelperDelegate::SetTabContentBlocked(
    TabContents* tab_contents, bool blocked) {
}

ConstrainedWindowTabHelperDelegate::~ConstrainedWindowTabHelperDelegate() {}
