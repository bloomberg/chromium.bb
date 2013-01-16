// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <string>

#include "ash/accelerators/focus_manager_factory.h"
#include "ash/ash_switches.h"
#include "ash/caps_lock_delegate.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/screen_position_controller.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/focus_cycler.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/tooltips/tooltip_controller.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/app_list_controller.h"
#include "ash/wm/ash_activation_controller.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/capture_controller.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/custom_frame_view_ash.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/event_rewriter_event_filter.h"
#include "ash/wm/overlay_event_filter.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/property_util.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/screen_dimmer.h"
#include "ash/wm/session_state_controller.h"
#include "ash/wm/session_state_controller_impl.h"
#include "ash/wm/session_state_controller_impl2.h"
#include "ash/wm/system_gesture_event_filter.h"
#include "ash/wm/system_modal_container_event_filter.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/user_activity_detector.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/user_action_client.h"
#include "ui/aura/env.h"
#include "ui/aura/focus_manager.h"
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
#include "ui/views/corewm/compound_event_filter.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/corewm/focus_controller.h"
#include "ui/views/corewm/input_method_event_filter.h"
#include "ui/views/corewm/shadow_controller.h"
#include "ui/views/corewm/visibility_controller.h"
#include "ui/views/corewm/window_modality_controller.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_frame_view.h"

#if !defined(OS_MACOSX)
#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_filter.h"
#include "ash/accelerators/nested_dispatcher_controller.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/display/display_change_observer_x11.h"
#include "ash/display/output_configurator_animation.h"
#include "base/chromeos/chromeos_version.h"
#include "base/message_pump_aurax11.h"
#include "chromeos/display/output_configurator.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/gpu_feature_type.h"
#endif  // defined(OS_CHROMEOS)

namespace ash {

namespace {

using aura::Window;
using views::Widget;

// This dummy class is used for shell unit tests. We dont have chrome delegate
// in these tests.
class DummyUserWallpaperDelegate : public UserWallpaperDelegate {
 public:
  DummyUserWallpaperDelegate() {}

  virtual ~DummyUserWallpaperDelegate() {}

  virtual int GetAnimationType() OVERRIDE {
    return views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
  }

  virtual bool ShouldShowInitialAnimation() OVERRIDE {
    return false;
  }

  virtual void UpdateWallpaper() OVERRIDE {
  }

  virtual void InitializeWallpaper() OVERRIDE {
    ash::Shell::GetInstance()->desktop_background_controller()->
        CreateEmptyWallpaper();
  }

  virtual void OpenSetWallpaperPage() OVERRIDE {
  }

  virtual bool CanOpenSetWallpaperPage() OVERRIDE {
    return false;
  }

  virtual void OnWallpaperAnimationFinished() OVERRIDE {
  }

  virtual void OnWallpaperBootAnimationFinished() OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyUserWallpaperDelegate);
};

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

Shell::Shell(ShellDelegate* delegate)
    : screen_(new ScreenAsh),
      active_root_window_(NULL),
      delegate_(delegate),
      activation_client_(NULL),
#if defined(OS_CHROMEOS)
      output_configurator_(new chromeos::OutputConfigurator()),
      output_configurator_animation_(
          new internal::OutputConfiguratorAnimation()),
#endif  // defined(OS_CHROMEOS)
      browser_context_(NULL),
      simulate_modal_window_open_for_testing_(false) {
  DCHECK(delegate_.get());
  display_manager_.reset(new internal::DisplayManager);
  ANNOTATE_LEAKING_OBJECT_PTR(screen_);  // see crbug.com/156466
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_ALTERNATE, screen_);
  if (!gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE))
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_);
#if defined(OS_CHROMEOS)
  content::GpuFeatureType blacklisted_features =
      content::GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  bool is_panel_fitting_disabled =
      (blacklisted_features & content::GPU_FEATURE_TYPE_PANEL_FITTING) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDisablePanelFitting);
  output_configurator_->Init(!is_panel_fitting_disabled);

  output_configurator_->AddObserver(output_configurator_animation_.get());
  base::MessagePumpAuraX11::Current()->AddDispatcherForRootWindow(
      output_configurator());
