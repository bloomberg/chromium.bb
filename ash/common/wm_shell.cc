// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_shell.h"

#include <utility>

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/accelerators/ash_focus_manager_factory.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/cast_config_controller.h"
#include "ash/common/devtools/ash_devtools_css_agent.h"
#include "ash/common/devtools/ash_devtools_dom_agent.h"
#include "ash/common/focus_cycler.h"
#include "ash/common/keyboard/keyboard_ui.h"
#include "ash/common/media_controller.h"
#include "ash/common/new_window_controller.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/app_list_shelf_item_delegate.h"
#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_window_watcher.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shutdown_controller.h"
#include "ash/common/system/brightness_control_delegate.h"
#include "ash/common/system/chromeos/brightness/brightness_controller_chromeos.h"
#include "ash/common/system/chromeos/keyboard_brightness_controller.h"
#include "ash/common/system/chromeos/network/vpn_list.h"
#include "ash/common/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/common/system/keyboard_brightness_control_delegate.h"
#include "ash/common/system/locale/locale_notification_controller.h"
#include "ash/common/system/toast/toast_manager.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm/immersive_context_ash.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm/root_window_finder.h"
#include "ash/common/wm/system_modal_container_layout_manager.h"
#include "ash/common/wm/window_cycle_controller.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "services/preferences/public/cpp/pref_client_store.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/display/display.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

// static
WmShell* WmShell::instance_ = nullptr;

WmShell::~WmShell() {
  session_controller_->RemoveSessionStateObserver(this);
}

// static
void WmShell::Set(WmShell* instance) {
  instance_ = instance;
}

// static
WmShell* WmShell::Get() {
  return instance_;
}

void WmShell::Initialize(const scoped_refptr<base::SequencedWorkerPool>& pool) {
  blocking_pool_ = pool;

  // Some delegates access WmShell during their construction. Create them here
  // instead of the WmShell constructor.
  accessibility_delegate_.reset(delegate_->CreateAccessibilityDelegate());
  palette_delegate_ = delegate_->CreatePaletteDelegate();
  toast_manager_.reset(new ToastManager);

  // Create the app list item in the shelf data model.
  AppListShelfItemDelegate::CreateAppListItemAndDelegate(shelf_model());

  // Install the custom factory early on so that views::FocusManagers for Tray,
  // Shelf, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  wallpaper_controller_.reset(new WallpaperController(blocking_pool_));

  // Start devtools server
  devtools_server_ = ui::devtools::UiDevToolsServer::Create(nullptr);
  if (devtools_server_) {
    auto dom_backend = base::MakeUnique<devtools::AshDevToolsDOMAgent>(this);
    auto css_backend =
        base::MakeUnique<devtools::AshDevToolsCSSAgent>(dom_backend.get());
    auto devtools_client = base::MakeUnique<ui::devtools::UiDevToolsClient>(
        "Ash", devtools_server_.get());
    devtools_client->AddAgent(std::move(dom_backend));
    devtools_client->AddAgent(std::move(css_backend));
    devtools_server_->AttachClient(std::move(devtools_client));
  }
}

void WmShell::Shutdown() {
  if (added_activation_observer_)
    Shell::GetInstance()->activation_client()->RemoveObserver(this);

  // These members access WmShell in their destructors.
  wallpaper_controller_.reset();
  accessibility_delegate_.reset();

  // ShelfWindowWatcher has window observers and a pointer to the shelf model.
  shelf_window_watcher_.reset();
  // ShelfItemDelegate subclasses it owns have complex cleanup to run (e.g. ARC
  // shelf items in Chrome) so explicitly shutdown early.
  shelf_model()->DestroyItemDelegates();
  // Must be destroyed before FocusClient.
  shelf_delegate_.reset();

  // Balances the Install() in Initialize().
  views::FocusManagerFactory::Install(nullptr);

  // Removes itself as an observer of |pref_store_|.
  shelf_controller_.reset();
}

ShelfModel* WmShell::shelf_model() {
  return shelf_controller_->model();
}

void WmShell::ShowContextMenu(const gfx::Point& location_in_screen,
                              ui::MenuSourceType source_type) {
  // Bail if there is no active user session or if the screen is locked.
  if (GetSessionStateDelegate()->NumberOfLoggedInUsers() < 1 ||
      GetSessionStateDelegate()->IsScreenLocked()) {
    return;
  }

  WmWindow* root = wm::GetRootWindowAt(location_in_screen);
  root->GetRootWindowController()->ShowContextMenu(location_in_screen,
                                                   source_type);
}

void WmShell::CreateShelfView() {
  // Must occur after SessionStateDelegate creation and user login.
  DCHECK(GetSessionStateDelegate());
  DCHECK_GT(GetSessionStateDelegate()->NumberOfLoggedInUsers(), 0);
  CreateShelfDelegate();

  for (WmWindow* root_window : GetAllRootWindows())
    root_window->GetRootWindowController()->CreateShelfView();
}

