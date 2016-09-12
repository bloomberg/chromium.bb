// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_root_window_controller.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wallpaper/wallpaper_widget_controller.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/root_window_layout_manager.h"
#include "ash/common/wm/system_modal_container_layout_manager.h"
#include "ash/common/wm/workspace/workspace_layout_manager.h"
#include "ash/common/wm/workspace_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/memory/ptr_util.h"

namespace ash {
namespace {

// Creates a new window for use as a container.
WmWindow* CreateContainer(int window_id, const char* name, WmWindow* parent) {
  WmWindow* window = WmShell::Get()->NewWindow(ui::wm::WINDOW_TYPE_UNKNOWN,
                                               ui::LAYER_NOT_DRAWN);
  window->SetShellWindowId(window_id);
  window->SetName(name);
  parent->AddChild(window);
  if (window_id != kShellWindowId_UnparentedControlContainer)
    window->Show();
  return window;
}

}  // namespace

WmRootWindowController::WmRootWindowController(WmWindow* root)
    : root_(root), root_window_layout_manager_(nullptr) {}

WmRootWindowController::~WmRootWindowController() {
  if (animating_wallpaper_widget_controller_.get())
    animating_wallpaper_widget_controller_->StopAnimating();
}

void WmRootWindowController::SetWallpaperWidgetController(
    WallpaperWidgetController* controller) {
  wallpaper_widget_controller_.reset(controller);
}

void WmRootWindowController::SetAnimatingWallpaperWidgetController(
    AnimatingWallpaperWidgetController* controller) {
  if (animating_wallpaper_widget_controller_.get())
    animating_wallpaper_widget_controller_->StopAnimating();
  animating_wallpaper_widget_controller_.reset(controller);
}

wm::WorkspaceWindowState WmRootWindowController::GetWorkspaceWindowState() {
  return workspace_controller_ ? workspace_controller()->GetWindowState()
                               : wm::WORKSPACE_WINDOW_STATE_DEFAULT;
}

SystemModalContainerLayoutManager*
WmRootWindowController::GetSystemModalLayoutManager(WmWindow* window) {
  WmWindow* modal_container = nullptr;
  if (window) {
    WmWindow* window_container = wm::GetContainerForWindow(window);
    if (window_container &&
        window_container->GetShellWindowId() >=
            kShellWindowId_LockScreenContainer) {
      modal_container = GetContainer(kShellWindowId_LockSystemModalContainer);
    } else {
      modal_container = GetContainer(kShellWindowId_SystemModalContainer);
    }
  } else {
    int modal_window_id =
        WmShell::Get()->GetSessionStateDelegate()->IsUserSessionBlocked()
            ? kShellWindowId_LockSystemModalContainer
            : kShellWindowId_SystemModalContainer;
    modal_container = GetContainer(modal_window_id);
  }
  return modal_container ? static_cast<SystemModalContainerLayoutManager*>(
                               modal_container->GetLayoutManager())
                         : nullptr;
}

WmWindow* WmRootWindowController::GetContainer(int container_id) {
  return root_->GetChildByShellWindowId(container_id);
}

const WmWindow* WmRootWindowController::GetContainer(int container_id) const {
  return root_->GetChildByShellWindowId(container_id);
}

void WmRootWindowController::OnInitialWallpaperAnimationStarted() {}

void WmRootWindowController::OnWallpaperAnimationFinished(
    views::Widget* widget) {
  WmShell::Get()->wallpaper_delegate()->OnWallpaperAnimationFinished();
  // Only removes old component when wallpaper animation finished. If we
  // remove the old one before the new wallpaper is done fading in there will
  // be a white flash during the animation.
  if (animating_wallpaper_widget_controller()) {
    WallpaperWidgetController* controller =
        animating_wallpaper_widget_controller()->GetController(true);
    DCHECK_EQ(controller->widget(), widget);
    // Release the old controller and close its wallpaper widget.
    SetWallpaperWidgetController(controller);
  }
}

void WmRootWindowController::CreateContainers() {
  // These containers are just used by PowerButtonController to animate groups
  // of containers simultaneously without messing up the current transformations
  // on those containers. These are direct children of the root window; all of
  // the other containers are their children.

  // The wallpaper container is not part of the lock animation, so it is not
  // included in those animate groups. When the screen is locked, the wallpaper
  // is moved to the lock screen wallpaper container (and moved back on unlock).
  // Ensure that there's an opaque layer occluding the non-lock-screen layers.
  WmWindow* wallpaper_container = CreateContainer(
      kShellWindowId_WallpaperContainer, "WallpaperContainer", root_);
  wallpaper_container->SetChildWindowVisibilityChangesAnimated();

  WmWindow* non_lock_screen_containers =
      CreateContainer(kShellWindowId_NonLockScreenContainersContainer,
                      "NonLockScreenContainersContainer", root_);
  // Clip all windows inside this container, as half pixel of the window's
  // texture may become visible when the screen is scaled. crbug.com/368591.
  non_lock_screen_containers->SetMasksToBounds(true);

  WmWindow* lock_wallpaper_containers =
      CreateContainer(kShellWindowId_LockScreenWallpaperContainer,
                      "LockScreenWallpaperContainer", root_);
  lock_wallpaper_containers->SetChildWindowVisibilityChangesAnimated();

  WmWindow* lock_screen_containers =
      CreateContainer(kShellWindowId_LockScreenContainersContainer,
                      "LockScreenContainersContainer", root_);
  WmWindow* lock_screen_related_containers =
      CreateContainer(kShellWindowId_LockScreenRelatedContainersContainer,
                      "LockScreenRelatedContainersContainer", root_);

  CreateContainer(kShellWindowId_UnparentedControlContainer,
                  "UnparentedControlContainer", non_lock_screen_containers);

  WmWindow* default_container =
      CreateContainer(kShellWindowId_DefaultContainer, "DefaultContainer",
                      non_lock_screen_containers);
  default_container->SetChildWindowVisibilityChangesAnimated();
  default_container->SetSnapsChildrenToPhysicalPixelBoundary();
  default_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
  default_container->SetChildrenUseExtendedHitRegion();

  WmWindow* always_on_top_container =
      CreateContainer(kShellWindowId_AlwaysOnTopContainer,
                      "AlwaysOnTopContainer", non_lock_screen_containers);
  always_on_top_container->SetChildWindowVisibilityChangesAnimated();
  always_on_top_container->SetSnapsChildrenToPhysicalPixelBoundary();
  always_on_top_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);

