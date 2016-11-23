// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <queue>
#include <vector>

#include "ash/aura/aura_layout_manager_adapter.h"
#include "ash/aura/wm_root_window_controller_aura.h"
#include "ash/aura/wm_shelf_aura.h"
#include "ash/aura/wm_window_aura.h"
#include "ash/common/ash_constants.h"
#include "ash/common/ash_switches.h"
#include "ash/common/focus_cycler.h"
#include "ash/common/login_status.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/system/status_area_layout_manager.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wallpaper/wallpaper_widget_controller.h"
#include "ash/common/wm/always_on_top_controller.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/dock/docked_window_layout_manager.h"
#include "ash/common/wm/fullscreen_window_finder.h"
#include "ash/common/wm/panels/panel_layout_manager.h"
#include "ash/common/wm/root_window_layout_manager.h"
#include "ash/common/wm/switchable_windows.h"
#include "ash/common/wm/system_modal_container_layout_manager.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/workspace/workspace_layout_manager.h"
#include "ash/common/wm/workspace_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_settings.h"
#include "ash/shelf/shelf_window_targeter.h"
#include "ash/shell.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/touch/touch_hud_projection.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wm/panels/attached_panel_window_targeter.h"
#include "ash/wm/panels/panel_window_event_handler.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/system_wallpaper_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/tooltip_client.h"
#include "ui/wm/public/window_types.h"

#if defined(OS_CHROMEOS)
#include "ash/ash_touch_exploration_manager_chromeos.h"
#include "ash/wm/boot_splash_screen_chromeos.h"
#include "chromeos/chromeos_switches.h"
#include "ui/chromeos/touch_exploration_controller.h"
#endif

