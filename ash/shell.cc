// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <string>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_filter.h"
#include "ash/accelerators/focus_manager_factory.h"
#include "ash/accelerators/nested_dispatcher_controller.h"
#include "ash/ash_switches.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/caps_lock_delegate.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_position_controller.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/first_run/first_run_helper_impl.h"
#include "ash/focus_cycler.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/keyboard_uma_event_filter.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_item_delegate.h"
#include "ash/launcher/launcher_item_delegate_manager.h"
#include "ash/launcher/launcher_model.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/media_delegate.h"
#include "ash/new_window_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/session_state_delegate.h"
#include "ash/shelf/app_list_shelf_item_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model_util.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/system/locale/locale_notification_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/app_list_controller.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/ash_native_cursor_manager.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/custom_frame_view_ash.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/event_rewriter_event_filter.h"
#include "ash/wm/lock_state_controller.h"
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
#include "ash/wm/user_activity_detector.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/trace_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/user_action_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/keyboard/keyboard.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/message_center/message_center.h"
#include "ui/views/corewm/compound_event_filter.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/corewm/focus_controller.h"
#include "ui/views/corewm/input_method_event_filter.h"
#include "ui/views/corewm/shadow_controller.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/corewm/visibility_controller.h"
#include "ui/views/corewm/window_modality_controller.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#if defined(USE_X11)
#include "ash/ash_constants.h"
#include "ash/display/display_change_observer_chromeos.h"
#include "ash/display/display_error_observer_chromeos.h"
#include "ash/display/output_configurator_animation.h"
#include "base/message_loop/message_pump_x11.h"
#include "base/sys_info.h"
#include "chromeos/display/output_configurator.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_feature_type.h"
#endif  // defined(USE_X11)
#include "ash/system/chromeos/brightness/brightness_controller_chromeos.h"
#include "ash/system/chromeos/power/power_event_observer.h"
#include "ash/system/chromeos/power/power_status.h"
#include "ash/system/chromeos/power/user_activity_notifier.h"
#include "ash/system/chromeos/power/video_activity_notifier.h"
#endif  // defined(OS_CHROMEOS)

namespace ash {

namespace {

using aura::Window;
using views::Widget;

// A Corewm VisibilityController subclass that calls the Ash animation routine
// so we can pick up our extended animations. See ash/wm/window_animations.h.
class AshVisibilityController : public views::corewm::VisibilityController {
 public:
  AshVisibilityController() {}
  virtual ~AshVisibilityController() {}

 private:
  // Overridden from views::corewm::VisibilityController:
  virtual bool CallAnimateOnChildWindowVisibilityChanged(
      aura::Window* window,
      bool visible) OVERRIDE {
    return AnimateOnChildWindowVisibilityChanged(window, visible);
  }

  DISALLOW_COPY_AND_ASSIGN(AshVisibilityController);
};

}  // namespace

// static
Shell* Shell::instance_ = NULL;
// static
bool Shell::initially_hide_cursor_ = false;

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

// static
Shell* Shell::CreateInstance(ShellDelegate* delegate) {
  CHECK(!instance_);
  instance_ = new Shell(delegate);
  instance_->Init();
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
internal::RootWindowController* Shell::GetPrimaryRootWindowController() {
  return internal::GetRootWindowController(GetPrimaryRootWindow());
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
Shell::RootWindowList Shell::GetAllRootWindows() {
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
  RootWindowList root_windows = GetAllRootWindows();
  for (RootWindowList::const_iterator it = root_windows.begin();
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
  internal::GetRootWindowController(root)->
      ShowContextMenu(location_in_screen, source_type);
}

void Shell::ToggleAppList(aura::Window* window) {
  // If the context window is not given, show it on the target root window.
  if (!window)
    window = GetTargetRootWindow();
  if (!app_list_controller_)
    app_list_controller_.reset(new internal::AppListController);
  app_list_controller_->SetVisible(!app_list_controller_->IsVisible(), window);
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
      internal::kShellWindowId_SystemModalContainer, NULL);
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
  focus_cycler_->RotateFocus(
      direction == FORWARD ? internal::FocusCycler::FORWARD :
                             internal::FocusCycler::BACKWARD);
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
  if (status != user::LOGGED_IN_NONE) {
    // TODO(bshe): Primary root window controller may not be the controller to
    // attach virtual keyboard. See http://crbug.com/303429
    InitKeyboard(GetPrimaryRootWindowController());
    GetPrimaryRootWindowController()->ActivateKeyboard(
        keyboard_controller_.get());
  }
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnLoginStateChanged(status));
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
        internal::kShellWindowId_LockSystemModalContainer,
        GetPrimaryRootWindow());
    for (std::vector<aura::Window*>::const_iterator iter = containers.begin();
         iter != containers.end(); ++iter) {
      DCHECK_EQ(0u, (*iter)->children().size());
    }
  }
#endif
}

