// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include <windows.h>

#include "chrome/browser/automation/automation_window_tracker.h"

void TestingAutomationProvider::ActivateWindow(int handle) {
  if (window_tracker_->ContainsHandle(handle)) {
    ::SetActiveWindow(window_tracker_->GetResource(handle));
  }
}

void TestingAutomationProvider::IsWindowMaximized(int handle,
                                                  bool* is_maximized,
                                                  bool* success) {
  *success = false;

  HWND hwnd = window_tracker_->GetResource(handle);
  if (hwnd) {
    *success = true;
    WINDOWPLACEMENT window_placement;
    GetWindowPlacement(hwnd, &window_placement);
    *is_maximized = (window_placement.showCmd == SW_MAXIMIZE);
  }
}

