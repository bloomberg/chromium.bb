// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "ash/wm/overview/overview_window_button.h"
#include "ash/wm/overview/overview_window_targeter.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/window_state.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Foreground label color.
static const SkColor kLabelColor = SK_ColorWHITE;

// Label shadow color.
static const SkColor kLabelShadow = 0xB0000000;

// Solid shadow length from the label
static const int kVerticalShadowOffset = 1;

// Amount of blur applied to the label shadow
static const int kShadowBlur = 10;

}  // namespace

// LabelButton shown under each of the windows.
class OverviewWindowButton::OverviewButtonView : public views::LabelButton {
 public:
  OverviewButtonView(views::ButtonListener* listener,
                     const base::string16& text)
      : views::LabelButton(listener, text),
        selector_item_bounds_(gfx::Rect()) {}

  ~OverviewButtonView() override {}

  // Updates the |selector_item_bounds_|, converting them to our coordinates.
  void SetSelectorItemBounds(const gfx::Rect& selector_item_bounds) {
    selector_item_bounds_ = ScreenUtil::ConvertRectFromScreen(
        GetWidget()->GetNativeWindow()->GetRootWindow(), selector_item_bounds);
    gfx::Point origin = selector_item_bounds_.origin();
    gfx::Rect target_bounds = GetWidget()->GetNativeWindow()->GetTargetBounds();
    origin.Offset(-target_bounds.x(), -target_bounds.y());
    selector_item_bounds_.set_origin(origin);
  }

  // views::View:
  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (!selector_item_bounds_.Contains(event.location()))
      return;

    NotifyClick(event);
  }

 private:
  // Bounds to check if a mouse release occurred outside the window item.
  gfx::Rect selector_item_bounds_;

  DISALLOW_COPY_AND_ASSIGN(OverviewButtonView);
};

OverviewWindowButton::OverviewWindowButton(aura::Window* target)
    : target_(target), window_label_(new views::Widget) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = Shell::GetContainer(target_->GetRootWindow(),
                                      kShellWindowId_OverlayContainer);
  params.visible_on_all_workspaces = true;
  window_label_->set_focus_on_creation(false);
  window_label_->Init(params);
  window_label_button_view_ = new OverviewButtonView(this, target_->title());
  window_label_button_view_->SetTextColor(views::LabelButton::STATE_NORMAL,
                                          kLabelColor);
  window_label_button_view_->SetTextColor(views::LabelButton::STATE_HOVERED,
                                          kLabelColor);
  window_label_button_view_->SetTextColor(views::LabelButton::STATE_PRESSED,
                                          kLabelColor);
  window_label_button_view_->set_animate_on_state_change(false);
  window_label_button_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  window_label_button_view_->SetBorder(views::Border::NullBorder());
  window_label_button_view_->SetTextShadows(gfx::ShadowValues(
      1, gfx::ShadowValue(gfx::Point(0, kVerticalShadowOffset), kShadowBlur,
                          kLabelShadow)));
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  window_label_button_view_->SetFontList(
      bundle.GetFontList(ui::ResourceBundle::BoldFont));
  window_label_->SetContentsView(window_label_button_view_);

  overview_window_targeter_ =
      new OverviewWindowTargeter(window_label_->GetNativeWindow());
  scoped_window_targeter_.reset(new aura::ScopedWindowTargeter(
      target, scoped_ptr<OverviewWindowTargeter>(overview_window_targeter_)));
}

OverviewWindowButton::~OverviewWindowButton() {
}

void OverviewWindowButton::SetBounds(
    const gfx::Rect& bounds,
    const OverviewAnimationType& animation_type) {
  if (!window_label_->IsVisible()) {
    window_label_->Show();
    ScopedOverviewAnimationSettings::SetupFadeInAfterLayout(
        window_label_->GetNativeWindow());
  }
  gfx::Rect converted_bounds =
      ScreenUtil::ConvertRectFromScreen(target_->GetRootWindow(), bounds);
  gfx::Rect label_bounds(converted_bounds.x(), converted_bounds.bottom(),
                         converted_bounds.width(), 0);
  label_bounds.set_height(
      window_label_->GetContentsView()->GetPreferredSize().height());
  label_bounds.set_y(
      label_bounds.y() -
      window_label_->GetContentsView()->GetPreferredSize().height());

  ScopedOverviewAnimationSettings animation_settings(
      animation_type, window_label_->GetNativeWindow());

  window_label_->GetNativeWindow()->SetBounds(label_bounds);
  window_label_button_view_->SetSelectorItemBounds(bounds);
  overview_window_targeter_->set_bounds(
      ScreenUtil::ConvertRectFromScreen(target_->GetRootWindow(), bounds));
}

void OverviewWindowButton::SendFocusAlert() const {
  window_label_button_view_->NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, true);
}

void OverviewWindowButton::SetLabelText(const base::string16& title) {
  window_label_button_view_->SetText(title);
}

void OverviewWindowButton::SetOpacity(float opacity) {
  window_label_->GetNativeWindow()->layer()->SetOpacity(opacity);
}

void OverviewWindowButton::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  wm::GetWindowState(target_)->Activate();
}

}  // namespace ash
