// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/window_positioning_utils.h"

#include <algorithm>

#include "ash/common/wm/system_modal_container_layout_manager.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm/wm_screen_util.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_tracker.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace wm {

namespace {

// Returns the default width of a snapped window.
int GetDefaultSnappedWindowWidth(WmWindow* window) {
  const float kSnappedWidthWorkspaceRatio = 0.5f;

  int work_area_width = GetDisplayWorkAreaBoundsInParent(window).width();
  int min_width = window->GetMinimumSize().width();
  int ideal_width =
      static_cast<int>(work_area_width * kSnappedWidthWorkspaceRatio);
  return std::min(work_area_width, std::max(ideal_width, min_width));
}

// Return true if the window or one of its ancestor returns true from
// IsLockedToRoot().
bool IsWindowOrAncestorLockedToRoot(const WmWindow* window) {
  return window && (window->IsLockedToRoot() ||
                    IsWindowOrAncestorLockedToRoot(window->GetParent()));
}

// Move all transient children to |dst_root|, including the ones in
// the child windows and transient children of the transient children.
void MoveAllTransientChildrenToNewRoot(const display::Display& display,
                                       WmWindow* window) {
  WmWindow* dst_root = WmLookup::Get()
                           ->GetRootWindowControllerWithDisplayId(display.id())
                           ->GetWindow();
  for (WmWindow* transient_child : window->GetTransientChildren()) {
    const int container_id = transient_child->GetParent()->GetShellWindowId();
    DCHECK_GE(container_id, 0);
    WmWindow* container = dst_root->GetChildByShellWindowId(container_id);
    const gfx::Rect transient_child_bounds_in_screen =
        transient_child->GetBoundsInScreen();
    container->AddChild(transient_child);
    transient_child->SetBoundsInScreen(transient_child_bounds_in_screen,
                                       display);

    // Transient children may have transient children.
    MoveAllTransientChildrenToNewRoot(display, transient_child);
  }
  // Move transient children of the child windows if any.
  for (WmWindow* child : window->GetChildren())
    MoveAllTransientChildrenToNewRoot(display, child);
}

}  // namespace

void AdjustBoundsSmallerThan(const gfx::Size& max_size, gfx::Rect* bounds) {
  bounds->set_width(std::min(bounds->width(), max_size.width()));
  bounds->set_height(std::min(bounds->height(), max_size.height()));
}

void AdjustBoundsToEnsureWindowVisibility(const gfx::Rect& visible_area,
                                          int min_width,
                                          int min_height,
                                          gfx::Rect* bounds) {
  AdjustBoundsSmallerThan(visible_area.size(), bounds);

  min_width = std::min(min_width, visible_area.width());
  min_height = std::min(min_height, visible_area.height());

  if (bounds->right() < visible_area.x() + min_width) {
    bounds->set_x(visible_area.x() + std::min(bounds->width(), min_width) -
                  bounds->width());
  } else if (bounds->x() > visible_area.right() - min_width) {
    bounds->set_x(visible_area.right() - std::min(bounds->width(), min_width));
  }
  if (bounds->bottom() < visible_area.y() + min_height) {
    bounds->set_y(visible_area.y() + std::min(bounds->height(), min_height) -
                  bounds->height());
  } else if (bounds->y() > visible_area.bottom() - min_height) {
    bounds->set_y(visible_area.bottom() -
                  std::min(bounds->height(), min_height));
  }
  if (bounds->y() < visible_area.y())
    bounds->set_y(visible_area.y());
}

void AdjustBoundsToEnsureMinimumWindowVisibility(const gfx::Rect& visible_area,
                                                 gfx::Rect* bounds) {
  AdjustBoundsToEnsureWindowVisibility(visible_area, kMinimumOnScreenArea,
                                       kMinimumOnScreenArea, bounds);
}