  WmWindow* docked_container =
      CreateContainer(kShellWindowId_DockedContainer, "DockedContainer",
                      non_lock_screen_containers);
  docked_container->SetChildWindowVisibilityChangesAnimated();
  docked_container->SetSnapsChildrenToPhysicalPixelBoundary();
  docked_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
  docked_container->SetChildrenUseExtendedHitRegion();

  WmWindow* shelf_container =
      CreateContainer(kShellWindowId_ShelfContainer, "ShelfContainer",
                      non_lock_screen_containers);
  shelf_container->SetSnapsChildrenToPhysicalPixelBoundary();
  shelf_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
  shelf_container->SetDescendantsStayInSameRootWindow(true);

  WmWindow* panel_container =
      CreateContainer(kShellWindowId_PanelContainer, "PanelContainer",
                      non_lock_screen_containers);
  panel_container->SetSnapsChildrenToPhysicalPixelBoundary();
  panel_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);

  WmWindow* shelf_bubble_container =
      CreateContainer(kShellWindowId_ShelfBubbleContainer,
                      "ShelfBubbleContainer", non_lock_screen_containers);
  shelf_bubble_container->SetSnapsChildrenToPhysicalPixelBoundary();
  shelf_bubble_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
  shelf_bubble_container->SetDescendantsStayInSameRootWindow(true);

  WmWindow* app_list_container =
      CreateContainer(kShellWindowId_AppListContainer, "AppListContainer",
                      non_lock_screen_containers);
  app_list_container->SetSnapsChildrenToPhysicalPixelBoundary();
  app_list_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);

  WmWindow* modal_container =
      CreateContainer(kShellWindowId_SystemModalContainer,
                      "SystemModalContainer", non_lock_screen_containers);
  modal_container->SetSnapsChildrenToPhysicalPixelBoundary();
  modal_container->SetChildWindowVisibilityChangesAnimated();
  modal_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
  modal_container->SetChildrenUseExtendedHitRegion();

  // TODO(beng): Figure out if we can make this use
  // SystemModalContainerEventFilter instead of stops_event_propagation.
  WmWindow* lock_container =
      CreateContainer(kShellWindowId_LockScreenContainer, "LockScreenContainer",
                      lock_screen_containers);
  lock_container->SetSnapsChildrenToPhysicalPixelBoundary();
  lock_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
  // TODO(beng): stopsevents

  WmWindow* lock_modal_container =
      CreateContainer(kShellWindowId_LockSystemModalContainer,
                      "LockSystemModalContainer", lock_screen_containers);
  lock_modal_container->SetSnapsChildrenToPhysicalPixelBoundary();
  lock_modal_container->SetChildWindowVisibilityChangesAnimated();
  lock_modal_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
  lock_modal_container->SetChildrenUseExtendedHitRegion();

  WmWindow* status_container =
      CreateContainer(kShellWindowId_StatusContainer, "StatusContainer",
                      lock_screen_related_containers);
  status_container->SetSnapsChildrenToPhysicalPixelBoundary();
  status_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
  status_container->SetDescendantsStayInSameRootWindow(true);

  WmWindow* settings_bubble_container =
      CreateContainer(kShellWindowId_SettingBubbleContainer,
                      "SettingBubbleContainer", lock_screen_related_containers);
  settings_bubble_container->SetChildWindowVisibilityChangesAnimated();
  settings_bubble_container->SetSnapsChildrenToPhysicalPixelBoundary();
  settings_bubble_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
  settings_bubble_container->SetDescendantsStayInSameRootWindow(true);

  WmWindow* virtual_keyboard_parent_container = CreateContainer(
      kShellWindowId_ImeWindowParentContainer, "VirtualKeyboardParentContainer",
      lock_screen_related_containers);
  virtual_keyboard_parent_container->SetSnapsChildrenToPhysicalPixelBoundary();
  virtual_keyboard_parent_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);

  WmWindow* menu_container =
      CreateContainer(kShellWindowId_MenuContainer, "MenuContainer",
                      lock_screen_related_containers);
  menu_container->SetChildWindowVisibilityChangesAnimated();
  menu_container->SetSnapsChildrenToPhysicalPixelBoundary();
  menu_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);

  WmWindow* drag_drop_container = CreateContainer(
      kShellWindowId_DragImageAndTooltipContainer,
      "DragImageAndTooltipContainer", lock_screen_related_containers);
  drag_drop_container->SetChildWindowVisibilityChangesAnimated();
  drag_drop_container->SetSnapsChildrenToPhysicalPixelBoundary();
  drag_drop_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);

  WmWindow* overlay_container =
      CreateContainer(kShellWindowId_OverlayContainer, "OverlayContainer",
                      lock_screen_related_containers);
  overlay_container->SetSnapsChildrenToPhysicalPixelBoundary();
  overlay_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);

