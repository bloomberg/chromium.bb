// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_delegate.h"
#include "ash/accelerators/ash_focus_manager_factory.h"
#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/app_list/app_list_delegate_impl.h"
#include "ash/ash_constants.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/cast_config_controller.h"
#include "ash/display/ash_display_controller.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_color_manager_chromeos.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_error_observer_chromeos.h"
#include "ash/display/display_shutdown_observer.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/projecting_observer_chromeos.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_ash.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/first_run/first_run_helper_impl.h"
#include "ash/focus_cycler.h"
#include "ash/frame/custom_frame_view_ash.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/ime/ime_controller.h"
#include "ash/keyboard/keyboard_ui.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login_status.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/media_controller.h"
#include "ash/message_center/message_center_controller.h"
#include "ash/metrics/time_to_first_present_recorder.h"
#include "ash/new_window_controller.h"
#include "ash/note_taking_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screenshot_delegate.h"
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
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/brightness/brightness_controller_chromeos.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/locale/locale_notification_controller.h"
#include "ash/system/network/sms_observer.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/night_light/night_light_controller.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/palette/palette_welcome_bubble.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/peripheral_battery_notifier.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/power_event_observer.h"
#include "ash/system/power/power_status.h"
#include "ash/system/power/video_activity_notifier.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/tray_action/tray_action.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/virtual_keyboard_controller.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
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
#include "chromeos/chromeos_switches.h"
#include "chromeos/system/devicemode.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mash/public/interfaces/launchable.mojom.h"
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
#include "ui/display/mojo/dev_display_controller.mojom.h"
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
  AshVisibilityController() = default;
  ~AshVisibilityController() override = default;

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

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

// static
Shell* Shell::CreateInstance(ShellInitParams init_params) {
  CHECK(!instance_);
  instance_ = new Shell(std::move(init_params.delegate),
                        std::move(init_params.shell_port));
  instance_->Init(init_params.context_factory,
                  init_params.context_factory_private);
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
int Shell::GetOpenSystemModalWindowContainerId() {
  // The test boolean is not static to avoid leaking state between tests.
  if (Get()->simulate_modal_window_open_for_test_)
    return kShellWindowId_SystemModalContainer;

  // Traverse all system modal containers, and find its direct child window
  // with "SystemModal" setting, and visible.
  // Note: LockSystemModalContainer is more restrictive, so make it preferable
  // to SystemModalCotainer.
  constexpr int modal_window_ids[] = {kShellWindowId_LockSystemModalContainer,
                                      kShellWindowId_SystemModalContainer};
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    for (int modal_window_id : modal_window_ids) {
      aura::Window* system_modal = root->GetChildById(modal_window_id);
      if (!system_modal)
        continue;
      for (const aura::Window* child : system_modal->children()) {
        if (child->GetProperty(aura::client::kModalKey) ==
                ui::MODAL_TYPE_SYSTEM &&
            child->layer()->GetTargetVisibility()) {
          return modal_window_id;
        }
      }
    }
  }
  return -1;
}

// static
bool Shell::IsSystemModalWindowOpen() {
  return GetOpenSystemModalWindowContainerId() >= 0;
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
void Shell::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  PaletteTray::RegisterLocalStatePrefs(registry);
  WallpaperController::RegisterLocalStatePrefs(registry);
  BluetoothPowerController::RegisterLocalStatePrefs(registry);
}

// static
void Shell::RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  AccessibilityController::RegisterProfilePrefs(registry, for_test);
  BluetoothPowerController::RegisterProfilePrefs(registry);
  LoginScreenController::RegisterProfilePrefs(registry, for_test);
  LogoutButtonTray::RegisterProfilePrefs(registry);
  NightLightController::RegisterProfilePrefs(registry);
  PaletteTray::RegisterProfilePrefs(registry);
  PaletteWelcomeBubble::RegisterProfilePrefs(registry);
  ShelfController::RegisterProfilePrefs(registry);
  TouchDevicesController::RegisterProfilePrefs(registry);
  TrayCapsLock::RegisterProfilePrefs(registry, for_test);
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
  DCHECK(switches::IsNightLightEnabled());
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
  return local_state_.get();
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