namespace ash {
namespace {

#if defined(OS_CHROMEOS)
// Duration for the animation that hides the boot splash screen, in
// milliseconds.  This should be short enough in relation to
// wm/window_animation.cc's brightness/grayscale fade animation that the login
// wallpaper image animation isn't hidden by the splash screen animation.
const int kBootSplashScreenHideDurationMs = 500;
#endif

bool IsWindowAboveContainer(aura::Window* window,
                            aura::Window* blocking_container) {
  std::vector<aura::Window*> target_path;
  std::vector<aura::Window*> blocking_path;

  while (window) {
    target_path.push_back(window);
    window = window->parent();
  }

  while (blocking_container) {
    blocking_path.push_back(blocking_container);
    blocking_container = blocking_container->parent();
  }

  // The root window is put at the end so that we compare windows at
  // the same depth.
  while (!blocking_path.empty()) {
    if (target_path.empty())
      return false;

    aura::Window* target = target_path.back();
    target_path.pop_back();
    aura::Window* blocking = blocking_path.back();
    blocking_path.pop_back();

    // Still on the same path, continue.
    if (target == blocking)
      continue;

    // This can happen only if unparented window is passed because
    // first element must be the same root.
    if (!target->parent() || !blocking->parent())
      return false;

    aura::Window* common_parent = target->parent();
    DCHECK_EQ(common_parent, blocking->parent());
    aura::Window::Windows windows = common_parent->children();
    auto blocking_iter = std::find(windows.begin(), windows.end(), blocking);
    // If the target window is above blocking window, the window can handle
    // events.
    return std::find(blocking_iter, windows.end(), target) != windows.end();
  }

  return true;
}

}  // namespace

void RootWindowController::CreateForPrimaryDisplay(AshWindowTreeHost* host) {
  RootWindowController* controller = new RootWindowController(host);
  controller->Init(RootWindowController::PRIMARY);
}

void RootWindowController::CreateForSecondaryDisplay(AshWindowTreeHost* host) {
  RootWindowController* controller = new RootWindowController(host);
  controller->Init(RootWindowController::SECONDARY);
}

// static
RootWindowController* RootWindowController::ForWindow(
    const aura::Window* window) {
  CHECK(Shell::HasInstance());
  return GetRootWindowController(window->GetRootWindow());
}

// static
RootWindowController* RootWindowController::ForTargetRootWindow() {
  CHECK(Shell::HasInstance());
  return GetRootWindowController(Shell::GetTargetRootWindow());
}

RootWindowController::~RootWindowController() {
  Shutdown();
  ash_host_.reset();
  // The CaptureClient needs to be around for as long as the RootWindow is
  // valid.
  capture_client_.reset();
}

aura::WindowTreeHost* RootWindowController::GetHost() {
  return ash_host_->AsWindowTreeHost();
}

const aura::WindowTreeHost* RootWindowController::GetHost() const {
  return ash_host_->AsWindowTreeHost();
}

aura::Window* RootWindowController::GetRootWindow() {
  return GetHost()->window();
}

const aura::Window* RootWindowController::GetRootWindow() const {
  return GetHost()->window();
}

WorkspaceController* RootWindowController::workspace_controller() {
  return wm_root_window_controller_->workspace_controller();
}

void RootWindowController::Shutdown() {
  WmShell::Get()->RemoveShellObserver(this);

#if defined(OS_CHROMEOS)
  touch_exploration_manager_.reset();
#endif

  wm_root_window_controller_->ResetRootForNewWindowsIfNecessary();

  CloseChildWindows();
  aura::Window* root_window = GetRootWindow();
  GetRootWindowSettings(root_window)->controller = NULL;
  // Forget with the display ID so that display lookup
  // ends up with invalid display.
  GetRootWindowSettings(root_window)->display_id = display::kInvalidDisplayId;
  ash_host_->PrepareForShutdown();

  system_wallpaper_.reset();
  aura::client::SetScreenPositionClient(root_window, NULL);
}

bool RootWindowController::CanWindowReceiveEvents(aura::Window* window) {
  if (GetRootWindow() != window->GetRootWindow())
    return false;

  // Always allow events to fall through to the virtual keyboard even if
  // displaying a system modal dialog.
  if (IsVirtualKeyboardWindow(window))
    return true;

  aura::Window* blocking_container = nullptr;

  int modal_container_id = 0;
  if (WmShell::Get()->GetSessionStateDelegate()->IsUserSessionBlocked()) {
    blocking_container =
        GetContainer(kShellWindowId_LockScreenContainersContainer);
    modal_container_id = kShellWindowId_LockSystemModalContainer;
  } else {
    modal_container_id = kShellWindowId_SystemModalContainer;
  }
  aura::Window* modal_container = GetContainer(modal_container_id);
  SystemModalContainerLayoutManager* modal_layout_manager = nullptr;
  modal_layout_manager = static_cast<SystemModalContainerLayoutManager*>(
      WmWindowAura::Get(modal_container)->GetLayoutManager());

  if (modal_layout_manager->has_window_dimmer())
    blocking_container = modal_container;
  else
    modal_container = nullptr;  // Don't check modal dialogs.

  // In normal session.
  if (!blocking_container)
    return true;

  if (!IsWindowAboveContainer(window, blocking_container))
    return false;

  // If the window is in the target modal container, only allow the top most
  // one.
  if (modal_container && modal_container->Contains(window))
    return modal_layout_manager->IsPartOfActiveModalWindow(
        WmWindowAura::Get(window));

  return true;
}

aura::Window* RootWindowController::GetContainer(int container_id) {
  return GetRootWindow()->GetChildById(container_id);
}

const aura::Window* RootWindowController::GetContainer(int container_id) const {
  return ash_host_->AsWindowTreeHost()->window()->GetChildById(container_id);
}

void RootWindowController::OnInitialWallpaperAnimationStarted() {
#if defined(OS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshAnimateFromBootSplashScreen) &&
      boot_splash_screen_.get()) {
    // Make the splash screen fade out so it doesn't obscure the wallpaper's
    // brightness/grayscale animation.
    boot_splash_screen_->StartHideAnimation(
        base::TimeDelta::FromMilliseconds(kBootSplashScreenHideDurationMs));
  }
#endif
}

