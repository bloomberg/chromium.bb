// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_delegate.h"
#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/app_list/app_list_delegate_impl.h"
#include "ash/aura/wm_shell_aura.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/accelerators/ash_focus_manager_factory.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/ash_constants.h"
#include "ash/common/cast_config_controller.h"
#include "ash/common/devtools/ash_devtools_css_agent.h"
#include "ash/common/devtools/ash_devtools_dom_agent.h"
#include "ash/common/focus_cycler.h"
#include "ash/common/frame/custom_frame_view_ash.h"
#include "ash/common/gpu_support.h"
#include "ash/common/keyboard/keyboard_ui.h"
#include "ash/common/login_status.h"
#include "ash/common/media_controller.h"
#include "ash/common/new_window_controller.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_window_watcher.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shell_observer.h"
#include "ash/common/shutdown_controller.h"
#include "ash/common/system/brightness_control_delegate.h"
#include "ash/common/system/chromeos/bluetooth/bluetooth_notification_controller.h"
#include "ash/common/system/chromeos/bluetooth/tray_bluetooth_helper.h"
#include "ash/common/system/chromeos/brightness/brightness_controller_chromeos.h"
#include "ash/common/system/chromeos/keyboard_brightness_controller.h"
#include "ash/common/system/chromeos/network/sms_observer.h"
#include "ash/common/system/chromeos/network/vpn_list.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/common/system/keyboard_brightness_control_delegate.h"
#include "ash/common/system/locale/locale_notification_controller.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/toast/toast_manager.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/immersive_context_ash.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/maximize_mode/maximize_mode_window_manager.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm/root_window_finder.h"
#include "ash/common/wm/system_modal_container_layout_manager.h"
#include "ash/common/wm/window_cycle_controller.h"
#include "ash/common/wm/window_positioner.h"
#include "ash/common/wm/workspace_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_color_manager_chromeos.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_error_observer_chromeos.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/projecting_observer_chromeos.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_ash.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/shutdown_observer_chromeos.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/first_run/first_run_helper_impl.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/ime/input_method_event_handler.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell_init_params.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/system/chromeos/power/power_event_observer.h"
#include "ash/system/chromeos/power/video_activity_notifier.h"
#include "ash/system/chromeos/screen_layout_observer.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/virtual_keyboard_controller.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/ash_native_cursor_manager.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/immersive_handler_factory_ash.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/overlay_event_filter.h"
#include "ash/wm/overview/scoped_overview_animation_settings_factory_aura.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/system_gesture_event_filter.h"
#include "ash/wm/system_modal_container_event_filter.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/audio/audio_a11y_controller.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/system/devicemode.h"
#include "components/ui_devtools/devtools_server.h"
#include "services/preferences/public/cpp/pref_client_store.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/mus/user_activity_forwarder.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/chromeos/display_change_observer.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/accelerator_filter.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_modality_controller.h"

#if defined(USE_X11)
#include "ui/display/manager/chromeos/x11/native_display_delegate_x11.h"
#include "ui/gfx/x/x11_types.h"  // nogncheck
#endif

#if defined(USE_OZONE)
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ash {

namespace {

using aura::Window;
using views::Widget;

// A Corewm VisibilityController subclass that calls the Ash animation routine
// so we can pick up our extended animations. See ash/wm/window_animations.h.
class AshVisibilityController : public ::wm::VisibilityController {
 public:
  AshVisibilityController() {}
  ~AshVisibilityController() override {}

 private:
  // Overridden from ::wm::VisibilityController:
  bool CallAnimateOnChildWindowVisibilityChanged(aura::Window* window,
                                                 bool visible) override {
    return AnimateOnChildWindowVisibilityChanged(window, visible);
  }

  DISALLOW_COPY_AND_ASSIGN(AshVisibilityController);
};

}  // namespace

// static
Shell* Shell::instance_ = nullptr;
// static
aura::WindowTreeClient* Shell::window_tree_client_ = nullptr;
// static
aura::WindowManagerClient* Shell::window_manager_client_ = nullptr;
// static
bool Shell::initially_hide_cursor_ = false;

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

// static
Shell* Shell::CreateInstance(const ShellInitParams& init_params) {
  CHECK(!instance_);
  WmShell* wm_shell = init_params.wm_shell;
  if (!wm_shell)
    wm_shell = new WmShellAura();
  instance_ = new Shell(base::WrapUnique<ShellDelegate>(init_params.delegate),
                        base::WrapUnique<WmShell>(wm_shell));
  instance_->Init(init_params);
  return instance_;
}

// static
Shell* Shell::Get() {
  CHECK(instance_);
  return instance_;
}

// static
bool Shell::HasInstance() {
  return !!instance_;
}

// static
void Shell::DeleteInstance() {
  delete instance_;
}

// static
RootWindowController* Shell::GetPrimaryRootWindowController() {
  CHECK(HasInstance());
  return GetRootWindowController(GetPrimaryRootWindow());
}

// static
Shell::RootWindowControllerList Shell::GetAllRootWindowControllers() {
  CHECK(HasInstance());
  RootWindowControllerList root_window_controllers;
  for (WmWindow* root_window : instance_->wm_shell_->GetAllRootWindows())
    root_window_controllers.push_back(root_window->GetRootWindowController());
  return root_window_controllers;
}

// static
RootWindowController* Shell::GetRootWindowControllerWithDisplayId(
    int64_t display_id) {
  CHECK(HasInstance());
  WmWindow* root_window =
      instance_->wm_shell_->GetRootWindowForDisplayId(display_id);
  return root_window ? root_window->GetRootWindowController() : nullptr;
}

// static
aura::Window* Shell::GetPrimaryRootWindow() {
  CHECK(HasInstance());
  return instance_->wm_shell_->GetPrimaryRootWindow()->aura_window();
}

// static
aura::Window* Shell::GetRootWindowForNewWindows() {
  CHECK(Shell::HasInstance());
  return WmWindow::GetAuraWindow(Shell::GetWmRootWindowForNewWindows());
}

// static
WmWindow* Shell::GetWmRootWindowForNewWindows() {
  CHECK(Shell::HasInstance());
  Shell* shell = Shell::GetInstance();
  if (shell->scoped_root_window_for_new_windows_)
    return shell->scoped_root_window_for_new_windows_;
  return shell->root_window_for_new_windows_;
}

// static
aura::Window::Windows Shell::GetAllRootWindows() {
  CHECK(HasInstance());
  aura::Window::Windows windows;
  for (WmWindow* window : instance_->wm_shell_->GetAllRootWindows())
    windows.push_back(window->aura_window());
  return windows;
}

// static
aura::Window* Shell::GetContainer(aura::Window* root_window, int container_id) {
  return root_window->GetChildById(container_id);
}

// static
const aura::Window* Shell::GetContainer(const aura::Window* root_window,
                                        int container_id) {
  return root_window->GetChildById(container_id);
}

views::NonClientFrameView* Shell::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
  // Use translucent-style window frames for dialogs.
  return new CustomFrameViewAsh(widget);
}