FirstRunHelper* Shell::CreateFirstRunHelper() {
  return new FirstRunHelperImpl;
}

void Shell::SetLargeCursorSizeInDip(int large_cursor_size_in_dip) {
  window_tree_host_manager_->cursor_window_controller()
      ->SetLargeCursorSizeInDip(large_cursor_size_in_dip);
}

void Shell::UpdateCursorCompositingEnabled() {
  SetCursorCompositingEnabled(
      window_tree_host_manager_->cursor_window_controller()
          ->ShouldEnableCursorCompositing());
}

void Shell::SetCursorCompositingEnabled(bool enabled) {
  if (GetAshConfig() != Config::MASH) {
    // TODO: needs to work in mash. http://crbug.com/705592.
    CursorWindowController* cursor_window_controller =
        window_tree_host_manager_->cursor_window_controller();

    if (cursor_window_controller->is_cursor_compositing_enabled() == enabled)
      return;
    cursor_window_controller->SetCursorCompositingEnabled(enabled);
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

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

Shell::Shell(std::unique_ptr<ShellDelegate> shell_delegate,
             std::unique_ptr<ShellPort> shell_port)
    : shell_port_(std::move(shell_port)),
      ash_display_controller_(std::make_unique<AshDisplayController>()),
      brightness_control_delegate_(
          std::make_unique<system::BrightnessControllerChromeos>()),
      cast_config_(std::make_unique<CastConfigController>()),
      focus_cycler_(std::make_unique<FocusCycler>()),
      ime_controller_(std::make_unique<ImeController>()),
      immersive_context_(std::make_unique<ImmersiveContextAsh>()),
      keyboard_brightness_control_delegate_(
          std::make_unique<KeyboardBrightnessController>()),
      locale_notification_controller_(
          std::make_unique<LocaleNotificationController>()),
      login_screen_controller_(std::make_unique<LoginScreenController>()),
      media_controller_(std::make_unique<MediaController>()),
      new_window_controller_(std::make_unique<NewWindowController>()),
      session_controller_(std::make_unique<SessionController>(
          shell_delegate->GetShellConnector())),
      note_taking_controller_(std::make_unique<NoteTakingController>()),
      shell_delegate_(std::move(shell_delegate)),
      shutdown_controller_(std::make_unique<ShutdownController>()),
      system_tray_controller_(std::make_unique<SystemTrayController>()),
      system_tray_notifier_(std::make_unique<SystemTrayNotifier>()),
      vpn_list_(std::make_unique<VpnList>()),
      window_cycle_controller_(std::make_unique<WindowCycleController>()),
      window_selector_controller_(std::make_unique<WindowSelectorController>()),
      app_list_(std::make_unique<app_list::AppList>()),
      tray_bluetooth_helper_(std::make_unique<TrayBluetoothHelper>()),
      display_configurator_(new display::DisplayConfigurator()),
      native_cursor_manager_(nullptr),
      weak_factory_(this) {
  // TODO(sky): better refactor cash/mash dependencies. Perhaps put all cash
  // state on ShellPortClassic. http://crbug.com/671246.

  display_manager_.reset(ScreenAsh::CreateDisplayManager());
  window_tree_host_manager_ = std::make_unique<WindowTreeHostManager>();
  user_metrics_recorder_ = std::make_unique<UserMetricsRecorder>();

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

  for (aura::Window* root : GetAllRootWindows())
    Shelf::ForWindow(root)->ShutdownShelfWidget();
  tray_bluetooth_helper_.reset();

  // Accesses root window containers.
  logout_confirmation_controller_.reset();

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

  // |split_view_controller_| needs to be deleted after
  // |window_selector_controller_|.
  split_view_controller_.reset();

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

  tray_action_.reset();

  power_button_controller_.reset();
  lock_state_controller_.reset();
  backlights_forced_off_setter_.reset();

  screen_pinning_controller_.reset();

  resolution_notification_controller_.reset();
  screenshot_controller_.reset();
  mouse_cursor_filter_.reset();
  modality_filter_.reset();

  touch_transformer_controller_.reset();
  laser_pointer_controller_.reset();
  partial_magnification_controller_.reset();
  highlighter_controller_.reset();
  voice_interaction_controller_.reset();

  // This also deletes all RootWindows. Note that we invoke Shutdown() on
  // WindowTreeHostManager before resetting |window_tree_host_manager_|, since
  // destruction of its owned RootWindowControllers relies on the value.
  ScreenAsh::CreateScreenForShutdown();
  display_configuration_controller_.reset();

  // AppListDelegateImpl depends upon AppList.
  app_list_delegate_impl_.reset();

  // These members access Shell in their destructors.
  wallpaper_controller_.reset();
  accessibility_controller_.reset();
  accessibility_delegate_.reset();
  message_center_controller_.reset();

  // Balances the Install() in Initialize().
  views::FocusManagerFactory::Install(nullptr);

  // ShelfWindowWatcher has window observers and a pointer to the shelf model.
  shelf_window_watcher_.reset();

  // Removes itself as an observer of |pref_service_|.
  shelf_controller_.reset();

  // NightLightController depends on the PrefService as well as the window tree
  // host manager, and must be destructed before them. crbug.com/724231.
  night_light_controller_ = nullptr;

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
  projecting_observer_.reset();

  if (display_change_observer_)
    display_configurator_->RemoveObserver(display_change_observer_.get());
  if (display_error_observer_)
    display_configurator_->RemoveObserver(display_error_observer_.get());
  display_change_observer_.reset();
  display_shutdown_observer_.reset();

  PowerStatus::Shutdown();

  // Needs to happen right before |instance_| is reset.
  shell_port_.reset();
  session_controller_->RemoveObserver(this);
  wallpaper_delegate_.reset();
  // BluetoothPowerController depends on the PrefService and must be destructed
  // before it.
  bluetooth_power_controller_ = nullptr;
  // TouchDevicesController depends on the PrefService and must be destructed
  // before it.
  touch_devices_controller_ = nullptr;

  local_state_.reset();
  shell_delegate_.reset();

  for (auto& observer : shell_observers_)
    observer.OnShellDestroyed();

  DCHECK(instance_ == this);
  instance_ = nullptr;
}

void Shell::Init(ui::ContextFactory* context_factory,
                 ui::ContextFactoryPrivate* context_factory_private) {
  const Config config = shell_port_->GetAshConfig();

  // These controllers call Shell::Get() in their constructors, so they cannot
  // be in the member initialization list.
  if (switches::IsNightLightEnabled())
    night_light_controller_ = std::make_unique<NightLightController>();
  touch_devices_controller_ = std::make_unique<TouchDevicesController>();
  bluetooth_power_controller_ = std::make_unique<BluetoothPowerController>();

  wallpaper_delegate_ = shell_delegate_->CreateWallpaperDelegate();

  // Connector can be null in tests.
  if (shell_delegate_->GetShellConnector()) {
    // Connect to local state prefs now, but wait for an active user before
    // connecting to the profile pref service. The login screen has a temporary
    // user profile that is not associated with a real user.
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    RegisterLocalStatePrefs(pref_registry.get());
    prefs::ConnectToPrefService(
        shell_delegate_->GetShellConnector(), std::move(pref_registry),
        base::Bind(&Shell::OnLocalStatePrefServiceInitialized,
                   weak_factory_.GetWeakPtr()),
        prefs::mojom::kLocalStateServiceName);
  }

  // Some delegates access ShellPort during their construction. Create them here
  // instead of the ShellPort constructor.
  accessibility_delegate_.reset(shell_delegate_->CreateAccessibilityDelegate());
  accessibility_controller_ = std::make_unique<AccessibilityController>(
      shell_delegate_->GetShellConnector());
  toast_manager_ = std::make_unique<ToastManager>();

  // Install the custom factory early on so that views::FocusManagers for Tray,
  // Shelf, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  wallpaper_controller_ = std::make_unique<WallpaperController>();

  app_list_delegate_impl_ = std::make_unique<AppListDelegateImpl>();

  // TODO(sky): move creation to ShellPort.
  if (config != Config::MASH)
    immersive_handler_factory_ = std::make_unique<ImmersiveHandlerFactoryAsh>();

  window_positioner_ = std::make_unique<WindowPositioner>();

  if (config == Config::CLASSIC) {
    native_cursor_manager_ = new NativeCursorManagerAshClassic;
    cursor_manager_ = std::make_unique<CursorManager>(
        base::WrapUnique(native_cursor_manager_));
  } else if (config == Config::MUS) {
    native_cursor_manager_ = new NativeCursorManagerAshMus;
    cursor_manager_ = std::make_unique<CursorManager>(
        base::WrapUnique(native_cursor_manager_));
  }

  // TODO(stevenjb): ChromeShellDelegate::PreInit currently handles
  // DisplayPreference initialization, required for InitializeDisplayManager.
  // Before we can move that code into ash/display where it belongs, we need to
  // wait for |lcoal_state_| to be set in OnLocalStatePrefServiceInitialized
  // before initializing DisplayPreferences (and therefore DisplayManager).
  // http://crbug.com/678949.
  shell_delegate_->PreInit();

  InitializeDisplayManager();

  if (config == Config::CLASSIC) {
    // This will initialize aura::Env which requires |display_manager_| to
    // be initialized first.
    aura::Env::GetInstance()->set_context_factory(context_factory);
    aura::Env::GetInstance()->set_context_factory_private(
        context_factory_private);
  }

  // The WindowModalityController needs to be at the front of the input event
  // pretarget handler list to ensure that it processes input events when modal
  // windows are active.
  window_modality_controller_.reset(new ::wm::WindowModalityController(this));

  env_filter_.reset(new ::wm::CompoundEventFilter);
  AddPreTargetHandler(env_filter_.get());

  // FocusController takes ownership of AshFocusRules.
  focus_controller_ =
      std::make_unique<::wm::FocusController>(new wm::AshFocusRules());
  focus_controller_->AddObserver(this);
  if (config != Config::CLASSIC) {
    window_tree_client_->focus_synchronizer()->SetSingletonFocusClient(
        focus_controller_.get());
  }

  screen_position_controller_.reset(new ScreenPositionController);

  window_tree_host_manager_->Start();
  AshWindowTreeHostInitParams ash_init_params;
  window_tree_host_manager_->CreatePrimaryHost(ash_init_params);

  time_to_first_present_recorder_ =
      std::make_unique<TimeToFirstPresentRecorder>(GetPrimaryRootWindow());

  root_window_for_new_windows_ = GetPrimaryRootWindow();

  resolution_notification_controller_ =
      std::make_unique<ResolutionNotificationController>();

  if (cursor_manager_) {
    cursor_manager_->SetDisplay(
        display::Screen::GetScreen()->GetPrimaryDisplay());
  }

  accelerator_controller_ = shell_port_->CreateAcceleratorController();
  tablet_mode_controller_ = std::make_unique<TabletModeController>();
  shelf_controller_ = std::make_unique<ShelfController>();

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
      std::make_unique<ToplevelWindowEventHandler>();

  if (config != Config::MASH) {
    system_gesture_filter_.reset(new SystemGestureEventFilter);
    AddPreTargetHandler(system_gesture_filter_.get());
  }

  sticky_keys_controller_.reset(new StickyKeysController);
  screen_pinning_controller_ = std::make_unique<ScreenPinningController>();

  backlights_forced_off_setter_ = std::make_unique<BacklightsForcedOffSetter>();

  tray_action_ =
      std::make_unique<TrayAction>(backlights_forced_off_setter_.get());

  lock_state_controller_ =
      std::make_unique<LockStateController>(shutdown_controller_.get());
  power_button_controller_ = std::make_unique<PowerButtonController>(
      backlights_forced_off_setter_.get());
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
    user_activity_forwarder_ = std::make_unique<aura::UserActivityForwarder>(
        std::move(user_activity_monitor), user_activity_detector_.get());
  }

  // In mash drag and drop is handled by mus.
  if (config != Config::MASH)
    drag_drop_controller_ = std::make_unique<DragDropController>();

  // |screenshot_controller_| needs to be created (and prepended as a
  // pre-target handler) at this point, because |mouse_cursor_filter_| needs to
  // process mouse events prior to screenshot session.
  // See http://crbug.com/459214
  screenshot_controller_ = std::make_unique<ScreenshotController>(
      shell_delegate_->CreateScreenshotDelegate());
  mouse_cursor_filter_ = std::make_unique<MouseCursorEventFilter>();
  PrependPreTargetHandler(mouse_cursor_filter_.get());

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_.reset(new AshVisibilityController);

  laser_pointer_controller_.reset(new LaserPointerController());
  partial_magnification_controller_.reset(new PartialMagnificationController());
  highlighter_controller_.reset(new HighlighterController());
  voice_interaction_controller_ =
      std::make_unique<VoiceInteractionController>();

  magnification_controller_.reset(MagnificationController::CreateInstance());
  mru_window_tracker_ = std::make_unique<MruWindowTracker>();

  autoclick_controller_.reset(AutoclickController::CreateInstance());

  high_contrast_controller_.reset(new HighContrastController);

  viz::mojom::VideoDetectorObserverPtr observer;
  video_detector_ =
      std::make_unique<VideoDetector>(mojo::MakeRequest(&observer));
  shell_port_->AddVideoDetectorObserver(std::move(observer));

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

  logout_confirmation_controller_ =
      std::make_unique<LogoutConfirmationController>();

  // May trigger initialization of the Bluetooth adapter.
  tray_bluetooth_helper_->Initialize();

  // Create AshTouchTransformController before
  // WindowTreeHostManager::InitDisplays()
  // since AshTouchTransformController listens on
  // WindowTreeHostManager::Observer::OnDisplaysInitialized().
  touch_transformer_controller_ = std::make_unique<AshTouchTransformController>(
      display_configurator_.get(), display_manager_.get(),
      shell_port_->CreateTouchTransformDelegate());

  keyboard_ui_ = shell_port_->CreateKeyboardUI();

  window_tree_host_manager_->InitHosts();
  shell_port_->OnHostsInitialized();

  // Needs to be created after InitDisplays() since it may cause the virtual
  // keyboard to be deployed.
  if (config != Config::MASH)
    virtual_keyboard_controller_.reset(new VirtualKeyboardController);

  if (cursor_manager_) {
    cursor_manager_->HideCursor();  // Hide the mouse cursor on startup.
    cursor_manager_->SetCursor(ui::CursorType::kPointer);
  }

  peripheral_battery_notifier_ = std::make_unique<PeripheralBatteryNotifier>();
  power_event_observer_.reset(new PowerEventObserver());
  user_activity_notifier_.reset(
      new ui::UserActivityPowerManagerNotifier(user_activity_detector_.get()));
  video_activity_notifier_.reset(
      new VideoActivityNotifier(video_detector_.get()));
  bluetooth_notification_controller_.reset(new BluetoothNotificationController);
  screen_orientation_controller_ =
      std::make_unique<ScreenOrientationController>();
  screen_layout_observer_.reset(new ScreenLayoutObserver());
  sms_observer_.reset(new SmsObserver());

  split_view_controller_.reset(new SplitViewController());

  // The compositor thread and main message loop have to be running in
  // order to create mirror window. Run it after the main message loop
  // is started.
  display_manager_->CreateMirrorWindowAsyncIfAny();

  message_center_controller_ = std::make_unique<MessageCenterController>();

  // Mash implements the show taps feature with a separate mojo app.
  // GetShellConnector() is null in unit tests.
  if (config == Config::MASH && shell_delegate_->GetShellConnector() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kShowTaps)) {
    mash::mojom::LaunchablePtr launchable;
    shell_delegate_->GetShellConnector()->BindInterface("touch_hud",
                                                        &launchable);
    launchable->Launch(mash::mojom::kWindow, mash::mojom::LaunchMode::DEFAULT);
  }

  for (auto& observer : shell_observers_)
    observer.OnShellInitialized();

  user_metrics_recorder_->OnShellInitialized();
}

