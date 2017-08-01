// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_delegate.h"
#include "ash/accelerators/ash_focus_manager_factory.h"
#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/accessibility_delegate.h"
#include "ash/app_list/app_list_delegate_impl.h"
#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/cast_config_controller.h"
#include "ash/display/ash_display_controller.h"
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
#include "ash/focus_cycler.h"
#include "ash/frame/custom_frame_view_ash.h"
#include "ash/gpu_support.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/ime/ime_controller.h"
#include "ash/keyboard/keyboard_ui.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/login/lock_screen_controller.h"
#include "ash/login_status.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/media_controller.h"
#include "ash/new_window_controller.h"
#include "ash/palette_delegate.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_window_watcher.h"
#include "ash/shell_delegate.h"
#include "ash/shell_init_params.h"
#include "ash/shell_observer.h"
#include "ash/shell_port.h"
#include "ash/shell_port_classic.h"
#include "ash/shutdown_controller.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/system/bluetooth/bluetooth_notification_controller.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/brightness/brightness_controller_chromeos.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/locale/locale_notification_controller.h"
#include "ash/system/network/sms_observer.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/night_light/night_light_controller.h"
#include "ash/system/power/power_event_observer.h"
#include "ash/system/power/power_status.h"
#include "ash/system/power/video_activity_notifier.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/tray_action/tray_action.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/virtual_keyboard_controller.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_delegate.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/immersive_context_ash.h"
#include "ash/wm/immersive_handler_factory_ash.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/native_cursor_manager_ash_classic.h"
#include "ash/wm/native_cursor_manager_ash_mus.h"
#include "ash/wm/overlay_event_filter.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/system_gesture_event_filter.h"
#include "ash/wm/system_modal_container_event_filter.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
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
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/pref_service_factory.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/user_activity_forwarder.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/chromeos/default_touch_transform_setter.h"
#include "ui/display/manager/chromeos/display_change_observer.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#include "ui/display/manager/chromeos/touch_transform_setter.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_ui.h"
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

namespace ash {

namespace {

using aura::Window;
using views::Widget;

bool g_is_browser_process_with_mash = false;

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
  ShellPort* shell_port = init_params.shell_port;
  if (!shell_port)
    shell_port = new ShellPortClassic();
  instance_ = new Shell(base::WrapUnique<ShellDelegate>(init_params.delegate),
                        base::WrapUnique<ShellPort>(shell_port));
  instance_->Init(init_params);
  return instance_;
}

// static
Shell* Shell::Get() {
  CHECK(!g_is_browser_process_with_mash)  // Implies null |instance_|.
      << "Ash is running in its own process so Shell::Get() will return null. "
         "The browser process must use the mojo interfaces in //ash/public to "
         "access ash. See ash/README.md for details.";
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
  return RootWindowController::ForWindow(GetPrimaryRootWindow());
}

// static
Shell::RootWindowControllerList Shell::GetAllRootWindowControllers() {
  CHECK(HasInstance());
  RootWindowControllerList root_window_controllers;
  for (aura::Window* root : GetAllRootWindows())
    root_window_controllers.push_back(RootWindowController::ForWindow(root));
  return root_window_controllers;
}

// static
RootWindowController* Shell::GetRootWindowControllerWithDisplayId(
    int64_t display_id) {
  CHECK(HasInstance());
  aura::Window* root = GetRootWindowForDisplayId(display_id);
  return root ? RootWindowController::ForWindow(root) : nullptr;
}

// static
aura::Window* Shell::GetRootWindowForDisplayId(int64_t display_id) {
  CHECK(HasInstance());
  return instance_->window_tree_host_manager_->GetRootWindowForDisplayId(
      display_id);
}

// static
aura::Window* Shell::GetPrimaryRootWindow() {
  CHECK(HasInstance());
  return instance_->window_tree_host_manager_->GetPrimaryRootWindow();
}

// static
aura::Window* Shell::GetRootWindowForNewWindows() {
  CHECK(Shell::HasInstance());
  Shell* shell = Shell::Get();
  if (shell->scoped_root_window_for_new_windows_)
    return shell->scoped_root_window_for_new_windows_;
  return shell->root_window_for_new_windows_;
}

// static
aura::Window::Windows Shell::GetAllRootWindows() {
  CHECK(HasInstance());
  return instance_->window_tree_host_manager_->GetAllRootWindows();
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

// static
Config Shell::GetAshConfig() {
  return Get()->shell_port_->GetAshConfig();
}

// static
bool Shell::ShouldUseIMEService() {
  return Shell::GetAshConfig() == Config::MASH ||
         (Shell::GetAshConfig() == Config::MUS &&
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kUseIMEService));
}

// static
void Shell::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  LogoutButtonTray::RegisterProfilePrefs(registry);
  NightLightController::RegisterProfilePrefs(registry);
}