#endif  // defined(OS_CHROMEOS)
  AddPreTargetHandler(this);
}

Shell::~Shell() {
  views::FocusManagerFactory::Install(NULL);

  // Remove the focus from any window. This will prevent overhead and side
  // effects (e.g. crashes) from changing focus during shutdown.
  // See bug crbug.com/134502.
  if (active_root_window_)
    aura::client::GetFocusClient(active_root_window_)->FocusWindow(NULL);

  // Please keep in same order as in Init() because it's easy to miss one.
  RemovePreTargetHandler(user_activity_detector_.get());
  RemovePreTargetHandler(event_rewriter_filter_.get());
  RemovePreTargetHandler(overlay_filter_.get());
  RemovePreTargetHandler(input_method_filter_.get());
  RemovePreTargetHandler(window_modality_controller_.get());
  if (mouse_cursor_filter_.get())
    RemovePreTargetHandler(mouse_cursor_filter_.get());
  RemovePreTargetHandler(system_gesture_filter_.get());
#if !defined(OS_MACOSX)
  RemovePreTargetHandler(accelerator_filter_.get());
#endif
  if (touch_observer_hud_.get())
    RemovePreTargetHandler(touch_observer_hud_.get());

  // TooltipController is deleted with the Shell so removing its references.
  RemovePreTargetHandler(tooltip_controller_.get());

  // AppList needs to be released before shelf layout manager, which is
  // destroyed with launcher container in the loop below. However, app list
  // container is now on top of launcher container and released after it.
  // TODO(xiyuan): Move it back when app list container is no longer needed.
  app_list_controller_.reset();

  // Destroy SystemTrayDelegate before destroying the status area(s).
  system_tray_delegate_.reset();

  // Destroy all child windows including widgets.
  display_controller_->CloseChildWindows();

  // Destroy SystemTrayNotifier after destroying SystemTray as TrayItems
  // needs to remove observers from it.
  system_tray_notifier_.reset();

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  // Alphabetical. TODO(oshima): sort.
  drag_drop_controller_.reset();
  magnification_controller_.reset();
  partial_magnification_controller_.reset();
  resize_shadow_controller_.reset();
  shadow_controller_.reset();
  tooltip_controller_.reset();
  event_client_.reset();
  window_cycle_controller_.reset();
  capture_controller_.reset();
  nested_dispatcher_controller_.reset();
  user_action_client_.reset();
  visibility_controller_.reset();
  launcher_delegate_.reset();
  launcher_model_.reset();

  power_button_controller_.reset();
  session_state_controller_.reset();

  // This also deletes all RootWindows.
  display_controller_.reset();
  screen_position_controller_.reset();

  // Delete the activation controller after other controllers and launcher
  // because they might have registered ActivationChangeObserver.
  activation_controller_.reset();

  DCHECK(instance_ == this);
  instance_ = NULL;

#if defined(OS_CHROMEOS)
  output_configurator_->RemoveObserver(output_configurator_animation_.get());
  base::MessagePumpAuraX11::Current()->RemoveDispatcherForRootWindow(
      output_configurator());
#endif  // defined(OS_CHROMEOS)
}

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
  return GetRootWindowController(GetPrimaryRootWindow());
}

// static
Shell::RootWindowControllerList Shell::GetAllRootWindowControllers() {
  return Shell::GetInstance()->display_controller()->
      GetAllRootWindowControllers();
}

// static
aura::RootWindow* Shell::GetPrimaryRootWindow() {
  return GetInstance()->display_controller()->GetPrimaryRootWindow();
}

// static
aura::RootWindow* Shell::GetActiveRootWindow() {
  return GetInstance()->active_root_window_;
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
aura::Window* Shell::GetContainer(aura::RootWindow* root_window,
                                  int container_id) {
  return root_window->GetChildById(container_id);
}

// static
const aura::Window* Shell::GetContainer(const aura::RootWindow* root_window,
                                        int container_id) {
  return root_window->GetChildById(container_id);
}

// static
std::vector<aura::Window*> Shell::GetContainersFromAllRootWindows(
    int container_id,
    aura::RootWindow* priority_root) {
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

// static
bool Shell::IsLauncherPerDisplayEnabled() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kAshLauncherPerDisplay);
}

