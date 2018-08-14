// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/action_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/ui/logo_view/base_logo_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredSizeDip = 22;

}  // namespace

ActionView::ActionView(AssistantController* assistant_controller,
                       ActionViewListener* listener)
    : assistant_controller_(assistant_controller), listener_(listener) {
  InitLayout();
  UpdateState(/*animate=*/false);

  // The Assistant controller indirectly owns the view hierarchy to which
  // ActionView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

ActionView::~ActionView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size ActionView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredSizeDip, kPreferredSizeDip);
}

void ActionView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Voice action.
  voice_action_view_ = BaseLogoView::Create();
  voice_action_view_->SetPreferredSize(
      gfx::Size(kPreferredSizeDip, kPreferredSizeDip));
  AddChildView(voice_action_view_);
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

void ActionView::OnMicStateChanged(MicState mic_state) {
  is_user_speaking_ = false;
  UpdateState(/*animate=*/true);
}

void ActionView::OnSpeechLevelChanged(float speech_level_db) {
  // TODO: Work with UX to determine the threshold.
  constexpr float kSpeechLevelThreshold = -60.0f;
  if (speech_level_db < kSpeechLevelThreshold)
    return;

  voice_action_view_->SetSpeechLevel(speech_level_db);
  if (!is_user_speaking_) {
    is_user_speaking_ = true;
    UpdateState(/*animate=*/true);
  }
}

void ActionView::UpdateState(bool animate) {
  const AssistantInteractionModel* interaction_model =
      assistant_controller_->interaction_controller()->model();

  BaseLogoView::State mic_state;
  switch (interaction_model->mic_state()) {
    case MicState::kClosed:
      mic_state = BaseLogoView::State::kMicFab;
      break;
    case MicState::kOpen:
      mic_state = is_user_speaking_ ? BaseLogoView::State::kUserSpeaks
                                    : BaseLogoView::State::kListening;
      break;
  }
  voice_action_view_->SetState(mic_state, animate);
}

}  // namespace ash
