// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <string>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_delegate.h"
#include "ash/accelerators/focus_manager_factory.h"
#include "ash/accelerators/nested_accelerator_delegate.h"
#include "ash/accelerometer/accelerometer_controller.h"
#include "ash/ash_switches.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/virtual_keyboard_window_controller.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/first_run/first_run_helper_impl.h"
#include "ash/focus_cycler.h"
#include "ash/frame/custom_frame_view_ash.h"
#include "ash/gpu_support.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/keyboard_uma_event_filter.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/media_delegate.h"
#include "ash/new_window_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/app_list_shelf_item_delegate.h"
#include "ash/shelf/shelf_delegate.h"
#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/shelf_window_watcher.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_init_params.h"
#include "ash/shell_window_ids.h"
#include "ash/system/locale/locale_notification_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/app_list_controller.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/ash_native_cursor_manager.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/maximize_mode/maximize_mode_window_manager.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overlay_event_filter.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/screen_dimmer.h"
#include "ash/wm/system_gesture_event_filter.h"
#include "ash/wm/system_modal_container_event_filter.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/keyboard/keyboard.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/message_center/message_center.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/accelerator_filter.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/input_method_event_filter.h"
#include "ui/wm/core/nested_accelerator_controller.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/user_activity_detector.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_modality_controller.h"

#if defined(OS_CHROMEOS)
#if defined(USE_X11)
#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/touch/touch_transformer_controller.h"
#include "ui/gfx/x/x11_types.h"
#endif  // defined(USE_X11)
#include "ash/ash_constants.h"
#include "ash/display/display_change_observer_chromeos.h"
#include "ash/display/display_configurator_animation.h"
#include "ash/display/display_error_observer_chromeos.h"
#include "ash/display/projecting_observer_chromeos.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/system/chromeos/bluetooth/bluetooth_notification_controller.h"
#include "ash/system/chromeos/brightness/brightness_controller_chromeos.h"
#include "ash/system/chromeos/power/power_event_observer.h"
#include "ash/system/chromeos/power/power_status.h"
#include "ash/system/chromeos/power/video_activity_notifier.h"
#include "ash/system/chromeos/session/last_window_closed_logout_reminder.h"
#include "ash/system/chromeos/session/logout_confirmation_controller.h"
#include "base/bind_helpers.h"
#include "base/sys_info.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/display/chromeos/display_configurator.h"
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
  virtual ~AshVisibilityController() {}

 private:
  // Overridden from ::wm::VisibilityController:
  virtual bool CallAnimateOnChildWindowVisibilityChanged(
      aura::Window* window,
      bool visible) OVERRIDE {
    return AnimateOnChildWindowVisibilityChanged(window, visible);
  }

  DISALLOW_COPY_AND_ASSIGN(AshVisibilityController);
};

AshWindowTreeHostInitParams ShellInitParamsToAshWindowTreeHostInitParams(
    const ShellInitParams& shell_init_params) {
  AshWindowTreeHostInitParams ash_init_params;
#if defined(OS_WIN)
  ash_init_params.remote_hwnd = shell_init_params.remote_hwnd;
#endif
  return ash_init_params;
}

}  // namespace

// static
Shell* Shell::instance_ = NULL;
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
  DCHECK(instance_);
  return instance_;
}

// static
bool Shell::HasInstance() {
  return !!instance_;
}

// static
void Shell::DeleteInstance() {
  delete instance_;
  instance_ = NULL;
}

// static
RootWindowController* Shell::GetPrimaryRootWindowController() {
  return GetRootWindowController(GetPrimaryRootWindow());
}

// static
Shell::RootWindowControllerList Shell::GetAllRootWindowControllers() {
  return Shell::GetInstance()->display_controller()->
      GetAllRootWindowControllers();
}

// static
aura::Window* Shell::GetPrimaryRootWindow() {
  return GetInstance()->display_controller()->GetPrimaryRootWindow();
}

// static
aura::Window* Shell::GetTargetRootWindow() {
  Shell* shell = GetInstance();
  if (shell->scoped_target_root_window_)
    return shell->scoped_target_root_window_;
  return shell->target_root_window_;
}

// static
gfx::Screen* Shell::GetScreen() {
  return gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_ALTERNATE);
}

// static
aura::Window::Windows Shell::GetAllRootWindows() {
  return Shell::GetInstance()->display_controller()->
      GetAllRootWindows();
}

// static
aura::Window* Shell::GetContainer(aura::Window* root_window,
                                  int container_id) {
  return root_window->GetChildById(container_id);
}

