// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_parts_touch.h"

#include "chrome/browser/ui/touch/sensors/screen_orientation_listener.h"

ChromeBrowserPartsTouch::ChromeBrowserPartsTouch()
    : content::BrowserMainParts() {
}

void ChromeBrowserPartsTouch::PreMainMessageLoopRun() {
  // Make sure the singleton ScreenOrientationListener object is created.
  ScreenOrientationListener::GetInstance();
}

bool ChromeBrowserPartsTouch::MainMessageLoopRun(int* result_code) {
  return false;
}
