// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <vector>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/focus_cycler.h"
#include "ash/shelf_types.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/boot_splash_screen.h"
#include "ash/wm/panel_layout_manager.h"
#include "ash/wm/panel_window_event_filter.h"
#include "ash/wm/property_util.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/screen_dimmer.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/system_background_controller.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/time.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/corewm/visibility_controller.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"

namespace ash {
namespace {

#if defined(OS_CHROMEOS)
// Background color used for the Chrome OS boot splash screen.
const SkColor kChromeOsBootColor = SkColorSetARGB(0xff, 0xfe, 0xfe, 0xfe);
#endif

// Duration for the animation that hides the boot splash screen, in
// milliseconds.  This should be short enough in relation to
// wm/window_animation.cc's brightness/grayscale fade animation that the login
// background image animation isn't hidden by the splash screen animation.
const int kBootSplashScreenHideDurationMs = 500;

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

// Returns all the children of the workspace windows, eg the standard top-level
// windows.
std::vector<aura::Window*> GetWorkspaceWindows(aura::RootWindow* root) {
  using aura::Window;

  std::vector<Window*> windows;
  Window* container = Shell::GetContainer(
      root, internal::kShellWindowId_DefaultContainer);
  for (Window::Windows::const_reverse_iterator i =
           container->children().rbegin();
       i != container->children().rend(); ++i) {
    Window* workspace_window = *i;
    if (workspace_window->id() == internal::kShellWindowId_WorkspaceContainer) {
      windows.insert(windows.end(), workspace_window->children().begin(),
                     workspace_window->children().end());
    }
  }
  return windows;
}

// Reparents |window| to |new_parent|.
void ReparentWindow(aura::Window* window, aura::Window* new_parent) {
  // Update the restore bounds to make it relative to the display.
  gfx::Rect restore_bounds(GetRestoreBoundsInParent(window));
  new_parent->AddChild(window);
  if (!restore_bounds.IsEmpty())
    SetRestoreBoundsInParent(window, restore_bounds);
}

// Reparents the appropriate set of windows from |src| to |dst|.
void ReparentAllWindows(aura::RootWindow* src, aura::RootWindow* dst) {
  // Set of windows to move.
  const int kContainerIdsToMove[] = {
    internal::kShellWindowId_DefaultContainer,
    internal::kShellWindowId_AlwaysOnTopContainer,
    internal::kShellWindowId_SystemModalContainer,
    internal::kShellWindowId_LockSystemModalContainer,
    internal::kShellWindowId_InputMethodContainer,
  };
  // For workspace windows we need to manually reparent the windows. This way
  // workspace can move the windows to the appropriate workspace.
  std::vector<aura::Window*> windows(GetWorkspaceWindows(src));
  internal::WorkspaceController* workspace_controller =
      GetRootWindowController(dst)->workspace_controller();
  for (size_t i = 0; i < windows.size(); ++i) {
    aura::Window* new_parent =
        workspace_controller->GetParentForNewWindow(windows[i]);
    ReparentWindow(windows[i], new_parent);
  }
  for (size_t i = 0; i < arraysize(kContainerIdsToMove); i++) {
    int id = kContainerIdsToMove[i];
    if (id == internal::kShellWindowId_DefaultContainer)
      continue;

    aura::Window* src_container = Shell::GetContainer(src, id);
    aura::Window* dst_container = Shell::GetContainer(dst, id);
    aura::Window::Windows children = src_container->children();
    for (aura::Window::Windows::iterator iter = children.begin();
         iter != children.end(); ++iter) {
      aura::Window* window = *iter;
      // Don't move modal screen.
      if (internal::SystemModalContainerLayoutManager::IsModalBackground(
              window))
        continue;

      ReparentWindow(window, dst_container);
    }
  }
}

// Mark the container window so that a widget added to this container will
// use the virtual screeen coordinates instead of parent.
void SetUsesScreenCoordinates(aura::Window* container) {
  container->SetProperty(internal::kUsesScreenCoordinatesKey, true);
}

}  // namespace

namespace internal {

RootWindowController::RootWindowController(aura::RootWindow* root_window)
    : root_window_(root_window),
      root_window_layout_(NULL),
      status_area_widget_(NULL),
      shelf_(NULL),
      panel_layout_manager_(NULL) {
  SetRootWindowController(root_window, this);
  screen_dimmer_.reset(new ScreenDimmer(root_window));
}

RootWindowController::~RootWindowController() {
  Shutdown();
  root_window_.reset();
}

// static
RootWindowController* RootWindowController::ForLauncher(aura::Window* window) {
  if (Shell::IsLauncherPerDisplayEnabled())
    return GetRootWindowController(window->GetRootWindow());
  else
    return Shell::GetPrimaryRootWindowController();
}

// static
RootWindowController* RootWindowController::ForWindow(aura::Window* window) {
  return GetRootWindowController(window->GetRootWindow());
}

// static
RootWindowController* RootWindowController::ForActiveRootWindow() {
  return GetRootWindowController(Shell::GetActiveRootWindow());
}

void RootWindowController::Shutdown() {
  CloseChildWindows();
  if (Shell::GetActiveRootWindow() == root_window_.get()) {
    Shell::GetInstance()->set_active_root_window(
        Shell::GetPrimaryRootWindow() == root_window_.get() ?
        NULL : Shell::GetPrimaryRootWindow());
  }
  SetRootWindowController(root_window_.get(), NULL);
  screen_dimmer_.reset();
  workspace_controller_.reset();
  // Forget with the display ID so that display lookup
  // ends up with invalid display.
  root_window_->ClearProperty(kDisplayIdKey);
  // And this root window should no longer process events.
  root_window_->PrepareForShutdown();

  system_background_.reset();

  // Launcher widget has an InputMethodBridge that references to
  // |input_method_filter_|'s |input_method_|. So explicitly release
  // |launcher_| before |input_method_filter_|. And this needs to be
  // after we delete all containers in case there are still live
  // browser windows which access LauncherModel during close.
  launcher_.reset();
}

SystemModalContainerLayoutManager*
RootWindowController::GetSystemModalLayoutManager(aura::Window* window) {
  aura::Window* container = NULL;
  if (window) {
    if (window->parent() &&
        window->parent()->id() >= kShellWindowId_LockScreenContainer) {
      container = GetContainer(kShellWindowId_LockSystemModalContainer);
    } else {
      container = GetContainer(kShellWindowId_SystemModalContainer);
    }
  } else {
    user::LoginStatus login = Shell::GetInstance()->system_tray_delegate() ?
        Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus() :
        user::LOGGED_IN_NONE;
    int modal_window_id = (login == user::LOGGED_IN_LOCKED ||
                           login == user::LOGGED_IN_NONE) ?
        kShellWindowId_LockSystemModalContainer :
        kShellWindowId_SystemModalContainer;
    container = GetContainer(modal_window_id);
  }
  return static_cast<SystemModalContainerLayoutManager*>(
      container->layout_manager());
}

aura::Window* RootWindowController::GetContainer(int container_id) {
  return root_window_->GetChildById(container_id);
}

void RootWindowController::InitLayoutManagers() {
  root_window_layout_ =
      new RootWindowLayoutManager(root_window_.get());
  root_window_->SetLayoutManager(root_window_layout_);

  aura::Window* default_container =
      GetContainer(kShellWindowId_DefaultContainer);
  // Workspace manager has its own layout managers.
  workspace_controller_.reset(
      new WorkspaceController(default_container));

  aura::Window* always_on_top_container =
      GetContainer(kShellWindowId_AlwaysOnTopContainer);
  always_on_top_container->SetLayoutManager(
      new BaseLayoutManager(
          always_on_top_container->GetRootWindow()));
}

void RootWindowController::InitForPrimaryDisplay() {
  DCHECK(!status_area_widget_);
  aura::Window* status_container =
      GetContainer(ash::internal::kShellWindowId_StatusContainer);
  // Initialize Primary RootWindow specific items.
  status_area_widget_ = new internal::StatusAreaWidget(status_container);
  status_area_widget_->CreateTrayViews();
  // Login screen manages status area visibility by itself.
  ShellDelegate* shell_delegate = Shell::GetInstance()->delegate();
  if (shell_delegate->IsSessionStarted())
    status_area_widget_->Show();

  Shell::GetInstance()->focus_cycler()->AddWidget(status_area_widget_);

  internal::ShelfLayoutManager* shelf_layout_manager =
      new internal::ShelfLayoutManager(status_area_widget_);
  GetContainer(internal::kShellWindowId_LauncherContainer)->
      SetLayoutManager(shelf_layout_manager);
  shelf_ = shelf_layout_manager;

  internal::StatusAreaLayoutManager* status_area_layout_manager =
      new internal::StatusAreaLayoutManager(shelf_layout_manager);
  GetContainer(internal::kShellWindowId_StatusContainer)->
      SetLayoutManager(status_area_layout_manager);

  shelf_layout_manager->set_workspace_controller(
      workspace_controller());

  workspace_controller()->SetShelf(shelf_);

  // TODO(oshima): Disable panels on non primary display for now.
  // crbug.com/166195.
  if (root_window_ == Shell::GetPrimaryRootWindow()) {
    // Create Panel layout manager
    aura::Window* panel_container = GetContainer(
        internal::kShellWindowId_PanelContainer);
    panel_layout_manager_ =
        new internal::PanelLayoutManager(panel_container);
    panel_container->AddPreTargetHandler(
        new internal::PanelWindowEventFilter(
            panel_container, panel_layout_manager_));
    panel_container->SetLayoutManager(panel_layout_manager_);
  }

  if (shell_delegate->IsUserLoggedIn())
    CreateLauncher();
}

void RootWindowController::CreateContainers() {
  CreateContainersInRootWindow(root_window_.get());
}

void RootWindowController::CreateSystemBackground(
    bool is_first_run_after_boot) {
  SkColor color = SK_ColorBLACK;
#if defined(OS_CHROMEOS)
  if (is_first_run_after_boot)
    color = kChromeOsBootColor;
#endif
  system_background_.reset(
      new SystemBackgroundController(root_window_.get(), color));

#if defined(OS_CHROMEOS)
  // Make a copy of the system's boot splash screen so we can composite it
  // onscreen until the desktop background is ready.
  if (is_first_run_after_boot &&
      (CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kAshCopyHostBackgroundAtBoot) ||
       CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kAshAnimateFromBootSplashScreen)))
    boot_splash_screen_.reset(new BootSplashScreen(root_window_.get()));
#endif
}

