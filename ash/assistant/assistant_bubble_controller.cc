// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_bubble_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/ui/assistant_bubble_view.h"
#include "base/optional.h"

namespace ash {

AssistantBubbleController::AssistantBubbleController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  assistant_controller_->AddInteractionModelObserver(this);
}

AssistantBubbleController::~AssistantBubbleController() {
  assistant_controller_->RemoveInteractionModelObserver(this);

  if (bubble_view_)
    bubble_view_->GetWidget()->RemoveObserver(this);
}

void AssistantBubbleController::AddModelObserver(
    AssistantBubbleModelObserver* observer) {
  assistant_bubble_model_.AddObserver(observer);
}

void AssistantBubbleController::RemoveModelObserver(
    AssistantBubbleModelObserver* observer) {
  assistant_bubble_model_.RemoveObserver(observer);
}

void AssistantBubbleController::OnWidgetActivationChanged(views::Widget* widget,
                                                          bool active) {
  if (active)
    bubble_view_->RequestFocus();
}

void AssistantBubbleController::OnWidgetVisibilityChanged(views::Widget* widget,
                                                          bool visible) {
  UpdateUiMode();
}

void AssistantBubbleController::OnWidgetDestroying(views::Widget* widget) {
  // We need to be sure that the Assistant interaction is stopped when the
  // widget is closed. Special cases, such as closing the widget via the |ESC|
  // key might otherwise go unhandled, causing inconsistencies between the
  // widget visibility state and the underlying interaction model state.
  assistant_controller_->StopInteraction();

  bubble_view_->GetWidget()->RemoveObserver(this);
  bubble_view_ = nullptr;
}

void AssistantBubbleController::OnInputModalityChanged(
    InputModality input_modality) {
  UpdateUiMode();
}

void AssistantBubbleController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  switch (interaction_state) {
    case InteractionState::kActive:
      Show();
      break;
    case InteractionState::kInactive:
      Dismiss();
      break;
  }
}

void AssistantBubbleController::OnMicStateChanged(MicState mic_state) {
  UpdateUiMode();
}

bool AssistantBubbleController::OnCaptionButtonPressed(CaptionButtonId id) {
  if (id == CaptionButtonId::kMinimize) {
    UpdateUiMode(AssistantUiMode::kMiniUi);
    return true;
  }
  return false;
}

bool AssistantBubbleController::IsVisible() const {
  return bubble_view_ && bubble_view_->GetWidget()->IsVisible();
}

void AssistantBubbleController::Show() {
  if (!bubble_view_) {
    bubble_view_ = new AssistantBubbleView(assistant_controller_);
    bubble_view_->GetWidget()->AddObserver(this);
  }
  bubble_view_->GetWidget()->Show();
}

void AssistantBubbleController::Dismiss() {
  if (bubble_view_)
    bubble_view_->GetWidget()->Hide();
}

void AssistantBubbleController::UpdateUiMode(
    base::Optional<AssistantUiMode> ui_mode) {
  // If a UI mode is provided, we will use it in lieu of updating UI mode on the
  // basis of interaction/widget visibility state.
  if (ui_mode.has_value()) {
    assistant_bubble_model_.SetUiMode(ui_mode.value());
    return;
  }

  // When Assistant UI is not visible, we should reset to main UI mode.
  if (!IsVisible()) {
    assistant_bubble_model_.SetUiMode(AssistantUiMode::kMainUi);
    return;
  }

  // When the mic is open, we should be in main UI mode.
  if (assistant_controller_->interaction_model()->mic_state() ==
      MicState::kOpen) {
    assistant_bubble_model_.SetUiMode(AssistantUiMode::kMainUi);
    return;
  }

  // When stylus input modality is selected, we should be in mini UI mode.
  if (assistant_controller_->interaction_model()->input_modality() ==
      InputModality::kStylus) {
    assistant_bubble_model_.SetUiMode(AssistantUiMode::kMiniUi);
    return;
  }

  // By default, we will fall back to main UI mode.
  assistant_bubble_model_.SetUiMode(AssistantUiMode::kMainUi);
}

}  // namespace ash