void Shell::SetDisplayWorkAreaInsets(Window* contains,
                                     const gfx::Insets& insets) {
  wm_shell_->SetDisplayWorkAreaInsets(WmWindow::Get(contains), insets);
}

void Shell::OnLoginStateChanged(LoginStatus status) {
  for (auto& observer : shell_observers_)
    observer.OnLoginStateChanged(status);
}

void Shell::OnAppTerminating() {
  for (auto& observer : shell_observers_)
    observer.OnAppTerminating();
}

void Shell::OnLockStateChanged(bool locked) {
  for (auto& observer : shell_observers_)
    observer.OnLockStateChanged(locked);
#ifndef NDEBUG
  // Make sure that there is no system modal in Lock layer when unlocked.
  if (!locked) {
    aura::Window::Windows containers = wm::GetContainersFromAllRootWindows(
        kShellWindowId_LockSystemModalContainer, GetPrimaryRootWindow());
    for (aura::Window* container : containers)
      DCHECK(container->children().empty());
  }
#endif
}

void Shell::OnCastingSessionStartedOrStopped(bool started) {
  for (auto& observer : shell_observers_)
    observer.OnCastingSessionStartedOrStopped(started);
}

void Shell::OnRootWindowAdded(WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnRootWindowAdded(root_window);
}

void Shell::CreateKeyboard() {
  InitKeyboard();
  GetPrimaryRootWindowController()->ActivateKeyboard(
      keyboard::KeyboardController::GetInstance());
}

void Shell::DeactivateKeyboard() {
  // TODO(jamescook): Move keyboard create and hide into WmShell.
  keyboard_ui_->Hide();
  if (keyboard::KeyboardController::GetInstance()) {
    RootWindowControllerList controllers = GetAllRootWindowControllers();
    for (RootWindowControllerList::iterator iter = controllers.begin();
         iter != controllers.end(); ++iter) {
      (*iter)->DeactivateKeyboard(keyboard::KeyboardController::GetInstance());
    }
  }
  keyboard::KeyboardController::ResetInstance(nullptr);
}

bool Shell::ShouldSaveDisplaySettings() {
  DCHECK(!wm_shell_->IsRunningInMash());
  return !(
      screen_orientation_controller_->ignore_display_configuration_updates() ||
      resolution_notification_controller_->DoesNotificationTimeout());
}

ShelfModel* Shell::shelf_model() {
  return shelf_controller_->model();
}

aura::client::ActivationClient* Shell::activation_client() {
  return focus_controller_.get();
}

void Shell::UpdateShelfVisibility() {
  for (WmWindow* root : wm_shell_->GetAllRootWindows())
    root->GetRootWindowController()->GetShelf()->UpdateVisibilityState();
}

WebNotificationTray* Shell::GetWebNotificationTray() {
  return GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->web_notification_tray();
}

bool Shell::HasPrimaryStatusArea() {
  return !!GetPrimaryRootWindowController()->GetStatusAreaWidget();
}

SystemTray* Shell::GetPrimarySystemTray() {
  return GetPrimaryRootWindowController()->GetSystemTray();
}

void Shell::SetTouchHudProjectionEnabled(bool enabled) {
  if (is_touch_hud_projection_enabled_ == enabled)
    return;

  is_touch_hud_projection_enabled_ = enabled;
  for (auto& observer : shell_observers_)
    observer.OnTouchHudProjectionToggled(enabled);
}