// static
const aura::Window* Shell::GetContainer(const aura::Window* root_window,
                                        int container_id) {
  return root_window->GetChildById(container_id);
}

// static
std::vector<aura::Window*> Shell::GetContainersFromAllRootWindows(
    int container_id,
    aura::Window* priority_root) {
  std::vector<aura::Window*> containers;
  aura::Window::Windows root_windows = GetAllRootWindows();
  for (aura::Window::Windows::const_iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    aura::Window* container = (*it)->GetChildById(container_id);
    if (container) {
      if (priority_root && priority_root->Contains(container))
        containers.insert(containers.begin(), container);
      else
        containers.push_back(container);
    }
  }
  return containers;
}

void Shell::ShowContextMenu(const gfx::Point& location_in_screen,
                            ui::MenuSourceType source_type) {
  // No context menus if there is no session with an active user.
  if (!session_state_delegate_->NumberOfLoggedInUsers())
    return;
  // No context menus when screen is locked.
  if (session_state_delegate_->IsScreenLocked())
    return;

  aura::Window* root =
      wm::GetRootWindowMatching(gfx::Rect(location_in_screen, gfx::Size()));
  GetRootWindowController(root)
      ->ShowContextMenu(location_in_screen, source_type);
}

void Shell::ShowAppList(aura::Window* window) {
  // If the context window is not given, show it on the target root window.
  if (!window)
    window = GetTargetRootWindow();
  if (!app_list_controller_)
    app_list_controller_.reset(new AppListController);
  app_list_controller_->SetVisible(true, window);
}

void Shell::DismissAppList() {
  if (!app_list_controller_)
    return;
  app_list_controller_->SetVisible(false, GetTargetRootWindow());
}

void Shell::ToggleAppList(aura::Window* window) {
  if (app_list_controller_ && app_list_controller_->IsVisible()) {
    DismissAppList();
    return;
  }

  ShowAppList(window);
}

bool Shell::GetAppListTargetVisibility() const {
  return app_list_controller_.get() &&
      app_list_controller_->GetTargetVisibility();
}

aura::Window* Shell::GetAppListWindow() {
  return app_list_controller_.get() ? app_list_controller_->GetWindow() : NULL;
}

app_list::AppListView* Shell::GetAppListView() {
  return app_list_controller_.get() ? app_list_controller_->GetView() : NULL;
}

bool Shell::IsSystemModalWindowOpen() const {
  if (simulate_modal_window_open_for_testing_)
    return true;
  const std::vector<aura::Window*> containers = GetContainersFromAllRootWindows(
      kShellWindowId_SystemModalContainer, NULL);
  for (std::vector<aura::Window*>::const_iterator cit = containers.begin();
       cit != containers.end(); ++cit) {
    for (aura::Window::Windows::const_iterator wit = (*cit)->children().begin();
         wit != (*cit)->children().end(); ++wit) {
      if ((*wit)->GetProperty(aura::client::kModalKey) ==
          ui::MODAL_TYPE_SYSTEM && (*wit)->TargetVisibility()) {
        return true;
      }
    }
  }
  return false;
}

views::NonClientFrameView* Shell::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
  // Use translucent-style window frames for dialogs.
  return new CustomFrameViewAsh(widget);
}

void Shell::RotateFocus(Direction direction) {
  focus_cycler_->RotateFocus(direction == FORWARD ? FocusCycler::FORWARD
                                                  : FocusCycler::BACKWARD);
}

void Shell::SetDisplayWorkAreaInsets(Window* contains,
                                     const gfx::Insets& insets) {
  if (!display_controller_->UpdateWorkAreaOfDisplayNearestWindow(
          contains, insets)) {
    return;
  }
  FOR_EACH_OBSERVER(ShellObserver, observers_,
                    OnDisplayWorkAreaInsetsChanged());
}

void Shell::OnLoginStateChanged(user::LoginStatus status) {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnLoginStateChanged(status));
}

void Shell::OnLoginUserProfilePrepared() {
  CreateShelf();
  CreateKeyboard();
}

void Shell::UpdateAfterLoginStatusChange(user::LoginStatus status) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->UpdateAfterLoginStatusChange(status);
}

void Shell::OnAppTerminating() {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnAppTerminating());
}

void Shell::OnLockStateChanged(bool locked) {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnLockStateChanged(locked));
#ifndef NDEBUG
  // Make sure that there is no system modal in Lock layer when unlocked.
  if (!locked) {
    std::vector<aura::Window*> containers = GetContainersFromAllRootWindows(
        kShellWindowId_LockSystemModalContainer, GetPrimaryRootWindow());
    for (std::vector<aura::Window*>::const_iterator iter = containers.begin();
         iter != containers.end(); ++iter) {
      DCHECK_EQ(0u, (*iter)->children().size());
    }
  }
