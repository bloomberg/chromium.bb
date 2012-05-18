// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <string>

#include "ash/accelerators/focus_manager_factory.h"
#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_resources.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/focus_cycler.h"
#include "ash/launcher/launcher.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/monitor/monitor_controller.h"
#include "ash/monitor/multi_monitor_manager.h"
#include "ash/screen_ash.h"
#include "ash/shell_context_menu.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/network/network_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/tooltips/tooltip_controller.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/app_list_controller.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/custom_frame_view_ash.h"
#include "ash/wm/dialog_frame_view.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/key_rewriter_event_filter.h"
#include "ash/wm/panel_layout_manager.h"
#include "ash/wm/panel_window_event_filter.h"
#include "ash/wm/partial_screenshot_event_filter.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/screen_dimmer.h"
#include "ash/wm/shadow_controller.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/system_gesture_event_filter.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/toplevel_window_event_filter.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/visibility_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_modality_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_filter.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/input_method_event_filter.h"
#include "ui/aura/shared/root_window_event_filter.h"
#include "ui/aura/ui_controls_aura.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/monitor.h"
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

namespace ash {

namespace {

using aura::Window;
using views::Widget;

// Creates a new window for use as a container.
aura::Window* CreateContainer(int window_id,
                              const char* name,
                              aura::Window* parent) {
  aura::Window* container = new aura::Window(NULL);
  container->set_id(window_id);
  container->SetName(name);
  container->Init(ui::LAYER_NOT_DRAWN);
  parent->AddChild(container);
  if (window_id != internal::kShellWindowId_UnparentedControlContainer)
    container->Show();
  return container;
}

// Creates each of the special window containers that holds windows of various
// types in the shell UI.
void CreateSpecialContainers(aura::RootWindow* root_window) {
  // These containers are just used by PowerButtonController to animate groups
  // of containers simultaneously without messing up the current transformations
  // on those containers.  These are direct children of the root window; all of
  // the other containers are their children.
  aura::Window* non_lock_screen_containers = CreateContainer(
      internal::kShellWindowId_NonLockScreenContainersContainer,
      "NonLockScreenContainersContainer",
      root_window);
  aura::Window* lock_screen_containers = CreateContainer(
      internal::kShellWindowId_LockScreenContainersContainer,
      "LockScreenContainersContainer",
      root_window);
  aura::Window* lock_screen_related_containers = CreateContainer(
      internal::kShellWindowId_LockScreenRelatedContainersContainer,
      "LockScreenRelatedContainersContainer",
      root_window);

  CreateContainer(internal::kShellWindowId_UnparentedControlContainer,
                  "UnparentedControlContainer",
                  non_lock_screen_containers);

  aura::Window* desktop_background_containers = CreateContainer(
      internal::kShellWindowId_DesktopBackgroundContainer,
      "DesktopBackgroundContainer",
      non_lock_screen_containers);
  SetChildWindowVisibilityChangesAnimated(desktop_background_containers);

  aura::Window* default_container = CreateContainer(
      internal::kShellWindowId_DefaultContainer,
      "DefaultContainer",
      non_lock_screen_containers);
  default_container->SetEventFilter(
      new ToplevelWindowEventFilter(default_container));
  SetChildWindowVisibilityChangesAnimated(default_container);

  aura::Window* always_on_top_container = CreateContainer(
      internal::kShellWindowId_AlwaysOnTopContainer,
      "AlwaysOnTopContainer",
      non_lock_screen_containers);
  always_on_top_container->SetEventFilter(
      new ToplevelWindowEventFilter(always_on_top_container));
  SetChildWindowVisibilityChangesAnimated(always_on_top_container);

  CreateContainer(internal::kShellWindowId_PanelContainer,
                  "PanelContainer",
                  non_lock_screen_containers);

  CreateContainer(internal::kShellWindowId_AppListContainer,
                  "AppListContainer",
                  non_lock_screen_containers);

  CreateContainer(internal::kShellWindowId_LauncherContainer,
                  "LauncherContainer",
                  non_lock_screen_containers);

  aura::Window* modal_container = CreateContainer(
      internal::kShellWindowId_SystemModalContainer,
      "SystemModalContainer",
      non_lock_screen_containers);
  modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(modal_container));
  modal_container->SetLayoutManager(
      new internal::SystemModalContainerLayoutManager(modal_container));
  SetChildWindowVisibilityChangesAnimated(modal_container);

