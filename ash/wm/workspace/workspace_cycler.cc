// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_cycler.h"

#include "ash/shell.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"

namespace ash {
namespace internal {

namespace {

// The required vertical distance to scrub to the next workspace.
const int kWorkspaceStepSize = 10;

// Returns true is scrubbing is enabled.
bool IsScrubbingEnabled() {
  // Scrubbing is disabled if the screen is locked or a modal dialog is open.
  return !Shell::GetInstance()->IsScreenLocked() &&
         !Shell::GetInstance()->IsSystemModalWindowOpen();
}

}  // namespace

WorkspaceCycler::WorkspaceCycler(WorkspaceManager* workspace_manager)
    : workspace_manager_(workspace_manager),
      scrubbing_(false),
      scroll_x_(0),
      scroll_y_(0) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

WorkspaceCycler::~WorkspaceCycler() {
  scrubbing_ = false;
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

ui::EventResult WorkspaceCycler::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->finger_count() != 3 ||
      event->type() != ui::ET_SCROLL) {
    scrubbing_ = false;
    return ui::ER_UNHANDLED;
  }

  if (!IsScrubbingEnabled()) {
    scrubbing_ = false;
    return ui::ER_UNHANDLED;
  }

  if (!scrubbing_) {
    scrubbing_ = true;
    scroll_x_ = 0;
    scroll_y_ = 0;
  }

  if (ui::IsNaturalScrollEnabled()) {
    scroll_x_ += event->x_offset();
    scroll_y_ += event->y_offset();
  } else {
    scroll_x_ -= event->x_offset();
    scroll_y_ -= event->y_offset();
  }

  // TODO(pkotwicz): Implement scrubbing through several workspaces as the
  // result of a single scroll event.
  if (std::abs(scroll_y_) > kWorkspaceStepSize) {
    workspace_manager_->CycleToWorkspace(scroll_y_ > 0 ?
        WorkspaceManager::CYCLE_NEXT : WorkspaceManager::CYCLE_PREVIOUS);

    scroll_x_ = 0;
    scroll_y_ = 0;
    return ui::ER_HANDLED;
  }

  if (std::abs(scroll_x_) > kWorkspaceStepSize) {
    // Update |scroll_x_| and |scroll_y_| such that workspaces are only cycled
    // through when there recently was a significant amount of vertical movement
    // as opposed to vertical movement accumulated over a long horizontal three
    // finger scroll.
    scroll_x_ = 0;
    scroll_y_ = 0;
  }

  // The active workspace was not changed, do not consume the event.
  return ui::ER_UNHANDLED;
}

}  // namespace internal
}  // namespace ash