views::NonClientFrameView* Shell::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
  // Use translucent-style window frames for dialogs.
  return new CustomFrameViewAsh(widget);
}

void Shell::SetDisplayWorkAreaInsets(Window* contains,
                                     const gfx::Insets& insets) {
  window_tree_host_manager_->UpdateWorkAreaOfDisplayNearestWindow(contains,
                                                                  insets);
}

void Shell::OnCastingSessionStartedOrStopped(bool started) {
  for (auto& observer : shell_observers_)
    observer.OnCastingSessionStartedOrStopped(started);
}

void Shell::OnRootWindowAdded(aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnRootWindowAdded(root_window);
}

void Shell::CreateKeyboard() {
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
    for (auto& observer : shell_observers_)
      observer.OnKeyboardControllerCreated();
  }

  GetPrimaryRootWindowController()->ActivateKeyboard(
      keyboard::KeyboardController::GetInstance());
}

void Shell::DestroyKeyboard() {
  // TODO(jamescook): Move keyboard create and hide into ShellPort.
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
  // This function is only called from Chrome, hence the DCHECK for not-MASH.
  DCHECK(GetAshConfig() != Config::MASH);
  return !(
      screen_orientation_controller_->ignore_display_configuration_updates() ||
      resolution_notification_controller_->DoesNotificationTimeout());
}

NightLightController* Shell::night_light_controller() {
  DCHECK(NightLightController::IsFeatureEnabled());
  return night_light_controller_.get();
}

ShelfModel* Shell::shelf_model() {
  return shelf_controller_->model();
}

::wm::ActivationClient* Shell::activation_client() {
  return focus_controller_.get();
}

void Shell::UpdateShelfVisibility() {
  for (aura::Window* root : GetAllRootWindows())
    Shelf::ForWindow(root)->UpdateVisibilityState();
}

