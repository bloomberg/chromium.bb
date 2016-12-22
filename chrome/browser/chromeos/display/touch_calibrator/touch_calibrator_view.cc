// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/touch_calibrator/touch_calibrator_view.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

constexpr char kWidgetName[] = "TouchCalibratorOverlay";

// Returns the initialization params for the widget that contains the touch
// calibrator view.
views::Widget::InitParams GetWidgetParams(aura::Window* root_window) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = kWidgetName;
  params.keep_on_top = true;
  params.accept_events = true;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = ash::Shell::GetContainer(
      root_window, ash::kShellWindowId_OverlayContainer);
  return params;
}

}  // namespace

TouchCalibratorView::TouchCalibratorView(const display::Display& target_display,
                                         bool is_primary_view)
    : display_(target_display), is_primary_view_(is_primary_view) {
  aura::Window* root = ash::Shell::GetInstance()
                           ->window_tree_host_manager()
                           ->GetRootWindowForDisplayId(display_.id());
  widget_.reset(new views::Widget);
  widget_->Init(GetWidgetParams(root));
  widget_->SetContentsView(this);
  widget_->SetBounds(display_.bounds());
  widget_->Show();
  set_owned_by_client();
}

TouchCalibratorView::~TouchCalibratorView() {
  state_ = UNKNOWN;
  widget_->Hide();
}

void TouchCalibratorView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
}

void TouchCalibratorView::OnPaintBackground(gfx::Canvas* canvas) {}

void TouchCalibratorView::AnimationProgressed(const gfx::Animation* animation) {
}

void TouchCalibratorView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TouchCalibratorView::AnimationEnded(const gfx::Animation* animation) {}

void TouchCalibratorView::AdvanceToNextState() {}

bool TouchCalibratorView::GetDisplayPointLocation(gfx::Point* location) {
  if (!is_primary_view_)
    return false;
  return false;
}

void TouchCalibratorView::SkipToFinalState() {}

}  // namespace chromeos
