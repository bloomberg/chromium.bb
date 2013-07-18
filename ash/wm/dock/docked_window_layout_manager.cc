// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_layout_manager.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace internal {

namespace {

DockedWindowLayoutManager* GetDockLayoutManager(aura::Window* window,
                                                const gfx::Point& location) {
  gfx::Rect near_location(location, gfx::Size());
  aura::Window* dock = Shell::GetContainer(
      wm::GetRootWindowMatching(near_location),
      kShellWindowId_DockedContainer);
  return static_cast<internal::DockedWindowLayoutManager*>(
      dock->layout_manager());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager public implementation:
DockedWindowLayoutManager::DockedWindowLayoutManager(
    aura::Window* dock_container)
    : dock_container_(dock_container),
      in_layout_(false),
      dragged_window_(NULL),
      launcher_(NULL),
      shelf_layout_manager_(NULL),
      shelf_hidden_(false),
      alignment_(DOCKED_ALIGNMENT_NONE) {
  DCHECK(dock_container);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

DockedWindowLayoutManager::~DockedWindowLayoutManager() {
  launcher_ = NULL;
  for (size_t i = 0; i < dock_container_->children().size(); ++i)
    dock_container_->children()[i]->RemoveObserver(this);
  if (shelf_layout_manager_)
    shelf_layout_manager_->RemoveObserver(this);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void DockedWindowLayoutManager::StartDragging(aura::Window* window) {
  if (window->parent() != dock_container_)
    return;
  DCHECK(!dragged_window_);
  dragged_window_ = window;
  Relayout();
}

void DockedWindowLayoutManager::FinishDragging() {
  dragged_window_ = NULL;
  Relayout();
}

// static
bool DockedWindowLayoutManager::ShouldWindowDock(aura::Window* window,
                                                 const gfx::Point& location) {
  DockedWindowLayoutManager* layout_manager = GetDockLayoutManager(window,
                                                                   location);
  const gfx::Rect& bounds(window->GetBoundsInScreen());
  gfx::Rect dock_bounds = layout_manager->dock_container_->GetBoundsInScreen();
  // Possible sides of the screen that a window can be touching.
  enum DockedEdge {
    DOCKED_EDGE_NONE,
    DOCKED_EDGE_LEFT,
    DOCKED_EDGE_RIGHT,
  } dock_edge = DOCKED_EDGE_NONE;
  if (bounds.x() == dock_bounds.x() &&
      layout_manager->alignment_ != DOCKED_ALIGNMENT_RIGHT) {
    dock_edge = DOCKED_EDGE_LEFT;
  } else if (bounds.right() == dock_bounds.right() &&
             layout_manager->alignment_ != DOCKED_ALIGNMENT_LEFT) {
    dock_edge = DOCKED_EDGE_RIGHT;
  }

  // do not allow dock on the same side as launcher shelf
  if (layout_manager->launcher_ && layout_manager->launcher_->shelf_widget()) {
    ShelfAlignment shelf_alignment =
        layout_manager->launcher_->shelf_widget()->GetAlignment();
    if ((shelf_alignment == SHELF_ALIGNMENT_LEFT &&
         dock_edge == DOCKED_EDGE_LEFT) ||
        (shelf_alignment == SHELF_ALIGNMENT_RIGHT &&
         dock_edge == DOCKED_EDGE_RIGHT)) {
      dock_edge = DOCKED_EDGE_NONE;
    }
  }
  return dock_edge != DOCKED_EDGE_NONE;
}

void DockedWindowLayoutManager::SetLauncher(ash::Launcher* launcher) {
  DCHECK(!launcher_);
  DCHECK(!shelf_layout_manager_);
  launcher_ = launcher;
  if (launcher_->shelf_widget()) {
    shelf_layout_manager_ = ash::internal::ShelfLayoutManager::ForLauncher(
        launcher_->shelf_widget()->GetNativeWindow());
    WillChangeVisibilityState(shelf_layout_manager_->visibility_state());
    shelf_layout_manager_->AddObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, aura::LayoutManager implementation:
void DockedWindowLayoutManager::OnWindowResized() {
  Relayout();
}

void DockedWindowLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  child->AddObserver(this);
  Relayout();
}

void DockedWindowLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void DockedWindowLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  child->RemoveObserver(this);
  Relayout();
}

void DockedWindowLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (visible)
    child->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  Relayout();
}

void DockedWindowLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  // Whenever one of our windows is moved or resized we need to enforce layout.
  SetChildBoundsDirect(child, requested_bounds);
  Relayout();
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, ash::ShellObserver implementation:

void DockedWindowLayoutManager::OnShelfAlignmentChanged(
    aura::RootWindow* root_window) {
  if (dock_container_->GetRootWindow() != root_window)
    return;

  if (!launcher_ || !launcher_->shelf_widget())
    return;

  if (alignment_ == DOCKED_ALIGNMENT_NONE)
    return;

  // It should not be possible to have empty children() while alignment is set
  // to anything other than DOCKED_ALIGNMENT_NONE - see Relayout.
  if (dock_container_->children().empty()) {
    LOG(ERROR) << "No children and alignment set to " << alignment_;
    return;
  }
  aura::Window* window = dock_container_->children()[0];
  const gfx::Rect& dock_bounds = dock_container_->bounds();
  gfx::Rect bounds = window->bounds();

  // Do not allow launcher and dock on the same side. Switch side that
  // the dock is attached to and move all dock windows to that new side.
  // We actually only need to move the first window and the rest will follow
  // in Relayout
  ShelfAlignment shelf_alignment =
      launcher_->shelf_widget()->GetAlignment();
  if (alignment_ == DOCKED_ALIGNMENT_LEFT &&
      shelf_alignment == SHELF_ALIGNMENT_LEFT) {
    bounds.set_x(dock_bounds.right() - bounds.width());
    SetChildBoundsDirect(window, bounds);
  } else if (alignment_ == DOCKED_ALIGNMENT_RIGHT &&
             shelf_alignment == SHELF_ALIGNMENT_RIGHT) {
    bounds.set_x(0);
    SetChildBoundsDirect(window, bounds);
  }
  Relayout();
}

/////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, WindowObserver implementation:

void DockedWindowLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                        const void* key,
                                                        intptr_t old) {
  if (key != aura::client::kShowStateKey)
    return;
  // The window property will still be set, but no actual change will occur
  // until WillChangeVisibilityState is called when the shelf is visible again
  if (shelf_hidden_)
    return;
  if (wm::IsWindowMinimized(window))
    MinimizeWindow(window);
  else
    RestoreWindow(window);
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, aura::client::ActivationChangeObserver implementation:

void DockedWindowLayoutManager::OnWindowActivated(aura::Window* gained_active,
                                                  aura::Window* lost_active) {
  // Ignore if the window that is not managed by this was activated.
  aura::Window* ancestor = NULL;
  for (aura::Window* parent = gained_active;
       parent; parent = parent->parent()) {
    if (parent->parent() == dock_container_) {
      ancestor = parent;
      break;
    }
  }
  if (ancestor)
    UpdateStacking(ancestor);
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, ShelfLayoutManagerObserver implementation:

void DockedWindowLayoutManager::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  // On entering / leaving full screen mode the shelf visibility state is
  // changed to / from SHELF_HIDDEN. In this state, docked windows should hide
  // to allow the full-screen application to use the full screen.

  // TODO(varkha): ShelfLayoutManager::UpdateVisibilityState sets state to
  // SHELF_AUTO_HIDE when in immersive mode. We need to distinguish this from
  // when shelf enters auto-hide state based on mouse hover when auto-hide
  // setting is enabled and hide all windows (immersive mode should hide docked
  // windows).
  shelf_hidden_ = new_state == ash::SHELF_HIDDEN;
  {
    // prevent Relayout from getting called multiple times during this
    base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
    for (size_t i = 0; i < dock_container_->children().size(); ++i) {
      aura::Window* window = dock_container_->children()[i];
      if (shelf_hidden_) {
        if (window->IsVisible())
          MinimizeWindow(window);
      } else {
        if (!wm::IsWindowMinimized(window))
          RestoreWindow(window);
      }
    }
  }
  Relayout();
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager private implementation:

void DockedWindowLayoutManager::MinimizeWindow(aura::Window* window) {
  window->Hide();
  if (wm::IsActiveWindow(window))
    wm::DeactivateWindow(window);
}

void DockedWindowLayoutManager::RestoreWindow(aura::Window* window) {
  window->Show();
}

void DockedWindowLayoutManager::Relayout() {
  if (in_layout_)
    return;
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  if (dock_container_->children().empty()) {
    alignment_ = DOCKED_ALIGNMENT_NONE;
    return;
  }

  gfx::Rect dock_bounds = dock_container_->bounds();
  aura::Window* active_window = NULL;
  std::vector<aura::Window*> visible_windows;
  alignment_ = DOCKED_ALIGNMENT_ANY;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* window(dock_container_->children()[i]);

    if (!window->IsVisible() || wm::IsWindowMinimized(window))
      continue;

    // If the shelf is currently hidden (full-screen mode), hide window until
    // full-screen mode is exited.
    if (shelf_hidden_) {
      // The call to Hide does not set the minimize property, so the window will
      // be restored when the shelf becomes visible again.
      window->Hide();
      continue;
    }

    gfx::Rect bounds = window->GetTargetBounds();
    if (window != dragged_window_ && alignment_ == DOCKED_ALIGNMENT_ANY) {
      if (bounds.x() == 0)
        alignment_ = DOCKED_ALIGNMENT_LEFT;
      else if (bounds.right() == dock_bounds.right())
        alignment_ = DOCKED_ALIGNMENT_RIGHT;
    }

    if (window->HasFocus() ||
        window->Contains(
            aura::client::GetFocusClient(window)->GetFocusedWindow())) {
      DCHECK(!active_window);
      active_window = window;
    }

    // all docked windows remain stuck to the screen edge
    if (window != dragged_window_) {
      switch (alignment_) {
        case DOCKED_ALIGNMENT_LEFT:
          bounds.set_x(0);
          break;
        case DOCKED_ALIGNMENT_RIGHT:
        case DOCKED_ALIGNMENT_ANY:
          bounds.set_x(dock_bounds.right() - bounds.width());
          break;
        case DOCKED_ALIGNMENT_NONE:
          NOTREACHED() << "Relayout called when dock alignment is 'NONE'";
          break;
      }
    }
    SetChildBoundsDirect(window, bounds);
  }

  UpdateStacking(active_window);
}

void DockedWindowLayoutManager::UpdateStacking(aura::Window* active_window) {
  // TODO(varkha): Implement restacking to ensure that all docked windows are at
  // least partially visible and selectable.
}

////////////////////////////////////////////////////////////////////////////////
// keyboard::KeyboardControllerObserver implementation:

void DockedWindowLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& keyboard_bounds) {
  // This bounds change will have caused a change to the Shelf which does not
  // propagate automatically to this class, so manually recalculate bounds.
  OnWindowResized();
}

}  // namespace internal
}  // namespace ash
