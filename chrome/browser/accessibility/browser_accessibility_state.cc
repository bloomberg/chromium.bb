// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility_state.h"

#include "base/memory/singleton.h"

BrowserAccessibilityState::BrowserAccessibilityState()
    : screen_reader_active_(false) {
}

BrowserAccessibilityState::~BrowserAccessibilityState() {
}

// static
BrowserAccessibilityState* BrowserAccessibilityState::GetInstance() {
  return Singleton<BrowserAccessibilityState>::get();
}

void BrowserAccessibilityState::OnScreenReaderDetected() {
  screen_reader_active_ = true;
}

bool BrowserAccessibilityState::IsAccessibleBrowser() {
  return screen_reader_active_;
}
