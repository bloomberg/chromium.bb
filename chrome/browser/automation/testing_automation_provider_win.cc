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
    WINDOWPLACEMENT window_placement;
    window_placement.length = sizeof(window_placement);
    if (GetWindowPlacement(hwnd, &window_placement)) {
      *success = true;
      *is_maximized = (window_placement.showCmd == SW_MAXIMIZE);
    }
  }
}

void TestingAutomationProvider::TerminateSession(int handle, bool* success) {
  *success = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    HWND window = browser->window()->GetNativeHandle();
    *success = (::PostMessageW(window, WM_ENDSESSION, 0, 0) == TRUE);
  }
}

void TestingAutomationProvider::GetWindowBounds(int handle,
                                                gfx::Rect* bounds,
                                                bool* success) {
  *success = false;
  HWND hwnd = window_tracker_->GetResource(handle);
  if (hwnd) {
    WINDOWPLACEMENT window_placement;
    window_placement.length = sizeof(window_placement);
    if (GetWindowPlacement(hwnd, &window_placement)) {
      *success = true;
      *bounds = window_placement.rcNormalPosition;
    }
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

void TestingAutomationProvider::SetWindowVisible(int handle,
                                                 bool visible,
                                                 bool* result) {
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    ::ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
    *result = true;
  } else {
    *result = false;
  }
}

void TestingAutomationProvider::GetWindowTitle(int handle, string16* text) {
  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  int length = ::GetWindowTextLength(window) + 1;
  if (length > 1)
    ::GetWindowText(window, WriteInto(text, length), length);
  else
    text->clear();
}