void Shell::InitializeDisplayManager() {
  const Config config = shell_port_->GetAshConfig();
  bool display_initialized = display_manager_->InitFromCommandLine();

  if (!display_initialized && config != Config::CLASSIC) {
    // Run display configuration off device in mus mode.
    display_manager_->set_configure_displays(true);
    display_configurator_->set_configure_display(true);
  }
  display_configuration_controller_ =
      std::make_unique<DisplayConfigurationController>(
          display_manager_.get(), window_tree_host_manager_.get());
  display_configurator_->Init(shell_port_->CreateNativeDisplayDelegate(),
                              false);

  projecting_observer_ =
      std::make_unique<ProjectingObserver>(display_configurator_.get());

  if (!display_initialized) {
    if (config != Config::CLASSIC && !chromeos::IsRunningAsSystemCompositor()) {
      display::mojom::DevDisplayControllerPtr controller;
      shell_delegate_->GetShellConnector()->BindInterface(
          ui::mojom::kServiceName, &controller);
      display_manager_->SetDevDisplayController(std::move(controller));
    }

    if (config != Config::CLASSIC || chromeos::IsRunningAsSystemCompositor()) {
      display_change_observer_ =
          std::make_unique<display::DisplayChangeObserver>(
              display_configurator_.get(), display_manager_.get());

      display_shutdown_observer_ = std::make_unique<DisplayShutdownObserver>(
          display_configurator_.get());

      // Register |display_change_observer_| first so that the rest of
      // observer gets invoked after the root windows are configured.
      display_configurator_->AddObserver(display_change_observer_.get());
      display_error_observer_.reset(new DisplayErrorObserver());
      display_configurator_->AddObserver(display_error_observer_.get());
      display_configurator_->set_state_controller(
          display_change_observer_.get());
      display_configurator_->set_mirroring_controller(display_manager_.get());
      display_configurator_->ForceInitialConfigure();
      display_initialized = true;
    }
  }

  display_color_manager_ =
      std::make_unique<DisplayColorManager>(display_configurator_.get());

  if (!display_initialized)
    display_manager_->InitDefaultDisplay();

  if (config == Config::CLASSIC)
    display_manager_->RefreshFontParams();
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

void Shell::OnSessionStateChanged(session_manager::SessionState state) {
  // Initialize the |shelf_window_watcher_| when a session becomes active.
  // Shelf itself is initialized in RootWindowController.
  if (state == session_manager::SessionState::ACTIVE) {
    if (!shelf_window_watcher_)
      shelf_window_watcher_ =
          std::make_unique<ShelfWindowWatcher>(shelf_model());
  }

  // NOTE: keyboard::IsKeyboardEnabled() is false in mash, but may not be in
  // unit tests. crbug.com/646565.
  if (keyboard::IsKeyboardEnabled()) {
    switch (state) {
      case session_manager::SessionState::OOBE:
      case session_manager::SessionState::LOGIN_PRIMARY:
        // Ensure that the keyboard controller is activated for the primary
        // window.
        GetPrimaryRootWindowController()->ActivateKeyboard(
            keyboard::KeyboardController::GetInstance());
        break;
      case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
      case session_manager::SessionState::ACTIVE:
        // Recreate the keyboard on user profile change, to refresh keyboard
        // extensions with the new profile and ensure the extensions call the
        // proper IME. |LOGGED_IN_NOT_ACTIVE| is needed so that the virtual
        // keyboard works on supervised user creation, http://crbug.com/712873.
        // |ACTIVE| is also needed for guest user workflow.
        CreateKeyboard();
        break;
      default:
        break;
    }
  }

  shell_port_->UpdateSystemModalAndBlockingContainers();
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

void Shell::OnLocalStatePrefServiceInitialized(
    std::unique_ptr<::PrefService> pref_service) {
  DCHECK(!local_state_);
  // |pref_service| is null if can't connect to Chrome (as happens when
  // running mash outside of chrome --mash and chrome isn't built).
  if (!pref_service)
    return;

  local_state_ = std::move(pref_service);

  for (auto& observer : shell_observers_)
    observer.OnLocalStatePrefServiceInitialized(local_state_.get());
}

}  // namespace ash
