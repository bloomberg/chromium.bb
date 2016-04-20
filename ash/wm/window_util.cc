// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/snap_to_pixel_layout_manager.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/wm_event.h"
#include "ash/wm/common/wm_screen_util.h"
#include "ash/wm/common/wm_window.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace wm {

namespace {

// Returns the default width of a snapped window.
int GetDefaultSnappedWindowWidth(wm::WmWindow* window) {
  const float kSnappedWidthWorkspaceRatio = 0.5f;

  int work_area_width = GetDisplayWorkAreaBoundsInParent(window).width();
  int min_width = window->GetMinimumSize().width();
  int ideal_width =
      static_cast<int>(work_area_width * kSnappedWidthWorkspaceRatio);
  return std::min(work_area_width, std::max(ideal_width, min_width));
}

int GetDefaultSnappedWindowWidth(aura::Window* window) {
  return GetDefaultSnappedWindowWidth(WmWindowAura::Get(window));
}

}  // namespace

// TODO(beng): replace many of these functions with the corewm versions.
void ActivateWindow(aura::Window* window) {
  ::wm::ActivateWindow(window);
}

void DeactivateWindow(aura::Window* window) {
  ::wm::DeactivateWindow(window);
}

bool IsActiveWindow(aura::Window* window) {
  return ::wm::IsActiveWindow(window);
}

aura::Window* GetActiveWindow() {
  return aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      GetActiveWindow();
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  return ::wm::GetActivatableWindow(window);
}

bool CanActivateWindow(aura::Window* window) {
  return ::wm::CanActivateWindow(window);
}

bool IsWindowMinimized(aura::Window* window) {
  return ash::wm::GetWindowState(window)->IsMinimized();
}

bool IsWindowUserPositionable(aura::Window* window) {
  return GetWindowState(window)->IsUserPositionable();
}

void CenterWindow(aura::Window* window) {
  wm::WMEvent event(wm::WM_EVENT_CENTER);
  wm::GetWindowState(window)->OnWMEvent(&event);
}

gfx::Rect GetDefaultLeftSnappedWindowBoundsInParent(wm::WmWindow* window) {
  gfx::Rect work_area_in_parent(GetDisplayWorkAreaBoundsInParent(window));
  return gfx::Rect(work_area_in_parent.x(), work_area_in_parent.y(),
                   GetDefaultSnappedWindowWidth(window),
                   work_area_in_parent.height());
}

gfx::Rect GetDefaultRightSnappedWindowBoundsInParent(wm::WmWindow* window) {
  gfx::Rect work_area_in_parent(GetDisplayWorkAreaBoundsInParent(window));
  int width = GetDefaultSnappedWindowWidth(window);
  return gfx::Rect(work_area_in_parent.right() - width, work_area_in_parent.y(),
                   width, work_area_in_parent.height());
}

gfx::Rect GetDefaultLeftSnappedWindowBoundsInParent(aura::Window* window) {
  gfx::Rect work_area_in_parent(ScreenUtil::GetDisplayWorkAreaBoundsInParent(
      window));
  return gfx::Rect(work_area_in_parent.x(),
                   work_area_in_parent.y(),
                   GetDefaultSnappedWindowWidth(window),
                   work_area_in_parent.height());
}

gfx::Rect GetDefaultRightSnappedWindowBoundsInParent(aura::Window* window) {
  gfx::Rect work_area_in_parent(ScreenUtil::GetDisplayWorkAreaBoundsInParent(
      window));
  int width = GetDefaultSnappedWindowWidth(window);
  return gfx::Rect(work_area_in_parent.right() - width,
                   work_area_in_parent.y(),
                   width,
                   work_area_in_parent.height());
}

bool MoveWindowToEventRoot(aura::Window* window, const ui::Event& event) {
  views::View* target = static_cast<views::View*>(event.target());
  if (!target)
    return false;
  aura::Window* target_root =
      target->GetWidget()->GetNativeView()->GetRootWindow();
  if (!target_root || target_root == window->GetRootWindow())
    return false;
  aura::Window* window_container =
      ash::Shell::GetContainer(target_root, window->parent()->id());
  // Move the window to the target launcher.
  window_container->AddChild(window);
  return true;
}

void ReparentChildWithTransientChildren(aura::Window* child,
                                        aura::Window* old_parent,
                                        aura::Window* new_parent) {
  if (child->parent() == old_parent)
    new_parent->AddChild(child);
  ReparentTransientChildrenOfChild(child, old_parent, new_parent);
}

void ReparentTransientChildrenOfChild(aura::Window* child,
                                      aura::Window* old_parent,
                                      aura::Window* new_parent) {
  for (size_t i = 0;
       i < ::wm::GetTransientChildren(child).size();
       ++i) {
    ReparentChildWithTransientChildren(
        ::wm::GetTransientChildren(child)[i],
        old_parent,
        new_parent);
  }
}

void SnapWindowToPixelBoundary(aura::Window* window) {
  aura::Window* snapped_ancestor = window->parent();
  while (snapped_ancestor) {
    if (snapped_ancestor->GetProperty(kSnapChildrenToPixelBoundary)) {
      ui::SnapLayerToPhysicalPixelBoundary(snapped_ancestor->layer(),
                                           window->layer());
      return;
    }
    snapped_ancestor = snapped_ancestor->parent();
  }
}

void SetSnapsChildrenToPhysicalPixelBoundary(aura::Window* container) {
  DCHECK(!container->GetProperty(kSnapChildrenToPixelBoundary))
      << container->name();
  container->SetProperty(kSnapChildrenToPixelBoundary, true);
}

void InstallSnapLayoutManagerToContainers(aura::Window* parent) {
  aura::Window::Windows children = parent->children();
  for (aura::Window::Windows::iterator iter = children.begin();
       iter != children.end();
       ++iter) {
    aura::Window* container = *iter;
    if (container->id() < 0)  // not a container
      continue;
    if (container->GetProperty(kSnapChildrenToPixelBoundary)) {
      if (!container->layout_manager())
        container->SetLayoutManager(new SnapToPixelLayoutManager(container));
    } else {
      InstallSnapLayoutManagerToContainers(container);
    }
  }
}

}  // namespace wm
}  // namespace ash
