// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_main_view.h"

#include <memory>

#include "ash/app_list/views/assistant/assistant_main_stage.h"
#include "ash/app_list/views/assistant/dialog_plate.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

constexpr int kBottomPaddingDip = 8;

}  // namespace

AssistantMainView::AssistantMainView(ash::AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();
}

AssistantMainView::~AssistantMainView() = default;

const char* AssistantMainView::GetClassName() const {
  return "AssistantMainView";
}

gfx::Size AssistantMainView::CalculatePreferredSize() const {
  return gfx::Size(ash::kPreferredWidthDip,
                   GetHeightForWidth(ash::kPreferredWidthDip));
}

void AssistantMainView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();

  // Even though the preferred size for |main_stage_| may change, its bounds
  // may not actually change due to height restrictions imposed by its parent.
  // For this reason, we need to explicitly trigger a layout pass so that the
  // children of |main_stage_| are properly updated.
  if (child == main_stage_) {
    Layout();
    SchedulePaint();
  }
}

void AssistantMainView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

views::View* AssistantMainView::FindFirstFocusableView() {
  // In those instances in which we want to override views::FocusSearch
  // behavior, DialogPlate will identify the first focusable view.
  return dialog_plate_->FindFirstFocusableView();
}

void AssistantMainView::RequestFocus() {
  dialog_plate_->RequestFocus();
}

void AssistantMainView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  layer_mask_->layer()->SetBounds(GetLocalBounds());
}

void AssistantMainView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  layer_mask_ = views::Painter::CreatePaintedLayer(
      views::Painter::CreateSolidRoundRectPainter(
          SK_ColorBLACK, search_box::kSearchBoxBorderCornerRadiusSearchResult));
  layer_mask_->layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMaskLayer(layer_mask_->layer());

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetBorder(views::CreateEmptyBorder(gfx::Insets(0, 0, kBottomPaddingDip, 0)));

  // Dialog plate.
  dialog_plate_ = new DialogPlate(delegate_);
  AddChildView(dialog_plate_);

  // Main stage.
  main_stage_ = new AssistantMainStage(delegate_);
  AddChildView(main_stage_);

  layout->SetFlexForView(main_stage_, 1);
}

}  // namespace app_list
