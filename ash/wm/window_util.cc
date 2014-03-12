// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace wm {

namespace {

// Returns the default width of a snapped window.
int GetDefaultSnappedWindowWidth(aura::Window* window) {
  const float kSnappedWidthWorkspaceRatio = 0.5f;

  int work_area_width =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(window).width();
  int min_width = window->delegate() ?
      window->delegate()->GetMinimumSize().width() : 0;
  int ideal_width =
      static_cast<int>(work_area_width * kSnappedWidthWorkspaceRatio);
  return std::min(work_area_width, std::max(ideal_width, min_width));
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

void CenterWindow(aura::Window* window) {
  wm::WindowState* window_state = wm::GetWindowState(window);
  if (!window_state->IsNormalOrSnapped())
    return;
  const gfx::Display display =
      Shell::GetScreen()->GetDisplayNearestWindow(window);
  gfx::Rect center = display.work_area();
  gfx::Size size = window->bounds().size();
  if (window_state->IsSnapped()) {
    if (window_state->HasRestoreBounds())
      size = window_state->GetRestoreBoundsInScreen().size();
    center.ClampToCenteredSize(size);
    window_state->SetRestoreBoundsInScreen(center);
    window_state->Restore();
  } else {
    center = ScreenUtil::ConvertRectFromScreen(window->parent(),
        center);
    center.ClampToCenteredSize(size);
    window->SetBounds(center);
  }
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

void AdjustBoundsSmallerThan(const gfx::Size& max_size, gfx::Rect* bounds) {
  bounds->set_width(std::min(bounds->width(), max_size.width()));
  bounds->set_height(std::min(bounds->height(), max_size.height()));
}

void AdjustBoundsToEnsureMinimumWindowVisibility(const gfx::Rect& visible_area,
                                                 gfx::Rect* bounds) {
  AdjustBoundsToEnsureWindowVisibility(
      visible_area, kMinimumOnScreenArea, kMinimumOnScreenArea, bounds);
}

void AdjustBoundsToEnsureWindowVisibility(const gfx::Rect& visible_area,
                                          int min_width,
                                          int min_height,
                                          gfx::Rect* bounds) {
  AdjustBoundsSmallerThan(visible_area.size(), bounds);

  min_width = std::min(min_width, visible_area.width());
  min_height = std::min(min_height, visible_area.height());

  if (bounds->right() < visible_area.x() + min_width) {
    bounds->set_x(visible_area.x() + min_width - bounds->width());
  } else if (bounds->x() > visible_area.right() - min_width) {
    bounds->set_x(visible_area.right() - min_width);
  }
  if (bounds->bottom() < visible_area.y() + min_height) {
    bounds->set_y(visible_area.y() + min_height - bounds->height());
  } else if (bounds->y() > visible_area.bottom() - min_height) {
    bounds->set_y(visible_area.bottom() - min_height);
  }
  if (bounds->y() < visible_area.y())
    bounds->set_y(visible_area.y());
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

}  // namespace wm
}  // namespace ash