void RootWindowController::OnWallpaperAnimationFinished(views::Widget* widget) {
  // Make sure the wallpaper is visible.
  system_wallpaper_->SetColor(SK_ColorBLACK);
#if defined(OS_CHROMEOS)
  boot_splash_screen_.reset();
#endif
}

void RootWindowController::CloseChildWindows() {
  // Remove observer as deactivating keyboard causes
  // docked_window_layout_manager() to fire notifications.
  if (docked_window_layout_manager() &&
      wm_shelf_aura_->shelf_layout_manager()) {
    docked_window_layout_manager()->RemoveObserver(
        wm_shelf_aura_->shelf_layout_manager());
  }

  // Deactivate keyboard container before closing child windows and shutting
  // down associated layout managers.
  DeactivateKeyboard(keyboard::KeyboardController::GetInstance());

  wm_root_window_controller_->CloseChildWindows();

  aura::client::SetDragDropClient(GetRootWindow(), nullptr);
  aura::client::SetTooltipClient(GetRootWindow(), nullptr);
}

void RootWindowController::MoveWindowsTo(aura::Window* dst) {
  wm_root_window_controller_->MoveWindowsTo(WmWindowAura::Get(dst));
}

ShelfLayoutManager* RootWindowController::GetShelfLayoutManager() {
  return wm_shelf_aura_->shelf_layout_manager();
}

StatusAreaWidget* RootWindowController::GetStatusAreaWidget() {
  ShelfWidget* shelf_widget = wm_shelf_aura_->shelf_widget();
  return shelf_widget ? shelf_widget->status_area_widget() : nullptr;
}

SystemTray* RootWindowController::GetSystemTray() {
  // We assume in throughout the code that this will not return NULL. If code
  // triggers this for valid reasons, it should test status_area_widget first.
  CHECK(wm_shelf_aura_->shelf_widget()->status_area_widget());
  return wm_shelf_aura_->shelf_widget()->status_area_widget()->system_tray();
}

void RootWindowController::UpdateShelfVisibility() {
  wm_shelf_aura_->UpdateVisibilityState();
}

aura::Window* RootWindowController::GetWindowForFullscreenMode() {
  return WmWindowAura::GetAuraWindow(
      wm::GetWindowForFullscreenMode(WmWindowAura::Get(GetRootWindow())));
}

void RootWindowController::ActivateKeyboard(
    keyboard::KeyboardController* keyboard_controller) {
  if (!keyboard::IsKeyboardEnabled() ||
      GetContainer(kShellWindowId_VirtualKeyboardContainer)) {
    return;
  }
  DCHECK(keyboard_controller);
  keyboard_controller->AddObserver(wm_shelf_aura_->shelf_layout_manager());
  keyboard_controller->AddObserver(panel_layout_manager());
  keyboard_controller->AddObserver(docked_window_layout_manager());
  keyboard_controller->AddObserver(workspace_controller()->layout_manager());
  keyboard_controller->AddObserver(
      wm_root_window_controller_->always_on_top_controller()
          ->GetLayoutManager());
  WmShell::Get()->NotifyVirtualKeyboardActivated(true);
  aura::Window* parent = GetContainer(kShellWindowId_ImeWindowParentContainer);
  DCHECK(parent);
  aura::Window* keyboard_container = keyboard_controller->GetContainerWindow();
  keyboard_container->set_id(kShellWindowId_VirtualKeyboardContainer);
  parent->AddChild(keyboard_container);
}

