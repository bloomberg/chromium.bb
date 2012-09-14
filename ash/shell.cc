// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <string>

#include "ash/accelerators/focus_manager_factory.h"
#include "ash/ash_switches.h"
#include "ash/caps_lock_delegate_stub.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_resources.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/display/display_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/multi_display_manager.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/secondary_display_view.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/focus_cycler.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/launcher/launcher.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell_context_menu.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/tooltips/tooltip_controller.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/app_list_controller.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/capture_controller.h"
#include "ash/wm/custom_frame_view_ash.h"
#include "ash/wm/dialog_frame_view.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/event_rewriter_event_filter.h"
#include "ash/wm/overlay_event_filter.h"
#include "ash/wm/panel_layout_manager.h"
#include "ash/wm/panel_window_event_filter.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/property_util.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/screen_dimmer.h"
#include "ash/wm/shadow_controller.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/system_gesture_event_filter.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/user_activity_detector.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/visibility_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_modality_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/user_action_client.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"
#include "ui/aura/shared/input_method_event_filter.h"
#include "ui/aura/ui_controls_aura.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/ui_controls/ui_controls.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

#if !defined(OS_MACOSX)
#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_filter.h"
#include "ash/accelerators/nested_dispatcher_controller.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/display/output_configurator_animation.h"
#include "base/message_pump_aurax11.h"
#include "chromeos/display/output_configurator.h"
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

  virtual ash::WindowVisibilityAnimationType GetAnimationType() OVERRIDE {
    return WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
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

}  // namespace

// static
Shell* Shell::instance_ = NULL;
// static
bool Shell::initially_hide_cursor_ = false;

////////////////////////////////////////////////////////////////////////////////
// Shell::TestApi

Shell::TestApi::TestApi(Shell* shell) : shell_(shell) {}

internal::RootWindowLayoutManager* Shell::TestApi::root_window_layout() {
  return shell_->GetPrimaryRootWindowController()->root_window_layout();
}

aura::shared::InputMethodEventFilter*
    Shell::TestApi::input_method_event_filter() {
  return shell_->input_method_filter_.get();
}

internal::SystemGestureEventFilter*
    Shell::TestApi::system_gesture_event_filter() {
  return shell_->system_gesture_filter_.get();
}