#endif
}

void Shell::OnCastingSessionStartedOrStopped(bool started) {
#if defined(OS_CHROMEOS) && defined(USE_X11)
  if (projecting_observer_)
    projecting_observer_->OnCastingSessionStartedOrStopped(started);
#endif
}

void Shell::OnOverviewModeStarting() {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnOverviewModeStarting());
}

void Shell::OnOverviewModeEnding() {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnOverviewModeEnding());
}

void Shell::OnMaximizeModeStarted() {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnMaximizeModeStarted());
}

void Shell::OnMaximizeModeEnded() {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnMaximizeModeEnded());
}

void Shell::OnRootWindowAdded(aura::Window* root_window) {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnRootWindowAdded(root_window));
}

void Shell::CreateShelf() {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->shelf()->CreateShelf();
}

void Shell::OnShelfCreatedForRootWindow(aura::Window* root_window) {
  FOR_EACH_OBSERVER(ShellObserver,
                    observers_,
                    OnShelfCreatedForRootWindow(root_window));
}

void Shell::CreateKeyboard() {
  // TODO(bshe): Primary root window controller may not be the controller to
  // attach virtual keyboard. See http://crbug.com/303429
  InitKeyboard();
  if (keyboard::IsKeyboardUsabilityExperimentEnabled()) {
    display_controller()->virtual_keyboard_window_controller()->
        ActivateKeyboard(keyboard::KeyboardController::GetInstance());
  } else {
    GetPrimaryRootWindowController()->
        ActivateKeyboard(keyboard::KeyboardController::GetInstance());
  }
}

void Shell::DeactivateKeyboard() {
  if (keyboard::KeyboardController::GetInstance()) {
    RootWindowControllerList controllers = GetAllRootWindowControllers();
    for (RootWindowControllerList::iterator iter = controllers.begin();
        iter != controllers.end(); ++iter) {
      (*iter)->DeactivateKeyboard(keyboard::KeyboardController::GetInstance());
    }
  }
  keyboard::KeyboardController::ResetInstance(NULL);
}

void Shell::ShowShelf() {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->ShowShelf();
}

void Shell::AddShellObserver(ShellObserver* observer) {
  observers_.AddObserver(observer);
}

void Shell::RemoveShellObserver(ShellObserver* observer) {
  observers_.RemoveObserver(observer);
}

#if defined(OS_CHROMEOS)
bool Shell::ShouldSaveDisplaySettings() {
  return !((maximize_mode_controller_->IsMaximizeModeWindowManagerEnabled() &&
            maximize_mode_controller_->
                ignore_display_configuration_updates()) ||
           resolution_notification_controller_->DoesNotificationTimeout());
}
#endif

void Shell::UpdateShelfVisibility() {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    if ((*iter)->shelf())
      (*iter)->UpdateShelfVisibility();
}

void Shell::SetShelfAutoHideBehavior(ShelfAutoHideBehavior behavior,
                                     aura::Window* root_window) {
  ash::ShelfLayoutManager::ForShelf(root_window)->SetAutoHideBehavior(behavior);
}

ShelfAutoHideBehavior Shell::GetShelfAutoHideBehavior(
    aura::Window* root_window) const {
  return ash::ShelfLayoutManager::ForShelf(root_window)->auto_hide_behavior();
}

void Shell::SetShelfAlignment(ShelfAlignment alignment,
                              aura::Window* root_window) {
  if (ash::ShelfLayoutManager::ForShelf(root_window)->SetAlignment(alignment)) {
    FOR_EACH_OBSERVER(
        ShellObserver, observers_, OnShelfAlignmentChanged(root_window));
  }
}

ShelfAlignment Shell::GetShelfAlignment(const aura::Window* root_window) {
  return GetRootWindowController(root_window)
      ->GetShelfLayoutManager()
      ->GetAlignment();
}

void Shell::SetDimming(bool should_dim) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->screen_dimmer()->SetDimming(should_dim);
}

void Shell::NotifyFullscreenStateChange(bool is_fullscreen,
                                        aura::Window* root_window) {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnFullscreenStateChanged(
      is_fullscreen, root_window));
}

void Shell::CreateModalBackground(aura::Window* window) {
  if (!modality_filter_) {
    modality_filter_.reset(new SystemModalContainerEventFilter(this));
    AddPreTargetHandler(modality_filter_.get());
  }
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->GetSystemModalLayoutManager(window)->CreateModalBackground();
}