gfx::Rect GetDefaultLeftSnappedWindowBoundsInParent(WmWindow* window) {
  gfx::Rect work_area_in_parent(GetDisplayWorkAreaBoundsInParent(window));
  return gfx::Rect(work_area_in_parent.x(), work_area_in_parent.y(),
                   GetDefaultSnappedWindowWidth(window),
                   work_area_in_parent.height());
}

gfx::Rect GetDefaultRightSnappedWindowBoundsInParent(WmWindow* window) {
  gfx::Rect work_area_in_parent(GetDisplayWorkAreaBoundsInParent(window));
  int width = GetDefaultSnappedWindowWidth(window);
  return gfx::Rect(work_area_in_parent.right() - width, work_area_in_parent.y(),
                   width, work_area_in_parent.height());
}

void CenterWindow(WmWindow* window) {
  WMEvent event(WM_EVENT_CENTER);
  window->GetWindowState()->OnWMEvent(&event);
}

void SetBoundsInScreen(WmWindow* window,
                       const gfx::Rect& bounds_in_screen,
                       const display::Display& display) {
  DCHECK_NE(display::kInvalidDisplayId, display.id());
  // Don't move a window to other root window if:
  // a) the window is a transient window. It moves when its
  //    transient parent moves.
  // b) if the window or its ancestor has IsLockedToRoot(). It's intentionally
  //    kept in the same root window even if the bounds is outside of the
  //    display.
  if (!window->GetTransientParent() &&
      !IsWindowOrAncestorLockedToRoot(window)) {
    WmRootWindowController* dst_root_window_controller =
        WmLookup::Get()->GetRootWindowControllerWithDisplayId(display.id());
    DCHECK(dst_root_window_controller);
    WmWindow* dst_root = dst_root_window_controller->GetWindow();
    DCHECK(dst_root);
    WmWindow* dst_container = nullptr;
    if (dst_root != window->GetRootWindow()) {
      int container_id = window->GetParent()->GetShellWindowId();
      // All containers that uses screen coordinates must have valid window ids.
      DCHECK_GE(container_id, 0);
      // Don't move modal background.
      if (!SystemModalContainerLayoutManager::IsModalBackground(window))
        dst_container = dst_root->GetChildByShellWindowId(container_id);
    }

    if (dst_container && window->GetParent() != dst_container) {
      WmWindow* focused = WmShell::Get()->GetFocusedWindow();
      WmWindow* active = WmShell::Get()->GetActiveWindow();

      WmWindowTracker tracker;
      if (focused)
        tracker.Add(focused);
      if (active && focused != active)
        tracker.Add(active);

      gfx::Point origin = bounds_in_screen.origin();
      const gfx::Point display_origin = display.bounds().origin();
      origin.Offset(-display_origin.x(), -display_origin.y());
      gfx::Rect new_bounds = gfx::Rect(origin, bounds_in_screen.size());

      // Set new bounds now so that the container's layout manager can adjust
      // the bounds if necessary.
      window->SetBounds(new_bounds);

      dst_container->AddChild(window);

      MoveAllTransientChildrenToNewRoot(display, window);

      // Restore focused/active window.
      if (tracker.Contains(focused)) {
        focused->SetFocused();
        WmShell::Get()->set_root_window_for_new_windows(
            focused->GetRootWindow());
      } else if (tracker.Contains(active)) {
        active->Activate();
      }
      // TODO(oshima): We should not have to update the bounds again
      // below in theory, but we currently do need as there is a code
      // that assumes that the bounds will never be overridden by the
      // layout mananger. We should have more explicit control how
      // constraints are applied by the layout manager.
    }
  }
  gfx::Point origin(bounds_in_screen.origin());
  const gfx::Point display_origin =
      window->GetDisplayNearestWindow().bounds().origin();
  origin.Offset(-display_origin.x(), -display_origin.y());
  window->SetBounds(gfx::Rect(origin, bounds_in_screen.size()));
}

}  // namespace wm
}  // namespace ash