  // TODO(beng): Figure out if we can make this use
  // SystemModalContainerEventFilter instead of stops_event_propagation.
  aura::Window* lock_container = CreateContainer(
      internal::kShellWindowId_LockScreenContainer,
      "LockScreenContainer",
      lock_screen_containers);
  lock_container->SetLayoutManager(
      new internal::BaseLayoutManager(root_window));
  // TODO(beng): stopsevents

  aura::Window* lock_modal_container = CreateContainer(
      internal::kShellWindowId_LockSystemModalContainer,
      "LockSystemModalContainer",
      lock_screen_containers);
  lock_modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(lock_modal_container));
  lock_modal_container->SetLayoutManager(
      new internal::SystemModalContainerLayoutManager(lock_modal_container));
  SetChildWindowVisibilityChangesAnimated(lock_modal_container);

  CreateContainer(internal::kShellWindowId_StatusContainer,
                  "StatusContainer",
                  lock_screen_related_containers);

  aura::Window* menu_container = CreateContainer(
      internal::kShellWindowId_MenuContainer,
      "MenuContainer",
      lock_screen_related_containers);
  SetChildWindowVisibilityChangesAnimated(menu_container);

  aura::Window* drag_drop_container = CreateContainer(
      internal::kShellWindowId_DragImageAndTooltipContainer,
      "DragImageAndTooltipContainer",
      lock_screen_related_containers);
  SetChildWindowVisibilityChangesAnimated(drag_drop_container);

  aura::Window* settings_bubble_container = CreateContainer(
      internal::kShellWindowId_SettingBubbleContainer,
      "SettingBubbleContainer",
      lock_screen_related_containers);
  SetChildWindowVisibilityChangesAnimated(settings_bubble_container);

  CreateContainer(internal::kShellWindowId_OverlayContainer,
                  "OverlayContainer",
                  lock_screen_related_containers);
}

// This dummy class is used for shell unit tests. We dont have chrome delegate
// in these tests.
class DummyUserWallpaperDelegate : public UserWallpaperDelegate {
 public:
  DummyUserWallpaperDelegate() {}

  virtual ~DummyUserWallpaperDelegate() {}

  virtual const int GetUserWallpaperIndex() OVERRIDE {
    return -1;
  }

  virtual void OpenSetWallpaperPage() OVERRIDE {
  }

  virtual bool CanOpenSetWallpaperPage() OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyUserWallpaperDelegate);
};

class DummySystemTrayDelegate : public SystemTrayDelegate {
 public:
  DummySystemTrayDelegate()
      : muted_(false),
        wifi_enabled_(true),
        cellular_enabled_(true),
        bluetooth_enabled_(true),
        volume_(0.5),
        caps_lock_enabled_(false) {
  }

  virtual ~DummySystemTrayDelegate() {}

 private:
  virtual bool GetTrayVisibilityOnStartup() OVERRIDE { return true; }

  // Overridden from SystemTrayDelegate:
  virtual const std::string GetUserDisplayName() const OVERRIDE {
    return "Über tray Über tray Über tray Über tray";
  }

  virtual const std::string GetUserEmail() const OVERRIDE {
    return "über@tray";
  }

  virtual const SkBitmap& GetUserImage() const OVERRIDE {
    return null_image_;
  }

  virtual user::LoginStatus GetUserLoginStatus() const OVERRIDE {
    return user::LOGGED_IN_USER;
  }

  virtual bool SystemShouldUpgrade() const OVERRIDE {
    return true;
  }

  virtual int GetSystemUpdateIconResource() const OVERRIDE {
    return IDR_AURA_UBER_TRAY_UPDATE;
  }

  virtual base::HourClockType GetHourClockType() const OVERRIDE {
    return base::k24HourClock;
  }

  virtual PowerSupplyStatus GetPowerSupplyStatus() const OVERRIDE {
    return PowerSupplyStatus();
  }

  virtual void RequestStatusUpdate() const OVERRIDE {
  }

  virtual void ShowSettings() OVERRIDE {
  }

  virtual void ShowDateSettings() OVERRIDE {
  }

  virtual void ShowNetworkSettings() OVERRIDE {
  }

  virtual void ShowBluetoothSettings() OVERRIDE {
  }

  virtual void ShowDriveSettings() OVERRIDE {
  }

  virtual void ShowIMESettings() OVERRIDE {
  }

  virtual void ShowHelp() OVERRIDE {
  }