PrefService* Shell::GetLocalStatePrefService() const {
  if (shell_port_->GetAshConfig() == Config::MASH)
    return local_state_.get();

  return shell_delegate_->GetLocalStatePrefService();
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

void Shell::SetLargeCursorSizeInDip(int large_cursor_size_in_dip) {
  window_tree_host_manager_->cursor_window_controller()
      ->SetLargeCursorSizeInDip(large_cursor_size_in_dip);
}

void Shell::SetCursorCompositingEnabled(bool enabled) {
  if (GetAshConfig() != Config::MASH) {
    // TODO: needs to work in mash. http://crbug.com/705592.
    window_tree_host_manager_->cursor_window_controller()
        ->SetCursorCompositingEnabled(enabled);
    native_cursor_manager_->SetNativeCursorEnabled(!enabled);
  }
}

void Shell::DoInitialWorkspaceAnimation() {
  return GetPrimaryRootWindowController()
      ->workspace_controller()
      ->DoInitialAnimation();
}

bool Shell::IsSplitViewModeActive() const {
  return split_view_controller_->IsSplitViewModeActive();
}

void Shell::AddShellObserver(ShellObserver* observer) {
  shell_observers_.AddObserver(observer);
}

void Shell::RemoveShellObserver(ShellObserver* observer) {
  shell_observers_.RemoveObserver(observer);
}

void Shell::ShowAppList() {
  // Show the app list on the default display for new windows.
  app_list_->Show(display::Screen::GetScreen()
                      ->GetDisplayNearestWindow(GetRootWindowForNewWindows())
                      .id());
}

void Shell::UpdateAppListYPositionAndOpacity(int y_position_in_screen,
                                             float app_list_background_opacity,
                                             bool is_end_gesture) {
  app_list_->UpdateYPositionAndOpacity(
      y_position_in_screen, app_list_background_opacity, is_end_gesture);
}

void Shell::DismissAppList() {
  app_list_->Dismiss();
}

void Shell::ToggleAppList() {
  // Toggle the app list on the default display for new windows.
  app_list_->ToggleAppList(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(GetRootWindowForNewWindows())
          .id());
}

bool Shell::IsAppListVisible() const {
  return app_list_->IsVisible();
}

bool Shell::GetAppListTargetVisibility() const {
  return app_list_->GetTargetVisibility();
}

void Shell::UpdateAfterLoginStatusChange(LoginStatus status) {
  for (auto* root_window_controller : GetAllRootWindowControllers())
    root_window_controller->UpdateAfterLoginStatusChange(status);
}

void Shell::NotifyOverviewModeStarting() {
  for (auto& observer : shell_observers_)
    observer.OnOverviewModeStarting();
}

void Shell::NotifyOverviewModeEnded() {
  for (auto& observer : shell_observers_)
    observer.OnOverviewModeEnded();
}

void Shell::NotifySplitViewModeStarting() {
  for (auto& observer : shell_observers_)
    observer.OnSplitViewModeStarting();
}

void Shell::NotifySplitViewModeEnded() {
  for (auto& observer : shell_observers_)
    observer.OnSplitViewModeEnded();
}

void Shell::NotifyFullscreenStateChanged(bool is_fullscreen,
                                         aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnFullscreenStateChanged(is_fullscreen, root_window);
}

void Shell::NotifyPinnedStateChanged(aura::Window* pinned_window) {
  for (auto& observer : shell_observers_)
    observer.OnPinnedStateChanged(pinned_window);
}

void Shell::NotifyVirtualKeyboardActivated(bool activated,
                                           aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnVirtualKeyboardStateChanged(activated, root_window);
}

void Shell::NotifyShelfCreatedForRootWindow(aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfCreatedForRootWindow(root_window);
}

void Shell::NotifyShelfAlignmentChanged(aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfAlignmentChanged(root_window);
}

void Shell::NotifyShelfAutoHideBehaviorChanged(aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfAutoHideBehaviorChanged(root_window);
}

// static
void Shell::SetIsBrowserProcessWithMash() {
  g_is_browser_process_with_mash = true;
}

void Shell::NotifyAppListVisibilityChanged(bool visible,
                                           aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnAppListVisibilityChanged(visible, root_window);
}

void Shell::NotifyVoiceInteractionStatusChanged(bool running) {
  for (auto& observer : shell_observers_)
    observer.OnVoiceInteractionStatusChanged(running);
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

Shell::Shell(std::unique_ptr<ShellDelegate> shell_delegate,
             std::unique_ptr<ShellPort> shell_port)
    : shell_port_(std::move(shell_port)),
      ash_display_controller_(base::MakeUnique<AshDisplayController>()),
      brightness_control_delegate_(
          base::MakeUnique<system::BrightnessControllerChromeos>()),
      cast_config_(base::MakeUnique<CastConfigController>()),
      focus_cycler_(base::MakeUnique<FocusCycler>()),
      ime_controller_(base::MakeUnique<ImeController>()),
      immersive_context_(base::MakeUnique<ImmersiveContextAsh>()),
      keyboard_brightness_control_delegate_(
          base::MakeUnique<KeyboardBrightnessController>()),
      locale_notification_controller_(
          base::MakeUnique<LocaleNotificationController>()),
      lock_screen_controller_(base::MakeUnique<LockScreenController>()),
      media_controller_(base::MakeUnique<MediaController>()),
      new_window_controller_(base::MakeUnique<NewWindowController>()),
      session_controller_(base::MakeUnique<SessionController>()),
      shelf_controller_(base::MakeUnique<ShelfController>()),
      shell_delegate_(std::move(shell_delegate)),
      shutdown_controller_(base::MakeUnique<ShutdownController>()),
      system_tray_controller_(base::MakeUnique<SystemTrayController>()),
      system_tray_notifier_(base::MakeUnique<SystemTrayNotifier>()),
      tray_action_(base::MakeUnique<TrayAction>()),
      vpn_list_(base::MakeUnique<VpnList>()),
      window_cycle_controller_(base::MakeUnique<WindowCycleController>()),
      window_selector_controller_(base::MakeUnique<WindowSelectorController>()),
      app_list_(base::MakeUnique<app_list::AppList>()),
      link_handler_model_factory_(nullptr),
      tray_bluetooth_helper_(base::MakeUnique<TrayBluetoothHelper>()),
      display_configurator_(new display::DisplayConfigurator()),
      native_cursor_manager_(nullptr),
      simulate_modal_window_open_for_testing_(false),
      is_touch_hud_projection_enabled_(false),
      weak_factory_(this) {
  // TODO(sky): better refactor cash/mash dependencies. Perhaps put all cash
  // state on ShellPortClassic. http://crbug.com/671246.

  gpu_support_.reset(shell_delegate_->CreateGPUSupport());

  display_manager_.reset(ScreenAsh::CreateDisplayManager());
  window_tree_host_manager_ = base::MakeUnique<WindowTreeHostManager>();
  user_metrics_recorder_ = base::MakeUnique<UserMetricsRecorder>();

  PowerStatus::Initialize();

  session_controller_->AddObserver(this);
}

Shell::~Shell() {
  TRACE_EVENT0("shutdown", "ash::Shell::Destructor");

  const Config config = shell_port_->GetAshConfig();

  user_metrics_recorder_->OnShellShuttingDown();

  shell_delegate_->PreShutdown();

  // Remove the focus from any window. This will prevent overhead and side
  // effects (e.g. crashes) from changing focus during shutdown.
  // See bug crbug.com/134502.
  aura::client::GetFocusClient(GetPrimaryRootWindow())->FocusWindow(nullptr);

  // Please keep in reverse order as in Init() because it's easy to miss one.
  split_view_controller_.reset();

  if (window_modality_controller_)
    window_modality_controller_.reset();

  RemovePreTargetHandler(magnifier_key_scroll_handler_.get());
  magnifier_key_scroll_handler_.reset();

  RemovePreTargetHandler(speech_feedback_handler_.get());
  speech_feedback_handler_.reset();

  RemovePreTargetHandler(overlay_filter_.get());
  overlay_filter_.reset();

  RemovePreTargetHandler(accelerator_filter_.get());
  RemovePreTargetHandler(event_transformation_handler_.get());
  RemovePreTargetHandler(toplevel_window_event_handler_.get());
  RemovePostTargetHandler(toplevel_window_event_handler_.get());
  RemovePreTargetHandler(system_gesture_filter_.get());
  RemovePreTargetHandler(mouse_cursor_filter_.get());
  RemovePreTargetHandler(modality_filter_.get());

  // TooltipController is deleted with the Shell so removing its references.
  RemovePreTargetHandler(tooltip_controller_.get());

  screen_orientation_controller_.reset();
  screen_layout_observer_.reset();

  // Destroy the virtual keyboard controller before the tablet mode controller
  // since the latters destructor triggers events that the former is listening
  // to but no longer cares about.
  virtual_keyboard_controller_.reset();

  // Destroy tablet mode controller early on since it has some observers which
  // need to be removed.
  tablet_mode_controller_.reset();

  // Destroy the keyboard before closing the shelf, since it will invoke a shelf
  // layout.
  DestroyKeyboard();

  toast_manager_.reset();

  // Destroy SystemTrayDelegate before destroying the status area(s). Make sure
  // to deinitialize the shelf first, as it is initialized after the delegate.
  for (aura::Window* root : GetAllRootWindows())
    Shelf::ForWindow(root)->ShutdownShelfWidget();
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
  highlighter_controller_.reset();

  // This also deletes all RootWindows. Note that we invoke Shutdown() on
  // WindowTreeHostManager before resetting |window_tree_host_manager_|, since
  // destruction of its owned RootWindowControllers relies on the value.
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

  // Notify the ShellDelegate that the shelf is shutting down.
  // TODO(msw): Refine ChromeLauncherController lifetime management.
  shell_delegate_->ShelfShutdown();

  // Removes itself as an observer of |pref_service_|.
  shelf_controller_.reset();

  shell_port_->Shutdown();
  window_tree_host_manager_->Shutdown();

  // Depends on |focus_controller_|, so must be destroyed before.
  window_tree_host_manager_.reset();
  focus_controller_->RemoveObserver(this);
  if (config != Config::CLASSIC &&
      window_tree_client_->focus_synchronizer()->active_focus_client() ==
          focus_controller_.get()) {
    window_tree_client_->focus_synchronizer()->SetSingletonFocusClient(nullptr);
  }
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
  shell_port_.reset();
  session_controller_->RemoveObserver(this);
  wallpaper_delegate_.reset();
  // NightLightController depeneds on the PrefService and must be destructed
  // before it. crbug.com/724231.
  night_light_controller_ = nullptr;
  profile_pref_service_ = nullptr;
  shell_delegate_.reset();

  for (auto& observer : shell_observers_)
    observer.OnShellDestroyed();

  DCHECK(instance_ == this);
  instance_ = nullptr;
}

void Shell::Init(const ShellInitParams& init_params) {
  const Config config = shell_port_->GetAshConfig();

  if (NightLightController::IsFeatureEnabled())
    night_light_controller_ = base::MakeUnique<NightLightController>();

  wallpaper_delegate_ = shell_delegate_->CreateWallpaperDelegate();

  // Connector can be null in tests.
  if (config == Config::MASH && shell_delegate_->GetShellConnector()) {
    // Connect to local state prefs now, but wait for an active user before
    // connecting to the profile pref service. The login screen has a temporary
    // user profile that is not associated with a real user.
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    prefs::ConnectToPrefService(
        shell_delegate_->GetShellConnector(), std::move(pref_registry),
        base::Bind(&Shell::OnLocalStatePrefServiceInitialized,
                   weak_factory_.GetWeakPtr()),
        prefs::mojom::kLocalStateServiceName);
  }

  // Some delegates access ShellPort during their construction. Create them here
  // instead of the ShellPort constructor.
  accessibility_delegate_.reset(shell_delegate_->CreateAccessibilityDelegate());
  palette_delegate_ = shell_delegate_->CreatePaletteDelegate();
  toast_manager_ = base::MakeUnique<ToastManager>();

  // Install the custom factory early on so that views::FocusManagers for Tray,
  // Shelf, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  wallpaper_controller_ = base::MakeUnique<WallpaperController>();

  if (config == Config::MASH)
    app_list_delegate_impl_ = base::MakeUnique<AppListDelegateImpl>();

  // TODO(sky): move creation to ShellPort.
  if (config != Config::MASH)
    immersive_handler_factory_ = base::MakeUnique<ImmersiveHandlerFactoryAsh>();

  window_positioner_ = base::MakeUnique<WindowPositioner>();

  if (config == Config::CLASSIC) {
    native_cursor_manager_ = new NativeCursorManagerAshClassic;
    cursor_manager_ = base::MakeUnique<CursorManager>(
        base::WrapUnique(native_cursor_manager_));
  } else if (config == Config::MUS) {
    native_cursor_manager_ = new NativeCursorManagerAshMus;
    cursor_manager_ = base::MakeUnique<CursorManager>(
        base::WrapUnique(native_cursor_manager_));
  }

  shell_delegate_->PreInit();
  bool display_initialized = display_manager_->InitFromCommandLine();
  if (!display_initialized && config != Config::CLASSIC) {
    // Run display configuration off device in mus mode.
    display_manager_->set_configure_displays(true);
    display_configurator_->set_configure_display(true);
  }
  display_configuration_controller_ =
      base::MakeUnique<DisplayConfigurationController>(
          display_manager_.get(), window_tree_host_manager_.get());
  display_configurator_->Init(shell_port_->CreateNativeDisplayDelegate(),
                              !gpu_support_->IsPanelFittingDisabled());

  // The DBusThreadManager must outlive this Shell. See the DCHECK in ~Shell.
  chromeos::DBusThreadManager* dbus_thread_manager =
      chromeos::DBusThreadManager::Get();
  projecting_observer_.reset(
      new ProjectingObserver(dbus_thread_manager->GetPowerManagerClient()));
  display_configurator_->AddObserver(projecting_observer_.get());
  AddShellObserver(projecting_observer_.get());

  if (!display_initialized &&
      (config != Config::CLASSIC || chromeos::IsRunningAsSystemCompositor())) {
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
  display_color_manager_ =
      base::MakeUnique<DisplayColorManager>(display_configurator_.get());

  if (!display_initialized)
    display_manager_->InitDefaultDisplay();

  if (config == Config::CLASSIC) {
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
  if (config != Config::CLASSIC) {
    window_tree_client_->focus_synchronizer()->SetSingletonFocusClient(
        focus_controller_.get());
  }

  screen_position_controller_.reset(new ScreenPositionController);

  window_tree_host_manager_->Start();
  AshWindowTreeHostInitParams ash_init_params;
  window_tree_host_manager_->CreatePrimaryHost(ash_init_params);

  root_window_for_new_windows_ = GetPrimaryRootWindow();

  resolution_notification_controller_ =
      base::MakeUnique<ResolutionNotificationController>();

  if (cursor_manager_) {
    cursor_manager_->SetDisplay(
        display::Screen::GetScreen()->GetPrimaryDisplay());
  }

  accelerator_controller_ = shell_port_->CreateAcceleratorController();
  tablet_mode_controller_ = base::MakeUnique<TabletModeController>();

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

  accelerator_filter_.reset(new ::wm::AcceleratorFilter(
      std::unique_ptr<::wm::AcceleratorDelegate>(new AcceleratorDelegate),
      accelerator_controller_->accelerator_history()));
  AddPreTargetHandler(accelerator_filter_.get());

  event_transformation_handler_.reset(new EventTransformationHandler);
  AddPreTargetHandler(event_transformation_handler_.get());

  toplevel_window_event_handler_ =
      base::MakeUnique<ToplevelWindowEventHandler>();

  if (config != Config::MASH) {
    system_gesture_filter_.reset(new SystemGestureEventFilter);
    AddPreTargetHandler(system_gesture_filter_.get());
  }

  sticky_keys_controller_.reset(new StickyKeysController);
  screen_pinning_controller_ = base::MakeUnique<ScreenPinningController>();

  lock_state_controller_ =
      base::MakeUnique<LockStateController>(shutdown_controller_.get());
  power_button_controller_.reset(
      new PowerButtonController(lock_state_controller_.get()));
  // Pass the initial display state to PowerButtonController.
  power_button_controller_->OnDisplayModeChanged(
      display_configurator_->cached_displays());

  // Forward user activity from the window server to |user_activity_detector_|.
  // The connector is unavailable in some tests.
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS &&
      shell_delegate_->GetShellConnector()) {
    ui::mojom::UserActivityMonitorPtr user_activity_monitor;
    shell_delegate_->GetShellConnector()->BindInterface(ui::mojom::kServiceName,
                                                        &user_activity_monitor);
    user_activity_forwarder_ = base::MakeUnique<aura::UserActivityForwarder>(
        std::move(user_activity_monitor), user_activity_detector_.get());
  }

  // In mash drag and drop is handled by mus.
  if (config != Config::MASH)
    drag_drop_controller_ = base::MakeUnique<DragDropController>();

  // |screenshot_controller_| needs to be created (and prepended as a
  // pre-target handler) at this point, because |mouse_cursor_filter_| needs to
  // process mouse events prior to screenshot session.
  // See http://crbug.com/459214
  screenshot_controller_.reset(new ScreenshotController());
  mouse_cursor_filter_ = base::MakeUnique<MouseCursorEventFilter>();
  PrependPreTargetHandler(mouse_cursor_filter_.get());

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_.reset(new AshVisibilityController);

  laser_pointer_controller_.reset(new LaserPointerController());
  partial_magnification_controller_.reset(new PartialMagnificationController());
  highlighter_controller_.reset(new HighlighterController());

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

  // Must occur after Shell has installed its early pre-target handlers (for
  // example, WindowModalityController).
  shell_port_->CreatePointerWatcherAdapter();

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
  touch_transformer_controller_ = base::MakeUnique<AshTouchTransformController>(
      display_configurator_.get(), display_manager_.get(),
      shell_port_->CreateTouchTransformDelegate());

  keyboard_ui_ = shell_port_->CreateKeyboardUI();

  window_tree_host_manager_->InitHosts();
  shell_port_->OnHostsInitialized();

  // Needs to be created after InitDisplays() since it may cause the virtual
  // keyboard to be deployed.
  if (config != Config::MASH)
    virtual_keyboard_controller_.reset(new VirtualKeyboardController);

  audio_a11y_controller_.reset(new chromeos::AudioA11yController);

  // Initialize the wallpaper after the RootWindowController has been created,
  // otherwise the widget will not paint when restoring after a browser crash.
  // Also, initialize after display initialization to ensure correct sizing.
  wallpaper_delegate_->InitializeWallpaper();

  if (cursor_manager_) {
    if (initially_hide_cursor_)
      cursor_manager_->HideCursor();
    cursor_manager_->SetCursor(ui::CursorType::kPointer);
  }

  power_event_observer_.reset(new PowerEventObserver());
  user_activity_notifier_.reset(
      new ui::UserActivityPowerManagerNotifier(user_activity_detector_.get()));
  video_activity_notifier_.reset(
      new VideoActivityNotifier(video_detector_.get()));
  bluetooth_notification_controller_.reset(new BluetoothNotificationController);
  screen_orientation_controller_ =
      base::MakeUnique<ScreenOrientationController>();
  screen_layout_observer_.reset(new ScreenLayoutObserver());
  sms_observer_.reset(new SmsObserver());

  split_view_controller_.reset(new SplitViewController());

  // The compositor thread and main message loop have to be running in
  // order to create mirror window. Run it after the main message loop
  // is started.
  display_manager_->CreateMirrorWindowAsyncIfAny();

  for (auto& observer : shell_observers_)
    observer.OnShellInitialized();

  user_metrics_recorder_->OnShellInitialized();
}

void Shell::InitRootWindow(aura::Window* root_window) {
  DCHECK(focus_controller_);
  DCHECK(visibility_controller_.get());

  aura::client::SetFocusClient(root_window, focus_controller_.get());
  ::wm::SetActivationClient(root_window, focus_controller_.get());
  root_window->AddPreTargetHandler(focus_controller_.get());
  aura::client::SetVisibilityClient(root_window, visibility_controller_.get());
  if (drag_drop_controller_) {
    DCHECK_NE(Config::MASH, GetAshConfig());
    aura::client::SetDragDropClient(root_window, drag_drop_controller_.get());
  } else {
    DCHECK_EQ(Config::MASH, GetAshConfig());
  }
  aura::client::SetScreenPositionClient(root_window,
                                        screen_position_controller_.get());
  aura::client::SetCursorClient(root_window, cursor_manager_.get());
  ::wm::SetTooltipClient(root_window, tooltip_controller_.get());
  aura::client::SetEventClient(root_window, event_client_.get());

  ::wm::SetWindowMoveClient(root_window, toplevel_window_event_handler_.get());
  root_window->AddPreTargetHandler(toplevel_window_event_handler_.get());
  root_window->AddPostTargetHandler(toplevel_window_event_handler_.get());
}

void Shell::SetSystemTrayDelegate(
    std::unique_ptr<SystemTrayDelegate> delegate) {
  DCHECK(delegate);
  system_tray_delegate_ = std::move(delegate);
  system_tray_delegate_->Initialize();
  // Accesses ShellPort in its constructor.
  logout_confirmation_controller_.reset(new LogoutConfirmationController(
      base::Bind(&SystemTrayController::SignOut,
                 base::Unretained(system_tray_controller_.get()))));
}

void Shell::DeleteSystemTrayDelegate() {
  DCHECK(system_tray_delegate_);
  // Accesses ShellPort in its destructor.
  logout_confirmation_controller_.reset();
  system_tray_delegate_.reset();
}

void Shell::CloseAllRootWindowChildWindows() {
  for (aura::Window* root : GetAllRootWindows()) {
    RootWindowController* controller = RootWindowController::ForWindow(root);
    if (controller) {
      controller->CloseChildWindows();
    } else {
      while (!root->children().empty()) {
        aura::Window* child = root->children()[0];
        delete child;
      }
    }
  }
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
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active)
    root_window_for_new_windows_ = gained_active->GetRootWindow();
}

void Shell::OnActiveUserSessionChanged(const AccountId& account_id) {
  if (GetAshConfig() == Config::MASH && shell_delegate_->GetShellConnector()) {
    // Profile pref service is null while connecting after profile switch.
    if (profile_pref_service_) {
      for (auto& observer : shell_observers_)
        observer.OnActiveUserPrefServiceChanged(nullptr);
      // Reset after notification so clients can unregister pref observers on
      // the old PrefService.
      profile_pref_service_.reset();
    }

    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    RegisterProfilePrefs(pref_registry.get());
    prefs::ConnectToPrefService(
        shell_delegate_->GetShellConnector(), pref_registry,
        base::Bind(&Shell::OnProfilePrefServiceInitialized,
                   weak_factory_.GetWeakPtr()),
        prefs::mojom::kForwarderServiceName);
    return;
  }

  // On classic ash user profile prefs are available immediately after login.
  // The login screen temporary profile is never available.
  PrefService* profile_prefs = shell_delegate_->GetActiveUserPrefService();
  for (auto& observer : shell_observers_)
    observer.OnActiveUserPrefServiceChanged(profile_prefs);
}

void Shell::OnSessionStateChanged(session_manager::SessionState state) {
  // Initialize the shelf when a session becomes active. It's safe to do this
  // multiple times (e.g. initial login vs. multiprofile add session).
  if (state == session_manager::SessionState::ACTIVE) {
    InitializeShelf();
  }
  // Recreates keyboard on user profile change, to refresh keyboard
  // extensions with the new profile and the extensions call proper IME.
  // |LOGGED_IN_NOT_ACTIVE| is needed so that the virtual keyboard works on
  // supervised user creation. crbug.com/712873
  // |ACTIVE| is also needed for guest user workflow.
  if ((state == session_manager::SessionState::LOGGED_IN_NOT_ACTIVE ||
       state == session_manager::SessionState::ACTIVE) &&
      keyboard::IsKeyboardEnabled()) {
    if (GetAshConfig() != Config::MASH) {
      // Recreate the keyboard after initial login and after multiprofile login.
      CreateKeyboard();
    }
  }
}

void Shell::OnLoginStatusChanged(LoginStatus login_status) {
  UpdateAfterLoginStatusChange(login_status);
}

void Shell::OnLockStateChanged(bool locked) {
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

void Shell::InitializeShelf() {
  // Must occur after SessionController creation and user login.
  DCHECK(session_controller());
  DCHECK_GT(session_controller()->NumberOfLoggedInUsers(), 0);

  // Notify the ShellDelegate that the shelf is being initialized.
  // TODO(msw): Refine ChromeLauncherController lifetime management.
  shell_delegate_->ShelfInit();

  if (!shelf_window_watcher_)
    shelf_window_watcher_ = base::MakeUnique<ShelfWindowWatcher>(shelf_model());

  for (RootWindowController* root : GetAllRootWindowControllers())
    root->InitializeShelf();
}

void Shell::OnProfilePrefServiceInitialized(
    std::unique_ptr<::PrefService> pref_service) {
  // |pref_service| can be null if can't connect to Chrome (as happens when
  // running mash outside of chrome --mash and chrome isn't built).
  for (auto& observer : shell_observers_)
    observer.OnActiveUserPrefServiceChanged(pref_service.get());
  // Reset after notifying clients so they can unregister pref observers on the
  // old PrefService.
  profile_pref_service_ = std::move(pref_service);
}

void Shell::OnLocalStatePrefServiceInitialized(
    std::unique_ptr<::PrefService> pref_service) {
  // |pref_service| is null if can't connect to Chrome (as happens when
  // running mash outside of chrome --mash and chrome isn't built).
  local_state_ = std::move(pref_service);
}

}  // namespace ash
