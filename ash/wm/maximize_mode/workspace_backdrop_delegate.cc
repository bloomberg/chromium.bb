// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/workspace_backdrop_delegate.h"

#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The opacity of the backdrop.
const float kBackdropOpacity = 0.5f;

}  // namespace

WorkspaceBackdropDelegate::WorkspaceBackdropDelegate(aura::Window* container)
    : background_(NULL),
      container_(container),
      in_restacking_(false) {
  background_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = container_;
  params.bounds = container_->GetBoundsInScreen();
  params.layer_type = aura::WINDOW_LAYER_SOLID_COLOR;
  // To disallow the MRU list from picking this window up it should not be
  // activateable.
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  background_->Init(params);
  // Do not use the animation system. We don't want the bounds animation and
  // opacity needs to get set to |kBackdropOpacity|.
  ::wm::SetWindowVisibilityAnimationTransition(
      background_->GetNativeView(),
      ::wm::ANIMATE_NONE);
  background_->GetNativeView()->SetName("WorkspaceBackdropDelegate");
  background_->GetNativeView()->layer()->SetColor(SK_ColorBLACK);
  // Make sure that the layer covers visibly everything - including the shelf.
  background_->GetNativeView()->layer()->SetBounds(params.bounds);
  Show();
  RestackBackdrop();
  container_->AddObserver(this);
}

WorkspaceBackdropDelegate::~WorkspaceBackdropDelegate() {
  container_->RemoveObserver(this);
  ::wm::ScopedHidingAnimationSettings hiding_settings(
      background_->GetNativeView());
  background_->Close();
  background_->GetNativeView()->layer()->SetOpacity(0.0f);
}

void WorkspaceBackdropDelegate::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  // The container size has changed and the layer needs to be adapt to it.
  AdjustToContainerBounds();
}

void WorkspaceBackdropDelegate::OnWindowAddedToLayout(aura::Window* child) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::OnWindowRemovedFromLayout(aura::Window* child) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::OnWindowStackingChanged(aura::Window* window) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::OnPostWindowStateTypeChange(
    wm::WindowState* window_state,
    wm::WindowStateType old_type) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::OnDisplayWorkAreaInsetsChanged() {
  AdjustToContainerBounds();
}

void WorkspaceBackdropDelegate::RestackBackdrop() {
  // Avoid recursive calls.
  if (in_restacking_)
    return;

  aura::Window* window = GetCurrentTopWindow();
  if (!window) {
    // Hide backdrop since no suitable window was found.
    background_->Hide();
    return;
  }
  if (window == background_->GetNativeWindow() &&
      background_->IsVisible()) {
    return;
  }
  // We are changing the order of windows which will cause recursion.
  base::AutoReset<bool> lock(&in_restacking_, true);
  if (!background_->IsVisible())
    Show();
  // Since the backdrop needs to be immediately behind the window and the
  // stacking functions only guarantee a "it's above or below", we need
  // to re-arrange the two windows twice.
  container_->StackChildAbove(background_->GetNativeView(), window);
  container_->StackChildAbove(window, background_->GetNativeView());
}

aura::Window* WorkspaceBackdropDelegate::GetCurrentTopWindow() {
  const aura::Window::Windows& windows = container_->children();
  for (aura::Window::Windows::const_reverse_iterator window_iter =
           windows.rbegin();
       window_iter != windows.rend(); ++window_iter) {
    aura::Window* window = *window_iter;
    if (window->TargetVisibility() &&
        window->type() == ui::wm::WINDOW_TYPE_NORMAL &&
        ash::wm::CanActivateWindow(window))
      return window;
  }
  return NULL;
}

void WorkspaceBackdropDelegate::AdjustToContainerBounds() {
  // Cover the entire container window.
  gfx::Rect target_rect(gfx::Point(0, 0), container_->bounds().size());
  if (target_rect != background_->GetNativeWindow()->bounds()) {
    // This needs to be instant.
    ui::ScopedLayerAnimationSettings settings(
        background_->GetNativeView()->layer()->GetAnimator());
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(0));
    background_->GetNativeWindow()->SetBounds(target_rect);
    if (!background_->IsVisible())
      background_->GetNativeView()->layer()->SetOpacity(kBackdropOpacity);
  }
}

void WorkspaceBackdropDelegate::Show() {
  background_->GetNativeView()->layer()->SetOpacity(0.0f);
  background_->Show();
  ui::ScopedLayerAnimationSettings settings(
      background_->GetNativeView()->layer()->GetAnimator());
  background_->GetNativeView()->layer()->SetOpacity(kBackdropOpacity);
}

}  // namespace ash