void Shell::CreateLauncher() {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->shelf()->CreateLauncher();
}

void Shell::ShowLauncher() {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->ShowLauncher();
}

void Shell::AddShellObserver(ShellObserver* observer) {
  observers_.AddObserver(observer);
}

void Shell::RemoveShellObserver(ShellObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Shell::UpdateShelfVisibility() {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    if ((*iter)->shelf())
      (*iter)->UpdateShelfVisibility();
}

void Shell::SetShelfAutoHideBehavior(ShelfAutoHideBehavior behavior,
                                     aura::Window* root_window) {
  ash::internal::ShelfLayoutManager::ForLauncher(root_window)->
      SetAutoHideBehavior(behavior);
}

ShelfAutoHideBehavior Shell::GetShelfAutoHideBehavior(
    aura::Window* root_window) const {
  return ash::internal::ShelfLayoutManager::ForLauncher(root_window)->
      auto_hide_behavior();
}

void Shell::SetShelfAlignment(ShelfAlignment alignment,
                              aura::Window* root_window) {
  if (ash::internal::ShelfLayoutManager::ForLauncher(root_window)->
      SetAlignment(alignment)) {
    FOR_EACH_OBSERVER(
        ShellObserver, observers_, OnShelfAlignmentChanged(root_window));
  }
}

ShelfAlignment Shell::GetShelfAlignment(aura::Window* root_window) {
  return internal::GetRootWindowController(root_window)->
      GetShelfLayoutManager()->GetAlignment();
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
    modality_filter_.reset(new internal::SystemModalContainerEventFilter(this));
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

LauncherDelegate* Shell::GetLauncherDelegate() {
  if (!launcher_delegate_) {
    launcher_model_.reset(new LauncherModel);
    // Creates LauncherItemDelegateManager before LauncherDelegate.
    launcher_item_delegate_manager_.reset(
        new LauncherItemDelegateManager(launcher_model_.get()));

    launcher_delegate_.reset(
        delegate_->CreateLauncherDelegate(launcher_model_.get()));
    scoped_ptr<LauncherItemDelegate> controller(
        new internal::AppListShelfItemDelegate);

    // Finding the launcher model's location of the app list and setting its
    // LauncherItemDelegate.
    int app_list_index =
        GetShelfItemIndexForType(ash::TYPE_APP_LIST, *launcher_model_);
    DCHECK_GE(app_list_index, 0);
    ash::LauncherID app_list_id = launcher_model_->items()[app_list_index].id;
    DCHECK(app_list_id);
    launcher_item_delegate_manager_->SetLauncherItemDelegate(
        app_list_id,
        controller.Pass());
  }
  return launcher_delegate_.get();
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
#endif  // defined(OS_CHROMEOS)

void Shell::DoInitialWorkspaceAnimation() {
  return GetPrimaryRootWindowController()->workspace_controller()->
      DoInitialAnimation();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

Shell::Shell(ShellDelegate* delegate)
    : screen_(new ScreenAsh),
      target_root_window_(NULL),
      scoped_target_root_window_(NULL),
      delegate_(delegate),
      window_positioner_(new WindowPositioner),
      activation_client_(NULL),
#if defined(OS_CHROMEOS) && defined(USE_X11)
      output_configurator_(new chromeos::OutputConfigurator()),
#endif  // defined(OS_CHROMEOS) && defined(USE_X11)
      native_cursor_manager_(new AshNativeCursorManager),
      cursor_manager_(scoped_ptr<views::corewm::NativeCursorManager>(
          native_cursor_manager_)),
      browser_context_(NULL),
      simulate_modal_window_open_for_testing_(false),
      is_touch_hud_projection_enabled_(false) {
  DCHECK(delegate_.get());
  display_manager_.reset(new internal::DisplayManager);

  ANNOTATE_LEAKING_OBJECT_PTR(screen_);  // see crbug.com/156466
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_ALTERNATE, screen_);
  if (!gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE))
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_);
  display_controller_.reset(new DisplayController);
#if defined(OS_CHROMEOS) && defined(USE_X11)
  bool is_panel_fitting_disabled =
      content::GpuDataManager::GetInstance()->IsFeatureBlacklisted(
          gpu::GPU_FEATURE_TYPE_PANEL_FITTING);

  output_configurator_->Init(!is_panel_fitting_disabled);

  base::MessagePumpX11::Current()->AddDispatcherForRootWindow(
      output_configurator());
  // We can't do this with a root window listener because XI_HierarchyChanged
  // messages don't have a target window.
  base::MessagePumpX11::Current()->AddObserver(output_configurator());
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  internal::PowerStatus::Initialize();
#endif
}

Shell::~Shell() {
  TRACE_EVENT0("shutdown", "ash::Shell::Destructor");

  views::FocusManagerFactory::Install(NULL);

  // Remove the focus from any window. This will prevent overhead and side
  // effects (e.g. crashes) from changing focus during shutdown.
  // See bug crbug.com/134502.
  aura::client::GetFocusClient(GetPrimaryRootWindow())->FocusWindow(NULL);

  // Please keep in same order as in Init() because it's easy to miss one.
  if (window_modality_controller_)
    window_modality_controller_.reset();
  RemovePreTargetHandler(event_rewriter_filter_.get());
  RemovePreTargetHandler(user_activity_detector_.get());
  RemovePreTargetHandler(overlay_filter_.get());
  RemovePreTargetHandler(input_method_filter_.get());
  if (mouse_cursor_filter_)
    RemovePreTargetHandler(mouse_cursor_filter_.get());
  RemovePreTargetHandler(system_gesture_filter_.get());
  RemovePreTargetHandler(keyboard_metrics_filter_.get());
  RemovePreTargetHandler(event_transformation_handler_.get());
  RemovePreTargetHandler(accelerator_filter_.get());

  // TooltipController is deleted with the Shell so removing its references.
  RemovePreTargetHandler(tooltip_controller_.get());

  // AppList needs to be released before shelf layout manager, which is
  // destroyed with launcher container in the loop below. However, app list
  // container is now on top of launcher container and released after it.
  // TODO(xiyuan): Move it back when app list container is no longer needed.
  app_list_controller_.reset();

  // Destroy SystemTrayDelegate before destroying the status area(s).
  system_tray_delegate_->Shutdown();
  system_tray_delegate_.reset();

  locale_notification_controller_.reset();

  // Drag-and-drop must be canceled prior to close all windows.
  drag_drop_controller_.reset();

  // Destroy all child windows including widgets.
  display_controller_->CloseChildWindows();

  // Destroy SystemTrayNotifier after destroying SystemTray as TrayItems
  // needs to remove observers from it.
  system_tray_notifier_.reset();

#if defined(OS_CHROMEOS)
  // Destroy VideoActivityNotifier before destroying VideoDetector.
  video_activity_notifier_.reset();
#endif  // defined(OS_CHROMEOS)

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  // Alphabetical. TODO(oshima): sort.
  magnification_controller_.reset();
  partial_magnification_controller_.reset();
  resize_shadow_controller_.reset();
  shadow_controller_.reset();
  tooltip_controller_.reset();
  event_client_.reset();
  window_cycle_controller_.reset();
  nested_dispatcher_controller_.reset();
  user_action_client_.reset();
  visibility_controller_.reset();
  launcher_delegate_.reset();
  // |launcher_item_delegate_manager_| observes |launcher_model_|. It must be
  // destroyed before |launcher_model_| is destroyed.
  launcher_item_delegate_manager_.reset();
  launcher_model_.reset();
  video_detector_.reset();

  power_button_controller_.reset();
  lock_state_controller_.reset();
  mru_window_tracker_.reset();

  resolution_notification_controller_.reset();
  desktop_background_controller_.reset();

  // This also deletes all RootWindows. Note that we invoke Shutdown() on
  // DisplayController before resetting |display_controller_|, since destruction
  // of its owned RootWindowControllers relies on the value.
  display_controller_->Shutdown();
  display_controller_.reset();
  screen_position_controller_.reset();

  keyboard_controller_.reset();
  accessibility_delegate_.reset();
  new_window_delegate_.reset();
  media_delegate_.reset();

#if defined(OS_CHROMEOS) && defined(USE_X11)
   if (display_change_observer_)
    output_configurator_->RemoveObserver(display_change_observer_.get());
  if (output_configurator_animation_)
    output_configurator_->RemoveObserver(output_configurator_animation_.get());
  if (display_error_observer_)
    output_configurator_->RemoveObserver(display_error_observer_.get());
  base::MessagePumpX11::Current()->RemoveDispatcherForRootWindow(
      output_configurator());
  base::MessagePumpX11::Current()->RemoveObserver(output_configurator());
  display_change_observer_.reset();
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  internal::PowerStatus::Shutdown();
#endif

  DCHECK(instance_ == this);
  instance_ = NULL;
}

void Shell::Init() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  delegate_->PreInit();
  if (command_line->HasSwitch(keyboard::switches::kKeyboardUsabilityTest)) {
    display_manager_->SetSecondDisplayMode(
        internal::DisplayManager::VIRTUAL_KEYBOARD);
  }
  bool display_initialized = display_manager_->InitFromCommandLine();
#if defined(OS_CHROMEOS) && defined(USE_X11)
  output_configurator_animation_.reset(
      new internal::OutputConfiguratorAnimation());
  output_configurator_->AddObserver(output_configurator_animation_.get());

  if (!display_initialized && base::SysInfo::IsRunningOnChromeOS()) {
    display_change_observer_.reset(new internal::DisplayChangeObserver);
    // Register |display_change_observer_| first so that the rest of
    // observer gets invoked after the root windows are configured.
    output_configurator_->AddObserver(display_change_observer_.get());
    display_error_observer_.reset(new internal::DisplayErrorObserver());
    output_configurator_->AddObserver(display_error_observer_.get());
    output_configurator_->set_state_controller(display_change_observer_.get());
    output_configurator_->set_mirroring_controller(display_manager_.get());
    output_configurator_->Start(
        delegate_->IsFirstRunAfterBoot() ? kChromeOsBootColor : 0);
    display_initialized = true;
  }
#endif  // defined(OS_CHROMEOS) && defined(USE_X11)
  if (!display_initialized)
    display_manager_->InitDefaultDisplay();

  // Install the custom factory first so that views::FocusManagers for Tray,
  // Launcher, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  // Env creates the compositor. Historically it seems to have been implicitly
  // initialized first by the ActivationController, but now that FocusController
  // no longer does this we need to do it explicitly.
  aura::Env::CreateInstance();

  // The WindowModalityController needs to be at the front of the input event
  // pretarget handler list to ensure that it processes input events when modal
  // windows are active.
  window_modality_controller_.reset(
      new views::corewm::WindowModalityController(this));

  AddPreTargetHandler(this);

  env_filter_.reset(new views::corewm::CompoundEventFilter);
  AddPreTargetHandler(env_filter_.get());

  views::corewm::FocusController* focus_controller =
      new views::corewm::FocusController(new wm::AshFocusRules);
  focus_client_.reset(focus_controller);
  activation_client_ = focus_controller;
  activation_client_->AddObserver(this);
  focus_cycler_.reset(new internal::FocusCycler());

  screen_position_controller_.reset(new internal::ScreenPositionController);
  root_window_host_factory_.reset(delegate_->CreateRootWindowHostFactory());

  display_controller_->Start();
  display_controller_->InitPrimaryDisplay();
  aura::Window* root_window = display_controller_->GetPrimaryRootWindow();
  target_root_window_ = root_window;

  resolution_notification_controller_.reset(
      new internal::ResolutionNotificationController);

  cursor_manager_.SetDisplay(DisplayController::GetPrimaryDisplay());

  nested_dispatcher_controller_.reset(new NestedDispatcherController);
  accelerator_controller_.reset(new AcceleratorController);

  // The order in which event filters are added is significant.
  event_rewriter_filter_.reset(new internal::EventRewriterEventFilter);
  AddPreTargetHandler(event_rewriter_filter_.get());

  // UserActivityDetector passes events to observers, so let them get
  // rewritten first.
  user_activity_detector_.reset(new UserActivityDetector);
  AddPreTargetHandler(user_activity_detector_.get());

  overlay_filter_.reset(new internal::OverlayEventFilter);
  AddPreTargetHandler(overlay_filter_.get());
  AddShellObserver(overlay_filter_.get());

  input_method_filter_.reset(new views::corewm::InputMethodEventFilter(
      root_window->GetDispatcher()->GetAcceleratedWidget()));
  AddPreTargetHandler(input_method_filter_.get());

  accelerator_filter_.reset(new internal::AcceleratorFilter);
  AddPreTargetHandler(accelerator_filter_.get());

  event_transformation_handler_.reset(new internal::EventTransformationHandler);
  AddPreTargetHandler(event_transformation_handler_.get());

  system_gesture_filter_.reset(new internal::SystemGestureEventFilter);
  AddPreTargetHandler(system_gesture_filter_.get());

  keyboard_metrics_filter_.reset(new internal::KeyboardUMAEventFilter);
  AddPreTargetHandler(keyboard_metrics_filter_.get());

  // The keyboard system must be initialized before the RootWindowController is
  // created.
  if (keyboard::IsKeyboardEnabled())
    keyboard::InitializeKeyboard();

  lock_state_controller_.reset(new LockStateController);
  power_button_controller_.reset(new PowerButtonController(
      lock_state_controller_.get()));
  AddShellObserver(lock_state_controller_.get());

  drag_drop_controller_.reset(new internal::DragDropController);
  mouse_cursor_filter_.reset(new internal::MouseCursorEventFilter());
  PrependPreTargetHandler(mouse_cursor_filter_.get());

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_.reset(new AshVisibilityController);
  user_action_client_.reset(delegate_->CreateUserActionClient());

  magnification_controller_.reset(
      MagnificationController::CreateInstance());
  mru_window_tracker_.reset(new MruWindowTracker(activation_client_));

  partial_magnification_controller_.reset(
      new PartialMagnificationController());

  autoclick_controller_.reset(AutoclickController::CreateInstance());

  high_contrast_controller_.reset(new HighContrastController);
  video_detector_.reset(new VideoDetector);
  window_cycle_controller_.reset(new WindowCycleController());
  window_selector_controller_.reset(new WindowSelectorController());

  tooltip_controller_.reset(
      new views::corewm::TooltipController(
          scoped_ptr<views::corewm::Tooltip>(
              new views::corewm::TooltipAura(gfx::SCREEN_TYPE_ALTERNATE))));
  AddPreTargetHandler(tooltip_controller_.get());

  event_client_.reset(new internal::EventClientImpl);

  // This controller needs to be set before SetupManagedWindowMode.
  desktop_background_controller_.reset(new DesktopBackgroundController());
  user_wallpaper_delegate_.reset(delegate_->CreateUserWallpaperDelegate());

  // StatusAreaWidget uses Shell's CapsLockDelegate.
  caps_lock_delegate_.reset(delegate_->CreateCapsLockDelegate());

  session_state_delegate_.reset(delegate_->CreateSessionStateDelegate());
  accessibility_delegate_.reset(delegate_->CreateAccessibilityDelegate());
  new_window_delegate_.reset(delegate_->CreateNewWindowDelegate());
  media_delegate_.reset(delegate_->CreateMediaDelegate());

  if (!command_line->HasSwitch(views::corewm::switches::kNoDropShadows)) {
    resize_shadow_controller_.reset(new internal::ResizeShadowController());
    shadow_controller_.reset(
        new views::corewm::ShadowController(activation_client_));
  }

  // Create system_tray_notifier_ before the delegate.
  system_tray_notifier_.reset(new ash::SystemTrayNotifier());

  // Initialize system_tray_delegate_ before initializing StatusAreaWidget.
  system_tray_delegate_.reset(delegate()->CreateSystemTrayDelegate());
  DCHECK(system_tray_delegate_.get());

  locale_notification_controller_.reset(
      new internal::LocaleNotificationController);

  // Initialize system_tray_delegate_ after StatusAreaWidget is created.
  system_tray_delegate_->Initialize();

  // TODO(oshima): Initialize all RootWindowControllers once, and
  // initialize controller/delegates above when initializing the
  // primary root window controller.
  internal::RootWindowController::CreateForPrimaryDisplay(
      root_window->GetDispatcher());

  display_controller_->InitSecondaryDisplays();

  // It needs to be created after RootWindowController has been created
  // (which calls OnWindowResized has been called, otherwise the
  // widget will not paint when restoring after a browser crash.  Also it needs
  // to be created after InitSecondaryDisplays() to initialize the wallpapers in
  // the correct size.
  user_wallpaper_delegate_->InitializeWallpaper();

  if (initially_hide_cursor_)
    cursor_manager_.HideCursor();
  cursor_manager_.SetCursor(ui::kCursorPointer);

  if (!cursor_manager_.IsCursorVisible()) {
    // Cursor might have been hidden by something other than chrome.
    // Let the first mouse event show the cursor.
    env_filter_->set_cursor_hidden_by_filter(true);
  }

#if defined(OS_CHROMEOS)
  // Set accelerator controller delegates.
  accelerator_controller_->SetBrightnessControlDelegate(
      scoped_ptr<ash::BrightnessControlDelegate>(
          new ash::system::BrightnessControllerChromeos).Pass());

  power_event_observer_.reset(new internal::PowerEventObserver());
  user_activity_notifier_.reset(
      new internal::UserActivityNotifier(user_activity_detector_.get()));
  video_activity_notifier_.reset(
      new internal::VideoActivityNotifier(video_detector_.get()));
#endif

  weak_display_manager_factory_.reset(
      new base::WeakPtrFactory<internal::DisplayManager>(
          display_manager_.get()));
  // The compositor thread and main message loop have to be running in
  // order to create mirror window. Run it after the main message loop
  // is started.
  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&internal::DisplayManager::CreateMirrorWindowIfAny,
                 weak_display_manager_factory_->GetWeakPtr()));
}