void Shell::OnModalWindowRemoved(aura::Window* removed) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  bool activated = false;
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end() && !activated; ++iter) {
    activated = (*iter)->GetSystemModalLayoutManager(removed)->
        ActivateNextModalWindow();
  }
  if (!activated) {
    RemovePreTargetHandler(modality_filter_.get());
    modality_filter_.reset();
    for (RootWindowControllerList::iterator iter = controllers.begin();
         iter != controllers.end(); ++iter)
      (*iter)->GetSystemModalLayoutManager(removed)->DestroyModalBackground();
  }
}

WebNotificationTray* Shell::GetWebNotificationTray() {
  return GetPrimaryRootWindowController()->shelf()->
      status_area_widget()->web_notification_tray();
}

bool Shell::HasPrimaryStatusArea() {
  ShelfWidget* shelf = GetPrimaryRootWindowController()->shelf();
  return shelf && shelf->status_area_widget();
}

SystemTray* Shell::GetPrimarySystemTray() {
  return GetPrimaryRootWindowController()->GetSystemTray();
}

ShelfDelegate* Shell::GetShelfDelegate() {
  if (!shelf_delegate_) {
    shelf_model_.reset(new ShelfModel);
    // Creates ShelfItemDelegateManager before ShelfDelegate.
    shelf_item_delegate_manager_.reset(
        new ShelfItemDelegateManager(shelf_model_.get()));

    shelf_delegate_.reset(delegate_->CreateShelfDelegate(shelf_model_.get()));
    scoped_ptr<ShelfItemDelegate> controller(new AppListShelfItemDelegate);

    // Finding the shelf model's location of the app list and setting its
    // ShelfItemDelegate.
    int app_list_index = shelf_model_->GetItemIndexForType(TYPE_APP_LIST);
    DCHECK_GE(app_list_index, 0);
    ShelfID app_list_id = shelf_model_->items()[app_list_index].id;
    DCHECK(app_list_id);
    shelf_item_delegate_manager_->SetShelfItemDelegate(app_list_id,
                                                       controller.Pass());
    shelf_window_watcher_.reset(new ShelfWindowWatcher(
        shelf_model_.get(), shelf_item_delegate_manager_.get()));
  }
  return shelf_delegate_.get();
}

void Shell::SetTouchHudProjectionEnabled(bool enabled) {
  if (is_touch_hud_projection_enabled_ == enabled)
    return;

  is_touch_hud_projection_enabled_ = enabled;
  FOR_EACH_OBSERVER(ShellObserver, observers_,
                    OnTouchHudProjectionToggled(enabled));
}

#if defined(OS_CHROMEOS)
ash::FirstRunHelper* Shell::CreateFirstRunHelper() {
  return new ash::FirstRunHelperImpl;
}

void Shell::SetCursorCompositingEnabled(bool enabled) {
  display_controller_->cursor_window_controller()->SetCursorCompositingEnabled(
      enabled);
  native_cursor_manager_->SetNativeCursorEnabled(!enabled);
}
#endif  // defined(OS_CHROMEOS)

