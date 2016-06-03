// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/root_window_controller_common.h"

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/root_window_layout_manager.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/memory/ptr_util.h"

namespace ash {
namespace {

// Creates a new window for use as a container.
WmWindow* CreateContainer(int window_id, const char* name, WmWindow* parent) {
  WmWindow* window = WmShell::Get()->NewContainerWindow();
  window->SetShellWindowId(window_id);
  window->SetName(name);
  parent->AddChild(window);
  if (window_id != kShellWindowId_UnparentedControlContainer)
    window->Show();
  return window;
}

}  // namespace

RootWindowControllerCommon::RootWindowControllerCommon(WmWindow* root)
    : root_(root), root_window_layout_(nullptr) {}

RootWindowControllerCommon::~RootWindowControllerCommon() {}

void RootWindowControllerCommon::CreateContainers() {
  // These containers are just used by PowerButtonController to animate groups
  // of containers simultaneously without messing up the current transformations
  // on those containers. These are direct children of the root window; all of
  // the other containers are their children.

  // The desktop background container is not part of the lock animation, so it
  // is not included in those animate groups.
  // When screen is locked desktop background is moved to lock screen background
  // container (moved back on unlock). We want to make sure that there's an
  // opaque layer occluding the non-lock-screen layers.
  WmWindow* desktop_background_container =
      CreateContainer(kShellWindowId_DesktopBackgroundContainer,
                      "DesktopBackgroundContainer", root_);
  desktop_background_container->SetChildWindowVisibilityChangesAnimated();

  WmWindow* non_lock_screen_containers =
      CreateContainer(kShellWindowId_NonLockScreenContainersContainer,
                      "NonLockScreenContainersContainer", root_);
  // Clip all windows inside this container, as half pixel of the window's
  // texture may become visible when the screen is scaled. crbug.com/368591.
  non_lock_screen_containers->SetMasksToBounds(true);

  WmWindow* lock_background_containers =
      CreateContainer(kShellWindowId_LockScreenBackgroundContainer,
                      "LockScreenBackgroundContainer", root_);
  lock_background_containers->SetChildWindowVisibilityChangesAnimated();

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

void RootWindowControllerCommon::CreateLayoutManagers() {
  root_window_layout_ = new wm::RootWindowLayoutManager(root_);
  root_->SetLayoutManager(base::WrapUnique(root_window_layout_));
}

}  // namespace ash