FirstRunHelper* Shell::CreateFirstRunHelper() {
  return new FirstRunHelperImpl;
}

void Shell::CreateShelfView() {
  // Must occur after SessionController creation and user login.
  DCHECK(session_controller());
  DCHECK_GT(session_controller()->NumberOfLoggedInUsers(), 0);
  CreateShelfDelegate();

  for (WmWindow* root_window : wm_shell_->GetAllRootWindows())
    root_window->GetRootWindowController()->CreateShelfView();
}

void Shell::SetLargeCursorSizeInDip(int large_cursor_size_in_dip) {
  window_tree_host_manager_->cursor_window_controller()
      ->SetLargeCursorSizeInDip(large_cursor_size_in_dip);
}

void Shell::SetCursorCompositingEnabled(bool enabled) {
  window_tree_host_manager_->cursor_window_controller()
      ->SetCursorCompositingEnabled(enabled);
  native_cursor_manager_->SetNativeCursorEnabled(!enabled);
}

void Shell::DoInitialWorkspaceAnimation() {
  return GetPrimaryRootWindowController()
      ->workspace_controller()
      ->DoInitialAnimation();
}

void Shell::AddShellObserver(ShellObserver* observer) {
  shell_observers_.AddObserver(observer);
}

void Shell::RemoveShellObserver(ShellObserver* observer) {
  shell_observers_.RemoveObserver(observer);
}

void Shell::ShowAppList() {
  // Show the app list on the default display for new windows.
  app_list_->Show(
      GetWmRootWindowForNewWindows()->GetDisplayNearestWindow().id());
}

void Shell::DismissAppList() {
  app_list_->Dismiss();
}

void Shell::ToggleAppList() {
  // Toggle the app list on the default display for new windows.
  app_list_->ToggleAppList(
      GetWmRootWindowForNewWindows()->GetDisplayNearestWindow().id());
}

bool Shell::IsAppListVisible() const {
  return app_list_->IsVisible();
}

bool Shell::GetAppListTargetVisibility() const {
  return app_list_->GetTargetVisibility();
}

void Shell::UpdateAfterLoginStatusChange(LoginStatus status) {
  for (WmWindow* root_window : wm_shell_->GetAllRootWindows()) {
    root_window->GetRootWindowController()->UpdateAfterLoginStatusChange(
        status);
  }
}

void Shell::NotifyMaximizeModeStarted() {
  for (auto& observer : shell_observers_)
    observer.OnMaximizeModeStarted();
}

void Shell::NotifyMaximizeModeEnding() {
  for (auto& observer : shell_observers_)
    observer.OnMaximizeModeEnding();
}

void Shell::NotifyMaximizeModeEnded() {
  for (auto& observer : shell_observers_)
    observer.OnMaximizeModeEnded();
}

void Shell::NotifyOverviewModeStarting() {
  for (auto& observer : shell_observers_)
    observer.OnOverviewModeStarting();
}

void Shell::NotifyOverviewModeEnded() {
  for (auto& observer : shell_observers_)
    observer.OnOverviewModeEnded();
}

void Shell::NotifyFullscreenStateChanged(bool is_fullscreen,
                                         WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnFullscreenStateChanged(is_fullscreen, root_window);
}

void Shell::NotifyPinnedStateChanged(WmWindow* pinned_window) {
  for (auto& observer : shell_observers_)
    observer.OnPinnedStateChanged(pinned_window);
}

void Shell::NotifyVirtualKeyboardActivated(bool activated,
                                           WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnVirtualKeyboardStateChanged(activated, root_window);
}

void Shell::NotifyShelfCreatedForRootWindow(WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfCreatedForRootWindow(root_window);
}

void Shell::NotifyShelfAlignmentChanged(WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfAlignmentChanged(root_window);
}

