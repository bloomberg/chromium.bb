// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/default_state.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace wm {

DefaultState::DefaultState() {}
DefaultState::~DefaultState() {}

void DefaultState::OnWMEvent(WindowState* window_state,
                             WMEvent event) {
  aura::Window* window = window_state->window();

  switch (event) {
    case TOGGLE_MAXIMIZE_CAPTION:
      if (window_state->IsFullscreen()) {
        window_state->ToggleFullscreen();
      } else if (window_state->IsMaximized()) {
        window_state->Restore();
      } else if (window_state->IsNormalShowType() ||
                 window_state->IsSnapped()) {
        if (window_state->CanMaximize())
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

    case TOGGLE_VERTICAL_MAXIMIZE: {
      gfx::Rect work_area =
          ScreenUtil::GetDisplayWorkAreaBoundsInParent(window);

      // Maximize vertically if:
      // - The window does not have a max height defined.
      // - The window has the normal show type. Snapped windows are excluded
      //   because they are already maximized vertically and reverting to the
      //   restored bounds looks weird.
      if (window->delegate()->GetMaximumSize().height() != 0 ||
          !window_state->IsNormalShowType()) {
        return;
      }
      if (window_state->HasRestoreBounds() &&
          (window->bounds().height() == work_area.height() &&
           window->bounds().y() == work_area.y())) {
        window_state->SetAndClearRestoreBounds();
      } else {
        window_state->SaveCurrentBoundsForRestore();
        window->SetBounds(gfx::Rect(window->bounds().x(),
                                    work_area.y(),
                                    window->bounds().width(),
                                    work_area.height()));
      }
      break;
    }
    case TOGGLE_HORIZONTAL_MAXIMIZE: {
      // Maximize horizontally if:
      // - The window does not have a max width defined.
      // - The window is snapped or has the normal show type.
      if (window->delegate()->GetMaximumSize().width() != 0)
        return;
      if (!window_state->IsNormalShowType() && !window_state->IsSnapped())
        return;

      gfx::Rect work_area =
          ScreenUtil::GetDisplayWorkAreaBoundsInParent(window);

      if (window_state->IsNormalShowType() &&
          window_state->HasRestoreBounds() &&
          (window->bounds().width() == work_area.width() &&
           window->bounds().x() == work_area.x())) {
        window_state->SetAndClearRestoreBounds();
      } else {
        gfx::Rect new_bounds(work_area.x(),
                             window->bounds().y(),
                             work_area.width(),
                             window->bounds().height());

        gfx::Rect restore_bounds = window->bounds();
        if (window_state->IsSnapped()) {
          window_state->SetRestoreBoundsInParent(new_bounds);
          window_state->Restore();

          // The restore logic prevents a window from being restored to bounds
          // which match the workspace bounds exactly so it is necessary to set
          // the bounds again below.
        }

        window_state->SetRestoreBoundsInParent(restore_bounds);
        window->SetBounds(new_bounds);
      }
    }
  }
};

}  // namespace wm
}  // namespace ash
