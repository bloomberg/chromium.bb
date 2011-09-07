// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "base/logging.h"

void TestingAutomationProvider::ActivateWindow(int handle) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::IsWindowMaximized(int handle,
                                                  bool* is_maximized,
                                                  bool* success) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::TerminateSession(int handle, bool* success) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::GetWindowBounds(int handle,
                                                gfx::Rect* bounds,
                                                bool* success) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::SetWindowBounds(int handle,
                                                const gfx::Rect& bounds,
                                                bool* success) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::SetWindowVisible(int handle,
                                                 bool visible,
                                                 bool* result) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::GetWindowTitle(int handle, string16* text) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

