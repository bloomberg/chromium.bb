// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_main_view.h"

#include <memory>

#include "ash/assistant/assistant_bubble_controller.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/caption_bar.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/assistant/ui/main_stage/ui_element_container_view.h"
#include "ash/assistant/ui/suggestion_container_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMinHeightDip = 200;
constexpr int kMaxHeightDip = 640;

}  // namespace

AssistantMainView::AssistantMainView(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      caption_bar_(new CaptionBar(assistant_controller->bubble_controller())),
      ui_element_container_(new UiElementContainerView(assistant_controller)),
      suggestions_container_(new SuggestionContainerView(assistant_controller)),
      dialog_plate_(new DialogPlate(assistant_controller)),
      min_height_dip_(kMinHeightDip) {
  InitLayout();

  // Observe changes to interaction model.
  assistant_controller_->AddInteractionModelObserver(this);
}

AssistantMainView::~AssistantMainView() {
  assistant_controller_->RemoveInteractionModelObserver(this);
}

gfx::Size AssistantMainView::CalculatePreferredSize() const {
  // |min_height_dip_| <= |preferred_height| <= |kMaxHeightDip|.
  int preferred_height = GetHeightForWidth(kPreferredWidthDip);
  preferred_height = std::min(preferred_height, kMaxHeightDip);
  preferred_height = std::max(preferred_height, min_height_dip_);
  return gfx::Size(kPreferredWidthDip, preferred_height);
}

void AssistantMainView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  // Until Assistant UI is hidden, the view may grow in height but not shrink.
  min_height_dip_ = std::max(min_height_dip_, height());
}

void AssistantMainView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();

  // We force a layout here because, though we are receiving a
  // ChildPreferredSizeChanged event, it may be that the
  // |ui_element_container_|'s bounds will not actually change due to the height
  // restrictions imposed by AssistantMainView. When this is the case, we
  // need to force a layout to see |ui_element_container_|'s new contents.
  if (child == ui_element_container_)
    Layout();
}

void AssistantMainView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kSpacingDip));

  // Caption bar.
  AddChildView(caption_bar_);

  // UI element container.
  AddChildView(ui_element_container_);

  layout_manager->SetFlexForView(ui_element_container_, 1);

  // Suggestions container.
  suggestions_container_->SetVisible(false);
  AddChildView(suggestions_container_);

  // Dialog plate.
  AddChildView(dialog_plate_);
}

void AssistantMainView::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kInactive)
    return;

  // When the Assistant UI is being hidden we need to reset our minimum height
  // restriction so that the default restrictions are restored for the next
  // time the view is shown.
  min_height_dip_ = kMinHeightDip;
  PreferredSizeChanged();
}

void AssistantMainView::RequestFocus() {
  dialog_plate_->RequestFocus();
}

}  // namespace ash
