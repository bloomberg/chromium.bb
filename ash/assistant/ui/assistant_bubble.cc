// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_bubble.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/ui/assistant_bubble_view.h"

namespace ash {

AssistantBubble::AssistantBubble(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  assistant_controller_->AddInteractionModelObserver(this);
}

AssistantBubble::~AssistantBubble() {
  assistant_controller_->RemoveInteractionModelObserver(this);

  if (bubble_view_)
    bubble_view_->GetWidget()->RemoveObserver(this);
}

void AssistantBubble::OnWidgetActivationChanged(views::Widget* widget,
                                                bool active) {
  if (active)
    bubble_view_->RequestFocus();
}

void AssistantBubble::OnWidgetDestroying(views::Widget* widget) {
  // We need to be sure that the Assistant interaction is stopped when the
  // widget is closed. Special cases, such as closing the widget via the |ESC|
  // key might otherwise go unhandled, causing inconsistencies between the
  // widget visibility state and the underlying interaction model state.
  assistant_controller_->StopInteraction();

  bubble_view_->GetWidget()->RemoveObserver(this);
  bubble_view_ = nullptr;
}

void AssistantBubble::OnInteractionStateChanged(
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

bool AssistantBubble::IsVisible() const {
  return bubble_view_ && bubble_view_->GetWidget()->IsVisible();
}

void AssistantBubble::Show() {
  if (!bubble_view_) {
    bubble_view_ = new AssistantBubbleView(assistant_controller_);
    bubble_view_->GetWidget()->AddObserver(this);
  }
  bubble_view_->GetWidget()->Show();
}

void AssistantBubble::Dismiss() {
  if (bubble_view_)
    bubble_view_->GetWidget()->Hide();
}

}  // namespace ash
