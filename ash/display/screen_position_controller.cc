// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_position_controller.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/dip_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Return true if the window or its ancestor has |kStayInSameRootWindowkey|
// property.
bool ShouldStayInSameRootWindow(const aura::Window* window) {
  return window && (window->GetProperty(kStayInSameRootWindowKey) ||
                    ShouldStayInSameRootWindow(window->parent()));
}

// Move all transient children to |dst_root|, including the ones in
// the child windows and transient children of the transient children.
void MoveAllTransientChildrenToNewRoot(const display::Display& display,
                                       aura::Window* window) {
  aura::Window* dst_root = Shell::GetInstance()
                               ->window_tree_host_manager()
                               ->GetRootWindowForDisplayId(display.id());
  aura::Window::Windows transient_children =
      ::wm::GetTransientChildren(window);
  for (aura::Window::Windows::iterator iter = transient_children.begin();
       iter != transient_children.end(); ++iter) {
    aura::Window* transient_child = *iter;
    int container_id = transient_child->parent()->id();
    DCHECK_GE(container_id, 0);
    aura::Window* container = Shell::GetContainer(dst_root, container_id);
    gfx::Rect parent_bounds_in_screen = transient_child->GetBoundsInScreen();
    container->AddChild(transient_child);
    transient_child->SetBoundsInScreen(parent_bounds_in_screen, display);

    // Transient children may have transient children.
    MoveAllTransientChildrenToNewRoot(display, transient_child);
  }
  // Move transient children of the child windows if any.
  aura::Window::Windows children = window->children();
  for (aura::Window::Windows::iterator iter = children.begin();
       iter != children.end(); ++iter)
    MoveAllTransientChildrenToNewRoot(display, *iter);
}

}  // namespace

// static
void ScreenPositionController::ConvertHostPointToRelativeToRootWindow(
    aura::Window* root_window,
    const aura::Window::Windows& root_windows,
    gfx::Point* point,
    aura::Window** target_root) {
  DCHECK(!root_window->parent());
  gfx::Point point_in_root(*point);
  root_window->GetHost()->ConvertPointFromHost(&point_in_root);

#if defined(USE_X11) || defined(USE_OZONE)
  gfx::Rect host_bounds(root_window->GetHost()->GetBounds().size());
  if (!host_bounds.Contains(*point)) {
    // This conversion is necessary to deal with X's passive input
    // grab while dragging window. For example, if we have two
    // displays, say 1000x1000 (primary) and 500x500 (extended one
    // on the right), and start dragging a window at (999, 123), and
    // then move the pointer to the right, the pointer suddenly
    // warps to the extended display. The destination is (0, 123) in
    // the secondary root window's coordinates, or (1000, 123) in
    // the screen coordinates. However, since the mouse is captured
    // by X during drag, a weird LocatedEvent, something like (0, 1123)
    // in the *primary* root window's coordinates, is sent to Chrome
    // (Remember that in the native X11 world, the two root windows
    // are always stacked vertically regardless of the display
    // layout in Ash). We need to figure out that (0, 1123) in the
    // primary root window's coordinates is actually (0, 123) in the
    // extended root window's coordinates.
    //
    // For now Ozone works in a similar manner as X11. Transitioning from one
    // display's coordinate system to anothers may cause events in the
    // primary's coordinate system which fall in the extended display.

    gfx::Point location_in_native(point_in_root);

    root_window->GetHost()->ConvertPointToNativeScreen(&location_in_native);

    for (size_t i = 0; i < root_windows.size(); ++i) {
      aura::WindowTreeHost* host = root_windows[i]->GetHost();
      const gfx::Rect native_bounds = host->GetBounds();
      if (native_bounds.Contains(location_in_native)) {
        *target_root = root_windows[i];
        *point = location_in_native;
        host->ConvertPointFromNativeScreen(point);
        return;
      }
    }
  }
#endif
  *target_root = root_window;
  *point = point_in_root;
}

void ScreenPositionController::ConvertPointToScreen(
    const aura::Window* window,
    gfx::Point* point) {
  const aura::Window* root = window->GetRootWindow();
  aura::Window::ConvertPointToTarget(window, root, point);
  const gfx::Point display_origin =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(const_cast<aura::Window*>(root))
          .bounds()
          .origin();
  point->Offset(display_origin.x(), display_origin.y());
}