void Shell::DoInitialWorkspaceAnimation() {
  return GetPrimaryRootWindowController()->workspace_controller()->
      DoInitialAnimation();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

Shell::Shell(ShellDelegate* delegate)
    : target_root_window_(NULL),
      scoped_target_root_window_(NULL),
      delegate_(delegate),
      window_positioner_(new WindowPositioner),
      activation_client_(NULL),
      accelerometer_controller_(new AccelerometerController()),
#if defined(OS_CHROMEOS)
      display_configurator_(new ui::DisplayConfigurator()),
#endif  // defined(OS_CHROMEOS)
      native_cursor_manager_(new AshNativeCursorManager),
      cursor_manager_(
          scoped_ptr< ::wm::NativeCursorManager>(native_cursor_manager_)),
      simulate_modal_window_open_for_testing_(false),
      is_touch_hud_projection_enabled_(false) {
  DCHECK(delegate_.get());
  gpu_support_.reset(delegate_->CreateGPUSupport());
  display_manager_.reset(new DisplayManager);
  display_controller_.reset(new DisplayController);
#if defined(OS_CHROMEOS) && defined(USE_X11)
  user_metrics_recorder_.reset(new UserMetricsRecorder);
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  PowerStatus::Initialize();
#endif
}

Shell::~Shell() {
  TRACE_EVENT0("shutdown", "ash::Shell::Destructor");

  delegate_->PreShutdown();

  views::FocusManagerFactory::Install(NULL);

  // Remove the focus from any window. This will prevent overhead and side
  // effects (e.g. crashes) from changing focus during shutdown.
  // See bug crbug.com/134502.
  aura::client::GetFocusClient(GetPrimaryRootWindow())->FocusWindow(NULL);

  // Please keep in same order as in Init() because it's easy to miss one.
  if (window_modality_controller_)
    window_modality_controller_.reset();
#if defined(OS_CHROMEOS) && defined(USE_X11)
  RemovePreTargetHandler(magnifier_key_scroll_handler_.get());
  magnifier_key_scroll_handler_.reset();

  RemovePreTargetHandler(speech_feedback_handler_.get());
  speech_feedback_handler_.reset();
#endif
  RemovePreTargetHandler(user_activity_detector_.get());
  RemovePreTargetHandler(overlay_filter_.get());
  RemovePreTargetHandler(input_method_filter_.get());
  RemovePreTargetHandler(accelerator_filter_.get());
  RemovePreTargetHandler(event_transformation_handler_.get());
  RemovePreTargetHandler(toplevel_window_event_handler_.get());
  RemovePostTargetHandler(toplevel_window_event_handler_.get());
  RemovePreTargetHandler(system_gesture_filter_.get());
  RemovePreTargetHandler(keyboard_metrics_filter_.get());
  RemovePreTargetHandler(mouse_cursor_filter_.get());

  // TooltipController is deleted with the Shell so removing its references.
  RemovePreTargetHandler(tooltip_controller_.get());

  // Destroy maximize mode controller early on since it has some observers which
  // need to be removed.
  maximize_mode_controller_->Shutdown();
  maximize_mode_controller_.reset();

  // AppList needs to be released before shelf layout manager, which is
  // destroyed with shelf container in the loop below. However, app list
  // container is now on top of shelf container and released after it.
  // TODO(xiyuan): Move it back when app list container is no longer needed.
  app_list_controller_.reset();

#if defined(OS_CHROMEOS)
  // Destroy the LastWindowClosedLogoutReminder before the
  // LogoutConfirmationController.
  last_window_closed_logout_reminder_.reset();

  // Destroy the LogoutConfirmationController before the SystemTrayDelegate.
  logout_confirmation_controller_.reset();
#endif

  // Destroy SystemTrayDelegate before destroying the status area(s).
  system_tray_delegate_->Shutdown();
  system_tray_delegate_.reset();

  locale_notification_controller_.reset();

  // Drag-and-drop must be canceled prior to close all windows.
  drag_drop_controller_.reset();

  // Controllers who have WindowObserver added must be deleted
  // before |display_controller_| is deleted.

#if defined(OS_CHROMEOS)
  // VideoActivityNotifier must be deleted before |video_detector_| is
  // deleted because it's observing video activity through
  // VideoDetectorObserver interface.
  video_activity_notifier_.reset();
#endif  // defined(OS_CHROMEOS)
  video_detector_.reset();
  high_contrast_controller_.reset();

  shadow_controller_.reset();
  resize_shadow_controller_.reset();

  window_cycle_controller_.reset();
  window_selector_controller_.reset();
  mru_window_tracker_.reset();

  // |shelf_window_watcher_| has a weak pointer to |shelf_Model_|
  // and has window observers.
  shelf_window_watcher_.reset();

  // Destroy all child windows including widgets.
  display_controller_->CloseChildWindows();
  display_controller_->CloseNonDesktopDisplay();

  // Chrome implementation of shelf delegate depends on FocusClient,
  // so must be deleted before |focus_client_|.
  shelf_delegate_.reset();
  focus_client_.reset();

  // Destroy SystemTrayNotifier after destroying SystemTray as TrayItems
  // needs to remove observers from it.
  system_tray_notifier_.reset();

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  // Alphabetical. TODO(oshima): sort.
  magnification_controller_.reset();
  partial_magnification_controller_.reset();
  tooltip_controller_.reset();
  event_client_.reset();
  nested_accelerator_controller_.reset();
  toplevel_window_event_handler_.reset();
  visibility_controller_.reset();
  // |shelf_item_delegate_manager_| observes |shelf_model_|. It must be
  // destroyed before |shelf_model_| is destroyed.
  shelf_item_delegate_manager_.reset();
  shelf_model_.reset();

  power_button_controller_.reset();
  lock_state_controller_.reset();

#if defined(OS_CHROMEOS)
  resolution_notification_controller_.reset();
#endif
  desktop_background_controller_.reset();
  mouse_cursor_filter_.reset();

#if defined(OS_CHROMEOS) && defined(USE_X11)
  touch_transformer_controller_.reset();
#endif  // defined(OS_CHROMEOS) && defined(USE_X11)

  // This also deletes all RootWindows. Note that we invoke Shutdown() on
  // DisplayController before resetting |display_controller_|, since destruction
  // of its owned RootWindowControllers relies on the value.
  display_manager_->CreateScreenForShutdown();
  display_controller_->Shutdown();
  display_controller_.reset();
  screen_position_controller_.reset();
  accessibility_delegate_.reset();
  new_window_delegate_.reset();
  media_delegate_.reset();

  keyboard::KeyboardController::ResetInstance(NULL);

#if defined(OS_CHROMEOS)
  if (display_change_observer_)
    display_configurator_->RemoveObserver(display_change_observer_.get());
  if (display_configurator_animation_)
    display_configurator_->RemoveObserver(
        display_configurator_animation_.get());
  if (display_error_observer_)
    display_configurator_->RemoveObserver(display_error_observer_.get());
  if (projecting_observer_)
    display_configurator_->RemoveObserver(projecting_observer_.get());
  display_change_observer_.reset();
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  PowerStatus::Shutdown();
#endif

  DCHECK(instance_ == this);
  instance_ = NULL;
}

void Shell::Init(const ShellInitParams& init_params) {
  delegate_->PreInit();
  if (keyboard::IsKeyboardUsabilityExperimentEnabled()) {
    display_manager_->SetSecondDisplayMode(DisplayManager::VIRTUAL_KEYBOARD);
  }
  bool display_initialized = display_manager_->InitFromCommandLine();
#if defined(OS_CHROMEOS)
  display_configurator_->Init(!gpu_support_->IsPanelFittingDisabled());
  display_configurator_animation_.reset(new DisplayConfiguratorAnimation());
  display_configurator_->AddObserver(display_configurator_animation_.get());

  projecting_observer_.reset(new ProjectingObserver());
  display_configurator_->AddObserver(projecting_observer_.get());

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
        delegate_->IsFirstRunAfterBoot() ? kChromeOsBootColor : 0);
    display_initialized = true;
  }