#if defined(OS_CHROMEOS)
  WmWindow* mouse_cursor_container = CreateContainer(
      kShellWindowId_MouseCursorContainer, "MouseCursorContainer", root_);
  mouse_cursor_container->SetBoundsInScreenBehaviorForChildren(
      WmWindow::BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
#endif

  CreateContainer(kShellWindowId_PowerButtonAnimationContainer,
                  "PowerButtonAnimationContainer", root_);
}

void WmRootWindowController::CreateLayoutManagers() {
  root_window_layout_manager_ = new wm::RootWindowLayoutManager(root_);
  root_->SetLayoutManager(base::WrapUnique(root_window_layout_manager_));

  WmWindow* default_container = GetContainer(kShellWindowId_DefaultContainer);
  workspace_controller_.reset(new WorkspaceController(default_container));

  WmWindow* modal_container = GetContainer(kShellWindowId_SystemModalContainer);
  DCHECK(modal_container);
  modal_container->SetLayoutManager(
      base::MakeUnique<SystemModalContainerLayoutManager>(modal_container));

  WmWindow* lock_modal_container =
      GetContainer(kShellWindowId_LockSystemModalContainer);
  DCHECK(lock_modal_container);
  lock_modal_container->SetLayoutManager(
      base::MakeUnique<SystemModalContainerLayoutManager>(
          lock_modal_container));
}

void WmRootWindowController::DeleteWorkspaceController() {
  workspace_controller_.reset();
}

}  // namespace ash
