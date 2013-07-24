// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_manager.h"
#include "ash/focus_cycler.h"
#include "ash/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/touch/touch_hud_projection.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/boot_splash_screen.h"
#include "ash/wm/dock/docked_window_layout_manager.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/panels/panel_window_event_handler.h"
#include "ash/wm/property_util.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/screen_dimmer.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/system_background_controller.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/screen.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/corewm/visibility_controller.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"

namespace ash {
namespace {

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
    internal::kShellWindowId_DockedContainer,
    internal::kShellWindowId_PanelContainer,
    internal::kShellWindowId_AlwaysOnTopContainer,
    internal::kShellWindowId_SystemModalContainer,
    internal::kShellWindowId_LockSystemModalContainer,
    internal::kShellWindowId_InputMethodContainer,
    internal::kShellWindowId_UnparentedControlContainer,
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
    while (!src_container->children().empty()) {
      // Restart iteration from the source container windows each time as they
      // may change as a result of moving other windows.
      aura::Window::Windows::const_iterator iter =
          src_container->children().begin();
      while (iter != src_container->children().end() &&
             internal::SystemModalContainerLayoutManager::IsModalBackground(
                *iter)) {
        ++iter;
      }
      // If the entire window list is modal background windows then stop.
      if (iter == src_container->children().end())
        break;
      ReparentWindow(*iter, dst_container);
    }
  }
}

// Mark the container window so that a widget added to this container will
// use the virtual screeen coordinates instead of parent.
void SetUsesScreenCoordinates(aura::Window* container) {
  container->SetProperty(internal::kUsesScreenCoordinatesKey, true);
}

// Mark the container window so that a widget added to this container will
// say in the same root window regardless of the bounds specified.
void DescendantShouldStayInSameRootWindow(aura::Window* container) {
  container->SetProperty(internal::kStayInSameRootWindowKey, true);
}

}  // namespace