  virtual bool IsAudioMuted() const OVERRIDE {
    return muted_;
  }

  virtual void SetAudioMuted(bool muted) OVERRIDE {
    muted_ = muted;
  }

  virtual float GetVolumeLevel() const OVERRIDE {
    return volume_;
  }

  virtual void SetVolumeLevel(float volume) OVERRIDE {
    volume_ = volume;
  }

  virtual bool IsCapsLockOn() const OVERRIDE {
    return caps_lock_enabled_;
  }

  virtual void SetCapsLockEnabled(bool enabled) OVERRIDE {
    caps_lock_enabled_ = enabled;
  }

  virtual bool IsInAccessibilityMode() const OVERRIDE {
    return false;
  }

  virtual void SetEnableSpokenFeedback(bool enable) OVERRIDE {}

  virtual void ShutDown() OVERRIDE {}

  virtual void SignOut() OVERRIDE {
    MessageLoop::current()->Quit();
  }

  virtual void RequestLockScreen() OVERRIDE {}

  virtual void RequestRestart() OVERRIDE {}

  virtual void GetAvailableBluetoothDevices(
      BluetoothDeviceList* list) OVERRIDE {
  }

  virtual void ToggleBluetoothConnection(const std::string& address) OVERRIDE {
  }

  virtual void GetCurrentIME(IMEInfo* info) OVERRIDE {
  }

  virtual void GetAvailableIMEList(IMEInfoList* list) OVERRIDE {
  }

  virtual void GetCurrentIMEProperties(IMEPropertyInfoList* list) OVERRIDE {
  }

  virtual void SwitchIME(const std::string& ime_id) OVERRIDE {
  }

  virtual void ActivateIMEProperty(const std::string& key) OVERRIDE {
  }

  virtual void CancelDriveOperation(const FilePath&) OVERRIDE {
  }

  virtual void GetDriveOperationStatusList(
      ash::DriveOperationStatusList*) OVERRIDE {
  }

  virtual void GetMostRelevantNetworkIcon(NetworkIconInfo* info,
                                          bool large) OVERRIDE {
  }

  virtual void GetAvailableNetworks(
      std::vector<NetworkIconInfo>* list) OVERRIDE {
  }

  virtual void ConnectToNetwork(const std::string& network_id) OVERRIDE {
  }

  virtual void GetNetworkAddresses(std::string* ip_address,
                                   std::string* ethernet_mac_address,
                                   std::string* wifi_mac_address) OVERRIDE {
    *ip_address = "127.0.0.1";
    *ethernet_mac_address = "00:11:22:33:44:55";
    *wifi_mac_address = "66:77:88:99:00:11";
  }

  virtual void RequestNetworkScan() OVERRIDE {
  }

  virtual void AddBluetoothDevice() OVERRIDE {
  }

  virtual void ToggleAirplaneMode() OVERRIDE {
  }

  virtual void ToggleWifi() OVERRIDE {
    wifi_enabled_ = !wifi_enabled_;
    ash::NetworkObserver* observer =
        ash::Shell::GetInstance()->tray()->network_observer();
    if (observer) {
      ash::NetworkIconInfo info;
      observer->OnNetworkRefresh(info);
    }
  }

  virtual void ToggleMobile() OVERRIDE {
    cellular_enabled_ = !cellular_enabled_;
    ash::NetworkObserver* observer =
        ash::Shell::GetInstance()->tray()->network_observer();
    if (observer) {
      ash::NetworkIconInfo info;
      observer->OnNetworkRefresh(info);
    }
  }

  virtual void ToggleBluetooth() OVERRIDE {
    bluetooth_enabled_ = !bluetooth_enabled_;
    ash::BluetoothObserver* observer =
        ash::Shell::GetInstance()->tray()->bluetooth_observer();
    if (observer)
      observer->OnBluetoothRefresh();
  }

  virtual void ShowOtherWifi() OVERRIDE {
  }

  virtual void ShowOtherCellular() OVERRIDE {
  }

  virtual bool IsNetworkConnected() OVERRIDE {
    return true;
  }

  virtual bool GetWifiAvailable() OVERRIDE {
    return true;
  }

  virtual bool GetMobileAvailable() OVERRIDE {
    return true;
  }

  virtual bool GetBluetoothAvailable() OVERRIDE {
    return true;
  }

  virtual bool GetWifiEnabled() OVERRIDE {
    return wifi_enabled_;
  }

  virtual bool GetMobileEnabled() OVERRIDE {
    return cellular_enabled_;
  }

