// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace internal {

// Minimum, maximum width of the dock area and a width of the gap
const int DockedWindowLayoutManager::kMinDockWidth = 200;
const int DockedWindowLayoutManager::kMaxDockWidth = 450;
const int DockedWindowLayoutManager::kMinDockGap = 2;
const int kWindowIdealSpacing = 4;

namespace {

const SkColor kDockBackgroundColor = SkColorSetARGB(0xff, 0x10, 0x10, 0x10);
const float kDockBackgroundOpacity = 0.5f;

class DockedBackgroundWidget : public views::Widget {
 public:
  explicit DockedBackgroundWidget(aura::Window* container) {
    InitWidget(container);
  }

 private:
  void InitWidget(aura::Window* parent) {
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_POPUP;
    params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
    params.can_activate = false;
    params.keep_on_top = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = parent;
    params.accept_events = false;
    set_focus_on_creation(false);
    Init(params);
    DCHECK_EQ(GetNativeView()->GetRootWindow(), parent->GetRootWindow());
    views::View* content_view = new views::View;
    content_view->set_background(
        views::Background::CreateSolidBackground(kDockBackgroundColor));
    SetContentsView(content_view);
    GetNativeWindow()->layer()->SetOpacity(kDockBackgroundOpacity);
    Hide();
  }

