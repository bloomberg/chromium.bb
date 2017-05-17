// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/workspace_backdrop_delegate.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/wm/workspace/workspace_layout_manager_backdrop_delegate.h"
#include "ash/wm_window.h"
#include "base/auto_reset.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The opacity of the backdrop.
const float kBackdropOpacity = 1.0f;

}  // namespace

WorkspaceBackdropDelegate::WorkspaceBackdropDelegate(WmWindow* container)
    : container_(container), in_restacking_(false) {
  background_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = container_->GetBoundsInScreen();
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.name = "WorkspaceBackdropDelegate";
  // To disallow the MRU list from picking this window up it should not be
  // activateable.
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  DCHECK_NE(kShellWindowId_Invalid, container_->aura_window()->id());
  container_->GetRootWindowController()->ConfigureWidgetInitParamsForContainer(
      background_, container_->aura_window()->id(), &params);
  background_->Init(params);
  background_window_ = WmWindow::Get(background_->GetNativeWindow());
  // Do not use the animation system. We don't want the bounds animation and
  // opacity needs to get set to |kBackdropOpacity|.
  background_window_->SetVisibilityAnimationTransition(::wm::ANIMATE_NONE);
  background_window_->GetLayer()->SetColor(SK_ColorBLACK);
  // Make sure that the layer covers visibly everything - including the shelf.
  background_window_->GetLayer()->SetBounds(params.bounds);
  DCHECK(background_window_->GetBounds() == params.bounds);
  Show();
  RestackBackdrop();
}

WorkspaceBackdropDelegate::~WorkspaceBackdropDelegate() {
  // TODO: animations won't work right with mus: http://crbug.com/548396.
  ::wm::ScopedHidingAnimationSettings hiding_settings(
      background_->GetNativeView());
  background_->Close();
  background_window_->GetLayer()->SetOpacity(0.0f);
}

void WorkspaceBackdropDelegate::OnWindowAddedToLayout(WmWindow* child) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::OnWindowRemovedFromLayout(WmWindow* child) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::OnChildWindowVisibilityChanged(WmWindow* child,
                                                               bool visible) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::OnWindowStackingChanged(WmWindow* window) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::OnPostWindowStateTypeChange(
    wm::WindowState* window_state,
    wm::WindowStateType old_type) {
  RestackBackdrop();
}

void WorkspaceBackdropDelegate::RestackBackdrop() {
  // Avoid recursive calls.
  if (in_restacking_)
    return;

  WmWindow* window = GetCurrentTopWindow();
  if (!window) {
    // Hide backdrop since no suitable window was found.
    background_->Hide();
    return;
  }
  if (window == background_window_ && background_->IsVisible())
    return;
  if (window->GetRootWindow() != background_window_->GetRootWindow())
    return;
  // We are changing the order of windows which will cause recursion.
  base::AutoReset<bool> lock(&in_restacking_, true);
  if (!background_->IsVisible())
    Show();
  // Since the backdrop needs to be immediately behind the window and the
  // stacking functions only guarantee a "it's above or below", we need
  // to re-arrange the two windows twice.
  container_->StackChildAbove(background_window_, window);
  container_->StackChildAbove(window, background_window_);
}

WmWindow* WorkspaceBackdropDelegate::GetCurrentTopWindow() {
  const WmWindow::Windows windows = container_->GetChildren();
  for (auto window_iter = windows.rbegin(); window_iter != windows.rend();
       ++window_iter) {
    WmWindow* window = *window_iter;
    if (window->GetTargetVisibility() &&
        window->GetType() == ui::wm::WINDOW_TYPE_NORMAL &&
        window->CanActivate())
      return window;
  }
  return nullptr;
}

void WorkspaceBackdropDelegate::Show() {
  background_window_->GetLayer()->SetOpacity(0.0f);
  background_->Show();
  background_->SetFullscreen(true);
  ui::ScopedLayerAnimationSettings settings(
      background_window_->GetLayer()->GetAnimator());
  background_window_->GetLayer()->SetOpacity(kBackdropOpacity);
}

}  // namespace ash