void Shell::Init() {
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    display_change_observer_.reset(new internal::DisplayChangeObserverX11);
    display_change_observer_->NotifyDisplayChange();
  }
#endif

  // Install the custom factory first so that views::FocusManagers for Tray,
  // Launcher, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  env_filter_.reset(new views::corewm::CompoundEventFilter);
  AddPreTargetHandler(env_filter_.get());

  if (views::corewm::UseFocusController()) {
    views::corewm::FocusController* focus_controller =
        new views::corewm::FocusController(new wm::AshFocusRules);
    focus_client_.reset(focus_controller);
    activation_client_ = focus_controller;
    activation_client_->AddObserver(this);
  } else {
    focus_client_.reset(new aura::FocusManager);
    activation_controller_.reset(
        new internal::ActivationController(
            focus_client_.get(),
            new internal::AshActivationController));
    activation_client_ = activation_controller_.get();
    AddPreTargetHandler(activation_controller_.get());
  }

  focus_cycler_.reset(new internal::FocusCycler());

  screen_position_controller_.reset(new internal::ScreenPositionController);
  root_window_host_factory_.reset(delegate_->CreateRootWindowHostFactory());
  display_controller_.reset(new DisplayController);
  display_controller_->InitPrimaryDisplay();
  aura::RootWindow* root_window = display_controller_->GetPrimaryRootWindow();
  active_root_window_ = root_window;

  cursor_manager_.SetDeviceScaleFactor(
      root_window->AsRootWindowHostDelegate()->GetDeviceScaleFactor());

#if !defined(OS_MACOSX)
  nested_dispatcher_controller_.reset(new NestedDispatcherController);
  accelerator_controller_.reset(new AcceleratorController);
#endif

  // The order in which event filters are added is significant.
  user_activity_detector_.reset(new UserActivityDetector);
  AddPreTargetHandler(user_activity_detector_.get());

  event_rewriter_filter_.reset(new internal::EventRewriterEventFilter);
  AddPreTargetHandler(event_rewriter_filter_.get());

  overlay_filter_.reset(new internal::OverlayEventFilter);
  AddPreTargetHandler(overlay_filter_.get());
  AddShellObserver(overlay_filter_.get());

  input_method_filter_.reset(new views::corewm::InputMethodEventFilter(
                                 root_window->GetAcceleratedWidget()));
  AddPreTargetHandler(input_method_filter_.get());

#if !defined(OS_MACOSX)
  accelerator_filter_.reset(new internal::AcceleratorFilter);
  AddPreTargetHandler(accelerator_filter_.get());
