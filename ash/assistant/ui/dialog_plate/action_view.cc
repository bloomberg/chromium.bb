// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/action_view.h"

#include "ash/assistant/assistant_controller.h"
#include "ui/gfx/canvas.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredSizeDip = 24;

// TODO(dmblack): Remove after LogoView is implemented.
// Temporary colors to represent states.
constexpr SkColor kKeyboardColor = SkColorSetRGB(0x4C, 0x8B, 0xF5);    // Blue
constexpr SkColor kMicOpenColor = SkColorSetRGB(0xDD, 0x51, 0x44);     // Red
constexpr SkColor kMicClosedColor = SkColorSetA(SK_ColorBLACK, 0x1F);  // Grey

}  // namespace

ActionView::ActionView(AssistantController* assistant_controller,
                       ActionViewListener* listener)
    : assistant_controller_(assistant_controller), listener_(listener) {
  // The Assistant controller indirectly owns the view hierarchy to which
  // ActionView belongs so is guaranteed to outlive it.
  assistant_controller_->AddInteractionModelObserver(this);

  // Set initial state.
  UpdateState();
}

ActionView::~ActionView() {
  assistant_controller_->RemoveInteractionModelObserver(this);
}

gfx::Size ActionView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredSizeDip, kPreferredSizeDip);
}

// TODO(dmblack): Remove after LogoView is implemented.
void ActionView::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color_);
  canvas->DrawRoundRect(GetContentsBounds(), kPreferredSizeDip / 2, flags);
}

void ActionView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return;

  event->SetHandled();

  if (listener_)
    listener_->OnActionPressed();
}

bool ActionView::OnMousePressed(const ui::MouseEvent& event) {
  if (listener_)
    listener_->OnActionPressed();
  return true;
}

void ActionView::OnInputModalityChanged(InputModality input_modality) {
  UpdateState();
}

void ActionView::OnMicStateChanged(MicState mic_state) {
  UpdateState();
}

// TODO(dmblack): Currently state is being indicated by painting a different
// background color. This will be replaced once the stateful LogoView has been
// implemented (b/78191547).
void ActionView::UpdateState() {
  const AssistantInteractionModel* interaction_model =
      assistant_controller_->interaction_model();

  if (interaction_model->input_modality() == InputModality::kKeyboard) {
    color_ = kKeyboardColor;
    SchedulePaint();
    return;
  }

  switch (interaction_model->mic_state()) {
    case MicState::kClosed:
      color_ = kMicClosedColor;
      break;
    case MicState::kOpen:
      color_ = kMicOpenColor;
      break;
  }

  SchedulePaint();
}

}  // namespace ash
