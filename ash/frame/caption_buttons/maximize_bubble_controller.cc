// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/caption_buttons/maximize_bubble_controller.h"

#include "ash/frame/caption_buttons/frame_maximize_button.h"
#include "ash/frame/caption_buttons/maximize_bubble_controller_bubble.h"
#include "base/timer/timer.h"


namespace ash {

MaximizeBubbleController::MaximizeBubbleController(
    FrameMaximizeButton* frame_maximize_button,
    MaximizeBubbleFrameState maximize_type,
    int appearance_delay_ms)
    : frame_maximize_button_(frame_maximize_button),
      bubble_(NULL),
      maximize_type_(maximize_type),
      snap_type_for_creation_(SNAP_NONE),
      appearance_delay_ms_(appearance_delay_ms) {
  // Create the task which will create the bubble delayed.
  base::OneShotTimer<MaximizeBubbleController>* new_timer =
      new base::OneShotTimer<MaximizeBubbleController>();
  // Note: Even if there was no delay time given, we need to have a timer.
  new_timer->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          appearance_delay_ms_ ? appearance_delay_ms_ : 10),
      this,
      &MaximizeBubbleController::CreateBubble);
  timer_.reset(new_timer);
  if (!appearance_delay_ms_)
    CreateBubble();
}

MaximizeBubbleController::~MaximizeBubbleController() {
  // Note: The destructor only gets initiated through the owner.
  timer_.reset();
  if (bubble_) {
    bubble_->ControllerRequestsCloseAndDelete();
    bubble_ = NULL;
  }
}

void MaximizeBubbleController::SetSnapType(SnapType snap_type) {
  if (bubble_) {
    bubble_->SetSnapType(snap_type);
  } else {
    // The bubble has not been created yet. This can occur if bubble creation is
    // delayed.
    snap_type_for_creation_ = snap_type;
  }
}

aura::Window* MaximizeBubbleController::GetBubbleWindow() {
  return bubble_ ? bubble_->GetBubbleWindow() : NULL;
}

void MaximizeBubbleController::DelayCreation() {
  if (timer_.get() && timer_->IsRunning())
    timer_->Reset();
}

void MaximizeBubbleController::OnButtonClicked(SnapType snap_type) {
  frame_maximize_button_->ExecuteSnapAndCloseMenu(snap_type);
}

void MaximizeBubbleController::OnButtonHover(SnapType snap_type) {
  frame_maximize_button_->SnapButtonHovered(snap_type);
}

views::CustomButton* MaximizeBubbleController::GetButtonForUnitTest(
    SnapType state) {
  return bubble_ ? bubble_->GetButtonForUnitTest(state) : NULL;
}

void MaximizeBubbleController::RequestDestructionThroughOwner() {
  // Tell the parent to destroy us (if this didn't happen yet).
  if (timer_) {
    timer_.reset(NULL);
    // Informs the owner that the menu is gone and requests |this| destruction.
    frame_maximize_button_->DestroyMaximizeMenu();
    // Note: After this call |this| is destroyed.
  }
}

void MaximizeBubbleController::CreateBubble() {
  if (!bubble_) {
    bubble_ = new MaximizeBubbleControllerBubble(this, appearance_delay_ms_,
                                                 snap_type_for_creation_);
    frame_maximize_button_->OnMaximizeBubbleShown(bubble_->GetWidget());
  }

  timer_->Stop();
}

}  // namespace ash