#endif

  event_transformation_handler_.reset(new internal::EventTransformationHandler);
  AddPreTargetHandler(event_transformation_handler_.get());

  system_gesture_filter_.reset(new internal::SystemGestureEventFilter);
  AddPreTargetHandler(system_gesture_filter_.get());

  capture_controller_.reset(new internal::CaptureController);

  internal::RootWindowController* root_window_controller =
      new internal::RootWindowController(root_window);
  root_window_controller->CreateContainers();
  root_window_controller->CreateSystemBackground(
      delegate_->IsFirstRunAfterBoot());

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(ash::switches::kAshDisableNewLockAnimations))
    session_state_controller_.reset(new SessionStateControllerImpl);
  else
    session_state_controller_.reset(new SessionStateControllerImpl2);
  power_button_controller_.reset(new PowerButtonController(
      session_state_controller_.get()));
  AddShellObserver(session_state_controller_.get());

  if (command_line->HasSwitch(switches::kAshTouchHud)) {
    touch_observer_hud_.reset(new internal::TouchObserverHUD);
    AddPreTargetHandler(touch_observer_hud_.get());
  }

  mouse_cursor_filter_.reset(new internal::MouseCursorEventFilter());
  AddPreTargetHandler(mouse_cursor_filter_.get());

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_.reset(new AshVisibilityController);
  drag_drop_controller_.reset(new internal::DragDropController);
  user_action_client_.reset(delegate_->CreateUserActionClient());
  window_modality_controller_.reset(
      new views::corewm::WindowModalityController);
  AddPreTargetHandler(window_modality_controller_.get());

  magnification_controller_.reset(
      MagnificationController::CreateInstance());

  partial_magnification_controller_.reset(
      new PartialMagnificationController());

  high_contrast_controller_.reset(new HighContrastController);
  video_detector_.reset(new VideoDetector);
  window_cycle_controller_.reset(new WindowCycleController(activation_client_));

  tooltip_controller_.reset(new internal::TooltipController(
      drag_drop_controller_.get()));
  AddPreTargetHandler(tooltip_controller_.get());

  event_client_.reset(new internal::EventClientImpl);

  InitRootWindowController(root_window_controller);

  // This controller needs to be set before SetupManagedWindowMode.
  desktop_background_controller_.reset(new DesktopBackgroundController());
  user_wallpaper_delegate_.reset(delegate_->CreateUserWallpaperDelegate());
  if (!user_wallpaper_delegate_.get())
    user_wallpaper_delegate_.reset(new DummyUserWallpaperDelegate());

  // StatusAreaWidget uses Shell's CapsLockDelegate.
  caps_lock_delegate_.reset(delegate_->CreateCapsLockDelegate());

  if (!command_line->HasSwitch(switches::kAuraNoShadows)) {
    resize_shadow_controller_.reset(new internal::ResizeShadowController());
    shadow_controller_.reset(
        new views::corewm::ShadowController(GetPrimaryRootWindow()));
  }

  // Create system_tray_notifier_ before the delegate.
  system_tray_notifier_.reset(new ash::SystemTrayNotifier());

  // Initialize system_tray_delegate_ before initializing StatusAreaWidget.
  system_tray_delegate_.reset(delegate()->CreateSystemTrayDelegate());
  if (!system_tray_delegate_.get())
    system_tray_delegate_.reset(SystemTrayDelegate::CreateDummyDelegate());

  // Creates StatusAreaWidget.
  root_window_controller->InitForPrimaryDisplay();

  // Initialize system_tray_delegate_ after StatusAreaWidget is created.
  system_tray_delegate_->Initialize();

  display_controller_->InitSecondaryDisplays();

  // Force Layout
  root_window_controller->root_window_layout()->OnWindowResized();

  // It needs to be created after OnWindowResized has been called, otherwise the
  // widget will not paint when restoring after a browser crash.  Also it needs
  // to be created after InitSecondaryDisplays() to initialize the wallpapers in
  // the correct size.
  user_wallpaper_delegate_->InitializeWallpaper();

  if (initially_hide_cursor_)
    cursor_manager_.DisableMouseEvents();
  cursor_manager_.SetCursor(ui::kCursorPointer);

  // Cursor might have been hidden by somethign other than chrome.
  // Let the first mouse event show the cursor.
  env_filter_->set_cursor_hidden_by_filter(true);
}

void Shell::ShowContextMenu(const gfx::Point& location_in_screen) {
  // No context menus if user have not logged in.
  if (!delegate_->IsUserLoggedIn())
    return;
  // No context menus when screen is locked.
  if (IsScreenLocked())
    return;

  aura::RootWindow* root =
      wm::GetRootWindowMatching(gfx::Rect(location_in_screen, gfx::Size()));
  // TODO(oshima): The root and root window controller shouldn't be
  // NULL even for the out-of-bounds |location_in_screen| (It should
  // return the primary root). Investigate why/how this is
  // happening. crbug.com/165214.
  internal::RootWindowController* rwc = GetRootWindowController(root);
  CHECK(rwc) << "root=" << root
             << ", location:" << location_in_screen.ToString();
  if (rwc)
    rwc->ShowContextMenu(location_in_screen);
}

