// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "base/logging.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

void TestingAutomationProvider::ActivateWindow(int handle) {
  aura::Window* window = window_tracker_->GetResource(handle);
  if (window) {
    window->Activate();
  }
}

void TestingAutomationProvider::IsWindowMaximized(int handle,
                                                  bool* is_maximized,
                                                  bool* success) {
  aura::Window* window = window_tracker_->GetResource(handle);
  if (window) {
    int show_state = window->GetIntProperty(aura::kShowStateKey);
    *is_maximized = (show_state == ui::SHOW_STATE_MAXIMIZED);
    *success = true;
  } else {
    *success = false;
  }
}

void TestingAutomationProvider::TerminateSession(int handle, bool* success) {
  // TODO(benrg): what should this do in aura? It's
  // currently unimplemented in most other providers.
  *success = false;
  NOTIMPLEMENTED();
}

void TestingAutomationProvider::GetWindowBounds(int handle,
                                                gfx::Rect* bounds,
                                                bool* success) {
  const aura::Window* window = window_tracker_->GetResource(handle);
  if (window) {
    *bounds = window->bounds();
    *success = true;
  } else {
    *success = false;
  }
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

void TestingAutomationProvider::SetWindowVisible(int handle,
                                                 bool visible,
                                                 bool* result) {
  aura::Window* window = window_tracker_->GetResource(handle);
  if (window) {
    if (visible) {
      window->Show();
    } else {
      window->Hide();
    }
    *result = true;
  } else {
    *result = false;
  }
}

void TestingAutomationProvider::GetWindowTitle(int handle, string16* text) {
  const aura::Window* window = window_tracker_->GetResource(handle);
  DCHECK(window);
  *text = window->title();
}

