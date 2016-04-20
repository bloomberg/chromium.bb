// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller.h"

#include <utility>

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/common/workspace/workspace_layout_manager_delegate.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_layout_manager_backdrop_delegate.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Amount of time to pause before animating anything. Only used during initial
// animation (when logging in).
const int kInitialPauseTimeMS = 750;

// Returns true if there are visible docked windows in the same screen as the
// |shelf|.
bool IsDockedAreaVisible(const ShelfLayoutManager* shelf) {
  return shelf->dock_bounds().width() > 0;
}

}  // namespace

WorkspaceController::WorkspaceController(
    aura::Window* viewport,
    std::unique_ptr<wm::WorkspaceLayoutManagerDelegate> delegate)
    : viewport_(viewport),
      shelf_(NULL),
      event_handler_(new WorkspaceEventHandler),
      layout_manager_(
          new WorkspaceLayoutManager(viewport, std::move(delegate))) {
  SetWindowVisibilityAnimationTransition(
      viewport_, ::wm::ANIMATE_NONE);

  viewport_->SetLayoutManager(layout_manager_);
  viewport_->AddPreTargetHandler(event_handler_.get());
}

WorkspaceController::~WorkspaceController() {
  viewport_->SetLayoutManager(NULL);
  viewport_->RemovePreTargetHandler(event_handler_.get());
}

wm::WorkspaceWindowState WorkspaceController::GetWindowState() const {
  if (!shelf_)
    return wm::WORKSPACE_WINDOW_STATE_DEFAULT;
  const aura::Window* topmost_fullscreen_window = GetRootWindowController(
      viewport_->GetRootWindow())->GetWindowForFullscreenMode();
  if (topmost_fullscreen_window &&
      !wm::GetWindowState(topmost_fullscreen_window)->ignored_by_shelf()) {
    return wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN;
  }

  // These are the container ids of containers which may contain windows that
  // may overlap the launcher shelf and affect its transparency.
  const int kWindowContainerIds[] = {kShellWindowId_DefaultContainer,
                                     kShellWindowId_DockedContainer, };
  const gfx::Rect shelf_bounds(shelf_->GetIdealBounds());
  bool window_overlaps_launcher = false;
  for (size_t idx = 0; idx < arraysize(kWindowContainerIds); idx++) {
    const aura::Window* container = Shell::GetContainer(
        viewport_->GetRootWindow(), kWindowContainerIds[idx]);
    const aura::Window::Windows& windows(container->children());
    for (aura::Window::Windows::const_iterator i = windows.begin();
         i != windows.end(); ++i) {
      wm::WindowState* window_state = wm::GetWindowState(*i);
      if (window_state->ignored_by_shelf())
        continue;
      ui::Layer* layer = (*i)->layer();
      if (!layer->GetTargetVisibility())
        continue;
      if (window_state->IsMaximized())
        return wm::WORKSPACE_WINDOW_STATE_MAXIMIZED;
      if (!window_overlaps_launcher &&
          ((*i)->bounds().Intersects(shelf_bounds))) {
        window_overlaps_launcher = true;
      }
    }
  }

  return (window_overlaps_launcher || IsDockedAreaVisible(shelf_))
             ? wm::WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF
             : wm::WORKSPACE_WINDOW_STATE_DEFAULT;
}

void WorkspaceController::SetShelf(ShelfLayoutManager* shelf) {
  shelf_ = shelf;
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
        ui::LayerAnimationElement::TRANSFORM |
            ui::LayerAnimationElement::OPACITY |
            ui::LayerAnimationElement::BRIGHTNESS |
            ui::LayerAnimationElement::VISIBILITY);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kCrossFadeDurationMS));
    viewport_->layer()->SetTransform(gfx::Transform());
    viewport_->layer()->SetOpacity(1.0f);
  }
}

void WorkspaceController::SetMaximizeBackdropDelegate(
    std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> delegate) {
  layout_manager_->SetMaximizeBackdropDelegate(std::move(delegate));
}

}  // namespace ash
