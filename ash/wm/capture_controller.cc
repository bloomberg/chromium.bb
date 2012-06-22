// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/capture_controller.h"

#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/aura/root_window.h"

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// CaptureController, public:

CaptureController::CaptureController()
    : capture_window_(NULL) {
}

CaptureController::~CaptureController() {
}

////////////////////////////////////////////////////////////////////////////////
// CaptureController, client::CaptureClient implementation:

void CaptureController::SetCapture(aura::Window* new_capture_window) {
  if (capture_window_ == new_capture_window)
    return;
  // Make sure window has a root window.
  DCHECK(!new_capture_window || new_capture_window->GetRootWindow());
  DCHECK(!capture_window_ || capture_window_->GetRootWindow());

  aura::Window* old_capture_window = capture_window_;
  capture_window_ = new_capture_window;

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::RootWindow* root_window = *iter;
    root_window->gesture_recognizer()->
        TransferEventsTo(old_capture_window, new_capture_window);
    root_window->UpdateCapture(old_capture_window, new_capture_window);
  }
  return;
}

void CaptureController::ReleaseCapture(aura::Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

aura::Window* CaptureController::GetCaptureWindow() {
  return capture_window_;
}

}  // namespace internal
}  // namespace ash