void RootWindowController::CreateLauncher() {
  if (launcher_.get())
    return;

  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);
  // Get the delegate first to make sure the launcher model is created.
  LauncherDelegate* launcher_delegate =
      Shell::GetInstance()->GetLauncherDelegate();
  launcher_.reset(new Launcher(Shell::GetInstance()->launcher_model(),
                               launcher_delegate,
                               default_container,
                               shelf_));

  launcher_->SetFocusCycler(Shell::GetInstance()->focus_cycler());
  shelf_->SetLauncher(launcher_.get());

  if (panel_layout_manager_)
    panel_layout_manager_->SetLauncher(launcher_.get());

  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  if (delegate)
    launcher_->SetVisible(delegate->IsSessionStarted());
  launcher_->widget()->Show();
}

void RootWindowController::ShowLauncher() {
  if (!launcher_.get())
    return;
  launcher_->SetVisible(true);
  status_area_widget_->Show();
}

void RootWindowController::OnLoginStateChanged(user::LoginStatus status) {
  // TODO(oshima): remove if when launcher per display is enabled by
  // default.
  if (shelf_)
    shelf_->UpdateVisibilityState();
}

void RootWindowController::UpdateAfterLoginStatusChange(
    user::LoginStatus status) {
  if (status_area_widget_)
    status_area_widget_->UpdateAfterLoginStatusChange(status);
}