void Shell::InitKeyboard(internal::RootWindowController* root) {
  if (keyboard::IsKeyboardEnabled()) {
    if (keyboard_controller_.get()) {
      RootWindowControllerList controllers = GetAllRootWindowControllers();
      for (RootWindowControllerList::iterator iter = controllers.begin();
           iter != controllers.end(); ++iter) {
        (*iter)->DeactivateKeyboard(keyboard_controller_.get());
      }
    }
    keyboard::KeyboardControllerProxy* proxy =
        delegate_->CreateKeyboardControllerProxy();
    keyboard_controller_.reset(
        new keyboard::KeyboardController(proxy));
  }
}

void Shell::InitRootWindow(aura::RootWindow* root_window) {
  DCHECK(activation_client_);
  DCHECK(visibility_controller_.get());
  DCHECK(drag_drop_controller_.get());
  DCHECK(window_cycle_controller_.get());

  aura::client::SetFocusClient(root_window, focus_client_.get());
  input_method_filter_->SetInputMethodPropertyInRootWindow(root_window);
  aura::client::SetActivationClient(root_window, activation_client_);
  views::corewm::FocusController* focus_controller =
      static_cast<views::corewm::FocusController*>(activation_client_);
  root_window->AddPreTargetHandler(focus_controller);
  aura::client::SetVisibilityClient(root_window, visibility_controller_.get());
  aura::client::SetDragDropClient(root_window, drag_drop_controller_.get());
  aura::client::SetScreenPositionClient(root_window,
                                        screen_position_controller_.get());
  aura::client::SetCursorClient(root_window, &cursor_manager_);
  aura::client::SetTooltipClient(root_window, tooltip_controller_.get());
  aura::client::SetEventClient(root_window, event_client_.get());

  if (nested_dispatcher_controller_) {
    aura::client::SetDispatcherClient(root_window,
                                      nested_dispatcher_controller_.get());
  }
  if (user_action_client_)
    aura::client::SetUserActionClient(root_window, user_action_client_.get());
}

bool Shell::CanWindowReceiveEvents(aura::Window* window) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter) {
    internal::SystemModalContainerLayoutManager* layout_manager =
        (*iter)->GetSystemModalLayoutManager(window);
    if (layout_manager && layout_manager->CanWindowReceiveEvents(window))
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