void Shell::ToggleAppList(aura::Window* window) {
  // If the context window is not given, show it on the active root window.
  if (!window)
    window = GetActiveRootWindow();
  if (!app_list_controller_.get())
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

bool Shell::CanLockScreen() {
  return delegate_->CanLockScreen();
}

bool Shell::IsScreenLocked() const {
  return delegate_->IsScreenLocked();
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableNewDialogStyle)) {
    return new views::DialogFrameView(string16());
  }
  // Use translucent-style window frames for dialogs.
  CustomFrameViewAsh* frame_view = new CustomFrameViewAsh;
  frame_view->Init(widget);
  return frame_view;
}

void Shell::RotateFocus(Direction direction) {
  focus_cycler_->RotateFocus(
      direction == FORWARD ? internal::FocusCycler::FORWARD :
                             internal::FocusCycler::BACKWARD);
}

void Shell::SetDisplayWorkAreaInsets(Window* contains,
                                     const gfx::Insets& insets) {
  if (!display_manager_->UpdateWorkAreaOfDisplayNearestWindow(contains, insets))
    return;
  FOR_EACH_OBSERVER(ShellObserver, observers_,
                    OnDisplayWorkAreaInsetsChanged());
}

void Shell::OnLoginStateChanged(user::LoginStatus status) {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnLoginStateChanged(status));
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->OnLoginStateChanged(status);
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
}

void Shell::CreateLauncher() {
  if (IsLauncherPerDisplayEnabled()) {
    RootWindowControllerList controllers = GetAllRootWindowControllers();
    for (RootWindowControllerList::iterator iter = controllers.begin();
         iter != controllers.end(); ++iter)
      (*iter)->CreateLauncher();
  } else {
    GetPrimaryRootWindowController()->CreateLauncher();
  }
}

void Shell::ShowLauncher() {
  if (IsLauncherPerDisplayEnabled()) {
    RootWindowControllerList controllers = GetAllRootWindowControllers();
    for (RootWindowControllerList::iterator iter = controllers.begin();
         iter != controllers.end(); ++iter)
      (*iter)->ShowLauncher();
  } else {
    GetPrimaryRootWindowController()->ShowLauncher();
  }
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
                                     aura::RootWindow* root_window) {
  GetRootWindowController(root_window)->SetShelfAutoHideBehavior(behavior);
}

ShelfAutoHideBehavior Shell::GetShelfAutoHideBehavior(
    aura::RootWindow* root_window) const {
  return GetRootWindowController(root_window)->GetShelfAutoHideBehavior();
}

void Shell::SetShelfAlignment(ShelfAlignment alignment,
                              aura::RootWindow* root_window) {
  if (GetRootWindowController(root_window)->SetShelfAlignment(alignment)) {
    FOR_EACH_OBSERVER(
        ShellObserver, observers_, OnShelfAlignmentChanged(root_window));
  }
}

ShelfAlignment Shell::GetShelfAlignment(aura::RootWindow* root_window) {
  return GetRootWindowController(root_window)->GetShelfAlignment();
}

void Shell::SetDimming(bool should_dim) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->screen_dimmer()->SetDimming(should_dim);
}