void Shell::NotifyShelfAutoHideBehaviorChanged(WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfAutoHideBehaviorChanged(root_window);
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

Shell::Shell(std::unique_ptr<ShellDelegate> shell_delegate,
             std::unique_ptr<WmShell> wm_shell)
    : wm_shell_(std::move(wm_shell)),
      brightness_control_delegate_(
          base::MakeUnique<system::BrightnessControllerChromeos>()),
      cast_config_(base::MakeUnique<CastConfigController>()),
      focus_cycler_(base::MakeUnique<FocusCycler>()),
      immersive_context_(base::MakeUnique<ImmersiveContextAsh>()),
      keyboard_brightness_control_delegate_(
          base::MakeUnique<KeyboardBrightnessController>()),
      locale_notification_controller_(
          base::MakeUnique<LocaleNotificationController>()),
      media_controller_(base::MakeUnique<MediaController>()),
      new_window_controller_(base::MakeUnique<NewWindowController>()),
      session_controller_(base::MakeUnique<SessionController>()),
      shelf_controller_(base::MakeUnique<ShelfController>()),
      shell_delegate_(std::move(shell_delegate)),
      shutdown_controller_(base::MakeUnique<ShutdownController>()),
      system_tray_controller_(base::MakeUnique<SystemTrayController>()),
      system_tray_notifier_(base::MakeUnique<SystemTrayNotifier>()),
      vpn_list_(base::MakeUnique<VpnList>()),
      window_cycle_controller_(base::MakeUnique<WindowCycleController>()),
      window_selector_controller_(base::MakeUnique<WindowSelectorController>()),
      app_list_(base::MakeUnique<app_list::AppList>()),
      link_handler_model_factory_(nullptr),
      tray_bluetooth_helper_(base::MakeUnique<TrayBluetoothHelper>()),
      display_configurator_(new display::DisplayConfigurator()),
      native_cursor_manager_(nullptr),
      simulate_modal_window_open_for_testing_(false),
      is_touch_hud_projection_enabled_(false) {
  // TODO(sky): better refactor cash/mash dependencies. Perhaps put all cash
  // state on WmShellAura. http://crbug.com/671246.

  if (!wm_shell_->IsRunningInMash()) {
    gpu_support_.reset(shell_delegate_->CreateGPUSupport());
    display_manager_.reset(ScreenAsh::CreateDisplayManager());
    window_tree_host_manager_.reset(new WindowTreeHostManager);
    user_metrics_recorder_.reset(new UserMetricsRecorder);
  }

  PowerStatus::Initialize();

  session_controller_->AddSessionStateObserver(this);
}

Shell::~Shell() {
  TRACE_EVENT0("shutdown", "ash::Shell::Destructor");

  const bool is_mash = wm_shell_->IsRunningInMash();

  if (!is_mash)
    user_metrics_recorder_->OnShellShuttingDown();

  shell_delegate_->PreShutdown();

  // Remove the focus from any window. This will prevent overhead and side
  // effects (e.g. crashes) from changing focus during shutdown.
  // See bug crbug.com/134502.
  aura::client::GetFocusClient(GetPrimaryRootWindow())->FocusWindow(nullptr);

  // Please keep in same order as in Init() because it's easy to miss one.
  if (window_modality_controller_)
    window_modality_controller_.reset();
  if (!is_mash) {
    RemovePreTargetHandler(
        window_tree_host_manager_->input_method_event_handler());
  }

  RemovePreTargetHandler(magnifier_key_scroll_handler_.get());
  magnifier_key_scroll_handler_.reset();

  RemovePreTargetHandler(speech_feedback_handler_.get());
  speech_feedback_handler_.reset();

  RemovePreTargetHandler(overlay_filter_.get());
  RemovePreTargetHandler(accelerator_filter_.get());
  RemovePreTargetHandler(event_transformation_handler_.get());
  RemovePreTargetHandler(toplevel_window_event_handler_.get());
  RemovePostTargetHandler(toplevel_window_event_handler_.get());
  if (!is_mash) {
    RemovePreTargetHandler(system_gesture_filter_.get());
    RemovePreTargetHandler(mouse_cursor_filter_.get());
  }
  RemovePreTargetHandler(modality_filter_.get());

  // TooltipController is deleted with the Shell so removing its references.
  RemovePreTargetHandler(tooltip_controller_.get());

  screen_orientation_controller_.reset();
  screen_layout_observer_.reset();

  // Destroy the virtual keyboard controller before the maximize mode controller
  // since the latters destructor triggers events that the former is listening
  // to but no longer cares about.
  virtual_keyboard_controller_.reset();

  // Destroy maximize mode controller early on since it has some observers which
  // need to be removed.
  maximize_mode_controller_.reset();

  // Destroy the keyboard before closing the shelf, since it will invoke a shelf
  // layout.
  DeactivateKeyboard();

  toast_manager_.reset();

  // Destroy SystemTrayDelegate before destroying the status area(s). Make sure
  // to deinitialize the shelf first, as it is initialized after the delegate.
  for (WmWindow* root : wm_shell_->GetAllRootWindows())
    root->GetRootWindowController()->GetShelf()->ShutdownShelfWidget();
  tray_bluetooth_helper_.reset();
  DeleteSystemTrayDelegate();

  // Drag-and-drop must be canceled prior to close all windows.
  drag_drop_controller_.reset();

// Controllers who have WindowObserver added must be deleted
// before |window_tree_host_manager_| is deleted.

  // VideoActivityNotifier must be deleted before |video_detector_| is
  // deleted because it's observing video activity through
  // VideoDetector::Observer interface.
  video_activity_notifier_.reset();
  video_detector_.reset();
  high_contrast_controller_.reset();

  shadow_controller_.reset();
  resize_shadow_controller_.reset();

  // Has to happen before ~MruWindowTracker.
  window_cycle_controller_.reset();
  window_selector_controller_.reset();

  CloseAllRootWindowChildWindows();

  // MruWindowTracker must be destroyed after all windows have been deleted to
  // avoid a possible crash when Shell is destroyed from a non-normal shutdown
  // path. (crbug.com/485438).
  mru_window_tracker_.reset();

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  // Alphabetical. TODO(oshima): sort.
  magnification_controller_.reset();
  tooltip_controller_.reset();
  event_client_.reset();
  toplevel_window_event_handler_.reset();
  visibility_controller_.reset();

  power_button_controller_.reset();
  lock_state_controller_.reset();

  screen_pinning_controller_.reset();

  resolution_notification_controller_.reset();
  screenshot_controller_.reset();
  mouse_cursor_filter_.reset();
  modality_filter_.reset();

  touch_transformer_controller_.reset();
  audio_a11y_controller_.reset();
  laser_pointer_controller_.reset();
  partial_magnification_controller_.reset();

  // This also deletes all RootWindows. Note that we invoke Shutdown() on
  // WindowTreeHostManager before resetting |window_tree_host_manager_|, since
  // destruction
  // of its owned RootWindowControllers relies on the value.
  ScreenAsh::CreateScreenForShutdown();
  display_configuration_controller_.reset();

  // AppListDelegateImpl depends upon AppList.
  app_list_delegate_impl_.reset();

  // These members access Shell in their destructors.
  wallpaper_controller_.reset();
  accessibility_delegate_.reset();

  // Balances the Install() in Initialize().
  views::FocusManagerFactory::Install(nullptr);

  // ShelfWindowWatcher has window observers and a pointer to the shelf model.
  shelf_window_watcher_.reset();

  // ShelfItemDelegate subclasses it owns have complex cleanup to run (e.g. ARC
  // shelf items in Chrome) so explicitly shutdown early.
  shelf_model()->DestroyItemDelegates();

  // Must be destroyed before FocusController.
  shelf_delegate_.reset();

  // Removes itself as an observer of |pref_store_|.
  shelf_controller_.reset();

  wm_shell_->Shutdown();

  // Depends on |focus_controller_|, so must be destroyed before.
  window_tree_host_manager_.reset();
  focus_controller_->RemoveObserver(this);
  focus_controller_.reset();
  screen_position_controller_.reset();

  display_color_manager_.reset();
  if (display_change_observer_)
    display_configurator_->RemoveObserver(display_change_observer_.get());
  if (display_error_observer_)
    display_configurator_->RemoveObserver(display_error_observer_.get());
  if (projecting_observer_) {
    display_configurator_->RemoveObserver(projecting_observer_.get());
    RemoveShellObserver(projecting_observer_.get());
  }
  display_change_observer_.reset();
  shutdown_observer_.reset();

  PowerStatus::Shutdown();

  // Ensure that DBusThreadManager outlives this Shell.
  DCHECK(chromeos::DBusThreadManager::IsInitialized());

  // Needs to happen right before |instance_| is reset.
  wm_shell_.reset();
  session_controller_->RemoveSessionStateObserver(this);
  wallpaper_delegate_.reset();
  pref_store_ = nullptr;
  shell_delegate_.reset();

  DCHECK(instance_ == this);
  instance_ = nullptr;
}

void Shell::Init(const ShellInitParams& init_params) {
  const bool is_mash = wm_shell_->IsRunningInMash();

  blocking_pool_ = init_params.blocking_pool;

  wallpaper_delegate_ = shell_delegate_->CreateWallpaperDelegate();

  // Can be null in tests.
  if (shell_delegate_->GetShellConnector()) {
    prefs::mojom::PreferencesServiceFactoryPtr pref_factory_ptr;
    shell_delegate_->GetShellConnector()->BindInterface(
        prefs::mojom::kServiceName, &pref_factory_ptr);
    pref_store_ = new preferences::PrefClientStore(std::move(pref_factory_ptr));
  }

  // Some delegates access WmShell during their construction. Create them here
  // instead of the WmShell constructor.
  accessibility_delegate_.reset(shell_delegate_->CreateAccessibilityDelegate());
  palette_delegate_ = shell_delegate_->CreatePaletteDelegate();
  toast_manager_ = base::MakeUnique<ToastManager>();

  // Install the custom factory early on so that views::FocusManagers for Tray,
  // Shelf, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  wallpaper_controller_ = base::MakeUnique<WallpaperController>(blocking_pool_);

  // Start devtools server
  devtools_server_ = ui::devtools::UiDevToolsServer::Create(nullptr);
  if (devtools_server_) {
    auto dom_backend = base::MakeUnique<devtools::AshDevToolsDOMAgent>();
    auto css_backend =
        base::MakeUnique<devtools::AshDevToolsCSSAgent>(dom_backend.get());
    auto devtools_client = base::MakeUnique<ui::devtools::UiDevToolsClient>(
        "Ash", devtools_server_.get());
    devtools_client->AddAgent(std::move(dom_backend));
    devtools_client->AddAgent(std::move(css_backend));
    devtools_server_->AttachClient(std::move(devtools_client));
  }

  if (is_mash)
    app_list_delegate_impl_ = base::MakeUnique<AppListDelegateImpl>();

  // TODO(sky): move creation to WmShell.
  if (!is_mash)
    immersive_handler_factory_ = base::MakeUnique<ImmersiveHandlerFactoryAsh>();

  scoped_overview_animation_settings_factory_.reset(
      new ScopedOverviewAnimationSettingsFactoryAura);
  window_positioner_ = base::MakeUnique<WindowPositioner>();

  if (!is_mash) {
    native_cursor_manager_ = new AshNativeCursorManager;
    cursor_manager_.reset(
        new CursorManager(base::WrapUnique(native_cursor_manager_)));
  }

  shell_delegate_->PreInit();
  bool display_initialized = true;
  if (!is_mash) {
    display_initialized = display_manager_->InitFromCommandLine();

    display_configuration_controller_.reset(new DisplayConfigurationController(
        display_manager_.get(), window_tree_host_manager_.get()));

#if defined(USE_OZONE)
    display_configurator_->Init(
        ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(),
        !gpu_support_->IsPanelFittingDisabled());
#elif defined(USE_X11)
    display_configurator_->Init(
        base::MakeUnique<display::NativeDisplayDelegateX11>(),
        !gpu_support_->IsPanelFittingDisabled());
#endif
  }

  // The DBusThreadManager must outlive this Shell. See the DCHECK in ~Shell.
  chromeos::DBusThreadManager* dbus_thread_manager =
      chromeos::DBusThreadManager::Get();
  projecting_observer_.reset(
      new ProjectingObserver(dbus_thread_manager->GetPowerManagerClient()));
  display_configurator_->AddObserver(projecting_observer_.get());
  AddShellObserver(projecting_observer_.get());

  if (!display_initialized && chromeos::IsRunningAsSystemCompositor()) {
    display_change_observer_ = base::MakeUnique<display::DisplayChangeObserver>(
        display_configurator_.get(), display_manager_.get());

    shutdown_observer_ =
        base::MakeUnique<ShutdownObserver>(display_configurator_.get());

    // Register |display_change_observer_| first so that the rest of
    // observer gets invoked after the root windows are configured.
    display_configurator_->AddObserver(display_change_observer_.get());
    display_error_observer_.reset(new DisplayErrorObserver());
    display_configurator_->AddObserver(display_error_observer_.get());
    display_configurator_->set_state_controller(display_change_observer_.get());
    display_configurator_->set_mirroring_controller(display_manager_.get());
    display_configurator_->ForceInitialConfigure(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kFirstExecAfterBoot)
            ? kChromeOsBootColor
            : 0);
    display_initialized = true;
  }
  display_color_manager_.reset(new DisplayColorManager(
      display_configurator_.get(), init_params.blocking_pool));

  if (!display_initialized)
    display_manager_->InitDefaultDisplay();

  if (!is_mash) {
    display_manager_->RefreshFontParams();

    aura::Env::GetInstance()->set_context_factory(init_params.context_factory);
    aura::Env::GetInstance()->set_context_factory_private(
        init_params.context_factory_private);
  }

  // The WindowModalityController needs to be at the front of the input event
  // pretarget handler list to ensure that it processes input events when modal
  // windows are active.
  window_modality_controller_.reset(new ::wm::WindowModalityController(this));

  env_filter_.reset(new ::wm::CompoundEventFilter);
  AddPreTargetHandler(env_filter_.get());

  // FocusController takes ownership of AshFocusRules.
  focus_controller_ =
      base::MakeUnique<::wm::FocusController>(new wm::AshFocusRules());
  focus_controller_->AddObserver(this);

  screen_position_controller_.reset(new ScreenPositionController);

  wm_shell_->CreatePrimaryHost();
  root_window_for_new_windows_ = WmWindow::Get(GetPrimaryRootWindow());

  if (!is_mash) {
    resolution_notification_controller_.reset(
        new ResolutionNotificationController);
  }

  if (cursor_manager_) {
    cursor_manager_->SetDisplay(
        display::Screen::GetScreen()->GetPrimaryDisplay());
  }

  accelerator_controller_ = wm_shell_->CreateAcceleratorController();
  maximize_mode_controller_ = base::MakeUnique<MaximizeModeController>();

  if (!is_mash) {
    AddPreTargetHandler(
        window_tree_host_manager_->input_method_event_handler());
  }

  magnifier_key_scroll_handler_ = MagnifierKeyScroller::CreateHandler();
  AddPreTargetHandler(magnifier_key_scroll_handler_.get());
  speech_feedback_handler_ = SpokenFeedbackToggler::CreateHandler();
  AddPreTargetHandler(speech_feedback_handler_.get());

  // The order in which event filters are added is significant.

  // ui::UserActivityDetector passes events to observers, so let them get
  // rewritten first.
  user_activity_detector_.reset(new ui::UserActivityDetector);

  overlay_filter_.reset(new OverlayEventFilter);
  AddPreTargetHandler(overlay_filter_.get());
  AddShellObserver(overlay_filter_.get());

  accelerator_filter_.reset(new ::wm::AcceleratorFilter(
      std::unique_ptr<::wm::AcceleratorDelegate>(new AcceleratorDelegate),
      accelerator_controller_->accelerator_history()));
  AddPreTargetHandler(accelerator_filter_.get());

  event_transformation_handler_.reset(new EventTransformationHandler);
  AddPreTargetHandler(event_transformation_handler_.get());

  toplevel_window_event_handler_.reset(
      new ToplevelWindowEventHandler(wm_shell_.get()));

  if (!is_mash) {
    system_gesture_filter_.reset(new SystemGestureEventFilter);
    AddPreTargetHandler(system_gesture_filter_.get());
  }

  sticky_keys_controller_.reset(new StickyKeysController);
  if (!is_mash) {
    screen_pinning_controller_.reset(
        new ScreenPinningController(window_tree_host_manager_.get()));
  }

  lock_state_controller_ =
      base::MakeUnique<LockStateController>(shutdown_controller_.get());
  power_button_controller_.reset(
      new PowerButtonController(lock_state_controller_.get()));
  // Pass the initial display state to PowerButtonController.
  power_button_controller_->OnDisplayModeChanged(
      display_configurator_->cached_displays());

  AddShellObserver(lock_state_controller_.get());

  // The connector is unavailable in some tests.
  if (is_mash && shell_delegate_->GetShellConnector()) {
    ui::mojom::UserActivityMonitorPtr user_activity_monitor;
    shell_delegate_->GetShellConnector()->BindInterface(ui::mojom::kServiceName,
                                                        &user_activity_monitor);
    user_activity_forwarder_ = base::MakeUnique<aura::UserActivityForwarder>(
        std::move(user_activity_monitor), user_activity_detector_.get());
  }

  drag_drop_controller_.reset(new DragDropController);
  // |screenshot_controller_| needs to be created (and prepended as a
  // pre-target handler) at this point, because |mouse_cursor_filter_| needs to
  // process mouse events prior to screenshot session.
  // See http://crbug.com/459214
  screenshot_controller_.reset(new ScreenshotController());
  if (!is_mash) {
    mouse_cursor_filter_.reset(new MouseCursorEventFilter());
    PrependPreTargetHandler(mouse_cursor_filter_.get());
  }

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_.reset(new AshVisibilityController);

  laser_pointer_controller_.reset(new LaserPointerController());
  partial_magnification_controller_.reset(new PartialMagnificationController());

  magnification_controller_.reset(MagnificationController::CreateInstance());
  mru_window_tracker_ = base::MakeUnique<MruWindowTracker>();

  autoclick_controller_.reset(AutoclickController::CreateInstance());

  high_contrast_controller_.reset(new HighContrastController);
  video_detector_.reset(new VideoDetector);

  tooltip_controller_.reset(new views::corewm::TooltipController(
      std::unique_ptr<views::corewm::Tooltip>(new views::corewm::TooltipAura)));
  AddPreTargetHandler(tooltip_controller_.get());

  modality_filter_.reset(new SystemModalContainerEventFilter(this));
  AddPreTargetHandler(modality_filter_.get());

  event_client_.reset(new EventClientImpl);

  session_state_delegate_.reset(shell_delegate_->CreateSessionStateDelegate());
  // Must occur after Shell has installed its early pre-target handlers (for
  // example, WindowModalityController).
  wm_shell_->CreatePointerWatcherAdapter();

  resize_shadow_controller_.reset(new ResizeShadowController());
  shadow_controller_.reset(new ::wm::ShadowController(focus_controller_.get()));

  SetSystemTrayDelegate(
      base::WrapUnique(shell_delegate_->CreateSystemTrayDelegate()));

  // May trigger initialization of the Bluetooth adapter.
  tray_bluetooth_helper_->Initialize();

  // Create AshTouchTransformController before
  // WindowTreeHostManager::InitDisplays()
  // since AshTouchTransformController listens on
  // WindowTreeHostManager::Observer::OnDisplaysInitialized().
  if (!is_mash) {
    touch_transformer_controller_.reset(new AshTouchTransformController(
        display_configurator_.get(), display_manager_.get()));
  }

  keyboard_ui_ = wm_shell_->CreateKeyboardUI();

  wm_shell_->InitHosts(init_params);

  // Needs to be created after InitDisplays() since it may cause the virtual
  // keyboard to be deployed.
  if (!is_mash)
    virtual_keyboard_controller_.reset(new VirtualKeyboardController);

  audio_a11y_controller_.reset(new chromeos::AudioA11yController);

  // Initialize the wallpaper after the RootWindowController has been created,
  // otherwise the widget will not paint when restoring after a browser crash.
  // Also, initialize after display initialization to ensure correct sizing.
  wallpaper_delegate_->InitializeWallpaper();

  if (cursor_manager_) {
    if (initially_hide_cursor_)
      cursor_manager_->HideCursor();
    cursor_manager_->SetCursor(ui::kCursorPointer);
  }

  power_event_observer_.reset(new PowerEventObserver());
  user_activity_notifier_.reset(
      new ui::UserActivityPowerManagerNotifier(user_activity_detector_.get()));
  video_activity_notifier_.reset(
      new VideoActivityNotifier(video_detector_.get()));
  bluetooth_notification_controller_.reset(new BluetoothNotificationController);
  if (!is_mash) {
    screen_orientation_controller_.reset(new ScreenOrientationController());
    screen_layout_observer_.reset(new ScreenLayoutObserver());
  }
  sms_observer_.reset(new SmsObserver());

  // The compositor thread and main message loop have to be running in
  // order to create mirror window. Run it after the main message loop
  // is started.
  if (!is_mash)
    display_manager_->CreateMirrorWindowAsyncIfAny();

  for (auto& observer : shell_observers_)
    observer.OnShellInitialized();

  if (!is_mash)
    user_metrics_recorder_->OnShellInitialized();
}