  DISALLOW_COPY_AND_ASSIGN(DockedBackgroundWidget);
};

DockedWindowLayoutManager* GetDockLayoutManager(aura::Window* window,
                                                const gfx::Point& location) {
  gfx::Rect near_location(location, gfx::Size());
  aura::Window* dock = Shell::GetContainer(
      wm::GetRootWindowMatching(near_location),
      kShellWindowId_DockedContainer);
  return static_cast<internal::DockedWindowLayoutManager*>(
      dock->layout_manager());
}

// Certain windows (minimized, hidden or popups) do not matter to docking.
bool IsUsedByLayout(aura::Window* window) {
  return (window->IsVisible() &&
          !wm::IsWindowMinimized(window) &&
          window->type() != aura::client::WINDOW_TYPE_POPUP);
}

bool CompareWindowPos(const aura::Window* win1, const aura::Window* win2) {
  return win1->GetTargetBounds().CenterPoint().y() <
      win2->GetTargetBounds().CenterPoint().y();
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
      docked_width_(0),
      alignment_(DOCKED_ALIGNMENT_NONE),
      last_active_window_(NULL),
      background_widget_(new DockedBackgroundWidget(dock_container_)) {
  DCHECK(dock_container);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

DockedWindowLayoutManager::~DockedWindowLayoutManager() {
  Shutdown();
}

void DockedWindowLayoutManager::Shutdown() {
  if (shelf_layout_manager_) {
    shelf_layout_manager_->RemoveObserver(this);
  }
  shelf_layout_manager_ = NULL;
  launcher_ = NULL;
  for (size_t i = 0; i < dock_container_->children().size(); ++i)
    dock_container_->children()[i]->RemoveObserver(this);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void DockedWindowLayoutManager::AddObserver(
    DockedWindowLayoutManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DockedWindowLayoutManager::RemoveObserver(
    DockedWindowLayoutManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void DockedWindowLayoutManager::StartDragging(aura::Window* window) {
  if (window->parent() != dock_container_)
    return;
  DCHECK(!dragged_window_);
  dragged_window_ = window;
  Relayout();
}

void DockedWindowLayoutManager::FinishDragging() {
  if (!dragged_window_)
    return;

  // If this is the first window getting docked - update alignment.
  if (alignment_ == DOCKED_ALIGNMENT_NONE) {
    alignment_ = AlignmentOfWindow(dragged_window_);
  } else {
    // There were some docked windows before. Check if the |dragged_window_|
    // was the only one remaining and if so possibly switch alignment to
    // opposite side. Ignore popups.
    bool found_docked_window = false;
    for (size_t i = 0; i < dock_container_->children().size(); ++i) {
      aura::Window* window(dock_container_->children()[i]);
      if (window != dragged_window_ &&
          window->type() != aura::client::WINDOW_TYPE_POPUP) {
        found_docked_window = true;
        break;
      }
    }
    if (!found_docked_window)
      alignment_ = AlignmentOfWindow(dragged_window_);
  }

  // We no longer need to observe |dragged_window_| unless it is added back;
  if (dragged_window_) {
    if (dragged_window_->parent() != dock_container_)
      dragged_window_->RemoveObserver(this);
    dragged_window_ = NULL;
  }

  Relayout();
  UpdateDockBounds();
}

// static
bool DockedWindowLayoutManager::ShouldWindowDock(aura::Window* window,
                                                 const gfx::Point& location) {
  DockedWindowLayoutManager* layout_manager = GetDockLayoutManager(window,
                                                                   location);
  const DockedAlignment alignment = layout_manager->CalculateAlignment();
  const gfx::Rect& bounds(window->GetBoundsInScreen());
  const gfx::Rect docked_bounds = (alignment == DOCKED_ALIGNMENT_NONE) ?
      layout_manager->dock_container_->GetBoundsInScreen() :
      layout_manager->docked_bounds_;
  DockedAlignment dock_edge = DOCKED_ALIGNMENT_NONE;
  if (alignment == DOCKED_ALIGNMENT_NONE) {
    // Check if the window is touching the edge - it may need to get docked.
    dock_edge = layout_manager->AlignmentOfWindow(window);
  } else if ((docked_bounds.Intersects(bounds) &&
              docked_bounds.Contains(location)) ||
             alignment == layout_manager->AlignmentOfWindow(window)) {
    // A window is being added to other docked windows (on the same side).
    // Both bounds and pointer location are checked because some drags involve
    // sticky edges and so the |location| may be outside of the |bounds|.
    // It is also possible that all the docked windows are minimized or hidden
    // in which case the dragged window needs to be exactly touching the same
    // edge that those docked windows were aligned before they got minimized.
    dock_edge = alignment;
  }

  // Do not allow docking on the same side as launcher shelf.
  if (layout_manager->launcher_ && layout_manager->launcher_->shelf_widget()) {
    ShelfAlignment shelf_alignment =
        layout_manager->launcher_->shelf_widget()->GetAlignment();
    if ((shelf_alignment == SHELF_ALIGNMENT_LEFT &&
         dock_edge == DOCKED_ALIGNMENT_LEFT) ||
        (shelf_alignment == SHELF_ALIGNMENT_RIGHT &&
         dock_edge == DOCKED_ALIGNMENT_RIGHT)) {
      dock_edge = DOCKED_ALIGNMENT_NONE;
    }
  }

  // Do not allow docking if a window is vertically maximized (as is the case
  // when it is snapped).
  const gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(window).work_area();
  if (bounds.y() == work_area.y() && bounds.height() == work_area.height())
    dock_edge = DOCKED_ALIGNMENT_NONE;

  return dock_edge != DOCKED_ALIGNMENT_NONE;
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

DockedAlignment DockedWindowLayoutManager::CalculateAlignment() const {
  // Find a child that is not being dragged and is not a popup.
  // If such exists the current alignment is returned - even if some of the
  // children are hidden or minimized (so they can be restored without losing
  // the docked state).
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* window(dock_container_->children()[i]);
    if (window != dragged_window_ &&
        window->type() != aura::client::WINDOW_TYPE_POPUP) {
      return alignment_;
    }
  }
  // No docked windows remain other than possibly the window being dragged.
  // Return |NONE| to indicate that windows may get docked on either side.
  return DOCKED_ALIGNMENT_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, aura::LayoutManager implementation:
void DockedWindowLayoutManager::OnWindowResized() {
  Relayout();
  // When screen resizes we need to update the insets even when dock width or
  // alignment does not change.
  UpdateDockBounds();
}

void DockedWindowLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;

  // If this is the first window getting docked - update alignment.
  if (alignment_ == DOCKED_ALIGNMENT_NONE)
    alignment_ = AlignmentOfWindow(child);
  // We need to observe a child unless it is a |dragged_window_| that just got
  // added back in which case we are already observing it.
  if (child != dragged_window_)
    child->AddObserver(this);
  Relayout();
  UpdateDockBounds();
}

void DockedWindowLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;

  // Try to find a child that is not a popup and is not being dragged.
  bool found_docked_window = false;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* window(dock_container_->children()[i]);
    if (window != dragged_window_ &&
        window->type() != aura::client::WINDOW_TYPE_POPUP) {
      found_docked_window = true;
      break;
    }
  }
  if (!found_docked_window)
    alignment_ = DOCKED_ALIGNMENT_NONE;

  // Keep track of former children if they are dragged as they can be reparented
  // temporarily just for the drag.
  if (child != dragged_window_)
    child->RemoveObserver(this);

  if (last_active_window_ == child)
    last_active_window_ = NULL;

  // Close the dock and expand workspace work area.
  Relayout();

  // When a panel is removed from this layout manager it may still be dragged.
  // Delay updating dock and workspace bounds until the drag is completed. This
  // preserves the docked area width until the panel drag is finished.
  if (child->type() != aura::client::WINDOW_TYPE_PANEL)
    UpdateDockBounds();
}

void DockedWindowLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;
  if (visible)
    child->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  Relayout();
  UpdateDockBounds();
}

void DockedWindowLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  // Whenever one of our windows is moved or resized we need to enforce layout.
  SetChildBoundsDirect(child, requested_bounds);
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;
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

  // Do not allow launcher and dock on the same side. Switch side that
  // the dock is attached to and move all dock windows to that new side.
  ShelfAlignment shelf_alignment = launcher_->shelf_widget()->GetAlignment();
  if (alignment_ == DOCKED_ALIGNMENT_LEFT &&
      shelf_alignment == SHELF_ALIGNMENT_LEFT) {
    alignment_ = DOCKED_ALIGNMENT_RIGHT;
  } else if (alignment_ == DOCKED_ALIGNMENT_RIGHT &&
             shelf_alignment == SHELF_ALIGNMENT_RIGHT) {
    alignment_ = DOCKED_ALIGNMENT_LEFT;
  }
  Relayout();
  UpdateDockBounds();
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

void DockedWindowLayoutManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (window == dragged_window_)
    Relayout();
}

void DockedWindowLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (dragged_window_ == window)
    dragged_window_ = NULL;
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
  UpdateDockBounds();
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

DockedAlignment DockedWindowLayoutManager::AlignmentOfWindow(
    const aura::Window* window) const {
  const gfx::Rect& bounds(window->GetBoundsInScreen());
  const gfx::Rect docked_bounds = dock_container_->GetBoundsInScreen();
  if (bounds.x() == docked_bounds.x())
    return DOCKED_ALIGNMENT_LEFT;
  if (bounds.right() == docked_bounds.right())
    return DOCKED_ALIGNMENT_RIGHT;
  return DOCKED_ALIGNMENT_NONE;
}

void DockedWindowLayoutManager::Relayout() {
  if (in_layout_ || alignment_ == DOCKED_ALIGNMENT_NONE)
    return;
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  gfx::Rect dock_bounds = dock_container_->bounds();
  aura::Window* active_window = NULL;
  aura::Window::Windows visible_windows;
  docked_width_ = 0;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* window(dock_container_->children()[i]);

    if (!IsUsedByLayout(window))
      continue;

    // If the shelf is currently hidden (full-screen mode), hide window until
    // full-screen mode is exited.
    if (shelf_hidden_) {
      // The call to Hide does not set the minimize property, so the window will
      // be restored when the shelf becomes visible again.
      window->Hide();
      continue;
    }
    if (window->HasFocus() ||
        window->Contains(
            aura::client::GetFocusClient(window)->GetFocusedWindow())) {
      DCHECK(!active_window);
      active_window = window;
    }
    visible_windows.push_back(window);
  }

  // Consider windows that were formerly children of the |dock_container_|
  // when fanning out other child windows.
  if (dragged_window_ && dragged_window_->parent() != dock_container_)
    visible_windows.push_back(dragged_window_);

  // Sort windows by their center positions and fan out overlapping
  // windows.
  std::sort(visible_windows.begin(), visible_windows.end(), CompareWindowPos);
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestWindow(
      dock_container_);
  const gfx::Rect work_area = display.work_area();
  int available_room = work_area.height();
  for (aura::Window::Windows::const_iterator iter = visible_windows.begin();
      iter != visible_windows.end(); ++iter) {
    available_room -= (*iter)->bounds().height();
  }
  const int num_windows = visible_windows.size();
  const float delta = (float)available_room /
      ((available_room > 0 || num_windows <= 1) ?
          num_windows + 1 : num_windows - 1);
  float y_pos = (available_room > 0) ? delta : 0;

  for (aura::Window::Windows::const_iterator iter = visible_windows.begin();
      iter != visible_windows.end(); ++iter) {
    aura::Window* window = *iter;
    gfx::Rect bounds = window->GetTargetBounds();
    // Do not force position of a single window.
    if (num_windows == 1)
      y_pos = bounds.y();

    // Fan out windows evenly distributing the overlap or remaining free space.
    bounds.set_y(std::max(work_area.y(),
                          std::min(work_area.bottom() - bounds.height(),
                                   static_cast<int>(y_pos + 0.5))));
    y_pos += bounds.height() + delta;

    // All docked windows other than the one currently dragged remain stuck
    // to the screen edge.
    if (window == dragged_window_)
      continue;
    switch (alignment_) {
      case DOCKED_ALIGNMENT_LEFT:
        bounds.set_x(0);
        break;
      case DOCKED_ALIGNMENT_RIGHT:
        bounds.set_x(dock_bounds.right() - bounds.width());
        break;
      case DOCKED_ALIGNMENT_NONE:
        NOTREACHED() << "Relayout called when dock alignment is 'NONE'";
        break;
    }
    // Keep the dock at least kMinDockWidth when all windows in it overhang.
    docked_width_ = std::min(kMaxDockWidth,
                             std::max(docked_width_,
                                      bounds.width() > kMaxDockWidth ?
                                          kMinDockWidth : bounds.width()));
    SetChildBoundsDirect(window, bounds);
  }
  UpdateStacking(active_window);
}

void DockedWindowLayoutManager::UpdateDockBounds() {
  int dock_inset = docked_width_ + (docked_width_ > 0 ? kMinDockGap : 0);
  gfx::Rect bounds = gfx::Rect(
      alignment_ == DOCKED_ALIGNMENT_RIGHT ?
          dock_container_->bounds().right() - dock_inset:
          dock_container_->bounds().x(),
      dock_container_->bounds().y(),
      dock_inset,
      dock_container_->bounds().height());
  docked_bounds_ = bounds +
      dock_container_->GetBoundsInScreen().OffsetFromOrigin();
  FOR_EACH_OBSERVER(
      DockedWindowLayoutManagerObserver,
      observer_list_,
      OnDockBoundsChanging(bounds));

  // Show or hide background for docked area.
  gfx::Rect background_bounds(docked_bounds_);
  const gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(dock_container_).work_area();
  background_bounds.set_height(work_area.height());
  background_widget_->SetBounds(background_bounds);
  if (docked_width_ > 0)
    background_widget_->Show();
  else
    background_widget_->Hide();
}

void DockedWindowLayoutManager::UpdateStacking(aura::Window* active_window) {
  if (!active_window) {
    if (!last_active_window_)
      return;
    active_window = last_active_window_;
  }

  // We want to to stack the windows like a deck of cards:
  //  ,------.
  // |,------.|
  // |,------.|
  // | active |
  // | window |
  // |`------'|
  // |`------'|
  //  `------'
  // We use the middle of each window to figure out how to stack the window.
  // This allows us to update the stacking when a window is being dragged around
  // by the titlebar.
  std::map<int, aura::Window*> window_ordering;
  for (aura::Window::Windows::const_iterator it =
           dock_container_->children().begin();
       it != dock_container_->children().end(); ++it) {
    if (!IsUsedByLayout(*it))
      continue;
    gfx::Rect bounds = (*it)->bounds();
    window_ordering.insert(std::make_pair(bounds.y() + bounds.height() / 2,
                                          *it));
  }
  int active_center_y = active_window->bounds().CenterPoint().y();

  aura::Window* previous_window = background_widget_->GetNativeWindow();
  for (std::map<int, aura::Window*>::const_iterator it =
       window_ordering.begin();
       it != window_ordering.end() && it->first < active_center_y; ++it) {
    if (previous_window)
      dock_container_->StackChildAbove(it->second, previous_window);
    previous_window = it->second;
  }

  previous_window = NULL;
  for (std::map<int, aura::Window*>::const_reverse_iterator it =
       window_ordering.rbegin();
       it != window_ordering.rend() && it->first > active_center_y; ++it) {
    if (previous_window)
      dock_container_->StackChildAbove(it->second, previous_window);
    previous_window = it->second;
  }

  if (active_window->parent() == dock_container_)
    dock_container_->StackChildAtTop(active_window);
  if (dragged_window_ && dragged_window_->parent() == dock_container_)
    dock_container_->StackChildAtTop(dragged_window_);
  last_active_window_ = active_window;
}

////////////////////////////////////////////////////////////////////////////////
// keyboard::KeyboardControllerObserver implementation:

void DockedWindowLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& keyboard_bounds) {
  // This bounds change will have caused a change to the Shelf which does not
  // propagate automatically to this class, so manually recalculate bounds.
  UpdateDockBounds();
}

}  // namespace internal
}  // namespace ash
