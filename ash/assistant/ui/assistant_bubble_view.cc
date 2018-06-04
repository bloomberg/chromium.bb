// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_bubble_view.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/ui/assistant_main_view.h"
#include "ash/assistant/ui/assistant_mini_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// Appearance.
constexpr SkColor kBackgroundColor = SK_ColorWHITE;
constexpr int kCornerRadiusDip = 20;
constexpr int kMarginDip = 8;

}  // namespace

AssistantBubbleView::AssistantBubbleView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  set_accept_events(true);
  SetAnchor();
  set_arrow(views::BubbleBorder::Arrow::BOTTOM_CENTER);
  set_close_on_deactivate(false);
  set_color(kBackgroundColor);
  set_margins(gfx::Insets());
  set_shadow(views::BubbleBorder::Shadow::NO_ASSETS);
  set_title_margins(gfx::Insets());

  views::BubbleDialogDelegateView::CreateBubble(this);

  // These attributes can only be set after bubble creation:
  GetBubbleFrameView()->bubble_border()->SetCornerRadius(kCornerRadiusDip);
  SetAlignment(views::BubbleBorder::BubbleAlignment::ALIGN_EDGE_TO_ANCHOR_EDGE);
  SetArrowPaintType(views::BubbleBorder::PAINT_NONE);

  // The Assistant controller indirectly owns the view hierarchy to which
  // AssistantBubbleView belongs so is guaranteed to outlive it.
  assistant_controller_->AddInteractionModelObserver(this);
}

AssistantBubbleView::~AssistantBubbleView() {
  assistant_controller_->RemoveInteractionModelObserver(this);
}

void AssistantBubbleView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantBubbleView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();

  if (GetWidget())
    SizeToContents();
}

int AssistantBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void AssistantBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->corner_radius = kCornerRadiusDip;
  params->keep_on_top = true;
  params->shadow_type = views::Widget::InitParams::SHADOW_TYPE_DROP;
  params->shadow_elevation = wm::kShadowElevationActiveWindow;
}

void AssistantBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Main view.
  assistant_main_view_ =
      std::make_unique<AssistantMainView>(assistant_controller_);
  assistant_main_view_->set_owned_by_client();

  // Mini view.
  assistant_mini_view_ = std::make_unique<AssistantMiniView>();
  assistant_mini_view_->set_owned_by_client();

  // Update the view state based on the current input modality.
  OnInputModalityChanged(
      assistant_controller_->interaction_model()->input_modality());

  // TODO(dmblack): Add support for AssistantSettingsView.
}

void AssistantBubbleView::RequestFocus() {
  if (assistant_main_view_)
    assistant_main_view_->RequestFocus();
}

void AssistantBubbleView::SetAnchor() {
  // TODO(dmblack): Handle multiple displays, dynamic shelf repositioning, and
  // any other corner cases.
  // Anchors to bottom center of primary display's work area.
  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Rect work_area = primary_display.work_area();
  gfx::Rect anchor = gfx::Rect(work_area.x(), work_area.bottom() - kMarginDip,
                               work_area.width(), 0);

  SetAnchorRect(anchor);
}

void AssistantBubbleView::OnInputModalityChanged(InputModality input_modality) {
  if (input_modality == InputModality::kStylus) {
    // When switching to stylus input modality, only the AssistantMiniView
    // should be visible.
    RemoveAllChildViews(/*delete_children=*/false);
    AddChildView(assistant_mini_view_.get());
    PreferredSizeChanged();
  } else if (!assistant_main_view_->parent()) {
    // When switching to a non-stylus input modality, only the AssistantMainView
    // should be visible. Note that because there are multiple non-stylus input
    // modalities, this is only necessary if we are not already showing the
    // AssistantMainView.
    RemoveAllChildViews(/*delete_children=*/false);
    AddChildView(assistant_main_view_.get());
    PreferredSizeChanged();
  }
}

}  // namespace ash
