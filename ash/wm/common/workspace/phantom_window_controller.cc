// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/workspace/phantom_window_controller.h"

#include <math.h>

#include "ash/wm/common/root_window_finder.h"
#include "ash/wm/common/wm_root_window_controller.h"
#include "ash/wm/common/wm_shell_window_ids.h"
#include "ash/wm/common/wm_window.h"
#include "grit/ash_resources.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/background.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// The duration of the show animation.
const int kAnimationDurationMs = 200;

// The size of the phantom window at the beginning of the show animation in
// relation to the size of the phantom window at the end of the animation.
const float kStartBoundsRatio = 0.85f;

// The amount of pixels that the phantom window's shadow should extend past
// the bounds passed into Show().
const int kShadowThickness = 15;

// The minimum size of a phantom window including the shadow. The minimum size
// is derived from the size of the IDR_AURA_PHANTOM_WINDOW image assets.
const int kMinSizeWithShadow = 100;

// Adjusts the phantom window's bounds so that the bounds:
// - Include the size of the shadow.
// - Have a size equal to or larger than the minimum phantom window size.
gfx::Rect GetAdjustedBounds(const gfx::Rect& bounds) {
  int x_inset = std::max(
      static_cast<int>(ceil((kMinSizeWithShadow - bounds.width()) / 2.0f)),
      kShadowThickness);
  int y_inset = std::max(
      static_cast<int>(ceil((kMinSizeWithShadow - bounds.height()) / 2.0f)),
      kShadowThickness);

  gfx::Rect adjusted_bounds(bounds);
  adjusted_bounds.Inset(-x_inset, -y_inset);
  return adjusted_bounds;
}

// Starts an animation of |widget| to |new_bounds_in_screen|. No-op if |widget|
// is NULL.
void AnimateToBounds(views::Widget* widget,
                     const gfx::Rect& new_bounds_in_screen) {
  if (!widget)
    return;

  ui::ScopedLayerAnimationSettings scoped_setter(
      wm::WmWindow::Get(widget)->GetLayer()->GetAnimator());
  scoped_setter.SetTweenType(gfx::Tween::EASE_IN);
  scoped_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  scoped_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  widget->SetBounds(new_bounds_in_screen);
}

}  // namespace

// PhantomWindowController ----------------------------------------------------

PhantomWindowController::PhantomWindowController(wm::WmWindow* window)
    : window_(window) {}

PhantomWindowController::~PhantomWindowController() {}

void PhantomWindowController::Show(const gfx::Rect& bounds_in_screen) {
  gfx::Rect adjusted_bounds_in_screen = GetAdjustedBounds(bounds_in_screen);
  if (adjusted_bounds_in_screen == target_bounds_in_screen_)
    return;
  target_bounds_in_screen_ = adjusted_bounds_in_screen;

  gfx::Rect start_bounds_in_screen = target_bounds_in_screen_;
  int start_width = std::max(
      kMinSizeWithShadow,
      static_cast<int>(start_bounds_in_screen.width() * kStartBoundsRatio));
  int start_height = std::max(
      kMinSizeWithShadow,
      static_cast<int>(start_bounds_in_screen.height() * kStartBoundsRatio));
  start_bounds_in_screen.Inset(
      floor((start_bounds_in_screen.width() - start_width) / 2.0f),
      floor((start_bounds_in_screen.height() - start_height) / 2.0f));
  phantom_widget_ =
      CreatePhantomWidget(wm::GetRootWindowMatching(target_bounds_in_screen_),
                          start_bounds_in_screen);

  AnimateToBounds(phantom_widget_.get(), target_bounds_in_screen_);
}

std::unique_ptr<views::Widget> PhantomWindowController::CreatePhantomWidget(
    wm::WmWindow* root_window,
    const gfx::Rect& bounds_in_screen) {
  std::unique_ptr<views::Widget> phantom_widget(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  // PhantomWindowController is used by FrameMaximizeButton to highlight the
  // launcher button. Put the phantom in the same window as the launcher so that
  // the phantom is visible.
  params.keep_on_top = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "PhantomWindow";
  root_window->GetRootWindowController()->ConfigureWidgetInitParamsForContainer(
      phantom_widget.get(), kShellWindowId_ShelfContainer, &params);
  phantom_widget->set_focus_on_creation(false);
  phantom_widget->Init(params);
  phantom_widget->SetVisibilityChangedAnimationsEnabled(false);
  wm::WmWindow* phantom_widget_window = wm::WmWindow::Get(phantom_widget.get());
  phantom_widget_window->SetShellWindowId(kShellWindowId_PhantomWindow);
  phantom_widget->SetBounds(bounds_in_screen);
  // TODO(sky): I suspect this is never true, verify that.
  if (phantom_widget_window->GetParent() == window_->GetParent()) {
    phantom_widget_window->GetParent()->StackChildAbove(phantom_widget_window,
                                                        window_);
  }

  const int kImages[] = IMAGE_GRID(IDR_AURA_PHANTOM_WINDOW);
  views::Painter* background_painter =
      views::Painter::CreateImageGridPainter(kImages);
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateBackgroundPainter(true, background_painter));
  phantom_widget->SetContentsView(content_view);

  // Show the widget after all the setups.
  phantom_widget->Show();

  // Fade the window in.
  ui::Layer* widget_layer = phantom_widget_window->GetLayer();
  widget_layer->SetOpacity(0);
  ui::ScopedLayerAnimationSettings scoped_setter(widget_layer->GetAnimator());
  scoped_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  widget_layer->SetOpacity(1);

  return phantom_widget;
}

}  // namespace ash
