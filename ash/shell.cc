// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/accelerators/accelerator_delegate.h"
#include "ash/aura/wm_shell_aura.h"
#include "ash/aura/wm_window_aura.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/frame/custom_frame_view_ash.h"
#include "ash/common/gpu_support.h"
#include "ash/common/keyboard/keyboard_ui.h"
#include "ash/common/login_status.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/app_list_shelf_item_delegate.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/maximize_mode/maximize_mode_window_manager.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/root_window_finder.h"
#include "ash/common/wm/system_modal_container_layout_manager.h"
#include "ash/common/wm/window_positioner.h"
#include "ash/common/wm/workspace_controller.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/screen_ash.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/first_run/first_run_helper_impl.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/ime/input_method_event_handler.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell_init_params.h"
#include "ash/system/chromeos/screen_layout_observer.h"
#include "ash/utility/screenshot_controller.h"
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
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/message_center/message_center.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/accelerator_filter.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_modality_controller.h"

#if defined(OS_CHROMEOS)
#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"  // nogncheck
#endif                           // defined(USE_X11)
#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/common/ash_constants.h"
#include "ash/common/system/chromeos/bluetooth/bluetooth_notification_controller.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/display/display_change_observer_chromeos.h"
#include "ash/display/display_color_manager_chromeos.h"
#include "ash/display/display_error_observer_chromeos.h"
#include "ash/display/projecting_observer_chromeos.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/system/chromeos/power/power_event_observer.h"
#include "ash/system/chromeos/power/video_activity_notifier.h"
#include "ash/touch/touch_transformer_controller.h"
#include "ash/virtual_keyboard_controller.h"
#include "base/bind_helpers.h"
#include "base/sys_info.h"
#include "chromeos/audio/audio_a11y_controller.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/display/chromeos/display_configurator.h"

#if defined(USE_X11)
#include "ui/display/chromeos/x11/native_display_delegate_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/public/ozone_platform.h"
#endif
#endif  // defined(OS_CHROMEOS)

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
bool Shell::initially_hide_cursor_ = false;

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

// static
Shell* Shell::CreateInstance(const ShellInitParams& init_params) {
  CHECK(!instance_);
  instance_ = new Shell(init_params.delegate);
  instance_->Init(init_params);
  return instance_;
}

// static
Shell* Shell::GetInstance() {
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
  return Shell::GetInstance()
      ->window_tree_host_manager()
      ->GetAllRootWindowControllers();
}

// static
aura::Window* Shell::GetPrimaryRootWindow() {
  CHECK(HasInstance());
  return GetInstance()->window_tree_host_manager()->GetPrimaryRootWindow();
}

// static
aura::Window* Shell::GetTargetRootWindow() {
  CHECK(WmShell::HasInstance());
  return WmWindowAura::GetAuraWindow(
      WmShell::Get()->GetRootWindowForNewWindows());
}

// static
int64_t Shell::GetTargetDisplayId() {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(GetTargetRootWindow())
      .id();
}

// static
aura::Window::Windows Shell::GetAllRootWindows() {
  CHECK(HasInstance());
  return Shell::GetInstance()->window_tree_host_manager()->GetAllRootWindows();
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
  if (!window_tree_host_manager_->UpdateWorkAreaOfDisplayNearestWindow(
          contains, insets)) {
    return;
  }
}

void Shell::OnLoginStateChanged(LoginStatus status) {
  for (auto& observer : *wm_shell_->shell_observers())
    observer.OnLoginStateChanged(status);
}

void Shell::OnLoginUserProfilePrepared() {
  wm_shell_->CreateShelf();
  CreateKeyboard();
}

void Shell::OnAppTerminating() {
  for (auto& observer : *wm_shell_->shell_observers())
    observer.OnAppTerminating();
}

