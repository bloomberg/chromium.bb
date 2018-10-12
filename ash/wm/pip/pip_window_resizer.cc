// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

PipWindowResizer::PipWindowResizer(wm::WindowState* window_state)
    : WindowResizer(window_state) {
  window_state->OnDragStarted(details().window_component);
}

PipWindowResizer::~PipWindowResizer() {}

void PipWindowResizer::Drag(const gfx::Point& location_in_parent,
                            int event_flags) {
  last_location_in_screen_ = location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(), &last_location_in_screen_);

  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(GetTarget());
  gfx::Rect work_area = display.work_area();
  bounds.AdjustToFit(work_area);

  if (bounds != GetTarget()->bounds()) {
    moved_or_resized_ = true;
    GetTarget()->SetBounds(bounds);
  }
}

void PipWindowResizer::CompleteDrag() {
  window_state()->OnCompleteDrag(last_location_in_screen_);
  window_state()->DeleteDragDetails();
  window_state()->ClearRestoreBounds();
  window_state()->set_bounds_changed_by_user(moved_or_resized_);
}

void PipWindowResizer::RevertDrag() {
  // Handle cancel as a complete drag for pip. Having the PIP window
  // go back to where it was on cancel looks strange, so instead just
  // will just stop it where it is and animate to the edge of the screen.
  CompleteDrag();
}

void PipWindowResizer::FlingOrSwipe(ui::GestureEvent* event) {
  CompleteDrag();
}

}  // namespace ash
