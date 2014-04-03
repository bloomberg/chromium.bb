// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_window.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

views::Widget* CreateCloseWindowButton(aura::Window* root_window,
                                       views::ButtonListener* listener) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.can_activate = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window, ash::kShellWindowId_OverlayContainer);
  widget->set_focus_on_creation(false);
  widget->Init(params);
  views::ImageButton* button = new views::ImageButton(listener);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  button->SetImage(views::CustomButton::STATE_NORMAL,
      rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE));
  button->SetImage(views::CustomButton::STATE_HOVERED,
      rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE_H));
  button->SetImage(views::CustomButton::STATE_PRESSED,
      rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE_P));
  widget->SetContentsView(button);
  widget->SetSize(rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE)->size());
  widget->Show();
  return widget;
}

// The time for the close button to fade in when initially shown on entering
// overview mode.
const int kCloseButtonFadeInMilliseconds = 80;

}  // namespace

WindowSelectorWindow::WindowSelectorWindow(aura::Window* window)
    : transform_window_(window) {
}

WindowSelectorWindow::~WindowSelectorWindow() {
}

aura::Window* WindowSelectorWindow::GetRootWindow() {
  return transform_window_.window()->GetRootWindow();
}

bool WindowSelectorWindow::HasSelectableWindow(const aura::Window* window) {
  return transform_window_.window() == window;
}

aura::Window* WindowSelectorWindow::TargetedWindow(const aura::Window* target) {
  if (transform_window_.Contains(target))
    return transform_window_.window();
  return NULL;
}

void WindowSelectorWindow::RestoreWindowOnExit(aura::Window* window) {
  transform_window_.RestoreWindowOnExit();
}

aura::Window* WindowSelectorWindow::SelectionWindow() {
  return transform_window_.window();
}

void WindowSelectorWindow::RemoveWindow(const aura::Window* window) {
  DCHECK_EQ(transform_window_.window(), window);
  transform_window_.OnWindowDestroyed();
  // Remove the close button now so that the exited mouse event which is
  // delivered to the destroyed button as it is destroyed does not happen while
  // this item is being removed from the list of windows in overview.
  close_button_.reset();
}

bool WindowSelectorWindow::empty() const {
  return transform_window_.window() == NULL;
}

void WindowSelectorWindow::PrepareForOverview() {
  transform_window_.PrepareForOverview();
}

void WindowSelectorWindow::SetItemBounds(aura::Window* root_window,
                                         const gfx::Rect& target_bounds,
                                         bool animate) {
  gfx::Rect src_rect = transform_window_.GetBoundsInScreen();
  set_bounds(ScopedTransformOverviewWindow::
      ShrinkRectToFitPreservingAspectRatio(src_rect, target_bounds));
  transform_window_.SetTransform(root_window,
      ScopedTransformOverviewWindow::GetTransformForRect(src_rect, bounds()),
      animate);
  UpdateCloseButtonBounds();
}

void WindowSelectorWindow::ButtonPressed(views::Button* sender,
                         const ui::Event& event) {
  views::Widget::GetTopLevelWidgetForNativeView(
      transform_window_.window())->Close();
}

void WindowSelectorWindow::UpdateCloseButtonBounds() {
  aura::Window* root_window = GetRootWindow();
  gfx::Rect align_bounds(bounds());
  gfx::Transform close_button_transform;
  close_button_transform.Translate(align_bounds.right(), align_bounds.y());

  // If the root window has changed, force the close button to be recreated
  // and faded in on the new root window.
  if (close_button_ &&
      close_button_->GetNativeWindow()->GetRootWindow() != root_window) {
    close_button_.reset();
  }

  if (!close_button_) {
    close_button_.reset(CreateCloseWindowButton(root_window, this));
    gfx::Rect close_button_rect(close_button_->GetNativeWindow()->bounds());
    // Align the center of the button with position (0, 0) so that the
    // translate transform does not need to take the button dimensions into
    // account.
    close_button_rect.set_x(-close_button_rect.width() / 2);
    close_button_rect.set_y(-close_button_rect.height() / 2);
    close_button_->GetNativeWindow()->SetBounds(close_button_rect);
    close_button_->GetNativeWindow()->SetTransform(close_button_transform);
    // The close button is initialized when entering overview, fade the button
    // in after the window should be in place.
    ui::Layer* layer = close_button_->GetNativeWindow()->layer();
    layer->SetOpacity(0);
    layer->GetAnimator()->StopAnimating();
    layer->GetAnimator()->SchedulePauseForProperties(
        base::TimeDelta::FromMilliseconds(
            ScopedTransformOverviewWindow::kTransitionMilliseconds),
        ui::LayerAnimationElement::OPACITY);
    {
      ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
      settings.SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
            kCloseButtonFadeInMilliseconds));
      layer->SetOpacity(1);
    }
  } else {
    ui::ScopedLayerAnimationSettings settings(
        close_button_->GetNativeWindow()->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        ScopedTransformOverviewWindow::kTransitionMilliseconds));
    close_button_->GetNativeWindow()->SetTransform(close_button_transform);
  }
}

}  // namespace ash
