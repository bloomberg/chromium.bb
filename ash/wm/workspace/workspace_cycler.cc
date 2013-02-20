// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_cycler.h"

#include <cmath>

#include "ash/shell.h"
#include "ash/wm/workspace/workspace_cycler_configuration.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"

typedef ash::WorkspaceCyclerConfiguration Config;

namespace ash {
namespace internal {

namespace {

// Returns true if cycling is allowed.
bool IsCyclingAllowed() {
  // Cycling is disabled if the screen is locked or a modal dialog is open.
  return !Shell::GetInstance()->IsScreenLocked() &&
         !Shell::GetInstance()->IsSystemModalWindowOpen();
}

}  // namespace

WorkspaceCycler::WorkspaceCycler(WorkspaceManager* workspace_manager)
    : workspace_manager_(workspace_manager),
      animator_(NULL),
      state_(NOT_CYCLING),
      scroll_x_(0.0f),
      scroll_y_(0.0f) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

WorkspaceCycler::~WorkspaceCycler() {
  SetState(NOT_CYCLING);
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void WorkspaceCycler::AbortCycling() {
  SetState(NOT_CYCLING);
}

void WorkspaceCycler::SetState(State new_state) {
  if (state_ == NOT_CYCLING_TRACKING_SCROLL && new_state == STOPPING_CYCLING)
    new_state = NOT_CYCLING;

  if (state_ == new_state || !IsValidNextState(new_state))
    return;

  state_ = new_state;

  if (new_state == STARTING_CYCLING) {
    animator_.reset(new WorkspaceCyclerAnimator(this));
    workspace_manager_->InitWorkspaceCyclerAnimatorWithCurrentState(
        animator_.get());
    animator_->AnimateStartingCycler();
  } else if (new_state == STOPPING_CYCLING) {
    if (animator_.get())
      animator_->AnimateStoppingCycler();
  } else if (new_state == NOT_CYCLING) {
    scroll_x_ = 0.0f;
    scroll_y_ = 0.0f;
    if (animator_.get()) {
      animator_->AbortAnimations();
      animator_.reset();
    }
  }
}

bool WorkspaceCycler::IsValidNextState(State next_state) const {
  if (state_ == next_state)
    return true;

  switch (next_state) {
    case NOT_CYCLING:
      return true;
    case NOT_CYCLING_TRACKING_SCROLL:
      return state_ == NOT_CYCLING;
    case STARTING_CYCLING:
      return state_ == NOT_CYCLING_TRACKING_SCROLL;
    case CYCLING:
      return state_ == STARTING_CYCLING;
    case STOPPING_CYCLING:
      return (state_ == STARTING_CYCLING || state_ == CYCLING);
  }

  NOTREACHED();
  return false;
}

void WorkspaceCycler::OnEvent(ui::Event* event) {
  if (!IsCyclingAllowed())
    SetState(NOT_CYCLING);

  if (state_ != NOT_CYCLING) {
    if (event->type() == ui::ET_SCROLL_FLING_START ||
        event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_MOUSE_RELEASED ||
        event->IsKeyEvent()) {
      SetState(STOPPING_CYCLING);
      event->StopPropagation();
      return;
    }
  }
  ui::EventHandler::OnEvent(event);
}

void WorkspaceCycler::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->finger_count() != 3 ||
      event->type() != ui::ET_SCROLL) {
    if (state_ != NOT_CYCLING)
      event->StopPropagation();
    return;
  }

  if (!IsCyclingAllowed() ||
      !workspace_manager_->CanStartCyclingThroughWorkspaces()) {
    DCHECK_EQ(NOT_CYCLING, state_);
    return;
  }

  if (state_ == NOT_CYCLING)
    SetState(NOT_CYCLING_TRACKING_SCROLL);

  if (ui::IsNaturalScrollEnabled()) {
    scroll_x_ += event->x_offset();
    scroll_y_ += event->y_offset();
  } else {
    scroll_x_ -= event->x_offset();
    scroll_y_ -= event->y_offset();
  }

  if (state_ == NOT_CYCLING_TRACKING_SCROLL) {
    double distance_to_initiate_cycling = Config::GetDouble(
        Config::DISTANCE_TO_INITIATE_CYCLING);

    if (fabs(scroll_x_) > distance_to_initiate_cycling) {
      // Only initiate workspace cycling if there recently was a significant
      // amount of vertical movement as opposed to vertical movement
      // accumulated over a long horizontal three finger scroll.
      scroll_x_ = 0.0f;
      scroll_y_ = 0.0f;
    }

    if (fabs(scroll_y_) >= distance_to_initiate_cycling)
      SetState(STARTING_CYCLING);
  }

  if (state_ == CYCLING && event->y_offset() != 0.0f) {
    DCHECK(animator_.get());
    animator_->AnimateCyclingByScrollDelta(event->y_offset());
    event->SetHandled();
  }
}

void WorkspaceCycler::StartWorkspaceCyclerAnimationFinished() {
  DCHECK_EQ(STARTING_CYCLING, state_);
  SetState(CYCLING);
}

void WorkspaceCycler::StopWorkspaceCyclerAnimationFinished() {
  DCHECK_EQ(STOPPING_CYCLING, state_);
  Workspace* workspace_to_activate = animator_->get_selected_workspace();
  animator_.reset();
  SetState(NOT_CYCLING);

  // Activate the workspace after updating the state so that a call to
  // AbortCycling() as a result of SetActiveWorkspaceFromCycler() is a noop.
  workspace_manager_->SetActiveWorkspaceFromCycler(workspace_to_activate);
}

}  // namespace internal
}  // namespace ash
