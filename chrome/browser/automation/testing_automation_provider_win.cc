// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include <windows.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"

void TestingAutomationProvider::TerminateSession(int handle, bool* success) {
  *success = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    HWND window = browser->window()->GetNativeHandle();
    *success = (::PostMessageW(window, WM_ENDSESSION, 0, 0) == TRUE);
  }
}

void TestingAutomationProvider::SetWindowBounds(int handle,
                                                const gfx::Rect& bounds,
                                                bool* success) {
  *success = false;
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    if (::MoveWindow(hwnd, bounds.x(), bounds.y(), bounds.width(),
                     bounds.height(), true)) {
      *success = true;
    }
  }
}