#endif  // defined(OS_CHROMEOS)
  if (!display_initialized)
    display_manager_->InitDefaultDisplay();

  display_manager_->InitFontParams();

  // Install the custom factory first so that views::FocusManagers for Tray,
  // Shelf, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  aura::Env::CreateInstance(true);
  aura::Env::GetInstance()->set_context_factory(init_params.context_factory);

  // The WindowModalityController needs to be at the front of the input event
  // pretarget handler list to ensure that it processes input events when modal
  // windows are active.
  window_modality_controller_.reset(
      new ::wm::WindowModalityController(this));

  env_filter_.reset(new ::wm::CompoundEventFilter);
  AddPreTargetHandler(env_filter_.get());

  ::wm::FocusController* focus_controller =
      new ::wm::FocusController(new wm::AshFocusRules);
  focus_client_.reset(focus_controller);
  activation_client_ = focus_controller;
  activation_client_->AddObserver(this);
  focus_cycler_.reset(new FocusCycler());

  screen_position_controller_.reset(new ScreenPositionController);

  display_controller_->Start();
  display_controller_->CreatePrimaryHost(
      ShellInitParamsToAshWindowTreeHostInitParams(init_params));
  aura::Window* root_window = display_controller_->GetPrimaryRootWindow();
  target_root_window_ = root_window;

#if defined(OS_CHROMEOS)
  resolution_notification_controller_.reset(
      new ResolutionNotificationController);
#endif

  cursor_manager_.SetDisplay(GetScreen()->GetPrimaryDisplay());

  nested_accelerator_controller_.reset(
      new ::wm::NestedAcceleratorController(new NestedAcceleratorDelegate));
  accelerator_controller_.reset(new AcceleratorController);
  maximize_mode_controller_.reset(new MaximizeModeController());

#if defined(OS_CHROMEOS) && defined(USE_X11)
  magnifier_key_scroll_handler_ = MagnifierKeyScroller::CreateHandler().Pass();
  AddPreTargetHandler(magnifier_key_scroll_handler_.get());
  speech_feedback_handler_ = SpokenFeedbackToggler::CreateHandler().Pass();
  AddPreTargetHandler(speech_feedback_handler_.get());
