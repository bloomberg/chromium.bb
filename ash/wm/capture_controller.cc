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
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  // If we're actually starting capture, then cancel any touches/gestures
  // that aren't already locked to the new window, and transfer any on the
  // old capture window to the new one.  When capture is released we have no
  // distinction between the touches/gestures that were in the window all
  // along (and so shouldn't be canceled) and those that got moved, so
  // just leave them all where they are.
  if (new_capture_window) {
    for (Shell::RootWindowList::iterator iter = root_windows.begin();
         iter != root_windows.end(); ++iter) {
      aura::RootWindow* root_window = *iter;
      root_window->gesture_recognizer()->
          TransferEventsTo(old_capture_window, new_capture_window);
    }
  }

  capture_window_ = new_capture_window;

  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::RootWindow* root_window = *iter;
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