void RootWindowController::DeactivateKeyboard(
    keyboard::KeyboardController* keyboard_controller) {
  if (!keyboard_controller ||
      !keyboard_controller->keyboard_container_initialized()) {
    return;
  }
  aura::Window* keyboard_container = keyboard_controller->GetContainerWindow();
  if (keyboard_container->GetRootWindow() == GetRootWindow()) {
    aura::Window* parent =
        GetContainer(kShellWindowId_ImeWindowParentContainer);
    DCHECK(parent);
    parent->RemoveChild(keyboard_container);
    // Virtual keyboard may be deactivated while still showing, notify all
    // observers that keyboard bounds changed to 0 before remove them.
    keyboard_controller->NotifyKeyboardBoundsChanging(gfx::Rect());
    keyboard_controller->RemoveObserver(wm_shelf_aura_->shelf_layout_manager());
    keyboard_controller->RemoveObserver(panel_layout_manager());
    keyboard_controller->RemoveObserver(docked_window_layout_manager());
    keyboard_controller->RemoveObserver(
        workspace_controller()->layout_manager());
    keyboard_controller->RemoveObserver(
        wm_root_window_controller_->always_on_top_controller()
            ->GetLayoutManager());
    WmShell::Get()->NotifyVirtualKeyboardActivated(false);
  }
}

bool RootWindowController::IsVirtualKeyboardWindow(aura::Window* window) {
  aura::Window* parent = GetContainer(kShellWindowId_ImeWindowParentContainer);
  return parent ? parent->Contains(window) : false;
}

void RootWindowController::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
#if defined(OS_CHROMEOS)
  if (touch_exploration_manager_)
    touch_exploration_manager_->SetTouchAccessibilityAnchorPoint(anchor_point);
#endif  // defined(OS_CHROMEOS)
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowController, private:

RootWindowController::RootWindowController(AshWindowTreeHost* ash_host)
    : ash_host_(ash_host),
      wm_shelf_aura_(new WmShelfAura),
      touch_hud_debug_(NULL),
      touch_hud_projection_(NULL) {
  aura::Window* root_window = GetRootWindow();
  GetRootWindowSettings(root_window)->controller = this;

  // Has to happen after this is set as |controller| of RootWindowSettings.
  wm_root_window_controller_ = WmRootWindowControllerAura::Get(root_window);

  stacking_controller_.reset(new StackingController);
  aura::client::SetWindowParentingClient(root_window,
                                         stacking_controller_.get());
  capture_client_.reset(new ::wm::ScopedCaptureClient(root_window));
}

void RootWindowController::Init(RootWindowType root_window_type) {
  aura::Window* root_window = GetRootWindow();
  Shell* shell = Shell::GetInstance();
  shell->InitRootWindow(root_window);

  wm_root_window_controller_->CreateContainers();

  CreateSystemWallpaper(root_window_type);

  InitLayoutManagers();
  InitTouchHuds();

  if (WmShell::Get()
          ->GetPrimaryRootWindowController()
          ->GetSystemModalLayoutManager(nullptr)
          ->has_window_dimmer()) {
    wm_root_window_controller_->GetSystemModalLayoutManager(nullptr)
        ->CreateModalBackground();
  }

  WmShell::Get()->AddShellObserver(this);

  wm_root_window_controller_->root_window_layout_manager()->OnWindowResized();
  if (root_window_type == PRIMARY) {
    shell->InitKeyboard();
  } else {
    ash_host_->AsWindowTreeHost()->Show();

    // Create a shelf if a user is already logged in.
    if (WmShell::Get()->GetSessionStateDelegate()->NumberOfLoggedInUsers())
      wm_root_window_controller_->CreateShelf();

    // Notify shell observers about new root window.
    shell->OnRootWindowAdded(WmWindowAura::Get(root_window));
  }

#if defined(OS_CHROMEOS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDisableTouchExplorationMode)) {
    touch_exploration_manager_.reset(new AshTouchExplorationManager(this));
  }
#endif
}

