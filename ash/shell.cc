// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_delegate.h"
#include "ash/accelerators/focus_manager_factory.h"
#include "ash/ash_switches.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/first_run/first_run_helper_impl.h"
#include "ash/focus_cycler.h"
#include "ash/frame/custom_frame_view_ash.h"
#include "ash/gpu_support.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/ime/input_method_event_handler.h"
#include "ash/keyboard/keyboard_ui.h"
#include "ash/keyboard_uma_event_filter.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/media_delegate.h"
#include "ash/new_window_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/app_list_shelf_item_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_delegate.h"
#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/shelf_window_watcher.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_init_params.h"
#include "ash/shell_window_ids.h"
#include "ash/system/locale/locale_notification_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/utility/partial_screenshot_controller.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/ash_native_cursor_manager.h"
#include "ash/wm/aura/wm_globals_aura.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/root_window_finder.h"
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
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "ui/app_list/presenter/app_list_presenter.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
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
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_modality_controller.h"

#if defined(OS_CHROMEOS)
#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"  // nogncheck
#endif  // defined(USE_X11)
#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/ash_constants.h"
#include "ash/display/display_change_observer_chromeos.h"
#include "ash/display/display_color_manager_chromeos.h"
#include "ash/display/display_error_observer_chromeos.h"
#include "ash/display/projecting_observer_chromeos.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/system/chromeos/bluetooth/bluetooth_notification_controller.h"
#include "ash/system/chromeos/brightness/brightness_controller_chromeos.h"
#include "ash/system/chromeos/power/power_event_observer.h"
#include "ash/system/chromeos/power/power_status.h"
#include "ash/system/chromeos/power/video_activity_notifier.h"
#include "ash/system/chromeos/session/last_window_closed_logout_reminder.h"
#include "ash/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/touch/touch_transformer_controller.h"
#include "ash/virtual_keyboard_controller.h"
#include "base/bind_helpers.h"
#include "base/sys_info.h"
#include "chromeos/audio/audio_a11y_controller.h"
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
Shell* Shell::instance_ = nullptr;
// static
bool Shell::initially_hide_cursor_ = false;

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

// static
Shell* Shell::CreateInstance(const ShellInitParams& init_params) {
  CHECK(!instance_);
  instance_ = new Shell(init_params.delegate, init_params.blocking_pool);
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
  CHECK(HasInstance());
  Shell* shell = GetInstance();
  if (shell->scoped_target_root_window_)
    return shell->scoped_target_root_window_;
  return shell->target_root_window_;
}

// static
aura::Window::Windows Shell::GetAllRootWindows() {
  CHECK(HasInstance());
  return Shell::GetInstance()->window_tree_host_manager()->GetAllRootWindows();
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

  aura::Window* root = wm::WmWindowAura::GetAuraWindow(
      wm::GetRootWindowMatching(gfx::Rect(location_in_screen, gfx::Size())));
  GetRootWindowController(root)
      ->ShowContextMenu(location_in_screen, source_type);
}

void Shell::ShowAppList(aura::Window* window) {
  // If the context window is not given, show it on the target root window.
  if (!window)
    window = GetTargetRootWindow();
  delegate_->GetAppListPresenter()->Show(window);
}

void Shell::DismissAppList() {
  delegate_->GetAppListPresenter()->Dismiss();
}

void Shell::ToggleAppList(aura::Window* window) {
  if (delegate_->GetAppListPresenter()->IsVisible()) {
    DismissAppList();
    return;
  }

  ShowAppList(window);
}

bool Shell::GetAppListTargetVisibility() const {
  return delegate_->GetAppListPresenter()->GetTargetVisibility();
}