void Shell::InitKeyboard() {
  if (keyboard::IsKeyboardEnabled()) {
    if (keyboard::KeyboardController::GetInstance()) {
      RootWindowControllerList controllers = GetAllRootWindowControllers();
      for (RootWindowControllerList::iterator iter = controllers.begin();
           iter != controllers.end(); ++iter) {
        (*iter)->DeactivateKeyboard(
            keyboard::KeyboardController::GetInstance());
      }
    }
    keyboard::KeyboardController::ResetInstance(
        new keyboard::KeyboardController(shell_delegate_->CreateKeyboardUI(),
                                         virtual_keyboard_controller_.get()));
  }
}

void Shell::InitRootWindow(aura::Window* root_window) {
  DCHECK(focus_controller_);
  DCHECK(visibility_controller_.get());
  DCHECK(drag_drop_controller_.get());

  aura::client::SetFocusClient(root_window, focus_controller_.get());
  aura::client::SetActivationClient(root_window, focus_controller_.get());
  root_window->AddPreTargetHandler(focus_controller_.get());
  aura::client::SetVisibilityClient(root_window, visibility_controller_.get());
  aura::client::SetDragDropClient(root_window, drag_drop_controller_.get());
  aura::client::SetScreenPositionClient(root_window,
                                        screen_position_controller_.get());
  aura::client::SetCursorClient(root_window, cursor_manager_.get());
  aura::client::SetTooltipClient(root_window, tooltip_controller_.get());
  aura::client::SetEventClient(root_window, event_client_.get());

  aura::client::SetWindowMoveClient(root_window,
                                    toplevel_window_event_handler_.get());
  root_window->AddPreTargetHandler(toplevel_window_event_handler_.get());
  root_window->AddPostTargetHandler(toplevel_window_event_handler_.get());
}

