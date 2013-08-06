// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller.h"

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/corewm/visibility_controller.h"
#include "ui/views/corewm/window_animations.h"

namespace ash {
namespace internal {
namespace {

// Amount of time to pause before animating anything. Only used during initial
// animation (when logging in).
const int kInitialPauseTimeMS = 750;

}  // namespace

WorkspaceController::WorkspaceController(aura::Window* viewport)
    : viewport_(viewport),
      shelf_(NULL),
      event_handler_(new WorkspaceEventHandler(viewport_)) {
  SetWindowVisibilityAnimationTransition(
      viewport_, views::corewm::ANIMATE_NONE);
  // Do this so when animating out windows don't extend beyond the bounds.
  viewport_->layer()->SetMasksToBounds(true);

  // The layout-manager cannot be created in the initializer list since it
  // depends on the window to have been initialized.
  layout_manager_ = new WorkspaceLayoutManager(viewport_);
  viewport_->SetLayoutManager(layout_manager_);

  viewport_->Show();
}

WorkspaceController::~WorkspaceController() {
  viewport_->SetLayoutManager(NULL);
  viewport_->SetEventFilter(NULL);
  viewport_->RemovePreTargetHandler(event_handler_.get());
  viewport_->RemovePostTargetHandler(event_handler_.get());
}

WorkspaceWindowState WorkspaceController::GetWindowState() const {
  if (!shelf_)
    return WORKSPACE_WINDOW_STATE_DEFAULT;

  const gfx::Rect shelf_bounds(shelf_->GetIdealBounds());
  const aura::Window::Windows& windows(viewport_->children());
  bool window_overlaps_launcher = false;
  bool has_maximized_window = false;
  for (aura::Window::Windows::const_iterator i = windows.begin();
       i != windows.end(); ++i) {
    if (GetIgnoredByShelf(*i))
      continue;
    ui::Layer* layer = (*i)->layer();
    if (!layer->GetTargetVisibility() || layer->GetTargetOpacity() == 0.0f)
      continue;
    if (wm::IsWindowMaximized(*i)) {
      // An untracked window may still be fullscreen so we keep iterating when
      // we hit a maximized window.
      has_maximized_window = true;
    } else if (wm::IsWindowFullscreen(*i)) {
      return WORKSPACE_WINDOW_STATE_FULL_SCREEN;
    }
    if (!window_overlaps_launcher && (*i)->bounds().Intersects(shelf_bounds))
      window_overlaps_launcher = true;
  }
  if (has_maximized_window)
    return WORKSPACE_WINDOW_STATE_MAXIMIZED;

  return window_overlaps_launcher ?
      WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF :
      WORKSPACE_WINDOW_STATE_DEFAULT;
}

void WorkspaceController::SetShelf(ShelfLayoutManager* shelf) {
  shelf_ = shelf;
  layout_manager_->SetShelf(shelf);
}

void WorkspaceController::DoInitialAnimation() {
  viewport_->Show();

  viewport_->layer()->SetOpacity(0.0f);
  SetTransformForScaleAnimation(
      viewport_->layer(), LAYER_SCALE_ANIMATION_ABOVE);

  // In order for pause to work we need to stop animations.
  viewport_->layer()->GetAnimator()->StopAnimating();

  {
    ui::ScopedLayerAnimationSettings settings(
        viewport_->layer()->GetAnimator());

    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    viewport_->layer()->GetAnimator()->SchedulePauseForProperties(
        base::TimeDelta::FromMilliseconds(kInitialPauseTimeMS),
        ui::LayerAnimationElement::TRANSFORM,
        ui::LayerAnimationElement::OPACITY,
        ui::LayerAnimationElement::BRIGHTNESS,
        ui::LayerAnimationElement::VISIBILITY,
        -1);

    settings.SetTweenType(ui::Tween::EASE_OUT);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kCrossFadeDurationMS));
    viewport_->layer()->SetTransform(gfx::Transform());
    viewport_->layer()->SetOpacity(1.0f);
  }
}

}  // namespace internal
}  // namespace ash
