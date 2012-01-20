// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/compact_layout_manager.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {
// Convenience method to get the layer of this container.
ui::Layer* GetDefaultContainerLayer() {
  return Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer)->layer();
}

// Whether it is a window that should be animated on entrance.
bool ShouldAnimateOnEntrance(aura::Window* window) {
  return window &&
      window->type() == aura::client::WINDOW_TYPE_NORMAL &&
      window_util::IsWindowMaximized(window);
}

// Adjust layer bounds to grow or shrink in |delta_width|.
void AdjustContainerLayerWidth(int delta_width) {
  gfx::Rect bounds(GetDefaultContainerLayer()->bounds());
  bounds.set_width(bounds.width() + delta_width);
  GetDefaultContainerLayer()->SetBounds(bounds);
  GetDefaultContainerLayer()->GetCompositor()->WidgetSizeChanged(
      GetDefaultContainerLayer()->bounds().size());
}
}  // namespace

/////////////////////////////////////////////////////////////////////////////
// CompactLayoutManager, public:

CompactLayoutManager::CompactLayoutManager()
    : status_area_widget_(NULL),
      current_window_(NULL) {
}

CompactLayoutManager::~CompactLayoutManager() {
}

/////////////////////////////////////////////////////////////////////////////
// CompactLayoutManager, LayoutManager overrides:

void CompactLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  BaseLayoutManager::OnWindowAddedToLayout(child);
  UpdateStatusAreaVisibility();
  if (windows().size() > 1 &&
      child->type() == aura::client::WINDOW_TYPE_NORMAL) {
    // The first window is already contained in the current layer,
    // add subsequent windows to layer bounds calculation.
    AdjustContainerLayerWidth(child->bounds().width());
  }
}

void CompactLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  BaseLayoutManager::OnWillRemoveWindowFromLayout(child);
  UpdateStatusAreaVisibility();
  if (windows().size() > 1 && ShouldAnimateOnEntrance(child)) {
    AdjustContainerLayerWidth(-child->bounds().width());
  }
}

void CompactLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                          bool visible) {
  BaseLayoutManager::OnChildWindowVisibilityChanged(child, visible);
  UpdateStatusAreaVisibility();
  if (ShouldAnimateOnEntrance(child)) {
    LayoutWindows(visible ? NULL : child);
    if (visible) {
      AnimateSlideTo(child->bounds().x());
      current_window_ = child;
    }
  }
}

void CompactLayoutManager::SetChildBounds(aura::Window* child,
                                          const gfx::Rect& requested_bounds) {
  gfx::Rect child_bounds(requested_bounds);
  // Avoid a janky resize on startup by ensuring the initial bounds fill the
  // screen.
  if (window_util::IsWindowMaximized(child))
    child_bounds = gfx::Screen::GetMonitorWorkAreaNearestWindow(child);
  else if (window_util::IsWindowFullscreen(child))
    child_bounds = gfx::Screen::GetMonitorAreaNearestWindow(child);
  else if (current_window_) {
    // All other windows should be offset by the current viewport.
    int offset_x = current_window_->bounds().x();
    child_bounds.Offset(offset_x, 0);
  }
  SetChildBoundsDirect(child, child_bounds);
}

/////////////////////////////////////////////////////////////////////////////
// CompactLayoutManager, WindowObserver overrides:

void CompactLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                   const char* name,
                                                   void* old) {
  BaseLayoutManager::OnWindowPropertyChanged(window, name, old);
  if (name == aura::client::kShowStateKey)
    UpdateStatusAreaVisibility();
}

void CompactLayoutManager::OnWindowStackingChanged(aura::Window* window) {
  if (current_window_ != window && ShouldAnimateOnEntrance(window)) {
    AnimateSlideTo(window->bounds().x());
    current_window_ = window;
  }
}

//////////////////////////////////////////////////////////////////////////////
// CompactLayoutManager, private:

void CompactLayoutManager::UpdateStatusAreaVisibility() {
  if (!status_area_widget_)
    return;
  // Full screen windows should hide the status area widget.
  if (window_util::HasFullscreenWindow(windows()))
    status_area_widget_->Hide();
  else
    status_area_widget_->Show();
}

void CompactLayoutManager::AnimateSlideTo(int offset_x) {
  ui::ScopedLayerAnimationSettings settings(
      GetDefaultContainerLayer()->GetAnimator());
  ui::Transform transform;
  transform.ConcatTranslate(-offset_x, 0);
  GetDefaultContainerLayer()->SetTransform(transform);  // Will be animated!
}

void CompactLayoutManager::LayoutWindows(aura::Window* skip) {
  ShellDelegate* shell_delegate = ash::Shell::GetInstance()->delegate();
  const std::vector<aura::Window*>& windows_list =
      shell_delegate->GetCycleWindowList(
          ShellDelegate::SOURCE_KEYBOARD,
          ShellDelegate::ORDER_LINEAR);
  int new_x = 0;
  for (std::vector<aura::Window*>::const_iterator const_it =
           windows_list.begin();
       const_it != windows_list.end();
       ++const_it) {
    (*const_it)->Show();
   if (*const_it == skip)
      continue;
    gfx::Rect new_bounds((*const_it)->bounds());
    new_bounds.set_x(new_x);
    SetChildBoundsDirect(*const_it, new_bounds);
    new_x += (*const_it)->bounds().width();
  }
}

}  // namespace internal
}  // namespace ash