void RootWindowController::InitLayoutManagers() {
  // Create the shelf and status area widgets.
  DCHECK(!wm_shelf_aura_->shelf_widget());
  aura::Window* shelf_container = GetContainer(kShellWindowId_ShelfContainer);
  aura::Window* status_container = GetContainer(kShellWindowId_StatusContainer);
  WmWindow* wm_shelf_container = WmWindowAura::Get(shelf_container);
  WmWindow* wm_status_container = WmWindowAura::Get(status_container);

  wm_root_window_controller_->CreateLayoutManagers();

  // Make it easier to resize windows that partially overlap the shelf. Must
  // occur after the ShelfLayoutManager is constructed by ShelfWidget.
  shelf_container->SetEventTargeter(base::MakeUnique<ShelfWindowTargeter>(
      wm_shelf_container, wm_shelf_aura_.get()));
  status_container->SetEventTargeter(base::MakeUnique<ShelfWindowTargeter>(
      wm_status_container, wm_shelf_aura_.get()));

  panel_container_handler_ = base::MakeUnique<PanelWindowEventHandler>();
  GetContainer(kShellWindowId_PanelContainer)
      ->AddPreTargetHandler(panel_container_handler_.get());

  // Install an AttachedPanelWindowTargeter on the panel container to make it
  // easier to correctly target shelf buttons with touch.
  gfx::Insets mouse_extend(-kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize);
  gfx::Insets touch_extend =
      mouse_extend.Scale(kResizeOutsideBoundsScaleForTouch);
  aura::Window* panel_container = GetContainer(kShellWindowId_PanelContainer);
  panel_container->SetEventTargeter(std::unique_ptr<ui::EventTargeter>(
      new AttachedPanelWindowTargeter(panel_container, mouse_extend,
                                      touch_extend, panel_layout_manager())));
}

void RootWindowController::InitTouchHuds() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshTouchHud))
    set_touch_hud_debug(new TouchHudDebug(GetRootWindow()));
  if (Shell::GetInstance()->is_touch_hud_projection_enabled())
    EnableTouchHudProjection();
}

void RootWindowController::CreateSystemWallpaper(
    RootWindowType root_window_type) {
  SkColor color = SK_ColorBLACK;
#if defined(OS_CHROMEOS)
  // The splash screen appears on the primary display at boot. If this is a
  // secondary monitor (either connected at boot or connected later) or if the
  // browser restarted for a second login then don't use the boot color.
  const bool is_boot_splash_screen =
      root_window_type == PRIMARY &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kFirstExecAfterBoot);
  if (is_boot_splash_screen)
    color = kChromeOsBootColor;
#endif
  system_wallpaper_.reset(
      new SystemWallpaperController(GetRootWindow(), color));

#if defined(OS_CHROMEOS)
  // Make a copy of the system's boot splash screen so we can composite it
  // onscreen until the wallpaper is ready.
  if (is_boot_splash_screen &&
      (base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kAshCopyHostBackgroundAtBoot) ||
       base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kAshAnimateFromBootSplashScreen)))
    boot_splash_screen_.reset(new BootSplashScreen(GetHost()));
#endif
}

void RootWindowController::EnableTouchHudProjection() {
  if (touch_hud_projection_)
    return;
  set_touch_hud_projection(new TouchHudProjection(GetRootWindow()));
}

void RootWindowController::DisableTouchHudProjection() {
  if (!touch_hud_projection_)
    return;
  touch_hud_projection_->Remove();
}

DockedWindowLayoutManager*
RootWindowController::docked_window_layout_manager() {
  return wm_root_window_controller_->docked_window_layout_manager();
}

PanelLayoutManager* RootWindowController::panel_layout_manager() {
  return wm_root_window_controller_->panel_layout_manager();
}

void RootWindowController::OnLoginStateChanged(LoginStatus status) {
  wm_shelf_aura_->UpdateVisibilityState();
}

void RootWindowController::OnTouchHudProjectionToggled(bool enabled) {
  if (enabled)
    EnableTouchHudProjection();
  else
    DisableTouchHudProjection();
}

RootWindowController* GetRootWindowController(const aura::Window* root_window) {
  return root_window ? GetRootWindowSettings(root_window)->controller : nullptr;
}

}  // namespace ash