void ScreenPositionController::ConvertPointFromScreen(
    const aura::Window* window,
    gfx::Point* point) {
  const aura::Window* root = window->GetRootWindow();
  const gfx::Point display_origin =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(const_cast<aura::Window*>(root))
          .bounds()
          .origin();
  point->Offset(-display_origin.x(), -display_origin.y());
  aura::Window::ConvertPointToTarget(root, window, point);
}

void ScreenPositionController::ConvertHostPointToScreen(
    aura::Window* root_window,
    gfx::Point* point) {
  aura::Window* root = root_window->GetRootWindow();
  aura::Window* target_root = nullptr;
  ConvertHostPointToRelativeToRootWindow(root, Shell::GetAllRootWindows(),
                                         point, &target_root);
  ConvertPointToScreen(target_root, point);
}

void ScreenPositionController::SetBounds(aura::Window* window,
                                         const gfx::Rect& bounds,
                                         const display::Display& display) {
  DCHECK_NE(-1, display.id());
  if (!window->parent()->GetProperty(kUsesScreenCoordinatesKey)) {
    window->SetBounds(bounds);
    return;
  }

  // Don't move a window to other root window if:
  // a) the window is a transient window. It moves when its
  //    transient_parent moves.
  // b) if the window or its ancestor has kStayInSameRootWindowkey. It's
  //    intentionally kept in the same root window even if the bounds is
  //    outside of the display.
  if (!::wm::GetTransientParent(window) &&
      !ShouldStayInSameRootWindow(window)) {
    aura::Window* dst_root = Shell::GetInstance()
                                 ->window_tree_host_manager()
                                 ->GetRootWindowForDisplayId(display.id());
    DCHECK(dst_root);
    aura::Window* dst_container = NULL;
    if (dst_root != window->GetRootWindow()) {
      int container_id = window->parent()->id();
      // All containers that uses screen coordinates must have valid window ids.
      DCHECK_GE(container_id, 0);
      // Don't move modal background.
      if (!SystemModalContainerLayoutManager::IsModalBackground(window))
        dst_container = Shell::GetContainer(dst_root, container_id);
    }

    if (dst_container && window->parent() != dst_container) {
      aura::Window* focused = aura::client::GetFocusClient(window)->
          GetFocusedWindow();
      aura::client::ActivationClient* activation_client =
          aura::client::GetActivationClient(window->GetRootWindow());
      aura::Window* active = activation_client->GetActiveWindow();

      aura::WindowTracker tracker;
      if (focused)
        tracker.Add(focused);
      if (active && focused != active)
        tracker.Add(active);

      // Set new bounds now so that the container's layout manager
      // can adjust the bounds if necessary.
      gfx::Point origin = bounds.origin();
      const gfx::Point display_origin = display.bounds().origin();
      origin.Offset(-display_origin.x(), -display_origin.y());
      gfx::Rect new_bounds = gfx::Rect(origin, bounds.size());

      window->SetBounds(new_bounds);

      dst_container->AddChild(window);

      MoveAllTransientChildrenToNewRoot(display, window);

      // Restore focused/active window.
      if (tracker.Contains(focused)) {
        aura::client::GetFocusClient(window)->FocusWindow(focused);
        // TODO(beng): replace with GetRootWindow().
        ash::Shell::GetInstance()->set_target_root_window(
            focused->GetRootWindow());
      } else if (tracker.Contains(active)) {
        activation_client->ActivateWindow(active);
      }
      // TODO(oshima): We should not have to update the bounds again
      // below in theory, but we currently do need as there is a code
      // that assumes that the bounds will never be overridden by the
      // layout mananger. We should have more explicit control how
      // constraints are applied by the layout manager.
    }
  }
  gfx::Point origin(bounds.origin());
  const gfx::Point display_origin = display::Screen::GetScreen()
                                        ->GetDisplayNearestWindow(window)
                                        .bounds()
                                        .origin();
  origin.Offset(-display_origin.x(), -display_origin.y());
  window->SetBounds(gfx::Rect(origin, bounds.size()));
}

}  // namespace ash