void RootWindowController::HandleInitialDesktopBackgroundAnimationStarted() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshAnimateFromBootSplashScreen) &&
      boot_splash_screen_.get()) {
    // Make the splash screen fade out so it doesn't obscure the desktop
    // wallpaper's brightness/grayscale animation.
    boot_splash_screen_->StartHideAnimation(
        base::TimeDelta::FromMilliseconds(kBootSplashScreenHideDurationMs));
  }
}

void RootWindowController::HandleDesktopBackgroundVisible() {
  system_background_->SetColor(SK_ColorBLACK);
  boot_splash_screen_.reset();
}

void RootWindowController::CloseChildWindows() {
  // The status area needs to be shut down before the windows are destroyed.
  if (status_area_widget_) {
    status_area_widget_->Shutdown();
    status_area_widget_ = NULL;
  }

  // Closing the windows frees the workspace controller.
  if (shelf_)
    shelf_->set_workspace_controller(NULL);

  // Close background widget first as it depends on tooltip.
  root_window_->SetProperty(kDesktopController,
      static_cast<DesktopBackgroundWidgetController*>(NULL));
  root_window_->SetProperty(kAnimatingDesktopController,
                            static_cast<AnimatingDesktopController*>(NULL));

  workspace_controller_.reset();
  aura::client::SetTooltipClient(root_window_.get(), NULL);

  while (!root_window_->children().empty()) {
    aura::Window* child = root_window_->children()[0];
    delete child;
  }
  launcher_.reset();
  // All containers are deleted, so reset shelf_.
  shelf_ = NULL;
}

