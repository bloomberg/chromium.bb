// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/frame_painter.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/auto_window_management.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/corewm/window_util.h"

using aura::Window;

namespace ash {

namespace internal {

namespace {

// This specifies how much percent (2/3=66%) of a window must be visible when
// the window is added to the workspace.
const float kMinimumPercentOnScreenArea = 0.66f;

bool IsMaximizedState(ui::WindowShowState state) {
  return state == ui::SHOW_STATE_MAXIMIZED ||
      state == ui::SHOW_STATE_FULLSCREEN;
}

}  // namespace

WorkspaceLayoutManager::WorkspaceLayoutManager(aura::Window* window)
    : BaseLayoutManager(window->GetRootWindow()),
      shelf_(NULL),
      window_(window),
      work_area_(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
          window->parent())) {
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
}

void WorkspaceLayoutManager::SetShelf(internal::ShelfLayoutManager* shelf) {
  shelf_ = shelf;
}

void WorkspaceLayoutManager::OnWindowAddedToLayout(Window* child) {
  // Adjust window bounds in case that the new child is out of the workspace.
  AdjustWindowSizeForScreenChange(child, ADJUST_WINDOW_WINDOW_ADDED);
  BaseLayoutManager::OnWindowAddedToLayout(child);
  UpdateDesktopVisibility();
  RearrangeVisibleWindowOnShow(child);
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(Window* child) {
  BaseLayoutManager::OnWillRemoveWindowFromLayout(child);
  if (child->TargetVisibility())
    RearrangeVisibleWindowOnHideOrRemove(child);
}

void WorkspaceLayoutManager::OnWindowRemovedFromLayout(Window* child) {
  BaseLayoutManager::OnWindowRemovedFromLayout(child);
  UpdateDesktopVisibility();
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(Window* child,
                                                            bool visible) {
  BaseLayoutManager::OnChildWindowVisibilityChanged(child, visible);
  if (child->TargetVisibility())
    RearrangeVisibleWindowOnShow(child);
  else
    RearrangeVisibleWindowOnHideOrRemove(child);
  UpdateDesktopVisibility();
}

void WorkspaceLayoutManager::SetChildBounds(
    Window* child,
    const gfx::Rect& requested_bounds) {
  if (!GetTrackedByWorkspace(child)) {
    SetChildBoundsDirect(child, requested_bounds);
    return;
  }
  gfx::Rect child_bounds(requested_bounds);
  // Some windows rely on this to set their initial bounds.
  if (!SetMaximizedOrFullscreenBounds(child)) {
    // Non-maximized/full-screen windows have their size constrained to the
    // work-area.
    child_bounds.set_width(std::min(work_area_.width(), child_bounds.width()));
    child_bounds.set_height(
        std::min(work_area_.height(), child_bounds.height()));
    SetChildBoundsDirect(child, child_bounds);
  }
  UpdateDesktopVisibility();
}

void WorkspaceLayoutManager::OnDisplayWorkAreaInsetsChanged() {
  const gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
      window_->parent()));
  if (work_area != work_area_)
    AdjustWindowSizesForScreenChange(ADJUST_WINDOW_DISPLAY_INSETS_CHANGED);
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == aura::client::kShowStateKey) {
    ui::WindowShowState old_state = static_cast<ui::WindowShowState>(old);
    ui::WindowShowState new_state =
        window->GetProperty(aura::client::kShowStateKey);
    if (old_state != ui::SHOW_STATE_MINIMIZED &&
        GetRestoreBoundsInScreen(window) == NULL &&
        IsMaximizedState(new_state) &&
        !IsMaximizedState(old_state)) {
      SetRestoreBoundsInParent(window, window->bounds());
    }
    // When restoring from a minimized state, we want to restore to the
    // previous (maybe L/R maximized) state. Since we do also want to keep the
    // restore rectangle, we set the restore rectangle to the rectangle we want
    // to restore to and restore it after we switched so that it is preserved.
    gfx::Rect restore;
    if (old_state == ui::SHOW_STATE_MINIMIZED &&
        (new_state == ui::SHOW_STATE_NORMAL ||
         new_state == ui::SHOW_STATE_DEFAULT) &&
        GetRestoreBoundsInScreen(window) &&
        !GetWindowAlwaysRestoresToRestoreBounds(window)) {
      restore = *GetRestoreBoundsInScreen(window);
      SetRestoreBoundsInScreen(window, window->GetBoundsInScreen());
    }

    UpdateBoundsFromShowState(window);
    ShowStateChanged(window, old_state);

    // Set the restore rectangle to the previously set restore rectangle.
    if (!restore.IsEmpty())
      SetRestoreBoundsInScreen(window, restore);
  }

  if (key == internal::kWindowTrackedByWorkspaceKey &&
      GetTrackedByWorkspace(window)) {
    SetMaximizedOrFullscreenBounds(window);
  }