bool Shell::IsSystemModalWindowOpen() const {
  if (simulate_modal_window_open_for_testing_)
    return true;
  const std::vector<aura::Window*> containers = GetContainersFromAllRootWindows(
      kShellWindowId_SystemModalContainer, nullptr);
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
  if (!window_tree_host_manager_->UpdateWorkAreaOfDisplayNearestWindow(
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
#if defined(OS_CHROMEOS)
  FOR_EACH_OBSERVER(ShellObserver, observers_,
                    OnCastingSessionStartedOrStopped(started));
#endif
}

void Shell::OnOverviewModeStarting() {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnOverviewModeStarting());
}

void Shell::OnOverviewModeEnded() {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnOverviewModeEnded());
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
  if (in_mus_)
    return;
  // TODO(bshe): Primary root window controller may not be the controller to
  // attach virtual keyboard. See http://crbug.com/303429
  InitKeyboard();
  GetPrimaryRootWindowController()->
      ActivateKeyboard(keyboard::KeyboardController::GetInstance());
}

void Shell::DeactivateKeyboard() {
  keyboard_ui_->Hide();
  if (in_mus_)
    return;
  if (keyboard::KeyboardController::GetInstance()) {
    RootWindowControllerList controllers = GetAllRootWindowControllers();
    for (RootWindowControllerList::iterator iter = controllers.begin();
        iter != controllers.end(); ++iter) {
      (*iter)->DeactivateKeyboard(keyboard::KeyboardController::GetInstance());
    }
  }
  keyboard::KeyboardController::ResetInstance(nullptr);
}

void Shell::ShowShelf() {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->ShowShelf();
}

void Shell::HideShelf() {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter) {
    if ((*iter)->shelf())
      (*iter)->shelf()->ShutdownStatusAreaWidget();
  }
}

void Shell::AddShellObserver(ShellObserver* observer) {
  observers_.AddObserver(observer);
}

void Shell::RemoveShellObserver(ShellObserver* observer) {
  observers_.RemoveObserver(observer);
}