internal::WorkspaceController* Shell::TestApi::workspace_controller() {
  return shell_->GetPrimaryRootWindowController()->workspace_controller();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

Shell::Shell(ShellDelegate* delegate)
    : screen_(new ScreenAsh),
      active_root_window_(NULL),
      env_filter_(NULL),
      delegate_(delegate),
#if defined(OS_CHROMEOS)
      output_configurator_(new chromeos::OutputConfigurator()),
      output_configurator_animation_(
          new internal::OutputConfiguratorAnimation()),
#endif  // defined(OS_CHROMEOS)
      shelf_(NULL),
      panel_layout_manager_(NULL),
      status_area_widget_(NULL),
      browser_context_(NULL) {
  gfx::Screen::SetInstance(screen_);
  ui_controls::InstallUIControlsAura(internal::CreateUIControls());
#if defined(OS_CHROMEOS)
  output_configurator_->AddObserver(output_configurator_animation_.get());
  base::MessagePumpAuraX11::Current()->AddDispatcherForRootWindow(
      output_configurator());
  static_cast<internal::MultiDisplayManager*>(
      aura::Env::GetInstance()->display_manager())->InitInternalDisplayInfo();
#endif  // defined(OS_CHROMEOS)
}

Shell::~Shell() {
  views::FocusManagerFactory::Install(NULL);

  // Remove the focus from any window. This will prevent overhead and side
  // effects (e.g. crashes) from changing focus during shutdown.
  // See bug crbug.com/134502.
  if (active_root_window_)
    active_root_window_->GetFocusManager()->SetFocusedWindow(NULL, NULL);

  cursor_manager_.set_delegate(NULL);

  // Please keep in same order as in Init() because it's easy to miss one.
  RemoveEnvEventFilter(user_activity_detector_.get());
  RemoveEnvEventFilter(event_rewriter_filter_.get());
  RemoveEnvEventFilter(overlay_filter_.get());
  RemoveEnvEventFilter(input_method_filter_.get());
  RemoveEnvEventFilter(window_modality_controller_.get());
  if (mouse_cursor_filter_.get())
    RemoveEnvEventFilter(mouse_cursor_filter_.get());
  RemoveEnvEventFilter(system_gesture_filter_.get());
#if !defined(OS_MACOSX)
  RemoveEnvEventFilter(accelerator_filter_.get());
#endif
  if (touch_observer_hud_.get())
    RemoveEnvEventFilter(touch_observer_hud_.get());

  // TooltipController is deleted with the Shell so removing its references.
  RemoveEnvEventFilter(tooltip_controller_.get());

  // The status area needs to be shut down before the windows are destroyed.
  status_area_widget_->Shutdown();

  // AppList needs to be released before shelf layout manager, which is
  // destroyed with launcher container in the loop below. However, app list
  // container is now on top of launcher container and released after it.
  // TODO(xiyuan): Move it back when app list container is no longer needed.
  app_list_controller_.reset();

  // Destroy all child windows including widgets.
  display_controller_->CloseChildWindows();

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  // Alphabetical.
  drag_drop_controller_.reset();
  magnification_controller_.reset();
  power_button_controller_.reset();
  resize_shadow_controller_.reset();
  shadow_controller_.reset();
  tooltip_controller_.reset();
  window_cycle_controller_.reset();
  capture_controller_.reset();
  nested_dispatcher_controller_.reset();
  user_action_client_.reset();
  visibility_controller_.reset();

  // This also deletes all RootWindows.
  display_controller_.reset();
  screen_position_controller_.reset();

  // Launcher widget has a InputMethodBridge that references to
  // input_method_filter_'s input_method_. So explicitly release launcher_
  // before input_method_filter_. And this needs to be after we delete all
  // containers in case there are still live browser windows which access
  // LauncherModel during close.
  launcher_.reset();

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
  aura::Env::GetInstance()->SetDisplayManager(
      new internal::MultiDisplayManager());
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
std::vector<aura::Window*> Shell::GetAllContainers(int container_id) {
  std::vector<aura::Window*> containers;
  aura::Window* container = GetPrimaryRootWindow()->GetChildById(container_id);
  if (container)
    containers.push_back(container);
  return containers;
}

void Shell::Init() {
  // Install the custom factory first so that views::FocusManagers for Tray,
  // Launcher, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  env_filter_ = new aura::shared::CompoundEventFilter;
  // Pass ownership of the filter to the Env.
  aura::Env::GetInstance()->SetEventFilter(env_filter_);

  cursor_manager_.set_delegate(this);

  focus_manager_.reset(new aura::FocusManager);
  activation_controller_.reset(
      new internal::ActivationController(focus_manager_.get()));

  screen_position_controller_.reset(new internal::ScreenPositionController);
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
  shell_context_menu_.reset(new internal::ShellContextMenu);

  // The order in which event filters are added is significant.
  DCHECK(!GetEnvEventFilterCount());
  user_activity_detector_.reset(new UserActivityDetector);
  AddEnvEventFilter(user_activity_detector_.get());

  DCHECK_EQ(1U, GetEnvEventFilterCount());
  event_rewriter_filter_.reset(new internal::EventRewriterEventFilter);
  AddEnvEventFilter(event_rewriter_filter_.get());

  DCHECK_EQ(2U, GetEnvEventFilterCount());
  overlay_filter_.reset(new internal::OverlayEventFilter);
  AddEnvEventFilter(overlay_filter_.get());
  AddShellObserver(overlay_filter_.get());

  DCHECK_EQ(3U, GetEnvEventFilterCount());
  input_method_filter_.reset(new aura::shared::InputMethodEventFilter());
  AddEnvEventFilter(input_method_filter_.get());

#if !defined(OS_MACOSX)
  accelerator_filter_.reset(new internal::AcceleratorFilter);
  AddEnvEventFilter(accelerator_filter_.get());
#endif

  system_gesture_filter_.reset(new internal::SystemGestureEventFilter);
  AddEnvEventFilter(system_gesture_filter_.get());

  capture_controller_.reset(new internal::CaptureController);

  internal::RootWindowController* root_window_controller =
      new internal::RootWindowController(root_window);
  root_window_controller->CreateContainers();

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kAshTouchHud)) {
    touch_observer_hud_.reset(new internal::TouchObserverHUD);
    AddEnvEventFilter(touch_observer_hud_.get());
  }

  mouse_cursor_filter_.reset(new internal::MouseCursorEventFilter());
  AddEnvEventFilter(mouse_cursor_filter_.get());

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  stacking_controller_.reset(new internal::StackingController);
  visibility_controller_.reset(new internal::VisibilityController);
  drag_drop_controller_.reset(new internal::DragDropController);
  if (delegate_.get())
    user_action_client_.reset(delegate_->CreateUserActionClient());
  window_modality_controller_.reset(new internal::WindowModalityController);
  AddEnvEventFilter(window_modality_controller_.get());

  magnification_controller_.reset(
      internal::MagnificationController::CreateInstance());

  high_contrast_controller_.reset(new HighContrastController);
  video_detector_.reset(new VideoDetector);
  window_cycle_controller_.reset(
      new WindowCycleController(activation_controller_.get()));

  tooltip_controller_.reset(new internal::TooltipController(
      drag_drop_controller_.get()));
  AddEnvEventFilter(tooltip_controller_.get());

  InitRootWindowController(root_window_controller);

  // StatusAreaWidget uses Shell's CapsLockDelegate.
  if (delegate_.get())
    caps_lock_delegate_.reset(delegate_->CreateCapsLockDelegate());
  else
    caps_lock_delegate_.reset(new CapsLockDelegateStub);

  // Initialize Primary RootWindow specific items.
  status_area_widget_ = new internal::StatusAreaWidget();
  status_area_widget_->CreateTrayViews(delegate_.get());
  // Login screen manages status area visibility by itself.
  if (delegate_.get() && delegate_->IsSessionStarted())
    status_area_widget_->Show();

  focus_cycler_.reset(new internal::FocusCycler());
  focus_cycler_->AddWidget(status_area_widget_);

  // This controller needs to be set before SetupManagedWindowMode.
  desktop_background_controller_.reset(new DesktopBackgroundController());
  if (delegate_.get())
    user_wallpaper_delegate_.reset(delegate_->CreateUserWallpaperDelegate());
  if (!user_wallpaper_delegate_.get())
    user_wallpaper_delegate_.reset(new DummyUserWallpaperDelegate());

  InitLayoutManagersForPrimaryDisplay(root_window_controller);

  if (!command_line->HasSwitch(switches::kAuraNoShadows)) {
    resize_shadow_controller_.reset(new internal::ResizeShadowController());
    shadow_controller_.reset(new internal::ShadowController());
  }

  // Launcher must be created after secondary displays are initialized.
  display_controller_->InitSecondaryDisplays();

  if (!delegate_.get() || delegate_->IsUserLoggedIn())
    CreateLauncher();

  // Force Layout
  root_window_controller->root_window_layout()->OnWindowResized();

  // It needs to be created after OnWindowResized has been called, otherwise the
  // widget will not paint when restoring after a browser crash.  Also it needs
  // to be created after InitSecondaryDisplays() to initialize the wallpapers in
  // the correct size.
  user_wallpaper_delegate_->InitializeWallpaper();

  power_button_controller_.reset(new PowerButtonController);
  AddShellObserver(power_button_controller_.get());

  if (initially_hide_cursor_)
    cursor_manager_.ShowCursor(false);
}

