// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/default_window_resizer.h"

#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"

namespace ash {

DefaultWindowResizer::~DefaultWindowResizer() {
  shell_->UnlockCursor();
}

// static
DefaultWindowResizer* DefaultWindowResizer::Create(
    wm::WindowState* window_state) {
  return new DefaultWindowResizer(window_state);
}

void DefaultWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  gfx::Rect bounds(CalculateBoundsForDrag(location));
  if (bounds != GetTarget()->GetBounds()) {
    if (!did_move_or_resize_ && !details().restore_bounds.IsEmpty())
      window_state_->ClearRestoreBounds();
    did_move_or_resize_ = true;
    GetTarget()->SetBounds(bounds);
  }
}

void DefaultWindowResizer::CompleteDrag() {}

void DefaultWindowResizer::RevertDrag() {
  if (!did_move_or_resize_)
    return;

  GetTarget()->SetBounds(details().initial_bounds_in_parent);

  if (!details().restore_bounds.IsEmpty())
    window_state_->SetRestoreBoundsInScreen(details().restore_bounds);
}

DefaultWindowResizer::DefaultWindowResizer(wm::WindowState* window_state)
    : WindowResizer(window_state),
      did_move_or_resize_(false),
      shell_(GetTarget()->GetShell()) {
  DCHECK(details().is_resizable);
  shell_->LockCursor();
}

}  // namespace aura