#if defined(OS_CHROMEOS)
bool Shell::ShouldSaveDisplaySettings() {
  return !(screen_orientation_controller_
               ->ignore_display_configuration_updates() ||
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
  Shelf::ForWindow(root_window)->SetAutoHideBehavior(behavior);
}

ShelfAutoHideBehavior Shell::GetShelfAutoHideBehavior(
    aura::Window* root_window) const {
  return Shelf::ForWindow(root_window)->auto_hide_behavior();
}

void Shell::SetShelfAlignment(wm::ShelfAlignment alignment,
                              aura::Window* root_window) {
  Shelf::ForWindow(root_window)->SetAlignment(alignment);
}

wm::ShelfAlignment Shell::GetShelfAlignment(
    const aura::Window* root_window) const {
  return Shelf::ForWindow(root_window)->alignment();
}

void Shell::OnShelfAlignmentChanged(aura::Window* root_window) {
  FOR_EACH_OBSERVER(ShellObserver, observers_,
                    OnShelfAlignmentChanged(root_window));
}

void Shell::OnShelfAutoHideBehaviorChanged(aura::Window* root_window) {
  FOR_EACH_OBSERVER(ShellObserver, observers_,
                    OnShelfAutoHideBehaviorChanged(root_window));
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
    // Creates ShelfItemDelegateManager before ShelfDelegate.
    shelf_item_delegate_manager_.reset(
        new ShelfItemDelegateManager(shelf_model_.get()));

    shelf_delegate_.reset(delegate_->CreateShelfDelegate(shelf_model_.get()));
    std::unique_ptr<ShelfItemDelegate> controller(new AppListShelfItemDelegate);

    // Finding the shelf model's location of the app list and setting its
    // ShelfItemDelegate.
    int app_list_index = shelf_model_->GetItemIndexForType(TYPE_APP_LIST);
    DCHECK_GE(app_list_index, 0);
    ShelfID app_list_id = shelf_model_->items()[app_list_index].id;
    DCHECK(app_list_id);
    shelf_item_delegate_manager_->SetShelfItemDelegate(app_list_id,
                                                       std::move(controller));
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
  return GetPrimaryRootWindowController()->workspace_controller()->
      DoInitialAnimation();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

Shell::Shell(ShellDelegate* delegate, base::SequencedWorkerPool* blocking_pool)
    : target_root_window_(nullptr),
      scoped_target_root_window_(nullptr),
      delegate_(delegate),
      shelf_model_(new ShelfModel),
      link_handler_model_factory_(nullptr),
      activation_client_(nullptr),
#if defined(OS_CHROMEOS)
      display_configurator_(new ui::DisplayConfigurator()),
#endif  // defined(OS_CHROMEOS)
      native_cursor_manager_(nullptr),
      simulate_modal_window_open_for_testing_(false),
      is_touch_hud_projection_enabled_(false),
      blocking_pool_(blocking_pool) {
  DCHECK(delegate_.get());
  DCHECK(aura::Env::GetInstanceDontCreate());
  gpu_support_.reset(delegate_->CreateGPUSupport());
  display_manager_.reset(new DisplayManager);
  window_tree_host_manager_.reset(new WindowTreeHostManager);
  user_metrics_recorder_.reset(new UserMetricsRecorder);

#if defined(OS_CHROMEOS)
  PowerStatus::Initialize();
#endif
}

Shell::~Shell() {
  TRACE_EVENT0("shutdown", "ash::Shell::Destructor");

  user_metrics_recorder_->OnShellShuttingDown();

  delegate_->PreShutdown();

  views::FocusManagerFactory::Install(nullptr);

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
  RemovePreTargetHandler(keyboard_metrics_filter_.get());
  RemovePreTargetHandler(mouse_cursor_filter_.get());

  // TooltipController is deleted with the Shell so removing its references.
  RemovePreTargetHandler(tooltip_controller_.get());

#if defined(OS_CHROMEOS)
  screen_orientation_controller_.reset();
#endif

// Destroy the virtual keyboard controller before the maximize mode controller
// since the latters destructor triggers events that the former is listening
// to but no longer cares about.
#if defined(OS_CHROMEOS)
  virtual_keyboard_controller_.reset();
#endif

  // Destroy maximize mode controller early on since it has some observers which
  // need to be removed.
  maximize_mode_controller_.reset();

#if defined(OS_CHROMEOS)
  // Destroy the LastWindowClosedLogoutReminder before the
  // LogoutConfirmationController.
  last_window_closed_logout_reminder_.reset();

  // Destroy the LogoutConfirmationController before the SystemTrayDelegate.
  logout_confirmation_controller_.reset();
#endif

  // Destroy the keyboard before closing the shelf, since it will invoke a shelf
  // layout.
  DeactivateKeyboard();

  // Destroy toasts
  toast_manager_.reset();

  // Destroy SystemTrayDelegate before destroying the status area(s). Make sure
  // to deinitialize the shelf first, as it is initialized after the delegate.
  HideShelf();
  system_tray_delegate_->Shutdown();
  system_tray_delegate_.reset();

  locale_notification_controller_.reset();

  // Drag-and-drop must be canceled prior to close all windows.
  drag_drop_controller_.reset();

  // Controllers who have WindowObserver added must be deleted
  // before |window_tree_host_manager_| is deleted.

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

  // |shelf_window_watcher_| has a weak pointer to |shelf_Model_|
  // and has window observers.
  shelf_window_watcher_.reset();

  // Destroy all child windows including widgets.
  window_tree_host_manager_->CloseChildWindows();
  // MruWindowTracker must be destroyed after all windows have been deleted to
  // avoid a possible crash when Shell is destroyed from a non-normal shutdown
  // path. (crbug.com/485438).
  mru_window_tracker_.reset();

  // Chrome implementation of shelf delegate depends on FocusClient,
  // so must be deleted before |focus_client_| (below).
  shelf_delegate_.reset();

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
  partial_screenshot_controller_.reset();
  mouse_cursor_filter_.reset();

#if defined(OS_CHROMEOS)
  touch_transformer_controller_.reset();
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  audio_a11y_controller_.reset();
#endif  // defined(OS_CHROMEOS)

  // This also deletes all RootWindows. Note that we invoke Shutdown() on
  // WindowTreeHostManager before resetting |window_tree_host_manager_|, since
  // destruction
  // of its owned RootWindowControllers relies on the value.
  display_manager_->CreateScreenForShutdown();
  display_configuration_controller_.reset();

  // Needs to happen before |window_tree_host_manager_|. Calls back to Shell, so
  // also needs to be destroyed before |instance_| reset to null.
  wm_globals_.reset();

  // Depends on |focus_client_|, so must be destroyed before.
  window_tree_host_manager_->Shutdown();
  window_tree_host_manager_.reset();
  focus_client_.reset();
  screen_position_controller_.reset();
  accessibility_delegate_.reset();
  new_window_delegate_.reset();
  media_delegate_.reset();

  keyboard::KeyboardController::ResetInstance(nullptr);

#if defined(OS_CHROMEOS)
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

  PowerStatus::Shutdown();

  // Ensure that DBusThreadManager outlives this Shell.
  DCHECK(chromeos::DBusThreadManager::IsInitialized());
#endif

  DCHECK(instance_ == this);
  instance_ = nullptr;
}

void Shell::Init(const ShellInitParams& init_params) {
  in_mus_ = init_params.in_mus;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  DCHECK(in_mus_) << "linux desktop does not support ash.";
#endif

  wm_globals_.reset(new wm::WmGlobalsAura);
  window_positioner_.reset(new WindowPositioner(wm_globals_.get()));

  if (!in_mus_) {
    native_cursor_manager_ = new AshNativeCursorManager;
#if defined(OS_CHROMEOS)
    cursor_manager_.reset(
        new CursorManager(base::WrapUnique(native_cursor_manager_)));
#else
    cursor_manager_.reset(
        new ::wm::CursorManager(base::WrapUnique(native_cursor_manager_)));
#endif
  }

  delegate_->PreInit();
  bool display_initialized = display_manager_->InitFromCommandLine();

  display_configuration_controller_.reset(new DisplayConfigurationController(
      display_manager_.get(), window_tree_host_manager_.get()));

#if defined(OS_CHROMEOS)
  // When running as part of mash, OzonePlatform is not initialized in the
  // ash_sysui process. DisplayConfigurator will try to use OzonePlatform and
  // crash. Instead, mash can manually set default display size using
  // --ash-host-window-bounds flag.
  if (in_mus_)
    display_configurator_->set_configure_display(false);

#if defined(USE_OZONE)
  display_configurator_->Init(
      in_mus_ ? nullptr
              : ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(),
      !gpu_support_->IsPanelFittingDisabled());
#elif defined(USE_X11)
  display_configurator_->Init(
      base::WrapUnique(new ui::NativeDisplayDelegateX11()),
      !gpu_support_->IsPanelFittingDisabled());
#endif

  // The DBusThreadManager must outlive this Shell. See the DCHECK in ~Shell.
  chromeos::DBusThreadManager* dbus_thread_manager =
      chromeos::DBusThreadManager::Get();
  projecting_observer_.reset(
      new ProjectingObserver(dbus_thread_manager->GetPowerManagerClient()));
  display_configurator_->AddObserver(projecting_observer_.get());
  AddShellObserver(projecting_observer_.get());

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
  display_color_manager_.reset(
      new DisplayColorManager(display_configurator_.get(), blocking_pool_));
#endif  // defined(OS_CHROMEOS)

  if (!display_initialized)
    display_manager_->InitDefaultDisplay();

  display_manager_->RefreshFontParams();

  // Install the custom factory first so that views::FocusManagers for Tray,
  // Shelf, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  aura::Env::GetInstance()->set_context_factory(init_params.context_factory);

  // The WindowModalityController needs to be at the front of the input event
  // pretarget handler list to ensure that it processes input events when modal
  // windows are active.
  window_modality_controller_.reset(
      new ::wm::WindowModalityController(this));

  env_filter_.reset(new ::wm::CompoundEventFilter);
  AddPreTargetHandler(env_filter_.get());

  wm::AshFocusRules* focus_rules = new wm::AshFocusRules();

  ::wm::FocusController* focus_controller =
      new ::wm::FocusController(focus_rules);
  focus_client_.reset(focus_controller);
  activation_client_ = focus_controller;
  activation_client_->AddObserver(this);
  focus_cycler_.reset(new FocusCycler());

  screen_position_controller_.reset(new ScreenPositionController);

  window_tree_host_manager_->Start();
  window_tree_host_manager_->CreatePrimaryHost(
      ShellInitParamsToAshWindowTreeHostInitParams(init_params));
  aura::Window* root_window = window_tree_host_manager_->GetPrimaryRootWindow();
  target_root_window_ = root_window;

#if defined(OS_CHROMEOS)
  resolution_notification_controller_.reset(
      new ResolutionNotificationController);
#endif

  if (cursor_manager_)
    cursor_manager_->SetDisplay(gfx::Screen::GetScreen()->GetPrimaryDisplay());

  accelerator_controller_.reset(new AcceleratorController);
  maximize_mode_controller_.reset(new MaximizeModeController());

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
  AddShellObserver(overlay_filter_.get());

  accelerator_filter_.reset(new ::wm::AcceleratorFilter(
      std::unique_ptr<::wm::AcceleratorDelegate>(new AcceleratorDelegate),
      accelerator_controller_->accelerator_history()));
  AddPreTargetHandler(accelerator_filter_.get());

  event_transformation_handler_.reset(new EventTransformationHandler);
  AddPreTargetHandler(event_transformation_handler_.get());

  toplevel_window_event_handler_.reset(new ToplevelWindowEventHandler);

  system_gesture_filter_.reset(new SystemGestureEventFilter);
  AddPreTargetHandler(system_gesture_filter_.get());

  keyboard_metrics_filter_.reset(new KeyboardUMAEventFilter);
  AddPreTargetHandler(keyboard_metrics_filter_.get());

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
  // |partial_screenshot_controller_| needs to be created (and prepended as a
  // pre-target handler) at this point, because |mouse_cursor_filter_| needs to
  // process mouse events prior to partial screenshot session.
  // See http://crbug.com/459214
  partial_screenshot_controller_.reset(new PartialScreenshotController());
  mouse_cursor_filter_.reset(new MouseCursorEventFilter());
  PrependPreTargetHandler(mouse_cursor_filter_.get());

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_.reset(new AshVisibilityController);

  magnification_controller_.reset(
      MagnificationController::CreateInstance());
  mru_window_tracker_.reset(new MruWindowTracker(activation_client_,
                                                 focus_rules));

  partial_magnification_controller_.reset(
      new PartialMagnificationController());

  autoclick_controller_.reset(AutoclickController::CreateInstance());

  high_contrast_controller_.reset(new HighContrastController);
  video_detector_.reset(new VideoDetector);
  window_selector_controller_.reset(new WindowSelectorController());
  window_cycle_controller_.reset(new WindowCycleController());

  tooltip_controller_.reset(new views::corewm::TooltipController(
      std::unique_ptr<views::corewm::Tooltip>(new views::corewm::TooltipAura)));
  AddPreTargetHandler(tooltip_controller_.get());

  event_client_.reset(new EventClientImpl);

  // This controller needs to be set before SetupManagedWindowMode.
  desktop_background_controller_.reset(
      new DesktopBackgroundController(blocking_pool_));
  user_wallpaper_delegate_.reset(delegate_->CreateUserWallpaperDelegate());

  session_state_delegate_.reset(delegate_->CreateSessionStateDelegate());
  accessibility_delegate_.reset(delegate_->CreateAccessibilityDelegate());
  new_window_delegate_.reset(delegate_->CreateNewWindowDelegate());
  media_delegate_.reset(delegate_->CreateMediaDelegate());

  resize_shadow_controller_.reset(new ResizeShadowController());
  shadow_controller_.reset(
      new ::wm::ShadowController(activation_client_));

  // Create system_tray_notifier_ before the delegate.
  system_tray_notifier_.reset(new SystemTrayNotifier());

  // Initialize system_tray_delegate_ before initializing StatusAreaWidget.
  system_tray_delegate_.reset(delegate()->CreateSystemTrayDelegate());
  DCHECK(system_tray_delegate_.get());

  locale_notification_controller_.reset(new LocaleNotificationController);

  // Initialize system_tray_delegate_ after StatusAreaWidget is created.
  system_tray_delegate_->Initialize();

  // Initialize toast manager
  toast_manager_.reset(new ToastManager);

#if defined(OS_CHROMEOS)
  // Create the LogoutConfirmationController after the SystemTrayDelegate.
  logout_confirmation_controller_.reset(new LogoutConfirmationController(
      base::Bind(&SystemTrayDelegate::SignOut,
                 base::Unretained(system_tray_delegate_.get()))));

  // Create TouchTransformerController before
  // WindowTreeHostManager::InitDisplays()
  // since TouchTransformerController listens on
  // WindowTreeHostManager::Observer::OnDisplaysInitialized().
  touch_transformer_controller_.reset(new TouchTransformerController());
#endif  // defined(OS_CHROMEOS)

  keyboard_ui_ = init_params.keyboard_factory.is_null()
                     ? KeyboardUI::Create()
                     : init_params.keyboard_factory.Run();

  window_tree_host_manager_->InitHosts();

#if defined(OS_CHROMEOS)
  // Needs to be created after InitDisplays() since it may cause the virtual
  // keyboard to be deployed.
  virtual_keyboard_controller_.reset(new VirtualKeyboardController);
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  audio_a11y_controller_.reset(new chromeos::AudioA11yController);
#endif  // defined(OS_CHROMEOS)

  // It needs to be created after RootWindowController has been created
  // (which calls OnWindowResized has been called, otherwise the
  // widget will not paint when restoring after a browser crash.  Also it needs
  // to be created after InitSecondaryDisplays() to initialize the wallpapers in
  // the correct size.
  user_wallpaper_delegate_->InitializeWallpaper();

  if (cursor_manager_) {
    if (initially_hide_cursor_)
      cursor_manager_->HideCursor();
    cursor_manager_->SetCursor(ui::kCursorPointer);
  }

#if defined(OS_CHROMEOS)
  // Set accelerator controller delegates.
  accelerator_controller_->SetBrightnessControlDelegate(
      std::unique_ptr<BrightnessControlDelegate>(
          new system::BrightnessControllerChromeos));

  power_event_observer_.reset(new PowerEventObserver());
  user_activity_notifier_.reset(
      new ui::UserActivityPowerManagerNotifier(user_activity_detector_.get()));
  video_activity_notifier_.reset(
      new VideoActivityNotifier(video_detector_.get()));
  bluetooth_notification_controller_.reset(new BluetoothNotificationController);
  last_window_closed_logout_reminder_.reset(new LastWindowClosedLogoutReminder);
  screen_orientation_controller_.reset(new ScreenOrientationController());
#endif
  // The compositor thread and main message loop have to be running in
  // order to create mirror window. Run it after the main message loop
  // is started.
  display_manager_->CreateMirrorWindowAsyncIfAny();

  FOR_EACH_OBSERVER(ShellObserver, observers_, OnShellInitialized());

  user_metrics_recorder_->OnShellInitialized();
}

void Shell::InitKeyboard() {
  if (in_mus_)
    return;

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
        new keyboard::KeyboardController(delegate_->CreateKeyboardUI()));
  }
}

void Shell::InitRootWindow(aura::Window* root_window) {
  DCHECK(activation_client_);
  DCHECK(visibility_controller_.get());
  DCHECK(drag_drop_controller_.get());

  aura::client::SetFocusClient(root_window, focus_client_.get());
  aura::client::SetActivationClient(root_window, activation_client_);
  ::wm::FocusController* focus_controller =
      static_cast< ::wm::FocusController*>(activation_client_);
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

std::unique_ptr<ui::EventTargetIterator> Shell::GetChildIterator() const {
  return std::unique_ptr<ui::EventTargetIterator>();
}

ui::EventTargeter* Shell::GetEventTargeter() {
  NOTREACHED();
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Shell, aura::client::ActivationChangeObserver implementation:

void Shell::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active)
    target_root_window_ = gained_active->GetRootWindow();
}

}  // namespace ash