void Shell::SetSystemTrayDelegate(
    std::unique_ptr<SystemTrayDelegate> delegate) {
  DCHECK(delegate);
  system_tray_delegate_ = std::move(delegate);
  system_tray_delegate_->Initialize();
  // Accesses WmShell in its constructor.
  logout_confirmation_controller_.reset(new LogoutConfirmationController(
      base::Bind(&SystemTrayController::SignOut,
                 base::Unretained(system_tray_controller_.get()))));
}

void Shell::DeleteSystemTrayDelegate() {
  DCHECK(system_tray_delegate_);
  // Accesses WmShell in its destructor.
  logout_confirmation_controller_.reset();
  system_tray_delegate_.reset();
}

void Shell::CloseAllRootWindowChildWindows() {
  for (WmWindow* wm_root_window : wm_shell_->GetAllRootWindows()) {
    aura::Window* root_window = wm_root_window->aura_window();
    RootWindowController* controller = GetRootWindowController(root_window);
    if (controller) {
      controller->CloseChildWindows();
    } else {
      while (!root_window->children().empty()) {
        aura::Window* child = root_window->children()[0];
        delete child;
      }
    }
  }
}

void Shell::CreateShelfDelegate() {
  // May be called multiple times as shelves are created and destroyed.
  if (shelf_delegate_)
    return;
  // Must occur after SessionController creation and user login because
  // Chrome's implementation of ShelfDelegate assumes it can get information
  // about multi-profile login state.
  DCHECK(session_controller());
  DCHECK_GT(session_controller()->NumberOfLoggedInUsers(), 0);
  shelf_delegate_.reset(shell_delegate_->CreateShelfDelegate(shelf_model()));
  shelf_window_watcher_ = base::MakeUnique<ShelfWindowWatcher>(shelf_model());
}