void Shell::OnLockStateChanged(bool locked) {
  for (auto& observer : *wm_shell_->shell_observers())
    observer.OnLockStateChanged(locked);
#ifndef NDEBUG
  // Make sure that there is no system modal in Lock layer when unlocked.
  if (!locked) {
    std::vector<WmWindow*> containers = wm::GetContainersFromAllRootWindows(
        kShellWindowId_LockSystemModalContainer,
        WmWindowAura::Get(GetPrimaryRootWindow()));
    for (WmWindow* container : containers)
      DCHECK(container->GetChildren().empty());
  }
#endif
}

void Shell::OnCastingSessionStartedOrStopped(bool started) {
#if defined(OS_CHROMEOS)
  for (auto& observer : *wm_shell_->shell_observers())
    observer.OnCastingSessionStartedOrStopped(started);
#endif
}

void Shell::OnRootWindowAdded(WmWindow* root_window) {
  for (auto& observer : *wm_shell_->shell_observers())
    observer.OnRootWindowAdded(root_window);
}

void Shell::CreateKeyboard() {
  InitKeyboard();
  GetPrimaryRootWindowController()->ActivateKeyboard(
      keyboard::KeyboardController::GetInstance());
}

void Shell::DeactivateKeyboard() {
  // TODO(jamescook): Move keyboard create and hide into WmShell.
  wm_shell_->keyboard_ui()->Hide();
  if (keyboard::KeyboardController::GetInstance()) {
    RootWindowControllerList controllers = GetAllRootWindowControllers();
    for (RootWindowControllerList::iterator iter = controllers.begin();
         iter != controllers.end(); ++iter) {
      (*iter)->DeactivateKeyboard(keyboard::KeyboardController::GetInstance());
    }
  }
  keyboard::KeyboardController::ResetInstance(nullptr);
}

#if defined(OS_CHROMEOS)
bool Shell::ShouldSaveDisplaySettings() {
  return !(
      screen_orientation_controller_->ignore_display_configuration_updates() ||
      resolution_notification_controller_->DoesNotificationTimeout());
}
#endif

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
  for (auto& observer : *wm_shell_->shell_observers())
    observer.OnTouchHudProjectionToggled(enabled);
}

#if defined(OS_CHROMEOS)
FirstRunHelper* Shell::CreateFirstRunHelper() {
  return new FirstRunHelperImpl;
}

void Shell::SetCursorCompositingEnabled(bool enabled) {
  window_tree_host_manager_->cursor_window_controller()
      ->SetCursorCompositingEnabled(enabled);
  native_cursor_manager_->SetNativeCursorEnabled(!enabled);
}
#endif  // defined(OS_CHROMEOS)