void RootWindowController::MoveWindowsTo(aura::RootWindow* dst) {
  aura::Window* focused = aura::client::GetFocusClient(dst)->GetFocusedWindow();
  aura::WindowTracker tracker;
  if (focused)
    tracker.Add(focused);
  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(dst);
  aura::Window* active = activation_client->GetActiveWindow();
  if (active && focused != active)
    tracker.Add(active);
  // Deactivate the window to close menu / bubble windows.
  activation_client->DeactivateWindow(active);
  // Release capture if any.
  aura::client::GetCaptureClient(root_window_.get())->
      SetCapture(NULL);
  // Clear the focused window if any. This is necessary because a
  // window may be deleted when losing focus (fullscreen flash for
  // example).  If the focused window is still alive after move, it'll
  // be re-focused below.
  aura::client::GetFocusClient(dst)->FocusWindow(NULL);

  // Forget the shelf early so that shelf don't update itself using wrong
  // display info.
  workspace_controller_->SetShelf(NULL);

  ReparentAllWindows(root_window_.get(), dst);

  // Restore focused or active window if it's still alive.
  if (focused && tracker.Contains(focused) && dst->Contains(focused)) {
    aura::client::GetFocusClient(dst)->FocusWindow(focused);
  } else if (active && tracker.Contains(active) && dst->Contains(active)) {
    activation_client->ActivateWindow(active);
  }
}

SystemTray* RootWindowController::GetSystemTray() {
  // We assume in throughout the code that this will not return NULL. If code
  // triggers this for valid reasons, it should test status_area_widget first.
  internal::StatusAreaWidget* status_area = status_area_widget();
  CHECK(status_area);
  return status_area->system_tray();
}

void RootWindowController::ShowContextMenu(
    const gfx::Point& location_in_screen) {
  aura::RootWindow* target = Shell::IsLauncherPerDisplayEnabled() ?
      root_window() : Shell::GetPrimaryRootWindow();
  DCHECK(Shell::GetInstance()->delegate());
  scoped_ptr<ui::MenuModel> menu_model(
      Shell::GetInstance()->delegate()->CreateContextMenu(target));

  views::MenuModelAdapter menu_model_adapter(menu_model.get());
  views::MenuRunner menu_runner(menu_model_adapter.CreateMenu());
  views::Widget* widget =
      root_window_->GetProperty(kDesktopController)->widget();

  if (menu_runner.RunMenuAt(
          widget, NULL, gfx::Rect(location_in_screen, gfx::Size()),
          views::MenuItemView::TOPLEFT, views::MenuRunner::CONTEXT_MENU) ==
      views::MenuRunner::MENU_DELETED)
    return;

  Shell::GetInstance()->UpdateShelfVisibility();
}

void RootWindowController::UpdateShelfVisibility() {
  shelf_->UpdateVisibilityState();
}

void RootWindowController::SetShelfAutoHideBehavior(
    ShelfAutoHideBehavior behavior) {
  shelf_->SetAutoHideBehavior(behavior);
}

ShelfAutoHideBehavior RootWindowController::GetShelfAutoHideBehavior() const {
  return shelf_->auto_hide_behavior();
}

bool RootWindowController::SetShelfAlignment(ShelfAlignment alignment) {
  return shelf_->SetAlignment(alignment);
}

ShelfAlignment RootWindowController::GetShelfAlignment() {
  return shelf_->GetAlignment();
}