void Shell::AddEnvEventFilter(aura::EventFilter* filter) {
  env_filter_->AddFilter(filter);
}

void Shell::RemoveEnvEventFilter(aura::EventFilter* filter) {
  env_filter_->RemoveFilter(filter);
}

size_t Shell::GetEnvEventFilterCount() const {
  return env_filter_->GetFilterCount();
}

void Shell::ShowBackgroundMenu(views::Widget* widget,
                               const gfx::Point& location) {
  // No context menus if user have not logged in.
  if (!delegate_.get() || !delegate_->IsUserLoggedIn())
    return;
  // No context menus when screen is locked.
  if (IsScreenLocked())
    return;
  if (shell_context_menu_.get())
    shell_context_menu_->ShowMenu(widget, location);
}

void Shell::ToggleAppList() {
  if (!app_list_controller_.get())
    app_list_controller_.reset(new internal::AppListController);
  app_list_controller_->SetVisible(!app_list_controller_->IsVisible());
}

bool Shell::GetAppListTargetVisibility() const {
  return app_list_controller_.get() &&
      app_list_controller_->GetTargetVisibility();
}

aura::Window* Shell::GetAppListWindow() {
  return app_list_controller_.get() ? app_list_controller_->GetWindow() : NULL;
}

bool Shell::IsScreenLocked() const {
  return !delegate_.get() || delegate_->IsScreenLocked();
}