void WmShell::CreateShelfDelegate() {
  // May be called multiple times as shelves are created and destroyed.
  if (shelf_delegate_)
    return;
  // Must occur after SessionStateDelegate creation and user login because
  // Chrome's implementation of ShelfDelegate assumes it can get information
  // about multi-profile login state.
  DCHECK(GetSessionStateDelegate());
  DCHECK_GT(GetSessionStateDelegate()->NumberOfLoggedInUsers(), 0);
  shelf_delegate_.reset(delegate_->CreateShelfDelegate(shelf_model()));
  shelf_window_watcher_.reset(new ShelfWindowWatcher(shelf_model()));
}

void WmShell::OnMaximizeModeStarted() {
  for (auto& observer : shell_observers_)
    observer.OnMaximizeModeStarted();
}

void WmShell::OnMaximizeModeEnding() {
  for (auto& observer : shell_observers_)
    observer.OnMaximizeModeEnding();
}

void WmShell::OnMaximizeModeEnded() {
  for (auto& observer : shell_observers_)
    observer.OnMaximizeModeEnded();
}

void WmShell::UpdateAfterLoginStatusChange(LoginStatus status) {
  for (WmWindow* root_window : GetAllRootWindows()) {
    root_window->GetRootWindowController()->UpdateAfterLoginStatusChange(
        status);
  }
}

void WmShell::NotifyFullscreenStateChanged(bool is_fullscreen,
                                           WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnFullscreenStateChanged(is_fullscreen, root_window);
}

void WmShell::NotifyPinnedStateChanged(WmWindow* pinned_window) {
  for (auto& observer : shell_observers_)
    observer.OnPinnedStateChanged(pinned_window);
}

void WmShell::NotifyVirtualKeyboardActivated(bool activated) {
  for (auto& observer : shell_observers_)
    observer.OnVirtualKeyboardStateChanged(activated);
}

void WmShell::NotifyShelfCreatedForRootWindow(WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfCreatedForRootWindow(root_window);
}

void WmShell::NotifyShelfAlignmentChanged(WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfAlignmentChanged(root_window);
}

void WmShell::NotifyShelfAutoHideBehaviorChanged(WmWindow* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfAutoHideBehaviorChanged(root_window);
}

void WmShell::AddActivationObserver(WmActivationObserver* observer) {
  if (!added_activation_observer_) {
    added_activation_observer_ = true;
    Shell::GetInstance()->activation_client()->AddObserver(this);
  }
  activation_observers_.AddObserver(observer);
}

void WmShell::RemoveActivationObserver(WmActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
}

void WmShell::AddShellObserver(ShellObserver* observer) {
  shell_observers_.AddObserver(observer);
}

void WmShell::RemoveShellObserver(ShellObserver* observer) {
  shell_observers_.RemoveObserver(observer);
}

void WmShell::OnLockStateEvent(LockStateObserver::EventType event) {
  for (auto& observer : lock_state_observers_)
    observer.OnLockStateEvent(event);
}

void WmShell::AddLockStateObserver(LockStateObserver* observer) {
  lock_state_observers_.AddObserver(observer);
}

void WmShell::RemoveLockStateObserver(LockStateObserver* observer) {
  lock_state_observers_.RemoveObserver(observer);
}

void WmShell::SetShelfDelegateForTesting(
    std::unique_ptr<ShelfDelegate> test_delegate) {
  shelf_delegate_ = std::move(test_delegate);
}

void WmShell::SetPaletteDelegateForTesting(
    std::unique_ptr<PaletteDelegate> palette_delegate) {
  palette_delegate_ = std::move(palette_delegate);
}

WmShell::WmShell(std::unique_ptr<ShellDelegate> shell_delegate)
    : delegate_(std::move(shell_delegate)),
      app_list_(base::MakeUnique<app_list::AppList>()),
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
      shutdown_controller_(base::MakeUnique<ShutdownController>()),
      system_tray_controller_(base::MakeUnique<SystemTrayController>()),
      system_tray_notifier_(base::MakeUnique<SystemTrayNotifier>()),
      vpn_list_(base::MakeUnique<VpnList>()),
      wallpaper_delegate_(delegate_->CreateWallpaperDelegate()),
      window_cycle_controller_(base::MakeUnique<WindowCycleController>()),
      window_selector_controller_(
          base::MakeUnique<WindowSelectorController>()) {
  session_controller_->AddSessionStateObserver(this);

  prefs::mojom::PreferencesServiceFactoryPtr pref_factory_ptr;
  // Can be null in tests.
  if (!delegate_->GetShellConnector())
    return;
  delegate_->GetShellConnector()->BindInterface(prefs::mojom::kServiceName,
                                                &pref_factory_ptr);
  pref_store_ = new preferences::PrefClientStore(std::move(pref_factory_ptr));
}

RootWindowController* WmShell::GetPrimaryRootWindowController() {
  return GetPrimaryRootWindow()->GetRootWindowController();
}