bool RootWindowController::IsImmersiveMode() const {
  aura::Window* container = workspace_controller_->GetActiveWorkspaceWindow();
  for (size_t i = 0; i < container->children().size(); ++i) {
    aura::Window* child = container->children()[i];
    if (child->IsVisible() && child->GetProperty(kImmersiveModeKey))
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowController, private:

void RootWindowController::CreateContainersInRootWindow(
    aura::RootWindow* root_window) {
  // These containers are just used by PowerButtonController to animate groups
  // of containers simultaneously without messing up the current transformations
  // on those containers. These are direct children of the root window; all of
  // the other containers are their children.

  // The desktop background container is not part of the lock animation, so it
  // is not included in those animate groups.
  // When screen is locked desktop background is moved to lock screen background
  // container (moved back on unlock). We want to make sure that there's an
  // opaque layer occluding the non-lock-screen layers.
  aura::Window* desktop_background_container = CreateContainer(
      kShellWindowId_DesktopBackgroundContainer,
      "DesktopBackgroundContainer",
      root_window);
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      desktop_background_container);

  aura::Window* non_lock_screen_containers = CreateContainer(
      kShellWindowId_NonLockScreenContainersContainer,
      "NonLockScreenContainersContainer",
      root_window);

  aura::Window* lock_background_containers = CreateContainer(
      kShellWindowId_LockScreenBackgroundContainer,
      "LockScreenBackgroundContainer",
      root_window);
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      lock_background_containers);

  aura::Window* lock_screen_containers = CreateContainer(
      kShellWindowId_LockScreenContainersContainer,
      "LockScreenContainersContainer",
      root_window);
  aura::Window* lock_screen_related_containers = CreateContainer(
      kShellWindowId_LockScreenRelatedContainersContainer,
      "LockScreenRelatedContainersContainer",
      root_window);

  CreateContainer(kShellWindowId_UnparentedControlContainer,
                  "UnparentedControlContainer",
                  non_lock_screen_containers);

  aura::Window* default_container = CreateContainer(
      kShellWindowId_DefaultContainer,
      "DefaultContainer",
      non_lock_screen_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(default_container);
  SetUsesScreenCoordinates(default_container);

  aura::Window* always_on_top_container = CreateContainer(
      kShellWindowId_AlwaysOnTopContainer,
      "AlwaysOnTopContainer",
      non_lock_screen_containers);
  always_on_top_container_handler_.reset(
      new ToplevelWindowEventHandler(always_on_top_container));
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      always_on_top_container);
  SetUsesScreenCoordinates(always_on_top_container);

  aura::Window* panel_container = CreateContainer(
      kShellWindowId_PanelContainer,
      "PanelContainer",
      non_lock_screen_containers);
  SetUsesScreenCoordinates(panel_container);

  aura::Window* launcher_container =
      CreateContainer(kShellWindowId_LauncherContainer,
                      "LauncherContainer",
                      non_lock_screen_containers);
  SetUsesScreenCoordinates(launcher_container);

  aura::Window* app_list_container =
      CreateContainer(kShellWindowId_AppListContainer,
                      "AppListContainer",
                      non_lock_screen_containers);
  SetUsesScreenCoordinates(app_list_container);

  aura::Window* modal_container = CreateContainer(
      kShellWindowId_SystemModalContainer,
      "SystemModalContainer",
      non_lock_screen_containers);
  modal_container_handler_.reset(
      new ToplevelWindowEventHandler(modal_container));
  modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(modal_container));
  views::corewm::SetChildWindowVisibilityChangesAnimated(modal_container);
  SetUsesScreenCoordinates(modal_container);

  aura::Window* input_method_container = CreateContainer(
      kShellWindowId_InputMethodContainer,
      "InputMethodContainer",
      non_lock_screen_containers);
  SetUsesScreenCoordinates(input_method_container);

  // TODO(beng): Figure out if we can make this use
  // SystemModalContainerEventFilter instead of stops_event_propagation.
  aura::Window* lock_container = CreateContainer(
      kShellWindowId_LockScreenContainer,
      "LockScreenContainer",
      lock_screen_containers);
  lock_container->SetLayoutManager(
      new BaseLayoutManager(root_window));
  SetUsesScreenCoordinates(lock_container);
  // TODO(beng): stopsevents

  aura::Window* lock_modal_container = CreateContainer(
      kShellWindowId_LockSystemModalContainer,
      "LockSystemModalContainer",
      lock_screen_containers);
  lock_modal_container_handler_.reset(
      new ToplevelWindowEventHandler(lock_modal_container));
  lock_modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(lock_modal_container));
  views::corewm::SetChildWindowVisibilityChangesAnimated(lock_modal_container);
  SetUsesScreenCoordinates(lock_modal_container);

  aura::Window* status_container =
      CreateContainer(kShellWindowId_StatusContainer,
                      "StatusContainer",
                      lock_screen_related_containers);
  SetUsesScreenCoordinates(status_container);

  aura::Window* settings_bubble_container = CreateContainer(
      kShellWindowId_SettingBubbleContainer,
      "SettingBubbleContainer",
      lock_screen_related_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      settings_bubble_container);
  SetUsesScreenCoordinates(settings_bubble_container);

  aura::Window* menu_container = CreateContainer(
      kShellWindowId_MenuContainer,
      "MenuContainer",
      lock_screen_related_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(menu_container);
  SetUsesScreenCoordinates(menu_container);

  aura::Window* drag_drop_container = CreateContainer(
      kShellWindowId_DragImageAndTooltipContainer,
      "DragImageAndTooltipContainer",
      lock_screen_related_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(drag_drop_container);
  SetUsesScreenCoordinates(drag_drop_container);

  aura::Window* overlay_container = CreateContainer(
      kShellWindowId_OverlayContainer,
      "OverlayContainer",
      lock_screen_related_containers);
  SetUsesScreenCoordinates(overlay_container);

  CreateContainer(kShellWindowId_PowerButtonAnimationContainer,
                  "PowerButtonAnimationContainer", root_window) ;
}

}  // namespace internal
}  // namespace ash