bool Shell::IsModalWindowOpen() const {
  // TODO(oshima): Walk though all root windows.
  const aura::Window* modal_container = GetContainer(
      GetPrimaryRootWindow(),
      internal::kShellWindowId_SystemModalContainer);
  return !modal_container->children().empty();
}

views::NonClientFrameView* Shell::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAuraGoogleDialogFrames)) {
    return new internal::DialogFrameView;
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
  internal::MultiDisplayManager* display_manager =
      static_cast<internal::MultiDisplayManager*>(
          aura::Env::GetInstance()->display_manager());
  if (!display_manager->UpdateWorkAreaOfDisplayNearestWindow(contains, insets))
    return;
  FOR_EACH_OBSERVER(ShellObserver, observers_,
                    OnDisplayWorkAreaInsetsChanged());
}

void Shell::OnLoginStateChanged(user::LoginStatus status) {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnLoginStateChanged(status));
}

void Shell::OnAppTerminating() {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnAppTerminating());
}

void Shell::OnLockStateChanged(bool locked) {
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnLockStateChanged(locked));
}

void Shell::CreateLauncher() {
  if (launcher_.get())
    return;

  aura::Window* default_container =
      GetPrimaryRootWindowController()->
      GetContainer(internal::kShellWindowId_DefaultContainer);
  launcher_.reset(new Launcher(default_container, shelf_));

  launcher_->SetFocusCycler(focus_cycler_.get());
  shelf_->SetLauncher(launcher_.get());
  if (panel_layout_manager_)
    panel_layout_manager_->SetLauncher(launcher_.get());

  if (delegate())
    launcher_->SetVisible(delegate()->IsSessionStarted());
  launcher_->widget()->Show();
}

void Shell::ShowLauncher() {
  if (!launcher_.get())
    return;
  launcher_->SetVisible(true);
}

void Shell::AddShellObserver(ShellObserver* observer) {
  observers_.AddObserver(observer);
}

void Shell::RemoveShellObserver(ShellObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Shell::UpdateShelfVisibility() {
  shelf_->UpdateVisibilityState();
}

void Shell::SetShelfAutoHideBehavior(ShelfAutoHideBehavior behavior) {
  shelf_->SetAutoHideBehavior(behavior);
}

ShelfAutoHideBehavior Shell::GetShelfAutoHideBehavior() const {
  return shelf_->auto_hide_behavior();
}

void Shell::SetShelfAlignment(ShelfAlignment alignment) {
  if (!shelf_->SetAlignment(alignment))
    return;
  FOR_EACH_OBSERVER(ShellObserver, observers_, OnShelfAlignmentChanged());
}

ShelfAlignment Shell::GetShelfAlignment() {
  return shelf_->alignment();
}

void Shell::SetDimming(bool should_dim) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter)
    (*iter)->screen_dimmer()->SetDimming(should_dim);
}