void Shell::DoInitialWorkspaceAnimation() {
  return GetPrimaryRootWindowController()
      ->workspace_controller()
      ->DoInitialAnimation();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

Shell::Shell(ShellDelegate* delegate)
    : wm_shell_(new WmShellAura(base::WrapUnique(delegate))),
      link_handler_model_factory_(nullptr),
      activation_client_(nullptr),
#if defined(OS_CHROMEOS)
      display_configurator_(new ui::DisplayConfigurator()),
#endif  // defined(OS_CHROMEOS)
      native_cursor_manager_(nullptr),
      simulate_modal_window_open_for_testing_(false),
      is_touch_hud_projection_enabled_(false) {
  DCHECK(aura::Env::GetInstanceDontCreate());
  gpu_support_.reset(wm_shell_->delegate()->CreateGPUSupport());
  display_manager_.reset(ScreenAsh::CreateDisplayManager());
  window_tree_host_manager_.reset(new WindowTreeHostManager);
  user_metrics_recorder_.reset(new UserMetricsRecorder);

#if defined(OS_CHROMEOS)
  PowerStatus::Initialize();
#endif
}

Shell::~Shell() {
  TRACE_EVENT0("shutdown", "ash::Shell::Destructor");

  user_metrics_recorder_->OnShellShuttingDown();

  wm_shell_->delegate()->PreShutdown();

  // Remove the focus from any window. This will prevent overhead and side
  // effects (e.g. crashes) from changing focus during shutdown.
  // See bug crbug.com/134502.
  aura::client::GetFocusClient(GetPrimaryRootWindow())->FocusWindow(nullptr);

  // Please keep in same order as in Init() because it's easy to miss one.
  if (window_modality_controller_)
    window_modality_controller_.reset();
  RemovePreTargetHandler(
      window_tree_host_manager_->input_method_event_handler());
#if defined(OS_CHROMEOS)
  RemovePreTargetHandler(magnifier_key_scroll_handler_.get());
  magnifier_key_scroll_handler_.reset();

  RemovePreTargetHandler(speech_feedback_handler_.get());
  speech_feedback_handler_.reset();
#endif
  RemovePreTargetHandler(overlay_filter_.get());
  RemovePreTargetHandler(accelerator_filter_.get());
  RemovePreTargetHandler(event_transformation_handler_.get());
  RemovePreTargetHandler(toplevel_window_event_handler_.get());
  RemovePostTargetHandler(toplevel_window_event_handler_.get());
  RemovePreTargetHandler(system_gesture_filter_.get());
  RemovePreTargetHandler(mouse_cursor_filter_.get());
  RemovePreTargetHandler(modality_filter_.get());

  // TooltipController is deleted with the Shell so removing its references.
  RemovePreTargetHandler(tooltip_controller_.get());

#if defined(OS_CHROMEOS)
  screen_orientation_controller_.reset();
  screen_layout_observer_.reset();
#endif

// Destroy the virtual keyboard controller before the maximize mode controller
// since the latters destructor triggers events that the former is listening
// to but no longer cares about.
#if defined(OS_CHROMEOS)
  virtual_keyboard_controller_.reset();
#endif

  // Destroy maximize mode controller early on since it has some observers which
  // need to be removed.
  wm_shell_->DeleteMaximizeModeController();

  // Destroy the keyboard before closing the shelf, since it will invoke a shelf
  // layout.
  DeactivateKeyboard();

  // Destroy toasts
  wm_shell_->DeleteToastManager();

  // Destroy SystemTrayDelegate before destroying the status area(s). Make sure
  // to deinitialize the shelf first, as it is initialized after the delegate.
  for (WmWindow* root : wm_shell_->GetAllRootWindows())
    root->GetRootWindowController()->GetShelf()->ShutdownShelfWidget();
  wm_shell_->DeleteSystemTrayDelegate();

  // Drag-and-drop must be canceled prior to close all windows.
  drag_drop_controller_.reset();

// Controllers who have WindowObserver added must be deleted
// before |window_tree_host_manager_| is deleted.

#if defined(OS_CHROMEOS)
  // VideoActivityNotifier must be deleted before |video_detector_| is
  // deleted because it's observing video activity through
  // VideoDetector::Observer interface.
  video_activity_notifier_.reset();
#endif  // defined(OS_CHROMEOS)
  video_detector_.reset();
  high_contrast_controller_.reset();

  shadow_controller_.reset();
  resize_shadow_controller_.reset();

  // Has to happen before ~MruWindowTracker.
  wm_shell_->DeleteWindowCycleController();
  wm_shell_->DeleteWindowSelectorController();

  // Destroy all child windows including widgets.
  window_tree_host_manager_->CloseChildWindows();
  // MruWindowTracker must be destroyed after all windows have been deleted to
  // avoid a possible crash when Shell is destroyed from a non-normal shutdown
  // path. (crbug.com/485438).
  wm_shell_->DeleteMruWindowTracker();

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

#if defined(OS_CHROMEOS)
  resolution_notification_controller_.reset();
#endif
  screenshot_controller_.reset();
  mouse_cursor_filter_.reset();
  modality_filter_.reset();

#if defined(OS_CHROMEOS)
  touch_transformer_controller_.reset();
  audio_a11y_controller_.reset();
  laser_pointer_controller_.reset();
  partial_magnification_controller_.reset();
#endif  // defined(OS_CHROMEOS)

  // This also deletes all RootWindows. Note that we invoke Shutdown() on
  // WindowTreeHostManager before resetting |window_tree_host_manager_|, since
  // destruction
  // of its owned RootWindowControllers relies on the value.
  ScreenAsh::CreateScreenForShutdown();
  display_configuration_controller_.reset();

  wm_shell_->Shutdown();
  // Depends on |focus_client_|, so must be destroyed before.
  window_tree_host_manager_->Shutdown();
  window_tree_host_manager_.reset();
  focus_client_.reset();
  screen_position_controller_.reset();

  keyboard::KeyboardController::ResetInstance(nullptr);

#if defined(OS_CHROMEOS)
  display_color_manager_.reset();
  if (display_change_observer_)
    display_configurator_->RemoveObserver(display_change_observer_.get());
  if (display_error_observer_)
    display_configurator_->RemoveObserver(display_error_observer_.get());
  if (projecting_observer_) {
    display_configurator_->RemoveObserver(projecting_observer_.get());
    wm_shell_->RemoveShellObserver(projecting_observer_.get());
  }
  display_change_observer_.reset();

  PowerStatus::Shutdown();

  // Ensure that DBusThreadManager outlives this Shell.
  DCHECK(chromeos::DBusThreadManager::IsInitialized());
#endif

  // Needs to happen right before |instance_| is reset.
  wm_shell_.reset();

  DCHECK(instance_ == this);
  instance_ = nullptr;
}

void Shell::Init(const ShellInitParams& init_params) {
  wm_shell_->Initialize(init_params.blocking_pool);

  immersive_handler_factory_ = base::MakeUnique<ImmersiveHandlerFactoryAsh>();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  NOTREACHED() << "linux desktop does not support ash.";
#endif

  scoped_overview_animation_settings_factory_.reset(
      new ScopedOverviewAnimationSettingsFactoryAura);
  window_positioner_.reset(new WindowPositioner(wm_shell_.get()));

  native_cursor_manager_ = new AshNativeCursorManager;
#if defined(OS_CHROMEOS)
  cursor_manager_.reset(
      new CursorManager(base::WrapUnique(native_cursor_manager_)));
#else
  cursor_manager_.reset(
      new ::wm::CursorManager(base::WrapUnique(native_cursor_manager_)));
#endif

  wm_shell_->delegate()->PreInit();
  bool display_initialized = display_manager_->InitFromCommandLine();

  display_configuration_controller_.reset(new DisplayConfigurationController(
      display_manager_.get(), window_tree_host_manager_.get()));

#if defined(OS_CHROMEOS)

#if defined(USE_OZONE)
  display_configurator_->Init(
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(),
      !gpu_support_->IsPanelFittingDisabled());
#elif defined(USE_X11)
  display_configurator_->Init(base::MakeUnique<ui::NativeDisplayDelegateX11>(),
                              !gpu_support_->IsPanelFittingDisabled());
#endif

  // The DBusThreadManager must outlive this Shell. See the DCHECK in ~Shell.
  chromeos::DBusThreadManager* dbus_thread_manager =
      chromeos::DBusThreadManager::Get();
  projecting_observer_.reset(
      new ProjectingObserver(dbus_thread_manager->GetPowerManagerClient()));
  display_configurator_->AddObserver(projecting_observer_.get());
  wm_shell_->AddShellObserver(projecting_observer_.get());

  if (!display_initialized && base::SysInfo::IsRunningOnChromeOS()) {
    display_change_observer_.reset(new DisplayChangeObserver);
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
#endif  // defined(OS_CHROMEOS)

  if (!display_initialized)
    display_manager_->InitDefaultDisplay();

  display_manager_->RefreshFontParams();

  aura::Env::GetInstance()->set_context_factory(init_params.context_factory);

  // The WindowModalityController needs to be at the front of the input event
  // pretarget handler list to ensure that it processes input events when modal
  // windows are active.
  window_modality_controller_.reset(new ::wm::WindowModalityController(this));

  env_filter_.reset(new ::wm::CompoundEventFilter);
  AddPreTargetHandler(env_filter_.get());

  wm::AshFocusRules* focus_rules = new wm::AshFocusRules();

  ::wm::FocusController* focus_controller =
      new ::wm::FocusController(focus_rules);
  focus_client_.reset(focus_controller);
  activation_client_ = focus_controller;

  screen_position_controller_.reset(new ScreenPositionController);

  window_tree_host_manager_->Start();
  AshWindowTreeHostInitParams ash_init_params;
  window_tree_host_manager_->CreatePrimaryHost(ash_init_params);
  aura::Window* root_window = window_tree_host_manager_->GetPrimaryRootWindow();
  wm_shell_->set_root_window_for_new_windows(WmWindowAura::Get(root_window));

#if defined(OS_CHROMEOS)
  resolution_notification_controller_.reset(
      new ResolutionNotificationController);
#endif

  if (cursor_manager_)
    cursor_manager_->SetDisplay(
        display::Screen::GetScreen()->GetPrimaryDisplay());

  accelerator_controller_delegate_.reset(new AcceleratorControllerDelegateAura);
  wm_shell_->SetAcceleratorController(base::MakeUnique<AcceleratorController>(
      accelerator_controller_delegate_.get(), nullptr));
  wm_shell_->CreateMaximizeModeController();

  AddPreTargetHandler(window_tree_host_manager_->input_method_event_handler());

#if defined(OS_CHROMEOS)
  magnifier_key_scroll_handler_ = MagnifierKeyScroller::CreateHandler();
  AddPreTargetHandler(magnifier_key_scroll_handler_.get());
  speech_feedback_handler_ = SpokenFeedbackToggler::CreateHandler();
  AddPreTargetHandler(speech_feedback_handler_.get());
#endif

  // The order in which event filters are added is significant.

  // ui::UserActivityDetector passes events to observers, so let them get
  // rewritten first.
  user_activity_detector_.reset(new ui::UserActivityDetector);

  overlay_filter_.reset(new OverlayEventFilter);
  AddPreTargetHandler(overlay_filter_.get());
  wm_shell_->AddShellObserver(overlay_filter_.get());

  accelerator_filter_.reset(new ::wm::AcceleratorFilter(
      std::unique_ptr<::wm::AcceleratorDelegate>(new AcceleratorDelegate),
      wm_shell_->accelerator_controller()->accelerator_history()));
  AddPreTargetHandler(accelerator_filter_.get());

  event_transformation_handler_.reset(new EventTransformationHandler);
  AddPreTargetHandler(event_transformation_handler_.get());

  toplevel_window_event_handler_.reset(
      new ToplevelWindowEventHandler(wm_shell_.get()));

  system_gesture_filter_.reset(new SystemGestureEventFilter);
  AddPreTargetHandler(system_gesture_filter_.get());

#if defined(OS_CHROMEOS)
  sticky_keys_controller_.reset(new StickyKeysController);
#endif
  screen_pinning_controller_.reset(
      new ScreenPinningController(window_tree_host_manager_.get()));

  lock_state_controller_ =
      base::MakeUnique<LockStateController>(wm_shell_->shutdown_controller());
  power_button_controller_.reset(
      new PowerButtonController(lock_state_controller_.get()));
#if defined(OS_CHROMEOS)
  // Pass the initial display state to PowerButtonController.
  power_button_controller_->OnDisplayModeChanged(
      display_configurator_->cached_displays());
#endif
  wm_shell_->AddShellObserver(lock_state_controller_.get());

  drag_drop_controller_.reset(new DragDropController);
  // |screenshot_controller_| needs to be created (and prepended as a
  // pre-target handler) at this point, because |mouse_cursor_filter_| needs to
  // process mouse events prior to screenshot session.
  // See http://crbug.com/459214
  screenshot_controller_.reset(new ScreenshotController());
  mouse_cursor_filter_.reset(new MouseCursorEventFilter());
  PrependPreTargetHandler(mouse_cursor_filter_.get());

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_.reset(new AshVisibilityController);

#if defined(OS_CHROMEOS)
  laser_pointer_controller_.reset(new LaserPointerController());
  partial_magnification_controller_.reset(new PartialMagnificationController());
#endif

  magnification_controller_.reset(MagnificationController::CreateInstance());
  wm_shell_->CreateMruWindowTracker();

  autoclick_controller_.reset(AutoclickController::CreateInstance());

  high_contrast_controller_.reset(new HighContrastController);
  video_detector_.reset(new VideoDetector);

  tooltip_controller_.reset(new views::corewm::TooltipController(
      std::unique_ptr<views::corewm::Tooltip>(new views::corewm::TooltipAura)));
  AddPreTargetHandler(tooltip_controller_.get());

  modality_filter_.reset(new SystemModalContainerEventFilter(this));
  AddPreTargetHandler(modality_filter_.get());

  event_client_.reset(new EventClientImpl);

  session_state_delegate_.reset(
      wm_shell_->delegate()->CreateSessionStateDelegate());
  wm_shell_->CreatePointerWatcherAdapter();

  resize_shadow_controller_.reset(new ResizeShadowController());
  shadow_controller_.reset(new ::wm::ShadowController(activation_client_));

  wm_shell_->SetSystemTrayDelegate(
      base::WrapUnique(wm_shell_->delegate()->CreateSystemTrayDelegate()));

#if defined(OS_CHROMEOS)
  // Create TouchTransformerController before
  // WindowTreeHostManager::InitDisplays()
  // since TouchTransformerController listens on
  // WindowTreeHostManager::Observer::OnDisplaysInitialized().
  touch_transformer_controller_.reset(new TouchTransformerController());
#endif  // defined(OS_CHROMEOS)

  wm_shell_->SetKeyboardUI(KeyboardUI::Create());

  window_tree_host_manager_->InitHosts();

#if defined(OS_CHROMEOS)
  // Needs to be created after InitDisplays() since it may cause the virtual
  // keyboard to be deployed.
  virtual_keyboard_controller_.reset(new VirtualKeyboardController);
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  audio_a11y_controller_.reset(new chromeos::AudioA11yController);
#endif  // defined(OS_CHROMEOS)

  // Initialize the wallpaper after the RootWindowController has been created,
  // otherwise the widget will not paint when restoring after a browser crash.
  // Also, initialize after display initialization to ensure correct sizing.
  wm_shell_->wallpaper_delegate()->InitializeWallpaper();

  if (cursor_manager_) {
    if (initially_hide_cursor_)
      cursor_manager_->HideCursor();
    cursor_manager_->SetCursor(ui::kCursorPointer);
  }

#if defined(OS_CHROMEOS)
  power_event_observer_.reset(new PowerEventObserver());
  user_activity_notifier_.reset(
      new ui::UserActivityPowerManagerNotifier(user_activity_detector_.get()));
  video_activity_notifier_.reset(
      new VideoActivityNotifier(video_detector_.get()));
  bluetooth_notification_controller_.reset(new BluetoothNotificationController);
  screen_orientation_controller_.reset(new ScreenOrientationController());
  screen_layout_observer_.reset(new ScreenLayoutObserver());
#endif
  // The compositor thread and main message loop have to be running in
  // order to create mirror window. Run it after the main message loop
  // is started.
  display_manager_->CreateMirrorWindowAsyncIfAny();

  for (auto& observer : *wm_shell_->shell_observers())
    observer.OnShellInitialized();

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
#if defined(OS_CHROMEOS)
    keyboard::KeyboardController::ResetInstance(
        new keyboard::KeyboardController(
            wm_shell_->delegate()->CreateKeyboardUI(),
            virtual_keyboard_controller_.get()));
#else
    keyboard::KeyboardController::ResetInstance(
        new keyboard::KeyboardController(
            wm_shell_->delegate()->CreateKeyboardUI(), nullptr));

#endif
  }
}

void Shell::InitRootWindow(aura::Window* root_window) {
  DCHECK(activation_client_);
  DCHECK(visibility_controller_.get());
  DCHECK(drag_drop_controller_.get());

  aura::client::SetFocusClient(root_window, focus_client_.get());
  aura::client::SetActivationClient(root_window, activation_client_);
  ::wm::FocusController* focus_controller =
      static_cast<::wm::FocusController*>(activation_client_);
  root_window->AddPreTargetHandler(focus_controller);
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

}  // namespace ash