  if (key == aura::client::kAlwaysOnTopKey &&
      window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    internal::AlwaysOnTopController* controller =
        GetRootWindowController(window->GetRootWindow())->
            always_on_top_controller();
    controller->GetContainer(window)->AddChild(window);
  }
}

void WorkspaceLayoutManager::ShowStateChanged(
    Window* window,
    ui::WindowShowState last_show_state) {
  BaseLayoutManager::ShowStateChanged(window, last_show_state);
  UpdateDesktopVisibility();
}

void WorkspaceLayoutManager::AdjustWindowSizesForScreenChange(
    AdjustWindowReason reason) {
  work_area_ = ScreenAsh::GetDisplayWorkAreaBoundsInParent(window_->parent());
  BaseLayoutManager::AdjustWindowSizesForScreenChange(reason);
}

void WorkspaceLayoutManager::AdjustWindowSizeForScreenChange(
    Window* window,
    AdjustWindowReason reason) {
  if (!GetTrackedByWorkspace(window))
    return;

  // Use cross fade transition for the maximized window if the adjustment
  // happens due to the shelf's visibility change. Otherwise the background
  // can be seen slightly between the bottom edge of resized-window and
  // the animating shelf.
  // TODO(mukai): this cause slight blur at the window frame because of the
  // cross fade. I think this is better, but should reconsider if someone
  // raises voice for this.
  if (wm::IsWindowMaximized(window) &&
      reason == ADJUST_WINDOW_DISPLAY_INSETS_CHANGED) {
    CrossFadeToBounds(window, ScreenAsh::GetMaximizedWindowBoundsInParent(
        window->parent()->parent()));
    return;
  }

  if (SetMaximizedOrFullscreenBounds(window))
    return;

  gfx::Rect bounds = window->bounds();
  if (reason == ADJUST_WINDOW_SCREEN_SIZE_CHANGED) {
    // The work area may be smaller than the full screen.  Put as much of the
    // window as possible within the display area.
    bounds.AdjustToFit(work_area_);
  } else if (reason == ADJUST_WINDOW_DISPLAY_INSETS_CHANGED) {
    ash::wm::AdjustBoundsToEnsureMinimumWindowVisibility(work_area_, &bounds);
  } else if (reason == ADJUST_WINDOW_WINDOW_ADDED) {
    int min_width = bounds.width() * kMinimumPercentOnScreenArea;
    int min_height = bounds.height() * kMinimumPercentOnScreenArea;
    ash::wm::AdjustBoundsToEnsureWindowVisibility(
        work_area_, min_width, min_height, &bounds);
  }
  if (window->bounds() != bounds)
    window->SetBounds(bounds);
}

void WorkspaceLayoutManager::UpdateDesktopVisibility() {
  if (shelf_)
    shelf_->UpdateVisibilityState();
  FramePainter::UpdateSoloWindowHeader(window_->GetRootWindow());
}

void WorkspaceLayoutManager::UpdateBoundsFromShowState(Window* window) {
  // See comment in SetMaximizedOrFullscreenBounds() as to why we use parent in
  // these calculation.
  switch (window->GetProperty(aura::client::kShowStateKey)) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL: {
      const gfx::Rect* restore = GetRestoreBoundsInScreen(window);
      if (restore) {
        gfx::Rect bounds_in_parent =
            ScreenAsh::ConvertRectFromScreen(window->parent()->parent(),
                                             *restore);
        CrossFadeToBounds(
            window,
            BaseLayoutManager::BoundsWithScreenEdgeVisible(
                window->parent()->parent(),
                bounds_in_parent));
      }
      ClearRestoreBounds(window);
      break;
    }

    case ui::SHOW_STATE_MAXIMIZED:
      CrossFadeToBounds(window, ScreenAsh::GetMaximizedWindowBoundsInParent(
          window->parent()->parent()));
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      CrossFadeToBounds(window, ScreenAsh::GetDisplayBoundsInParent(
          window->parent()->parent()));
      break;

    default:
      break;
  }
}

bool WorkspaceLayoutManager::SetMaximizedOrFullscreenBounds(
    aura::Window* window) {
  if (!GetTrackedByWorkspace(window))
    return false;

  // During animations there is a transform installed on the workspace
  // windows. For this reason this code uses the parent so that the transform is
  // ignored.
  if (wm::IsWindowMaximized(window)) {
    SetChildBoundsDirect(
        window, ScreenAsh::GetMaximizedWindowBoundsInParent(
            window->parent()->parent()));
    return true;
  }
  if (wm::IsWindowFullscreen(window)) {
    SetChildBoundsDirect(
        window,
        ScreenAsh::GetDisplayBoundsInParent(window->parent()->parent()));
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace ash
