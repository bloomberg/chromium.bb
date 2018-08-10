// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_main_view.h"
#include "ash/assistant/ui/assistant_mini_view.h"
#include "ash/assistant/ui/assistant_web_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// Appearance.
constexpr SkColor kBackgroundColor = SK_ColorWHITE;
constexpr int kCornerRadiusDip = 20;
constexpr int kMarginDip = 8;

// AssistantContainerLayout ----------------------------------------------------

// The AssistantContainerLayout calculates preferred size to fit the largest
// visible child. Children that are not visible are not factored in. During
// layout, children are horizontally centered and bottom aligned.
class AssistantContainerLayout : public views::LayoutManager {
 public:
  AssistantContainerLayout() = default;
  ~AssistantContainerLayout() override = default;

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    int preferred_width = 0;

    for (int i = 0; i < host->child_count(); ++i) {
      const views::View* child = host->child_at(i);

      // We do not include invisible children in our size calculation.
      if (!child->visible())
        continue;

      // Our preferred width is the width of our largest visible child.
      preferred_width =
          std::max(child->GetPreferredSize().width(), preferred_width);
    }

    return gfx::Size(preferred_width,
                     GetPreferredHeightForWidth(host, preferred_width));
  }

  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override {
    int preferred_height = 0;

    for (int i = 0; i < host->child_count(); ++i) {
      const views::View* child = host->child_at(i);

      // We do not include invisible children in our size calculation.
      if (!child->visible())
        continue;

      // Our preferred height is the height of our largest visible child.
      preferred_height =
          std::max(child->GetHeightForWidth(width), preferred_height);
    }

    return preferred_height;
  }

  void Layout(views::View* host) override {
    const int host_width = host->width();
    const int host_height = host->height();

    for (int i = 0; i < host->child_count(); ++i) {
      views::View* child = host->child_at(i);

      const gfx::Size child_size = child->GetPreferredSize();

      // Children are horizontally centered and bottom aligned.
      int child_left = (host_width - child_size.width()) / 2;
      int child_top = host_height - child_size.height();

      child->SetBounds(child_left, child_top, child_size.width(),
                       child_size.height());
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantContainerLayout);
};

}  // namespace

// AssistantContainerView ------------------------------------------------------

AssistantContainerView::AssistantContainerView(
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

  // The AssistantController owns the view hierarchy to which
  // AssistantContainerView belongs so is guaranteed to outlive it.
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

AssistantContainerView::~AssistantContainerView() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

void AssistantContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantContainerView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();

  if (GetWidget())
    SizeToContents();
}

int AssistantContainerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void AssistantContainerView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->corner_radius = kCornerRadiusDip;
  params->keep_on_top = true;
  params->shadow_type = views::Widget::InitParams::SHADOW_TYPE_DROP;
  params->shadow_elevation = wm::kShadowElevationActiveWindow;
}

void AssistantContainerView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Apply a clip path to ensure children do not extend outside container
  // boundaries. The clip path also enforces our round corners on child views.
  gfx::Path clip_path;
  clip_path.addRoundRect(gfx::RectToSkRect(GetLocalBounds()), kCornerRadiusDip,
                         kCornerRadiusDip);
  set_clip_path(clip_path);
  SchedulePaint();
}

void AssistantContainerView::Init() {
  SetLayoutManager(std::make_unique<AssistantContainerLayout>());

  // Main view.
  assistant_main_view_ = new AssistantMainView(assistant_controller_);
  AddChildView(assistant_main_view_);

  // Mini view.
  assistant_mini_view_ = new AssistantMiniView(assistant_controller_);
  assistant_mini_view_->set_delegate(assistant_controller_->ui_controller());
  AddChildView(assistant_mini_view_);

  // Web view.
  assistant_web_view_ = new AssistantWebView(assistant_controller_);
  AddChildView(assistant_web_view_);

  // Update the view state based on the current UI mode.
  OnUiModeChanged(assistant_controller_->ui_controller()->model()->ui_mode());
}

void AssistantContainerView::RequestFocus() {
  if (assistant_main_view_)
    assistant_main_view_->RequestFocus();
}

void AssistantContainerView::SetAnchor() {
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

void AssistantContainerView::OnUiModeChanged(AssistantUiMode ui_mode) {
  for (int i = 0; i < child_count(); ++i) {
    child_at(i)->SetVisible(false);
  }

  switch (ui_mode) {
    case AssistantUiMode::kMiniUi:
      assistant_mini_view_->SetVisible(true);
      break;
    case AssistantUiMode::kMainUi:
      assistant_main_view_->SetVisible(true);
      break;
    case AssistantUiMode::kWebUi:
      assistant_web_view_->SetVisible(true);
      break;
  }

  PreferredSizeChanged();
}

}  // namespace ash
