// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/default_state.h"

#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace wm {

DefaultState::DefaultState() {}
DefaultState::~DefaultState() {}

void DefaultState::OnWMEvent(WindowState* window_state,
                             WMEvent event) {
  aura::Window* window = window_state->window();
  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window).work_area();

  switch (event) {
    case TOGGLE_MAXIMIZE_CAPTION:
      if (window_state->HasRestoreBounds()) {
        if (window_state->GetShowState() == ui::SHOW_STATE_NORMAL) {
          window_state->SetAndClearRestoreBounds();
        } else {
          window_state->Restore();
        }
      } else if (window_state->CanMaximize()) {
        window_state->Maximize();
      }
      break;

    case TOGGLE_MAXIMIZE:
      if (window_state->IsFullscreen())
        window_state->ToggleFullscreen();
      else if (window_state->IsMaximized())
        window_state->Restore();
      else if (window_state->CanMaximize())
        window_state->Maximize();
      break;

    case TOGGLE_VERTICAL_MAXIMIZE:
      if (window->delegate()->GetMaximumSize().height() != 0)
        return;
      if (window_state->HasRestoreBounds() &&
          (window->bounds().height() == work_area.height() &&
           window->bounds().y() == work_area.y())) {
        window_state->SetAndClearRestoreBounds();
      } else {
        window_state->SaveCurrentBoundsForRestore();
        gfx::Point origin = window->bounds().origin();
        wm::ConvertPointToScreen(window->parent(), &origin);
        window_state->SetBoundsInScreen(gfx::Rect(origin.x(),
                                                  work_area.y(),
                                                  window->bounds().width(),
                                                  work_area.height()));
      }
      break;

    case TOGGLE_HORIZONTAL_MAXIMIZE:
      if (window->delegate()->GetMaximumSize().width() != 0)
        return;
      if (window_state->HasRestoreBounds() &&
          (window->bounds().width() == work_area.width() &&
           window->bounds().x() == work_area.x())) {
        window_state->SetAndClearRestoreBounds();
      } else {
        window_state->SaveCurrentBoundsForRestore();
        gfx::Point origin = window->bounds().origin();
        wm::ConvertPointToScreen(window->parent(), &origin);
        window_state->SetBoundsInScreen(gfx::Rect(work_area.x(),
                                                  origin.y(),
                                                  work_area.width(),
                                                  window->bounds().height()));
      }
  }
};

}  // namespace wm
}  // namespace ash
