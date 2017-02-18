// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/workspace_controller.h"

#include <utility>

#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm/dock/docked_window_layout_manager.h"
#include "ash/common/wm/fullscreen_window_finder.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_window_animations.h"
#include "ash/common/wm/workspace/workspace_event_handler.h"
#include "ash/common/wm/workspace/workspace_layout_manager.h"
#include "ash/common/wm/workspace/workspace_layout_manager_backdrop_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "base/memory/ptr_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {
namespace {

// Amount of time to pause before animating anything. Only used during initial
// animation (when logging in).
const int kInitialPauseTimeMS = 750;

// The duration of the animation that occurs on first login.
const int kInitialAnimationDurationMS = 200;

}  // namespace

WorkspaceController::WorkspaceController(WmWindow* viewport)
    : viewport_(viewport),
      event_handler_(WmShell::Get()->CreateWorkspaceEventHandler(viewport)),
      layout_manager_(new WorkspaceLayoutManager(viewport)) {
  viewport_->aura_window()->AddObserver(this);
  viewport_->SetVisibilityAnimationTransition(::wm::ANIMATE_NONE);
  viewport_->SetLayoutManager(base::WrapUnique(layout_manager_));
}

WorkspaceController::~WorkspaceController() {
  if (!viewport_)
    return;

  viewport_->aura_window()->RemoveObserver(this);
  viewport_->SetLayoutManager(nullptr);
}

wm::WorkspaceWindowState WorkspaceController::GetWindowState() const {
  if (!viewport_ || !viewport_->GetRootWindowController()->HasShelf())
    return wm::WORKSPACE_WINDOW_STATE_DEFAULT;

  const WmWindow* fullscreen = wm::GetWindowForFullscreenMode(viewport_);
  if (fullscreen && !fullscreen->GetWindowState()->ignored_by_shelf())
    return wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN;

  // These are the container ids of containers which may contain windows that
  // may overlap the launcher shelf and affect its transparency.
  const int kWindowContainerIds[] = {
      kShellWindowId_DefaultContainer, kShellWindowId_DockedContainer,
  };
  const gfx::Rect shelf_bounds(WmShelf::ForWindow(viewport_)->GetIdealBounds());
  bool window_overlaps_launcher = false;
  for (size_t i = 0; i < arraysize(kWindowContainerIds); i++) {
    WmWindow* container = viewport_->GetRootWindow()->GetChildByShellWindowId(
        kWindowContainerIds[i]);
    for (WmWindow* window : container->GetChildren()) {
      wm::WindowState* window_state = window->GetWindowState();
      if (window_state->ignored_by_shelf() ||
          (window->GetLayer() && !window->GetLayer()->GetTargetVisibility())) {
        continue;
      }
      if (window_state->IsMaximized())
        return wm::WORKSPACE_WINDOW_STATE_MAXIMIZED;
      window_overlaps_launcher |= window->GetBounds().Intersects(shelf_bounds);
    }
  }

  // Check if there are visible docked windows in the same display.
  DockedWindowLayoutManager* dock = DockedWindowLayoutManager::Get(viewport_);
  const bool docked_area_visible = dock && !dock->docked_bounds().IsEmpty();
  return (window_overlaps_launcher || docked_area_visible)
             ? wm::WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF
             : wm::WORKSPACE_WINDOW_STATE_DEFAULT;
}

void WorkspaceController::DoInitialAnimation() {
  viewport_->Show();

  ui::Layer* layer = viewport_->GetLayer();
  layer->SetOpacity(0.0f);
  SetTransformForScaleAnimation(layer, LAYER_SCALE_ANIMATION_ABOVE);

  // In order for pause to work we need to stop animations.
  layer->GetAnimator()->StopAnimating();

  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());

    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    layer->GetAnimator()->SchedulePauseForProperties(
        base::TimeDelta::FromMilliseconds(kInitialPauseTimeMS),
        ui::LayerAnimationElement::TRANSFORM |
            ui::LayerAnimationElement::OPACITY |
            ui::LayerAnimationElement::BRIGHTNESS |
            ui::LayerAnimationElement::VISIBILITY);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kInitialAnimationDurationMS));
    layer->SetTransform(gfx::Transform());
    layer->SetOpacity(1.0f);
  }
}

void WorkspaceController::SetMaximizeBackdropDelegate(
    std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> delegate) {
  layout_manager_->SetMaximizeBackdropDelegate(std::move(delegate));
}

void WorkspaceController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(WmWindow::Get(window), viewport_);
  viewport_->aura_window()->RemoveObserver(this);
  viewport_ = nullptr;
  // Destroy |event_handler_| too as it depends upon |window|.
  event_handler_.reset();
  layout_manager_ = nullptr;
}

}  // namespace ash
