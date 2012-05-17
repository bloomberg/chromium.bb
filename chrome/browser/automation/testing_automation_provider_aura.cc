// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "base/logging.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

#if defined(USE_ASH)
#include "ash/wm/window_util.h"
#endif

void TestingAutomationProvider::TerminateSession(int handle, bool* success) {
  *success = false;
}

void TestingAutomationProvider::SetWindowBounds(int handle,
                                                const gfx::Rect& bounds,
                                                bool* success) {
  aura::Window* window = window_tracker_->GetResource(handle);
  if (window) {
    window->SetBounds(bounds);
    *success = true;
  } else {
    *success = false;
  }
}