#endif

  // The order in which event filters are added is significant.

  // wm::UserActivityDetector passes events to observers, so let them get
  // rewritten first.
  user_activity_detector_.reset(new ::wm::UserActivityDetector);
  AddPreTargetHandler(user_activity_detector_.get());

  overlay_filter_.reset(new OverlayEventFilter);
  AddPreTargetHandler(overlay_filter_.get());
  AddShellObserver(overlay_filter_.get());

  input_method_filter_.reset(new ::wm::InputMethodEventFilter(
      root_window->GetHost()->GetAcceleratedWidget()));
  AddPreTargetHandler(input_method_filter_.get());

  accelerator_filter_.reset(new ::wm::AcceleratorFilter(
      scoped_ptr< ::wm::AcceleratorDelegate>(new AcceleratorDelegate).Pass()));
  AddPreTargetHandler(accelerator_filter_.get());

  event_transformation_handler_.reset(new EventTransformationHandler);
  AddPreTargetHandler(event_transformation_handler_.get());

  toplevel_window_event_handler_.reset(new ToplevelWindowEventHandler);

  system_gesture_filter_.reset(new SystemGestureEventFilter);
  AddPreTargetHandler(system_gesture_filter_.get());

  keyboard_metrics_filter_.reset(new KeyboardUMAEventFilter);
  AddPreTargetHandler(keyboard_metrics_filter_.get());

  // The keyboard system must be initialized before the RootWindowController is
  // created.
#if defined(OS_CHROMEOS)
    keyboard::InitializeKeyboard();
#endif

#if defined(OS_CHROMEOS)
  sticky_keys_controller_.reset(new StickyKeysController);
#endif

  lock_state_controller_.reset(new LockStateController);
  power_button_controller_.reset(new PowerButtonController(
      lock_state_controller_.get()));
#if defined(OS_CHROMEOS)
  // Pass the initial display state to PowerButtonController.
  power_button_controller_->OnDisplayModeChanged(
      display_configurator_->cached_displays());
#endif
  AddShellObserver(lock_state_controller_.get());

  drag_drop_controller_.reset(new DragDropController);
  mouse_cursor_filter_.reset(new MouseCursorEventFilter());
  PrependPreTargetHandler(mouse_cursor_filter_.get());

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_.reset(new AshVisibilityController);

  magnification_controller_.reset(
      MagnificationController::CreateInstance());
  mru_window_tracker_.reset(new MruWindowTracker(activation_client_));

  partial_magnification_controller_.reset(
      new PartialMagnificationController());

  autoclick_controller_.reset(AutoclickController::CreateInstance());

  high_contrast_controller_.reset(new HighContrastController);
  video_detector_.reset(new VideoDetector);
  window_selector_controller_.reset(new WindowSelectorController());
  window_cycle_controller_.reset(new WindowCycleController());

  tooltip_controller_.reset(
      new views::corewm::TooltipController(
          scoped_ptr<views::corewm::Tooltip>(
              new views::corewm::TooltipAura(gfx::SCREEN_TYPE_ALTERNATE))));
  AddPreTargetHandler(tooltip_controller_.get());

  event_client_.reset(new EventClientImpl);

  // This controller needs to be set before SetupManagedWindowMode.
  desktop_background_controller_.reset(new DesktopBackgroundController());
  user_wallpaper_delegate_.reset(delegate_->CreateUserWallpaperDelegate());

  session_state_delegate_.reset(delegate_->CreateSessionStateDelegate());
  accessibility_delegate_.reset(delegate_->CreateAccessibilityDelegate());
  new_window_delegate_.reset(delegate_->CreateNewWindowDelegate());
  media_delegate_.reset(delegate_->CreateMediaDelegate());

  resize_shadow_controller_.reset(new ResizeShadowController());
  shadow_controller_.reset(
      new ::wm::ShadowController(activation_client_));

  // Create system_tray_notifier_ before the delegate.
  system_tray_notifier_.reset(new ash::SystemTrayNotifier());

  // Initialize system_tray_delegate_ before initializing StatusAreaWidget.
  system_tray_delegate_.reset(delegate()->CreateSystemTrayDelegate());
  DCHECK(system_tray_delegate_.get());

  locale_notification_controller_.reset(new LocaleNotificationController);

  // Initialize system_tray_delegate_ after StatusAreaWidget is created.
  system_tray_delegate_->Initialize();

#if defined(OS_CHROMEOS)
  // Create the LogoutConfirmationController after the SystemTrayDelegate.
  logout_confirmation_controller_.reset(new LogoutConfirmationController(
      base::Bind(&SystemTrayDelegate::SignOut,
                 base::Unretained(system_tray_delegate_.get()))));
#endif

#if defined(OS_CHROMEOS) && defined(USE_X11)
  // Create TouchTransformerController before DisplayController::InitDisplays()
  // since TouchTransformerController listens on
  // DisplayController::Observer::OnDisplaysInitialized().
  touch_transformer_controller_.reset(new TouchTransformerController());
