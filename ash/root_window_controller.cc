// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/property_util.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/screen_dimmer.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/toplevel_window_event_filter.h"
#include "ash/wm/visibility_controller.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/root_window.h"

namespace ash {
namespace {

// Creates a new window for use as a container.
aura::Window* CreateContainer(int window_id,
                              const char* name,
                              aura::Window* parent) {
  aura::Window* container = new aura::Window(NULL);
  container->set_id(window_id);
  container->SetName(name);
  container->Init(ui::LAYER_NOT_DRAWN);
  parent->AddChild(container);
  if (window_id != internal::kShellWindowId_UnparentedControlContainer)
    container->Show();
  return container;
}

// Creates each of the special window containers that holds windows of various
// types in the shell UI.
void CreateContainersInRootWindow(aura::RootWindow* root_window) {
  // These containers are just used by PowerButtonController to animate groups
  // of containers simultaneously without messing up the current transformations
  // on those containers.  These are direct children of the root window; all of
  // the other containers are their children.
  aura::Window* non_lock_screen_containers = CreateContainer(
      internal::kShellWindowId_NonLockScreenContainersContainer,
      "NonLockScreenContainersContainer",
      root_window);
  aura::Window* lock_screen_containers = CreateContainer(
      internal::kShellWindowId_LockScreenContainersContainer,
      "LockScreenContainersContainer",
      root_window);
  aura::Window* lock_screen_related_containers = CreateContainer(
      internal::kShellWindowId_LockScreenRelatedContainersContainer,
      "LockScreenRelatedContainersContainer",
      root_window);

  CreateContainer(internal::kShellWindowId_UnparentedControlContainer,
                  "UnparentedControlContainer",
                  non_lock_screen_containers);

  aura::Window* desktop_background_containers = CreateContainer(
      internal::kShellWindowId_DesktopBackgroundContainer,
      "DesktopBackgroundContainer",
      non_lock_screen_containers);
  SetChildWindowVisibilityChangesAnimated(desktop_background_containers);

  aura::Window* default_container = CreateContainer(
      internal::kShellWindowId_DefaultContainer,
      "DefaultContainer",
      non_lock_screen_containers);
  default_container->SetEventFilter(
      new ToplevelWindowEventFilter(default_container));
  SetChildWindowVisibilityChangesAnimated(default_container);

  aura::Window* always_on_top_container = CreateContainer(
      internal::kShellWindowId_AlwaysOnTopContainer,
      "AlwaysOnTopContainer",
      non_lock_screen_containers);
  always_on_top_container->SetEventFilter(
      new ToplevelWindowEventFilter(always_on_top_container));
  SetChildWindowVisibilityChangesAnimated(always_on_top_container);

  CreateContainer(internal::kShellWindowId_PanelContainer,
                  "PanelContainer",
                  non_lock_screen_containers);

  CreateContainer(internal::kShellWindowId_LauncherContainer,
                  "LauncherContainer",
                  non_lock_screen_containers);

  CreateContainer(internal::kShellWindowId_AppListContainer,
                  "AppListContainer",
                  non_lock_screen_containers);

  aura::Window* modal_container = CreateContainer(
      internal::kShellWindowId_SystemModalContainer,
      "SystemModalContainer",
      non_lock_screen_containers);
  modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(modal_container));
  modal_container->SetLayoutManager(
      new internal::SystemModalContainerLayoutManager(modal_container));
  SetChildWindowVisibilityChangesAnimated(modal_container);

  // TODO(beng): Figure out if we can make this use
  // SystemModalContainerEventFilter instead of stops_event_propagation.
  aura::Window* lock_container = CreateContainer(
      internal::kShellWindowId_LockScreenContainer,
      "LockScreenContainer",
      lock_screen_containers);
  lock_container->SetLayoutManager(
      new internal::BaseLayoutManager(root_window));
  // TODO(beng): stopsevents

  aura::Window* lock_modal_container = CreateContainer(
      internal::kShellWindowId_LockSystemModalContainer,
      "LockSystemModalContainer",
      lock_screen_containers);
  lock_modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(lock_modal_container));
  lock_modal_container->SetLayoutManager(
      new internal::SystemModalContainerLayoutManager(lock_modal_container));
  SetChildWindowVisibilityChangesAnimated(lock_modal_container);

  CreateContainer(internal::kShellWindowId_StatusContainer,
                  "StatusContainer",
                  lock_screen_related_containers);

  aura::Window* settings_bubble_container = CreateContainer(
      internal::kShellWindowId_SettingBubbleContainer,
      "SettingBubbleContainer",
      lock_screen_related_containers);
  SetChildWindowVisibilityChangesAnimated(settings_bubble_container);

  aura::Window* menu_container = CreateContainer(
      internal::kShellWindowId_MenuContainer,
      "MenuContainer",
      lock_screen_related_containers);
  SetChildWindowVisibilityChangesAnimated(menu_container);

  aura::Window* drag_drop_container = CreateContainer(
      internal::kShellWindowId_DragImageAndTooltipContainer,
      "DragImageAndTooltipContainer",
      lock_screen_related_containers);
  SetChildWindowVisibilityChangesAnimated(drag_drop_container);

  CreateContainer(internal::kShellWindowId_OverlayContainer,
                  "OverlayContainer",
                  lock_screen_related_containers);
}

}  // namespace

namespace internal {

RootWindowController::RootWindowController(aura::RootWindow* root_window)
    : root_window_(root_window) {
  SetRootWindowController(root_window, this);

  event_client_.reset(new internal::EventClientImpl(root_window));
  screen_dimmer_.reset(new internal::ScreenDimmer(root_window));
}

RootWindowController::~RootWindowController() {
  SetRootWindowController(root_window_.get(), NULL);
  event_client_.reset();
  screen_dimmer_.reset();
  workspace_controller_.reset();
  root_window_.reset();
}

aura::Window* RootWindowController::GetContainer(int container_id) {
  return root_window_->GetChildById(container_id);
}

void RootWindowController::InitLayoutManagers() {
  root_window_layout_ =
      new internal::RootWindowLayoutManager(root_window_.get());
  root_window_->SetLayoutManager(root_window_layout_);

  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);
  // Workspace manager has its own layout managers.
  workspace_controller_.reset(
      new internal::WorkspaceController(default_container));

  aura::Window* always_on_top_container =
      GetContainer(internal::kShellWindowId_AlwaysOnTopContainer);
  always_on_top_container->SetLayoutManager(
      new internal::BaseLayoutManager(
          always_on_top_container->GetRootWindow()));
}

void RootWindowController::CreateContainers() {
  CreateContainersInRootWindow(root_window_.get());
}

void RootWindowController::CloseChildWindows() {
  // Close background widget first as it depends on tooltip.
  root_window_layout_->SetBackgroundWidget(NULL);
  workspace_controller_.reset();
  aura::client::SetTooltipClient(root_window_.get(), NULL);

  while (!root_window_->children().empty()) {
    aura::Window* child = root_window_->children()[0];
    delete child;
  }
}

bool RootWindowController::IsInMaximizedMode() const {
  return workspace_controller_->workspace_manager()->IsInMaximizedMode();
}

}  // namespace internal
}  // namespace ash
