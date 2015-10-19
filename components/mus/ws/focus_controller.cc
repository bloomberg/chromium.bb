// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/focus_controller.h"

#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_drawn_tracker.h"

namespace mus {

FocusController::FocusController(FocusControllerDelegate* delegate)
    : delegate_(delegate) {}

FocusController::~FocusController() {}

void FocusController::SetFocusedWindow(ServerWindow* window) {
  if (GetFocusedWindow() == window)
    return;

  SetFocusedWindowImpl(window, CHANGE_SOURCE_EXPLICIT);
}

ServerWindow* FocusController::GetFocusedWindow() {
  return drawn_tracker_ ? drawn_tracker_->window() : nullptr;
}

void FocusController::SetFocusedWindowImpl(ServerWindow* window,
                                           ChangeSource change_source) {
  ServerWindow* old = GetFocusedWindow();

  DCHECK(!window || window->IsDrawn());

  if (window)
    drawn_tracker_.reset(new ServerWindowDrawnTracker(window, this));
  else
    drawn_tracker_.reset();

  if (change_source == CHANGE_SOURCE_DRAWN_STATE_CHANGED)
    delegate_->OnFocusChanged(old, window);
}

void FocusController::OnDrawnStateChanged(ServerWindow* ancestor,
                                          ServerWindow* window,
                                          bool is_drawn) {
  DCHECK(!is_drawn);  // We only observe when drawn.
  SetFocusedWindowImpl(ancestor, CHANGE_SOURCE_DRAWN_STATE_CHANGED);
}

}  // namespace mus