  virtual bool GetBluetoothEnabled() OVERRIDE {
    return bluetooth_enabled_;
  }

  virtual bool GetMobileScanSupported() OVERRIDE {
    return true;
  }

  virtual bool GetCellularCarrierInfo(std::string* carrier_id,
                                      std::string* topup_url,
                                      std::string* setup_url) OVERRIDE {
    return false;
  }

  virtual void ShowCellularURL(const std::string& url) OVERRIDE {
  }

  virtual void ChangeProxySettings() OVERRIDE {
  }

  bool muted_;
  bool wifi_enabled_;
  bool cellular_enabled_;
  bool bluetooth_enabled_;
  float volume_;
  bool caps_lock_enabled_;
  SkBitmap null_image_;

  DISALLOW_COPY_AND_ASSIGN(DummySystemTrayDelegate);
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
  return shell_->root_window_layout_;
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
  return shell_->workspace_controller_.get();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

Shell::Shell(ShellDelegate* delegate)
    : root_window_(aura::MonitorManager::CreateRootWindowForPrimaryMonitor()),
      screen_(new ScreenAsh(root_window_.get())),
      root_filter_(NULL),
      delegate_(delegate),
      shelf_(NULL),
      panel_layout_manager_(NULL),
      root_window_layout_(NULL) {
  gfx::Screen::SetInstance(screen_);
  ui_controls::InstallUIControlsAura(CreateUIControlsAura(root_window_.get()));
}

Shell::~Shell() {
  views::FocusManagerFactory::Install(NULL);

  RemoveRootWindowEventFilter(key_rewriter_filter_.get());
  RemoveRootWindowEventFilter(partial_screenshot_filter_.get());
  RemoveRootWindowEventFilter(input_method_filter_.get());
  RemoveRootWindowEventFilter(window_modality_controller_.get());
  RemoveRootWindowEventFilter(system_gesture_filter_.get());
#if !defined(OS_MACOSX)
  RemoveRootWindowEventFilter(accelerator_filter_.get());
#endif
  if (touch_observer_hud_.get())
    RemoveRootWindowEventFilter(touch_observer_hud_.get());

  // Close background widget now so that the focus manager of the
  // widget gets deleted in the final message loop run.
  root_window_layout_->SetBackgroundWidget(NULL);

  // TooltipController is deleted with the Shell so removing its references.
  RemoveRootWindowEventFilter(tooltip_controller_.get());
  aura::client::SetTooltipClient(GetRootWindow(), NULL);

  // Make sure we delete WorkspaceController before launcher is
  // deleted as it has a reference to launcher model.
  workspace_controller_.reset();

  // The system tray needs to be reset before all the windows are destroyed.
  tray_.reset();
  tray_delegate_.reset();

  // Desroy secondary monitor's widgets before all the windows are destroyed.
  monitor_controller_.reset();

  // Delete containers now so that child windows does not access
  // observers when they are destructed.
  aura::RootWindow* root_window = GetRootWindow();
  while (!root_window->children().empty()) {
    aura::Window* child = root_window->children()[0];
    delete child;
  }

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  // Alphabetical.
  activation_controller_.reset();
  drag_drop_controller_.reset();
  event_client_.reset();
  magnification_controller_.reset();
  monitor_controller_.reset();
  power_button_controller_.reset();
  resize_shadow_controller_.reset();
  screen_dimmer_.reset();
  shadow_controller_.reset();
  tooltip_controller_.reset();
  window_cycle_controller_.reset();

  // Launcher widget has a InputMethodBridge that references to
  // input_method_filter_'s input_method_. So explicitly release launcher_
  // before input_method_filter_. And this needs to be after we delete all
  // containers in case there are still live browser windows which access
  // LauncherModel during close.
  launcher_.reset();

  DCHECK(instance_ == this);
  instance_ = NULL;
}

// static
Shell* Shell::CreateInstance(ShellDelegate* delegate) {
  CHECK(!instance_);
  aura::Env::GetInstance()->SetMonitorManager(
      new internal::MultiMonitorManager());
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
aura::RootWindow* Shell::GetRootWindow() {
  return GetInstance()->root_window_.get();
}

void Shell::Init() {
  aura::RootWindow* root_window = GetRootWindow();
  root_filter_ = new aura::shared::RootWindowEventFilter(root_window);
#if !defined(OS_MACOSX)
  nested_dispatcher_controller_.reset(new NestedDispatcherController);
  accelerator_controller_.reset(new AcceleratorController);
#endif
  shell_context_menu_.reset(new internal::ShellContextMenu);
  // Pass ownership of the filter to the root window.
  GetRootWindow()->SetEventFilter(root_filter_);

  // KeyRewriterEventFilter must be the first one.
  DCHECK(!GetRootWindowEventFilterCount());
  key_rewriter_filter_.reset(new internal::KeyRewriterEventFilter);
  AddRootWindowEventFilter(key_rewriter_filter_.get());

  // PartialScreenshotEventFilter must be the second one to capture key
  // events when the taking partial screenshot UI is there.
  DCHECK_EQ(1U, GetRootWindowEventFilterCount());
  partial_screenshot_filter_.reset(new internal::PartialScreenshotEventFilter);
  AddRootWindowEventFilter(partial_screenshot_filter_.get());
  AddShellObserver(partial_screenshot_filter_.get());

  // InputMethodEventFilter must be the third one. It has to be added before
  // AcceleratorFilter.
  DCHECK_EQ(2U, GetRootWindowEventFilterCount());
  input_method_filter_.reset(
      new aura::shared::InputMethodEventFilter(root_window));
  AddRootWindowEventFilter(input_method_filter_.get());
#if !defined(OS_MACOSX)
  accelerator_filter_.reset(new internal::AcceleratorFilter);
  AddRootWindowEventFilter(accelerator_filter_.get());
#endif

  system_gesture_filter_.reset(new internal::SystemGestureEventFilter);
  AddRootWindowEventFilter(system_gesture_filter_.get());

  root_window->SetCursor(ui::kCursorPointer);
  if (initially_hide_cursor_)
    root_window->ShowCursor(false);

  activation_controller_.reset(new internal::ActivationController);

  CreateSpecialContainers(root_window);

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kAshTouchHud)) {
    touch_observer_hud_.reset(new internal::TouchObserverHUD);
    AddRootWindowEventFilter(touch_observer_hud_.get());
  }