namespace internal {

RootWindowController::RootWindowController(aura::RootWindow* root_window)
    : root_window_(root_window),
      root_window_layout_(NULL),
      docked_layout_manager_(NULL),
      panel_layout_manager_(NULL),
      touch_hud_debug_(NULL),
      touch_hud_projection_(NULL) {
  SetRootWindowController(root_window, this);
  screen_dimmer_.reset(new ScreenDimmer(root_window));

  stacking_controller_.reset(new StackingController);
  aura::client::SetStackingClient(root_window, stacking_controller_.get());
}

RootWindowController::~RootWindowController() {
  Shutdown();
  root_window_.reset();
}

// static
RootWindowController* RootWindowController::ForLauncher(aura::Window* window) {
  return GetRootWindowController(window->GetRootWindow());
}

// static
RootWindowController* RootWindowController::ForWindow(
    const aura::Window* window) {
  return GetRootWindowController(window->GetRootWindow());
}

// static
RootWindowController* RootWindowController::ForActiveRootWindow() {
  return GetRootWindowController(Shell::GetActiveRootWindow());
}

void RootWindowController::SetWallpaperController(
    DesktopBackgroundWidgetController* controller) {
  wallpaper_controller_.reset(controller);
}

void RootWindowController::SetAnimatingWallpaperController(
    AnimatingDesktopController* controller) {
  if (animating_wallpaper_controller_.get())
    animating_wallpaper_controller_->StopAnimating();
  animating_wallpaper_controller_.reset(controller);
}

void RootWindowController::Shutdown() {
  Shell::GetInstance()->RemoveShellObserver(this);

  if (animating_wallpaper_controller_.get())
    animating_wallpaper_controller_->StopAnimating();
  wallpaper_controller_.reset();
  animating_wallpaper_controller_.reset();

  CloseChildWindows();
  if (Shell::GetActiveRootWindow() == root_window_) {
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
    int modal_window_id = Shell::GetInstance()->session_state_delegate()
        ->IsUserSessionBlocked() ? kShellWindowId_LockSystemModalContainer :
                                   kShellWindowId_SystemModalContainer;
    container = GetContainer(modal_window_id);
  }
  return container ? static_cast<SystemModalContainerLayoutManager*>(
      container->layout_manager()) : NULL;
}

aura::Window* RootWindowController::GetContainer(int container_id) {
  return root_window_->GetChildById(container_id);
}

void RootWindowController::Init(bool first_run_after_boot) {
  root_window_->SetCursor(ui::kCursorPointer);
  CreateContainersInRootWindow(root_window_.get());
  CreateSystemBackground(first_run_after_boot);

  InitLayoutManagers();
  InitKeyboard();
  InitTouchHuds();

  if (Shell::GetPrimaryRootWindowController()->
      GetSystemModalLayoutManager(NULL)->has_modal_background()) {
    GetSystemModalLayoutManager(NULL)->CreateModalBackground();
  }

  Shell::GetInstance()->AddShellObserver(this);
}

void RootWindowController::ShowLauncher() {
  if (!shelf_->launcher())
    return;
  shelf_->launcher()->SetVisible(true);
  shelf_->status_area_widget()->Show();
}

void RootWindowController::OnLauncherCreated() {
  if (panel_layout_manager_)
    panel_layout_manager_->SetLauncher(shelf_->launcher());
  if (docked_layout_manager_) {
    docked_layout_manager_->SetLauncher(shelf_->launcher());
    if (shelf_->shelf_layout_manager())
      docked_layout_manager_->AddObserver(shelf_->shelf_layout_manager());
  }
}

void RootWindowController::UpdateAfterLoginStatusChange(
    user::LoginStatus status) {
  if (shelf_->status_area_widget())
    shelf_->status_area_widget()->UpdateAfterLoginStatusChange(status);
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

void RootWindowController::OnWallpaperAnimationFinished(views::Widget* widget) {
  // Make sure the wallpaper is visible.
  system_background_->SetColor(SK_ColorBLACK);
  boot_splash_screen_.reset();

  Shell::GetInstance()->user_wallpaper_delegate()->
      OnWallpaperAnimationFinished();
  // Only removes old component when wallpaper animation finished. If we
  // remove the old one before the new wallpaper is done fading in there will
  // be a white flash during the animation.
  if (animating_wallpaper_controller()) {
    DesktopBackgroundWidgetController* controller =
        animating_wallpaper_controller()->GetController(true);
    // |desktop_widget_| should be the same animating widget we try to move
    // to |kDesktopController|. Otherwise, we may close |desktop_widget_|
    // before move it to |kDesktopController|.
    DCHECK_EQ(controller->widget(), widget);
    // Release the old controller and close its background widget.
    SetWallpaperController(controller);
  }
}

void RootWindowController::CloseChildWindows() {
  if (!shelf_.get())
    return;
  // panel_layout_manager_ needs to be shut down before windows are destroyed.
  if (panel_layout_manager_) {
    panel_layout_manager_->Shutdown();
    panel_layout_manager_ = NULL;
  }
  // docked_layout_manager_ needs to be shut down before windows are destroyed.
  if (docked_layout_manager_) {
    if (shelf_->shelf_layout_manager())
      docked_layout_manager_->RemoveObserver(shelf_->shelf_layout_manager());
    docked_layout_manager_->Shutdown();
    docked_layout_manager_ = NULL;
  }

  // TODO(harrym): Remove when Status Area Widget is a child view.
  shelf_->ShutdownStatusAreaWidget();

  if (shelf_->shelf_layout_manager())
    shelf_->shelf_layout_manager()->PrepareForShutdown();

  // Close background widget first as it depends on tooltip.
  wallpaper_controller_.reset();
  animating_wallpaper_controller_.reset();

  workspace_controller_.reset();
  aura::client::SetTooltipClient(root_window_.get(), NULL);

  while (!root_window_->children().empty()) {
    aura::Window* child = root_window_->children()[0];
    delete child;
  }

  shelf_.reset(NULL);
}

void RootWindowController::MoveWindowsTo(aura::RootWindow* dst) {
  // Forget the shelf early so that shelf don't update itself using wrong
  // display info.
  workspace_controller_->SetShelf(NULL);
  ReparentAllWindows(root_window_.get(), dst);
}

ShelfLayoutManager* RootWindowController::GetShelfLayoutManager() {
  return shelf_->shelf_layout_manager();
}

SystemTray* RootWindowController::GetSystemTray() {
  // We assume in throughout the code that this will not return NULL. If code
  // triggers this for valid reasons, it should test status_area_widget first.
  CHECK(shelf_->status_area_widget());
  return shelf_->status_area_widget()->system_tray();
}

void RootWindowController::ShowContextMenu(const gfx::Point& location_in_screen,
                                           ui::MenuSourceType source_type) {
  DCHECK(Shell::GetInstance()->delegate());
  scoped_ptr<ui::MenuModel> menu_model(
      Shell::GetInstance()->delegate()->CreateContextMenu(root_window()));
  if (!menu_model)
    return;

  // Background controller may not be set yet if user clicked on status are
  // before initial animation completion. See crbug.com/222218
  if (!wallpaper_controller_.get())
    return;

  views::MenuRunner menu_runner(menu_model.get());
  if (menu_runner.RunMenuAt(wallpaper_controller_->widget(),
          NULL, gfx::Rect(location_in_screen, gfx::Size()),
          views::MenuItemView::TOPLEFT, source_type,
          views::MenuRunner::CONTEXT_MENU) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }

  Shell::GetInstance()->UpdateShelfVisibility();
}

void RootWindowController::UpdateShelfVisibility() {
  shelf_->shelf_layout_manager()->UpdateVisibilityState();
}

aura::Window* RootWindowController::GetFullscreenWindow() const {
  aura::Window* container = workspace_controller_->GetActiveWorkspaceWindow();
  for (size_t i = 0; i < container->children().size(); ++i) {
    aura::Window* child = container->children()[i];
    if (wm::IsWindowFullscreen(child))
      return child;
  }
  return NULL;
}

void RootWindowController::InitKeyboard() {
  if (keyboard::IsKeyboardEnabled()) {
    aura::Window* parent = root_window();

    keyboard::KeyboardControllerProxy* proxy =
        Shell::GetInstance()->delegate()->CreateKeyboardControllerProxy();
    keyboard_controller_.reset(
        new keyboard::KeyboardController(proxy));

    keyboard_controller_->AddObserver(shelf()->shelf_layout_manager());
    keyboard_controller_->AddObserver(panel_layout_manager_);

    aura::Window* keyboard_container =
        keyboard_controller_->GetContainerWindow();
    parent->AddChild(keyboard_container);
    keyboard_container->SetBounds(parent->bounds());
  }
}


////////////////////////////////////////////////////////////////////////////////
// RootWindowController, private:

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
  always_on_top_controller_.reset(new internal::AlwaysOnTopController);
  always_on_top_controller_->SetAlwaysOnTopContainer(always_on_top_container);

  DCHECK(!shelf_.get());
  aura::Window* shelf_container =
      GetContainer(internal::kShellWindowId_ShelfContainer);
  // TODO(harrym): Remove when status area is view.
  aura::Window* status_container =
      GetContainer(internal::kShellWindowId_StatusContainer);
  shelf_.reset(new ShelfWidget(
      shelf_container, status_container, workspace_controller()));

  // Create Docked windows layout manager
  aura::Window* docked_container = GetContainer(
      internal::kShellWindowId_DockedContainer);
  docked_layout_manager_ =
      new internal::DockedWindowLayoutManager(docked_container);
  docked_container_handler_.reset(
      new ToplevelWindowEventHandler(docked_container));
  docked_container->SetLayoutManager(docked_layout_manager_);

  // Create Panel layout manager
  aura::Window* panel_container = GetContainer(
      internal::kShellWindowId_PanelContainer);
  panel_layout_manager_ =
      new internal::PanelLayoutManager(panel_container);
  panel_container_handler_.reset(
      new PanelWindowEventHandler(panel_container));
  panel_container->SetLayoutManager(panel_layout_manager_);
}

void RootWindowController::InitTouchHuds() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshTouchHud))
    set_touch_hud_debug(new TouchHudDebug(root_window_.get()));
  if (Shell::GetInstance()->is_touch_hud_projection_enabled())
    EnableTouchHudProjection();
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

  aura::Window* docked_container = CreateContainer(
      kShellWindowId_DockedContainer,
      "DockedContainer",
      non_lock_screen_containers);
  SetUsesScreenCoordinates(docked_container);

  aura::Window* panel_container = CreateContainer(
      kShellWindowId_PanelContainer,
      "PanelContainer",
      non_lock_screen_containers);
  SetUsesScreenCoordinates(panel_container);

  aura::Window* shelf_container =
      CreateContainer(kShellWindowId_ShelfContainer,
                      "ShelfContainer",
                      non_lock_screen_containers);
  SetUsesScreenCoordinates(shelf_container);
  DescendantShouldStayInSameRootWindow(shelf_container);

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
  DescendantShouldStayInSameRootWindow(status_container);

  aura::Window* settings_bubble_container = CreateContainer(
      kShellWindowId_SettingBubbleContainer,
      "SettingBubbleContainer",
      lock_screen_related_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      settings_bubble_container);
  SetUsesScreenCoordinates(settings_bubble_container);
  DescendantShouldStayInSameRootWindow(settings_bubble_container);

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

void RootWindowController::EnableTouchHudProjection() {
  if (touch_hud_projection_)
    return;
  set_touch_hud_projection(new TouchHudProjection(root_window_.get()));
}

void RootWindowController::DisableTouchHudProjection() {
  if (!touch_hud_projection_)
    return;
  touch_hud_projection_->Remove();
}

void RootWindowController::OnLoginStateChanged(user::LoginStatus status) {
  shelf_->shelf_layout_manager()->UpdateVisibilityState();
}

void RootWindowController::OnTouchHudProjectionToggled(bool enabled) {
  if (enabled)
    EnableTouchHudProjection();
  else
    DisableTouchHudProjection();
}

}  // namespace internal
}  // namespace ash
