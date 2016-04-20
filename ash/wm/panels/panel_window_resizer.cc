// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_window_resizer.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/common/window_parenting_utils.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {
const int kPanelSnapToLauncherDistance = 30;

PanelLayoutManager* GetPanelLayoutManager(aura::Window* panel_container) {
  DCHECK(panel_container->id() == kShellWindowId_PanelContainer);
  return static_cast<PanelLayoutManager*>(panel_container->layout_manager());
}

}  // namespace

PanelWindowResizer::~PanelWindowResizer() {
}

// static
PanelWindowResizer*
PanelWindowResizer::Create(WindowResizer* next_window_resizer,
                           wm::WindowState* window_state) {
  return new PanelWindowResizer(next_window_resizer, window_state);
}

void PanelWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  last_location_ = location;
  ::wm::ConvertPointToScreen(GetTarget()->parent(), &last_location_);
  if (!did_move_or_resize_) {
    did_move_or_resize_ = true;
    StartedDragging();
  }

  // Check if the destination has changed displays.
  gfx::Screen* screen = gfx::Screen::GetScreen();
  const gfx::Display dst_display =
      screen->GetDisplayNearestPoint(last_location_);
  if (dst_display.id() !=
      screen->GetDisplayNearestWindow(panel_container_->GetRootWindow()).id()) {
    // The panel is being dragged to a new display. If the previous container is
    // the current parent of the panel it will be informed of the end of drag
    // when the panel is reparented, otherwise let the previous container know
    // the drag is complete. If we told the panel's parent that the drag was
    // complete it would begin positioning the panel.
    if (GetTarget()->parent() != panel_container_)
      GetPanelLayoutManager(panel_container_)->FinishDragging();
    aura::Window* dst_root = Shell::GetInstance()
                                 ->window_tree_host_manager()
                                 ->GetRootWindowForDisplayId(dst_display.id());
    panel_container_ =
        Shell::GetContainer(dst_root, kShellWindowId_PanelContainer);

    // The panel's parent already knows that the drag is in progress for this
    // panel.
    if (panel_container_ && GetTarget()->parent() != panel_container_)
      GetPanelLayoutManager(panel_container_)->StartDragging(GetTarget());
  }
  gfx::Point offset;
  gfx::Rect bounds(CalculateBoundsForDrag(location));
  if (!(details().bounds_change & WindowResizer::kBoundsChange_Resizes)) {
    window_state_->drag_details()->should_attach_to_shelf =
        AttachToLauncher(bounds, &offset);
  }
  gfx::Point modified_location(location.x() + offset.x(),
                               location.y() + offset.y());

  base::WeakPtr<PanelWindowResizer> resizer(weak_ptr_factory_.GetWeakPtr());
  next_window_resizer_->Drag(modified_location, event_flags);
  if (!resizer)
    return;

  if (details().should_attach_to_shelf &&
      !(details().bounds_change & WindowResizer::kBoundsChange_Resizes)) {
    UpdateLauncherPosition();
  }
}

void PanelWindowResizer::CompleteDrag() {
  // The root window can change when dragging into a different screen.
  next_window_resizer_->CompleteDrag();
  FinishDragging();
}

void PanelWindowResizer::RevertDrag() {
  next_window_resizer_->RevertDrag();
  window_state_->drag_details()->should_attach_to_shelf = was_attached_;
  FinishDragging();
}

PanelWindowResizer::PanelWindowResizer(WindowResizer* next_window_resizer,
                                       wm::WindowState* window_state)
    : WindowResizer(window_state),
      next_window_resizer_(next_window_resizer),
      panel_container_(NULL),
      initial_panel_container_(NULL),
      did_move_or_resize_(false),
      was_attached_(window_state->panel_attached()),
      weak_ptr_factory_(this) {
  DCHECK(details().is_resizable);
  panel_container_ = Shell::GetContainer(GetTarget()->GetRootWindow(),
                                         kShellWindowId_PanelContainer);
  initial_panel_container_ = panel_container_;
}

