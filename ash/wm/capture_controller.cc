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

  // TODO(oshima): Call this on all root windows.
  Shell::GetPrimaryRootWindow()->gesture_recognizer()->
      TransferEventsTo(capture_window_, new_capture_window);

  aura::Window* old_capture_window = capture_window_;
  aura::RootWindow* old_capture_root_window =
      old_capture_window ? old_capture_window->GetRootWindow() : NULL;
  aura::RootWindow* new_capture_root_window =
      new_capture_window ? new_capture_window->GetRootWindow() : NULL;

  capture_window_ = new_capture_window;

  // Release native input if the capture is either release on or
  // moved from old root window.
  if (old_capture_window &&
      (!new_capture_window ||
       new_capture_root_window != old_capture_root_window))
    old_capture_root_window->ReleaseNativeCapture();

  // Capture native input if new capture is set, or moved to
  // new root window.
  if (new_capture_window &&
      (!old_capture_window ||
       old_capture_root_window != new_capture_root_window))
    new_capture_root_window->SetNativeCapture();

  if (new_capture_root_window &&
      new_capture_root_window == old_capture_root_window) {
    // Capture changed within the same root window.
    new_capture_root_window->UpdateCapture(old_capture_window,
                                           new_capture_window);

  } else if (new_capture_root_window &&
             new_capture_root_window != old_capture_root_window) {
    // Either new window is captured, or the capture changed between different
    // root windows,
    if (old_capture_root_window)
      old_capture_root_window->UpdateCapture(old_capture_window, NULL);
    new_capture_root_window->UpdateCapture(NULL, capture_window_);
  } else if (old_capture_root_window) {
    // Capture is released.
    old_capture_root_window->UpdateCapture(old_capture_window, NULL);
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