#endif  // defined(OS_CHROMEOS) && defined(USE_X11)

  display_controller_->InitDisplays();

  // It needs to be created after RootWindowController has been created
  // (which calls OnWindowResized has been called, otherwise the
  // widget will not paint when restoring after a browser crash.  Also it needs
  // to be created after InitSecondaryDisplays() to initialize the wallpapers in
  // the correct size.
  user_wallpaper_delegate_->InitializeWallpaper();

  if (initially_hide_cursor_)
    cursor_manager_.HideCursor();
  cursor_manager_.SetCursor(ui::kCursorPointer);

#if defined(OS_CHROMEOS)
  // Set accelerator controller delegates.
  accelerator_controller_->SetBrightnessControlDelegate(
      scoped_ptr<ash::BrightnessControlDelegate>(
          new ash::system::BrightnessControllerChromeos).Pass());

  power_event_observer_.reset(new PowerEventObserver());
  user_activity_notifier_.reset(
      new ui::UserActivityPowerManagerNotifier(user_activity_detector_.get()));
  video_activity_notifier_.reset(
      new VideoActivityNotifier(video_detector_.get()));
  bluetooth_notification_controller_.reset(new BluetoothNotificationController);
  last_window_closed_logout_reminder_.reset(new LastWindowClosedLogoutReminder);
#endif

  weak_display_manager_factory_.reset(
      new base::WeakPtrFactory<DisplayManager>(display_manager_.get()));
  // The compositor thread and main message loop have to be running in
  // order to create mirror window. Run it after the main message loop
  // is started.
  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&DisplayManager::CreateMirrorWindowIfAny,
                 weak_display_manager_factory_->GetWeakPtr()));
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
    keyboard::KeyboardControllerProxy* proxy =
        delegate_->CreateKeyboardControllerProxy();
    keyboard::KeyboardController::ResetInstance(
        new keyboard::KeyboardController(proxy));
  }
}

void Shell::InitRootWindow(aura::Window* root_window) {
  DCHECK(activation_client_);
  DCHECK(visibility_controller_.get());
  DCHECK(drag_drop_controller_.get());

  aura::client::SetFocusClient(root_window, focus_client_.get());
  input_method_filter_->SetInputMethodPropertyInRootWindow(root_window);
  aura::client::SetActivationClient(root_window, activation_client_);
  ::wm::FocusController* focus_controller =
      static_cast< ::wm::FocusController*>(activation_client_);
  root_window->AddPreTargetHandler(focus_controller);
  aura::client::SetVisibilityClient(root_window, visibility_controller_.get());
  aura::client::SetDragDropClient(root_window, drag_drop_controller_.get());
  aura::client::SetScreenPositionClient(root_window,
                                        screen_position_controller_.get());
  aura::client::SetCursorClient(root_window, &cursor_manager_);
  aura::client::SetTooltipClient(root_window, tooltip_controller_.get());
  aura::client::SetEventClient(root_window, event_client_.get());

  aura::client::SetWindowMoveClient(root_window,
      toplevel_window_event_handler_.get());
  root_window->AddPreTargetHandler(toplevel_window_event_handler_.get());
  root_window->AddPostTargetHandler(toplevel_window_event_handler_.get());

  if (nested_accelerator_controller_) {
    aura::client::SetDispatcherClient(root_window,
                                      nested_accelerator_controller_.get());
  }
}

bool Shell::CanWindowReceiveEvents(aura::Window* window) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter) {
    SystemModalContainerLayoutManager* layout_manager =
        (*iter)->GetSystemModalLayoutManager(window);
    if (layout_manager && layout_manager->CanWindowReceiveEvents(window))
      return true;
    // Allow events to fall through to the virtual keyboard even if displaying
    // a system modal dialog.
    if ((*iter)->IsVirtualKeyboardWindow(window))
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

scoped_ptr<ui::EventTargetIterator> Shell::GetChildIterator() const {
  return scoped_ptr<ui::EventTargetIterator>();
}

ui::EventTargeter* Shell::GetEventTargeter() {
  NOTREACHED();
  return NULL;
}

void Shell::OnEvent(ui::Event* event) {
}

////////////////////////////////////////////////////////////////////////////////
// Shell, aura::client::ActivationChangeObserver implementation:

void Shell::OnWindowActivated(aura::Window* gained_active,
                              aura::Window* lost_active) {
  if (gained_active)
    target_root_window_ = gained_active->GetRootWindow();
}

}  // namespace ash