WmWindow* WmShell::GetRootWindowForNewWindows() {
  if (scoped_root_window_for_new_windows_)
    return scoped_root_window_for_new_windows_;
  return root_window_for_new_windows_;
}

bool WmShell::IsSystemModalWindowOpen() {
  if (simulate_modal_window_open_for_testing_)
    return true;

  // Traverse all system modal containers, and find its direct child window
  // with "SystemModal" setting, and visible.
  for (WmWindow* root : GetAllRootWindows()) {
    WmWindow* system_modal =
        root->GetChildByShellWindowId(kShellWindowId_SystemModalContainer);
    if (!system_modal)
      continue;
    for (const WmWindow* child : system_modal->GetChildren()) {
      if (child->IsSystemModal() && child->GetTargetVisibility()) {
        return true;
      }
    }
  }
  return false;
}

void WmShell::CreateModalBackground(WmWindow* window) {
  for (WmWindow* root_window : GetAllRootWindows()) {
    root_window->GetRootWindowController()
        ->GetSystemModalLayoutManager(window)
        ->CreateModalBackground();
  }
}

void WmShell::OnModalWindowRemoved(WmWindow* removed) {
  WmWindow::Windows root_windows = GetAllRootWindows();
  for (WmWindow* root_window : root_windows) {
    if (root_window->GetRootWindowController()
            ->GetSystemModalLayoutManager(removed)
            ->ActivateNextModalWindow()) {
      return;
    }
  }
  for (WmWindow* root_window : root_windows) {
    root_window->GetRootWindowController()
        ->GetSystemModalLayoutManager(removed)
        ->DestroyModalBackground();
  }
}

void WmShell::ShowAppList() {
  // Show the app list on the default display for new windows.
  app_list_->Show(GetRootWindowForNewWindows()->GetDisplayNearestWindow().id());
}

void WmShell::DismissAppList() {
  app_list_->Dismiss();
}

void WmShell::ToggleAppList() {
  // Toggle the app list on the default display for new windows.
  app_list_->ToggleAppList(
      GetRootWindowForNewWindows()->GetDisplayNearestWindow().id());
}

bool WmShell::IsApplistVisible() const {
  return app_list_->IsVisible();
}

bool WmShell::GetAppListTargetVisibility() const {
  return app_list_->GetTargetVisibility();
}

void WmShell::SetKeyboardUI(std::unique_ptr<KeyboardUI> keyboard_ui) {
  keyboard_ui_ = std::move(keyboard_ui);
}

void WmShell::SetSystemTrayDelegate(
    std::unique_ptr<SystemTrayDelegate> delegate) {
  DCHECK(delegate);
  system_tray_delegate_ = std::move(delegate);
  system_tray_delegate_->Initialize();
  // Accesses WmShell in its constructor.
  logout_confirmation_controller_.reset(new LogoutConfirmationController(
      base::Bind(&SystemTrayController::SignOut,
                 base::Unretained(system_tray_controller_.get()))));
}

void WmShell::DeleteSystemTrayDelegate() {
  DCHECK(system_tray_delegate_);
  // Accesses WmShell in its destructor.
  logout_confirmation_controller_.reset();
  system_tray_delegate_.reset();
}

void WmShell::DeleteWindowCycleController() {
  window_cycle_controller_.reset();
}

void WmShell::DeleteWindowSelectorController() {
  window_selector_controller_.reset();
}

void WmShell::CreateMaximizeModeController() {
  maximize_mode_controller_.reset(new MaximizeModeController);
}

void WmShell::DeleteMaximizeModeController() {
  maximize_mode_controller_.reset();
}

void WmShell::CreateMruWindowTracker() {
  mru_window_tracker_.reset(new MruWindowTracker);
}

void WmShell::DeleteMruWindowTracker() {
  mru_window_tracker_.reset();
}

void WmShell::DeleteToastManager() {
  toast_manager_.reset();
}

void WmShell::SetAcceleratorController(
    std::unique_ptr<AcceleratorController> accelerator_controller) {
  accelerator_controller_ = std::move(accelerator_controller);
}

void WmShell::SessionStateChanged(session_manager::SessionState state) {
  // Create the shelf when a session becomes active. It's safe to do this
  // multiple times (e.g. initial login vs. multiprofile add session).
  if (state == session_manager::SessionState::ACTIVE)
    CreateShelfView();
}

void WmShell::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  WmWindow* gained_active_wm = WmWindow::Get(gained_active);
  WmWindow* lost_active_wm = WmWindow::Get(lost_active);
  if (gained_active_wm)
    set_root_window_for_new_windows(gained_active_wm->GetRootWindow());
  for (auto& observer : activation_observers_)
    observer.OnWindowActivated(gained_active_wm, lost_active_wm);
}

void WmShell::OnAttemptToReactivateWindow(aura::Window* request_active,
                                          aura::Window* actual_active) {
  for (auto& observer : activation_observers_) {
    observer.OnAttemptToReactivateWindow(WmWindow::Get(request_active),
                                         WmWindow::Get(actual_active));
  }
}

}  // namespace ash
