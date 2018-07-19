// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_header_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/logo_view/base_logo_view.h"
#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kInitialHeightDip = 72;

}  // namespace

AssistantHeaderView::AssistantHeaderView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // AssistantHeaderView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

AssistantHeaderView::~AssistantHeaderView() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size AssistantHeaderView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int AssistantHeaderView::GetHeightForWidth(int width) const {
  return greeting_label_->visible() ? kInitialHeightDip
                                    : views::View::GetHeightForWidth(width);
}

void AssistantHeaderView::ChildVisibilityChanged(views::View* child) {
  layout_manager_->set_cross_axis_alignment(
      greeting_label_->visible()
          ? views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER
          : views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_START);

  PreferredSizeChanged();
}

void AssistantHeaderView::InitLayout() {
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), kSpacingDip));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Molecule icon.
  BaseLogoView* molecule_icon = BaseLogoView::Create();
  molecule_icon->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  molecule_icon->SetState(BaseLogoView::State::kMoleculeWavy,
                          /*animate=*/false);
  AddChildView(molecule_icon);

  // Greeting label.
  greeting_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
  greeting_label_->SetAutoColorReadabilityEnabled(false);
  greeting_label_->SetEnabledColor(kTextColorPrimary);
  greeting_label_->SetFontList(
      views::Label::GetDefaultFontList()
          .DeriveWithSizeDelta(8)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  greeting_label_->SetMultiLine(true);
  AddChildView(greeting_label_);

  // Progress indicator.
  // Note that we add an empty border to increase the top margin.
  progress_indicator_ = new AssistantProgressIndicator();
  progress_indicator_->SetBorder(
      views::CreateEmptyBorder(/*top=*/kSpacingDip, 0, 0, 0));
  progress_indicator_->SetVisible(false);
  AddChildView(progress_indicator_);
}

void AssistantHeaderView::OnCommittedQueryChanged(
    const AssistantQuery& committed_query) {
  greeting_label_->SetVisible(false);
  progress_indicator_->SetVisible(true);
}

void AssistantHeaderView::OnUiElementAdded(
    const AssistantUiElement* ui_element) {
  progress_indicator_->SetVisible(false);
}

void AssistantHeaderView::OnUiVisibilityChanged(bool visible,
                                                AssistantSource source) {
  if (visible)
    return;

  // When Assistant UI is being hidden, we need to restore default view state.
  greeting_label_->SetVisible(true);
  progress_indicator_->SetVisible(false);
}

}  // namespace ash