void Shell::CreateModalBackground(aura::Window* window) {
  if (!modality_filter_.get()) {
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
  return GetPrimaryRootWindowController()->status_area_widget()->
      web_notification_tray();
}

bool Shell::HasPrimaryStatusArea() {
  return !!GetPrimaryRootWindowController()->status_area_widget();
}

SystemTray* Shell::GetPrimarySystemTray() {
  return GetPrimaryRootWindowController()->GetSystemTray();
}

LauncherDelegate* Shell::GetLauncherDelegate() {
  if (!launcher_delegate_.get()) {
    launcher_model_.reset(new LauncherModel);
    launcher_delegate_.reset(
        delegate_->CreateLauncherDelegate(launcher_model_.get()));
  }
  return launcher_delegate_.get();
}

void Shell::InitRootWindowForSecondaryDisplay(aura::RootWindow* root) {
  aura::client::SetFocusClient(root, focus_client_.get());
  internal::RootWindowController* controller =
      new internal::RootWindowController(root);
  controller->CreateContainers();
  // Pass false for the |is_first_run_after_boot| parameter so we'll show a
  // black background on this display instead of trying to mimic the boot splash
  // screen.
  controller->CreateSystemBackground(false);
  InitRootWindowController(controller);
  if (IsLauncherPerDisplayEnabled())
    controller->InitForPrimaryDisplay();
  controller->root_window_layout()->OnWindowResized();
  desktop_background_controller_->OnRootWindowAdded(root);
  high_contrast_controller_->OnRootWindowAdded(root);
  root->ShowRootWindow();
  // Activate new root for testing.
  active_root_window_ = root;
}

void Shell::DoInitialWorkspaceAnimation() {
  return GetPrimaryRootWindowController()->workspace_controller()->
      DoInitialAnimation();
}

void Shell::InitRootWindowController(
    internal::RootWindowController* controller) {
  aura::RootWindow* root_window = controller->root_window();
  DCHECK(activation_client_);
  DCHECK(visibility_controller_.get());
  DCHECK(drag_drop_controller_.get());
  DCHECK(capture_controller_.get());
  DCHECK(window_cycle_controller_.get());

  aura::client::SetFocusClient(root_window, focus_client_.get());
  input_method_filter_->SetInputMethodPropertyInRootWindow(root_window);
  aura::client::SetActivationClient(root_window, activation_client_);
  if (views::corewm::UseFocusController()) {
    views::corewm::FocusController* controller =
        static_cast<views::corewm::FocusController*>(activation_client_);
    root_window->AddPreTargetHandler(controller);
  }
  aura::client::SetVisibilityClient(root_window, visibility_controller_.get());
  aura::client::SetDragDropClient(root_window, drag_drop_controller_.get());
  aura::client::SetCaptureClient(root_window, capture_controller_.get());
  aura::client::SetScreenPositionClient(root_window,
                                        screen_position_controller_.get());
  aura::client::SetCursorClient(root_window, &cursor_manager_);
  aura::client::SetTooltipClient(root_window, tooltip_controller_.get());
  aura::client::SetEventClient(root_window, event_client_.get());

  if (nested_dispatcher_controller_.get()) {
    aura::client::SetDispatcherClient(root_window,
                                      nested_dispatcher_controller_.get());
  }
  if (user_action_client_.get())
    aura::client::SetUserActionClient(root_window, user_action_client_.get());

  root_window->SetCursor(ui::kCursorPointer);
  controller->InitLayoutManagers();

  // TODO(oshima): Move the instance to RootWindowController when
  // the extended desktop is enabled by default.
  internal::AlwaysOnTopController* always_on_top_controller =
      new internal::AlwaysOnTopController;
  always_on_top_controller->SetAlwaysOnTopContainer(
      root_window->GetChildById(internal::kShellWindowId_AlwaysOnTopContainer));
  root_window->SetProperty(internal::kAlwaysOnTopControllerKey,
                           always_on_top_controller);
  if (GetPrimaryRootWindowController()->GetSystemModalLayoutManager(NULL)->
          has_modal_background()) {
    controller->GetSystemModalLayoutManager(NULL)->CreateModalBackground();
  }

  window_cycle_controller_->OnRootWindowAdded(root_window);
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

bool Shell::CanWindowReceiveEvents(aura::Window* window) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter) {
    if ((*iter)->GetSystemModalLayoutManager(window)->
            CanWindowReceiveEvents(window)) {
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Shell, ui::EventTarget overrides:

bool Shell::CanAcceptEvent(const ui::Event& event) {
  return true;
}

ui::EventTarget* Shell::GetParentTarget() {
  return NULL;
}

void Shell::OnEvent(ui::Event* event) {
}

////////////////////////////////////////////////////////////////////////////////
// Shell, aura::client::ActivationChangeObserver implementation:

void Shell::OnWindowActivated(aura::Window* gained_active,
                              aura::Window* lost_active) {
  if (gained_active)
    active_root_window_ = gained_active->GetRootWindow();
}

}  // namespace ash
