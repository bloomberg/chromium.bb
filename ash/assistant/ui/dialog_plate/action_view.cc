// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/action_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/ui/logo_view/base_logo_view.h"
#include "ash/assistant/util/views_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 22;
constexpr int kPreferredSizeDip = 32;

}  // namespace

ActionView::ActionView(views::ButtonListener* listener,
                       AssistantController* assistant_controller)
    : views::Button(/*listener=*/nullptr),
      assistant_controller_(assistant_controller),
      listener_(listener) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // ActionView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

ActionView::~ActionView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size ActionView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredSizeDip, GetHeightForWidth(kPreferredSizeDip));
}

int ActionView::GetHeightForWidth(int width) const {
  return kPreferredSizeDip;
}

void ActionView::RequestFocus() {
  button_->RequestFocus();
}

void ActionView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (listener_)
    listener_->ButtonPressed(this, event);
}

void ActionView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Button.
  button_ = assistant::util::CreateButton(this, kPreferredSizeDip);
  AddChildView(button_);

  // Voice action container.
  views::View* voice_action_container_ = new views::View();
  voice_action_container_->set_can_process_events_within_subtree(false);
  AddChildView(voice_action_container_);

  views::BoxLayout* layout_manager = voice_action_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::MAIN_AXIS_ALIGNMENT_CENTER);

  // Voice action.
  voice_action_view_ = BaseLogoView::Create();
  voice_action_view_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  voice_action_container_->AddChildView(voice_action_view_);

  // Initialize state.
  UpdateState(/*animate=*/false);
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

void ActionView::SetAccessibleName(const base::string16& accessible_name) {
  button_->SetAccessibleName(accessible_name);
}

void ActionView::SetFocusBehavior(FocusBehavior focus_behavior) {
  button_->SetFocusBehavior(focus_behavior);
}

}  // namespace ash
