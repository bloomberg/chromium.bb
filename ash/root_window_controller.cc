// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <vector>

#include "ash/display/display_controller.h"
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
#include "ash/wm/window_properties.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

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

void MoveAllWindows(aura::RootWindow* src,
                    aura::RootWindow* dst) {
  // Windows move only from secondary displays to the primary
  // display, so no need to move windows in the containers that are
  // available only in the primary display (launcher, panels etc)
  const int kContainerIdsToMove[] = {
    internal::kShellWindowId_DefaultContainer,
    internal::kShellWindowId_AlwaysOnTopContainer,
    internal::kShellWindowId_SystemModalContainer,
    internal::kShellWindowId_LockSystemModalContainer,
  };
  const gfx::Point src_origin =
      gfx::Screen::GetDisplayNearestWindow(src).bounds().origin();
  const gfx::Point dst_origin =
      gfx::Screen::GetDisplayNearestWindow(src).bounds().origin();

  for (size_t i = 0; i < arraysize(kContainerIdsToMove); i++) {
    int id = kContainerIdsToMove[i];
    aura::Window* src_container = Shell::GetContainer(src, id);
    aura::Window* dst_container = Shell::GetContainer(dst, id);
    aura::Window::Windows children = src_container->children();
    for (aura::Window::Windows::iterator iter = children.begin();
         iter != children.end(); ++iter) {
      aura::Window* window = *iter;
      // Don't move modal screen.
      if ((id == internal::kShellWindowId_SystemModalContainer ||
           id == internal::kShellWindowId_LockSystemModalContainer) &&
          window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_NONE) {
        continue;
      }
      // Update the restore bounds to make it relative to the display.
      gfx::Rect restore_bounds;
      if (internal::DisplayController::IsVirtualScreenCoordinatesEnabled())
        restore_bounds = GetRestoreBoundsInParent(window);
      dst_container->AddChild(window);
      if (!restore_bounds.IsEmpty())
        SetRestoreBoundsInParent(window, restore_bounds);
    }
  }
}

// Mark the container window so that a widget added to this container will
// use the virtual screeen coordinates instead of parent.
void SetUsesScreenCoordinates(aura::Window* container) {
  container->SetProperty(internal::kUsesScreenCoordinatesKey, true);
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
  SetUsesScreenCoordinates(default_container);

  aura::Window* always_on_top_container = CreateContainer(
      internal::kShellWindowId_AlwaysOnTopContainer,
      "AlwaysOnTopContainer",
      non_lock_screen_containers);
  always_on_top_container->SetEventFilter(
      new ToplevelWindowEventFilter(always_on_top_container));
  SetChildWindowVisibilityChangesAnimated(always_on_top_container);
  SetUsesScreenCoordinates(always_on_top_container);

  aura::Window* panel_container = CreateContainer(
      internal::kShellWindowId_PanelContainer,
      "PanelContainer",
      non_lock_screen_containers);
  SetUsesScreenCoordinates(panel_container);

  aura::Window* launcher_container =
      CreateContainer(internal::kShellWindowId_LauncherContainer,
                      "LauncherContainer",
                      non_lock_screen_containers);
  SetUsesScreenCoordinates(launcher_container);

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
  SetUsesScreenCoordinates(modal_container);

  aura::Window* input_method_container = CreateContainer(
      internal::kShellWindowId_InputMethodContainer,
      "InputMethodContainer",
      non_lock_screen_containers);
  SetUsesScreenCoordinates(input_method_container);

  // TODO(beng): Figure out if we can make this use
  // SystemModalContainerEventFilter instead of stops_event_propagation.
  aura::Window* lock_container = CreateContainer(
      internal::kShellWindowId_LockScreenContainer,
      "LockScreenContainer",
      lock_screen_containers);
  lock_container->SetLayoutManager(
      new internal::BaseLayoutManager(root_window));
  SetUsesScreenCoordinates(lock_container);
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
  SetUsesScreenCoordinates(lock_modal_container);

  CreateContainer(internal::kShellWindowId_StatusContainer,
                  "StatusContainer",
                  lock_screen_related_containers);

  aura::Window* settings_bubble_container = CreateContainer(
      internal::kShellWindowId_SettingBubbleContainer,
      "SettingBubbleContainer",
      lock_screen_related_containers);
  SetChildWindowVisibilityChangesAnimated(settings_bubble_container);
  SetUsesScreenCoordinates(settings_bubble_container);

  aura::Window* menu_container = CreateContainer(
      internal::kShellWindowId_MenuContainer,
      "MenuContainer",
      lock_screen_related_containers);
  SetChildWindowVisibilityChangesAnimated(menu_container);
  SetUsesScreenCoordinates(menu_container);

  aura::Window* drag_drop_container = CreateContainer(
      internal::kShellWindowId_DragImageAndTooltipContainer,
      "DragImageAndTooltipContainer",
      lock_screen_related_containers);
  SetChildWindowVisibilityChangesAnimated(drag_drop_container);
  SetUsesScreenCoordinates(drag_drop_container);

  aura::Window* overlay_container = CreateContainer(
      internal::kShellWindowId_OverlayContainer,
      "OverlayContainer",
      lock_screen_related_containers);
  SetUsesScreenCoordinates(overlay_container);
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

void RootWindowController::MoveWindowsTo(aura::RootWindow* dst) {
  aura::Window* focused = dst->GetFocusManager()->GetFocusedWindow();
  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(dst);
  aura::Window* active = activation_client->GetActiveWindow();
  // Deactivate the window to close menu / bubble windows.
  activation_client->DeactivateWindow(active);
  // Release capture if any.
  aura::client::GetCaptureClient(root_window_.get())->
      SetCapture(NULL);
  aura::WindowTracker tracker;
  if (focused)
    tracker.Add(focused);
  if (active && focused != active)
    tracker.Add(active);

  MoveAllWindows(root_window_.get(), dst);

  // Restore focused or active window if it's still alive.
  if (focused && tracker.Contains(focused) && dst->Contains(focused)) {
    dst->GetFocusManager()->SetFocusedWindow(focused, NULL);
  } else if (active && tracker.Contains(active) && dst->Contains(active)) {
    activation_client->ActivateWindow(active);
  }
}

}  // namespace internal
}  // namespace ash