  stacking_controller_.reset(new internal::StackingController);

  root_window_layout_ = new internal::RootWindowLayoutManager(root_window);
  root_window->SetLayoutManager(root_window_layout_);

  event_client_.reset(new internal::EventClientImpl(root_window));

  tray_.reset(new SystemTray());
  if (delegate_.get())
    tray_delegate_.reset(delegate_->CreateSystemTrayDelegate(tray_.get()));
  if (!tray_delegate_.get())
    tray_delegate_.reset(new DummySystemTrayDelegate());
  tray_->CreateItems();
  tray_->CreateWidget();

  // This controller needs to be set before SetupManagedWindowMode.
  desktop_background_controller_.reset(new DesktopBackgroundController);
  if (delegate_.get())
    user_wallpaper_delegate_.reset(delegate_->CreateUserWallpaperDelegate());
  if (!user_wallpaper_delegate_.get())
    user_wallpaper_delegate_.reset(new DummyUserWallpaperDelegate());

  InitLayoutManagers();

  if (!command_line->HasSwitch(switches::kAuraNoShadows)) {
    resize_shadow_controller_.reset(new internal::ResizeShadowController());
    shadow_controller_.reset(new internal::ShadowController());
  }

  focus_cycler_.reset(new internal::FocusCycler());
  focus_cycler_->AddWidget(tray_->widget());

  if (!delegate_.get() || delegate_->IsUserLoggedIn())
    CreateLauncher();

  // Force a layout.
  root_window->layout_manager()->OnWindowResized();

  // It needs to be created after OnWindowResized has been called, otherwise the
  // widget will not paint when restoring after a browser crash.
  desktop_background_controller_->SetLoggedInUserWallpaper();

  window_modality_controller_.reset(new internal::WindowModalityController);
  AddRootWindowEventFilter(window_modality_controller_.get());

  visibility_controller_.reset(new internal::VisibilityController);

  tooltip_controller_.reset(new internal::TooltipController);
  AddRootWindowEventFilter(tooltip_controller_.get());

  drag_drop_controller_.reset(new internal::DragDropController);
  magnification_controller_.reset(new internal::MagnificationController);
  power_button_controller_.reset(new PowerButtonController);
  AddShellObserver(power_button_controller_.get());
  video_detector_.reset(new VideoDetector);
  window_cycle_controller_.reset(new WindowCycleController);
  monitor_controller_.reset(new internal::MonitorController);
  screen_dimmer_.reset(new internal::ScreenDimmer);