SystemTrayDelegate* Shell::tray_delegate() {
  return status_area_widget_->system_tray_delegate();
}

SystemTray* Shell::system_tray() {
  return status_area_widget_->system_tray();
}

void Shell::InitRootWindowForSecondaryDisplay(aura::RootWindow* root) {
  root->set_focus_manager(focus_manager_.get());
  internal::RootWindowController* controller =
      new internal::RootWindowController(root);
  controller->CreateContainers();
  InitRootWindowController(controller);
  controller->root_window_layout()->OnWindowResized();
  desktop_background_controller_->OnRootWindowAdded(root);
  root->ShowRootWindow();
  // Activate new root for testing.
  active_root_window_ = root;
}

void Shell::InitRootWindowController(
    internal::RootWindowController* controller) {
  aura::RootWindow* root_window = controller->root_window();
  DCHECK(activation_controller_.get());
  DCHECK(visibility_controller_.get());
  DCHECK(drag_drop_controller_.get());
  DCHECK(capture_controller_.get());
  DCHECK(window_cycle_controller_.get());

  root_window->set_focus_manager(focus_manager_.get());
  input_method_filter_->SetInputMethodPropertyInRootWindow(root_window);
  aura::client::SetActivationClient(root_window, activation_controller_.get());
  aura::client::SetVisibilityClient(root_window, visibility_controller_.get());
  aura::client::SetDragDropClient(root_window, drag_drop_controller_.get());
  aura::client::SetCaptureClient(root_window, capture_controller_.get());
  aura::client::SetScreenPositionClient(root_window,
                                        screen_position_controller_.get());
  aura::client::SetCursorClient(root_window, &cursor_manager_);
  aura::client::SetTooltipClient(root_window, tooltip_controller_.get());

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
  always_on_top_controller->SetContainers(
      root_window->GetChildById(internal::kShellWindowId_DefaultContainer),
      root_window->GetChildById(internal::kShellWindowId_AlwaysOnTopContainer));
  root_window->SetProperty(internal::kAlwaysOnTopControllerKey,
                           always_on_top_controller);

  window_cycle_controller_->OnRootWindowAdded(root_window);
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

void Shell::InitLayoutManagersForPrimaryDisplay(
    internal::RootWindowController* controller) {
  DCHECK(status_area_widget_);

  internal::ShelfLayoutManager* shelf_layout_manager =
      new internal::ShelfLayoutManager(status_area_widget_);
  controller->GetContainer(internal::kShellWindowId_LauncherContainer)->
      SetLayoutManager(shelf_layout_manager);
  shelf_ = shelf_layout_manager;

  internal::StatusAreaLayoutManager* status_area_layout_manager =
      new internal::StatusAreaLayoutManager(shelf_layout_manager);
  controller->GetContainer(internal::kShellWindowId_StatusContainer)->
      SetLayoutManager(status_area_layout_manager);

  shelf_layout_manager->set_workspace_controller(
      controller->workspace_controller());

  // TODO(oshima): Support multiple displays.
  controller->workspace_controller()->SetShelf(shelf());

  // Create Panel layout manager
  aura::Window* panel_container = GetContainer(
      GetPrimaryRootWindow(),
      internal::kShellWindowId_PanelContainer);
  panel_layout_manager_ =
      new internal::PanelLayoutManager(panel_container);
  panel_container->SetEventFilter(
      new internal::PanelWindowEventFilter(
          panel_container, panel_layout_manager_));
  panel_container->SetLayoutManager(panel_layout_manager_);
}

void Shell::SetCursor(gfx::NativeCursor cursor) {
  RootWindowList root_windows = GetAllRootWindows();
  for (RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->SetCursor(cursor);
}

void Shell::ShowCursor(bool visible) {
  RootWindowList root_windows = GetAllRootWindows();
  for (RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->ShowCursor(visible);
}

}  // namespace ash