bool Shell::CanWindowReceiveEvents(aura::Window* window) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowController* controller : controllers) {
    if (controller->CanWindowReceiveEvents(window))
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Shell, ui::EventTarget overrides:

bool Shell::CanAcceptEvent(const ui::Event& event) {
  return true;
}

ui::EventTarget* Shell::GetParentTarget() {
  return aura::Env::GetInstance();
}

std::unique_ptr<ui::EventTargetIterator> Shell::GetChildIterator() const {
  return std::unique_ptr<ui::EventTargetIterator>();
}

ui::EventTargeter* Shell::GetEventTargeter() {
  NOTREACHED();
  return nullptr;
}

void Shell::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  WmWindow* gained_active_wm = WmWindow::Get(gained_active);
  if (gained_active_wm)
    root_window_for_new_windows_ = gained_active_wm->GetRootWindow();
}

void Shell::SessionStateChanged(session_manager::SessionState state) {
  // Create the shelf when a session becomes active. It's safe to do this
  // multiple times (e.g. initial login vs. multiprofile add session).
  if (state == session_manager::SessionState::ACTIVE) {
    CreateShelfView();

    if (!wm_shell_->IsRunningInMash()) {
      // Recreate the keyboard after initial login and after multiprofile login.
      CreateKeyboard();
    }
  }

  // Only trigger an update in mash because with classic ash chrome calls
  // UpdateAfterLoginStatusChange() directly.
  if (wm_shell_->IsRunningInMash()) {
    // TODO(jamescook): Should this call Shell::OnLoginStatusChanged() too?
    UpdateAfterLoginStatusChange(session_controller_->GetLoginStatus());
  }
}

}  // namespace ash