bool PanelWindowResizer::AttachToLauncher(const gfx::Rect& bounds,
                                          gfx::Point* offset) {
  bool should_attach = false;
  if (panel_container_) {
    PanelLayoutManager* panel_layout_manager =
        GetPanelLayoutManager(panel_container_);
    gfx::Rect launcher_bounds = ScreenUtil::ConvertRectFromScreen(
        GetTarget()->parent(),
        panel_layout_manager->shelf()->
        shelf_widget()->GetWindowBoundsInScreen());
    switch (panel_layout_manager->shelf()->alignment()) {
      case SHELF_ALIGNMENT_BOTTOM:
      case SHELF_ALIGNMENT_BOTTOM_LOCKED:
        if (bounds.bottom() >= (launcher_bounds.y() -
                                kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_y(launcher_bounds.y() - bounds.height() - bounds.y());
        }
        break;
      case SHELF_ALIGNMENT_LEFT:
        if (bounds.x() <= (launcher_bounds.right() +
                           kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_x(launcher_bounds.right() - bounds.x());
        }
        break;
      case SHELF_ALIGNMENT_RIGHT:
        if (bounds.right() >= (launcher_bounds.x() -
                               kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_x(launcher_bounds.x() - bounds.width() - bounds.x());
        }
        break;
    }
  }
  return should_attach;
}

void PanelWindowResizer::StartedDragging() {
  // Tell the panel layout manager that we are dragging this panel before
  // attaching it so that it does not get repositioned.
  if (panel_container_)
    GetPanelLayoutManager(panel_container_)->StartDragging(GetTarget());
  if (!was_attached_) {
    // Attach the panel while dragging placing it in front of other panels.
    window_state_->set_panel_attached(true);
    // We use root window coordinates to ensure that during the drag the panel
    // is reparented to a container in the root window that has that window.
    aura::Window* target = GetTarget();
    aura::Window* target_root = target->GetRootWindow();
    aura::Window* old_parent = target->parent();
    aura::client::ParentWindowWithContext(
        target, target_root, target_root->GetBoundsInScreen());
    ash::wm::ReparentTransientChildrenOfChild(
        ash::wm::WmWindowAura::Get(target),
        ash::wm::WmWindowAura::Get(old_parent),
        ash::wm::WmWindowAura::Get(target->parent()));
  }
}

void PanelWindowResizer::FinishDragging() {
  if (!did_move_or_resize_)
    return;
  if (window_state_->panel_attached() != details().should_attach_to_shelf) {
    window_state_->set_panel_attached(details().should_attach_to_shelf);
    // We use last known location to ensure that after the drag the panel
    // is reparented to a container in the root window that has that location.
    aura::Window* target = GetTarget();
    aura::Window* target_root = target->GetRootWindow();
    aura::Window* old_parent = target->parent();
    aura::client::ParentWindowWithContext(
        target, target_root, gfx::Rect(last_location_, gfx::Size()));
    ash::wm::ReparentTransientChildrenOfChild(
        ash::wm::WmWindowAura::Get(target),
        ash::wm::WmWindowAura::Get(old_parent),
        ash::wm::WmWindowAura::Get(target->parent()));
  }

  // If we started the drag in one root window and moved into another root
  // but then canceled the drag we may need to inform the original layout
  // manager that the drag is finished.
  if (initial_panel_container_ != panel_container_)
     GetPanelLayoutManager(initial_panel_container_)->FinishDragging();
  if (panel_container_)
    GetPanelLayoutManager(panel_container_)->FinishDragging();
}

void PanelWindowResizer::UpdateLauncherPosition() {
  if (panel_container_) {
    GetPanelLayoutManager(panel_container_)->shelf()->
        UpdateIconPositionForWindow(GetTarget());
  }
}

}  // namespace aura
