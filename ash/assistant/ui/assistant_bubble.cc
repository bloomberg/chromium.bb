// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_bubble.h"

#include <memory>

#include "ash/assistant/ash_assistant_controller.h"
#include "ash/assistant/ui/assistant_bubble_view.h"
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
constexpr int kCornerRadiusDip = 16;
constexpr int kMarginDip = 16;

// AssistantContainerView ------------------------------------------------------

class AssistantContainerView : public views::BubbleDialogDelegateView {
 public:
  explicit AssistantContainerView(AshAssistantController* assistant_controller)
      : assistant_controller_(assistant_controller) {
    set_accept_events(true);
    SetAnchor();
    set_arrow(views::BubbleBorder::Arrow::BOTTOM_LEFT);
    set_can_activate(false);
    set_color(kBackgroundColor);
    set_margins(gfx::Insets());
    set_shadow(views::BubbleBorder::Shadow::NO_ASSETS);
    set_title_margins(gfx::Insets());

    views::BubbleDialogDelegateView::CreateBubble(this);

    // These attributes can only be set after bubble creation:
    GetBubbleFrameView()->bubble_border()->SetCornerRadius(kCornerRadiusDip);
    SetAlignment(
        views::BubbleBorder::BubbleAlignment::ALIGN_EDGE_TO_ANCHOR_EDGE);
    SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
  }

  ~AssistantContainerView() override = default;

  // views::BubbleDialogDelegateView:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override {
    params->corner_radius = kCornerRadiusDip;
    params->keep_on_top = true;
    params->shadow_type = views::Widget::InitParams::SHADOW_TYPE_DROP;
    params->shadow_elevation = wm::kShadowElevationActiveWindow;
  }

  void Init() override { InitLayout(); }

  void ChildPreferredSizeChanged(views::View* child) override {
    SizeToContents();
  }

  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_NONE; }

 private:
  void InitLayout() {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(new AssistantBubbleView(assistant_controller_));
  }

  void SetAnchor() {
    // TODO(dmblack): Handle multiple displays, RTL orientation, dynamic shelf
    // repositioning and any other corner cases.
    // Anchors to bottom lefthand corner of primary display's work area.
    display::Display primary_display =
        display::Screen::GetScreen()->GetPrimaryDisplay();

    gfx::Rect work_area = primary_display.work_area();
    gfx::Rect anchor = gfx::Rect(work_area.x() + kMarginDip,
                                 work_area.bottom() - kMarginDip, 0, 0);

    SetAnchorRect(anchor);
  }

  AshAssistantController* assistant_controller_;  // Owned by Shell.

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerView);
};

}  // namespace

// AssistantBubble -------------------------------------------------------------

AssistantBubble::AssistantBubble(AshAssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  assistant_controller_->AddInteractionModelObserver(this);
}

AssistantBubble::~AssistantBubble() {
  assistant_controller_->RemoveInteractionModelObserver(this);

  if (container_view_)
    container_view_->GetWidget()->RemoveObserver(this);
}

void AssistantBubble::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);
  container_view_ = nullptr;
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

void AssistantBubble::Show() {
  if (!container_view_)
    container_view_ = new AssistantContainerView(assistant_controller_);

  container_view_->GetWidget()->AddObserver(this);
  container_view_->GetWidget()->ShowInactive();
}

void AssistantBubble::Dismiss() {
  if (container_view_)
    container_view_->GetWidget()->Close();
}

}  // namespace ash