  views::FocusManagerFactory::Install(new AshFocusManagerFactory);
}

aura::Window* Shell::GetContainer(int container_id) {
  return const_cast<aura::Window*>(
      const_cast<const Shell*>(this)->GetContainer(container_id));
}

const aura::Window* Shell::GetContainer(int container_id) const {
  return GetRootWindow()->GetChildById(container_id);
}

void Shell::AddRootWindowEventFilter(aura::EventFilter* filter) {
  static_cast<aura::shared::RootWindowEventFilter*>(
      GetRootWindow()->event_filter())->AddFilter(filter);
}

void Shell::RemoveRootWindowEventFilter(aura::EventFilter* filter) {
  static_cast<aura::shared::RootWindowEventFilter*>(
      GetRootWindow()->event_filter())->RemoveFilter(filter);
}

size_t Shell::GetRootWindowEventFilterCount() const {
  return static_cast<aura::shared::RootWindowEventFilter*>(
      GetRootWindow()->event_filter())->GetFilterCount();
}

void Shell::ShowBackgroundMenu(views::Widget* widget,
                               const gfx::Point& location) {
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
  const aura::Window* modal_container = GetContainer(
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

void Shell::SetMonitorWorkAreaInsets(Window* contains,
                                     const gfx::Insets& insets) {
  internal::MultiMonitorManager* monitor_manager =
      static_cast<internal::MultiMonitorManager*>(
          aura::Env::GetInstance()->monitor_manager());
  if (!monitor_manager->UpdateWorkAreaOfMonitorNearestWindow(contains, insets))
    return;
  FOR_EACH_OBSERVER(ShellObserver, observers_,
                    OnMonitorWorkAreaInsetsChanged());
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
      GetContainer(internal::kShellWindowId_DefaultContainer);
  launcher_.reset(new Launcher(default_container));

  launcher_->SetFocusCycler(focus_cycler_.get());
  shelf_->SetLauncher(launcher_.get());
  if (panel_layout_manager_)
    panel_layout_manager_->SetLauncher(launcher_.get());

  launcher_->widget()->Show();
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
  shelf_->SetAlignment(alignment);
}

ShelfAlignment Shell::GetShelfAlignment() {
  return shelf_->alignment();
}

int Shell::GetGridSize() const {
  return workspace_controller_->workspace_manager()->grid_size();
}

bool Shell::IsInMaximizedMode() const {
  return workspace_controller_->workspace_manager()->IsInMaximizedMode();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

void Shell::InitLayoutManagers() {
  DCHECK(root_window_layout_);
  DCHECK(tray_->widget());

  internal::ShelfLayoutManager* shelf_layout_manager =
      new internal::ShelfLayoutManager(tray_->widget());
  GetContainer(internal::kShellWindowId_LauncherContainer)->
      SetLayoutManager(shelf_layout_manager);
  shelf_ = shelf_layout_manager;

  internal::StatusAreaLayoutManager* status_area_layout_manager =
      new internal::StatusAreaLayoutManager(shelf_layout_manager);
  GetContainer(internal::kShellWindowId_StatusContainer)->
      SetLayoutManager(status_area_layout_manager);

  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);
  // Workspace manager has its own layout managers.
  workspace_controller_.reset(
      new internal::WorkspaceController(default_container));
  workspace_controller_->workspace_manager()->set_shelf(shelf_layout_manager);
  shelf_layout_manager->set_workspace_manager(
      workspace_controller_->workspace_manager());

  aura::Window* always_on_top_container =
      GetContainer(internal::kShellWindowId_AlwaysOnTopContainer);
  always_on_top_container->SetLayoutManager(
      new internal::BaseLayoutManager(
          always_on_top_container->GetRootWindow()));

  // Create Panel layout manager
  if (CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kAuraPanelManager)) {
    aura::Window* panel_container = GetContainer(
        internal::kShellWindowId_PanelContainer);
    panel_layout_manager_ =
        new internal::PanelLayoutManager(panel_container);
    panel_container->SetEventFilter(
        new internal::PanelWindowEventFilter(
            panel_container, panel_layout_manager_));
    panel_container->SetLayoutManager(panel_layout_manager_);
  }
}

void Shell::DisableWorkspaceGridLayout() {
  if (workspace_controller_.get())
    workspace_controller_->workspace_manager()->set_grid_size(0);
}

}  // namespace ash
