// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"

#include <string>

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/display/display_manager.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "components/arc/arc_bridge_service.h"
#include "components/exo/shell_surface.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/base/base_window.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget.h"

namespace {

enum class FullScreenMode {
  NOT_DEFINED,  // Fullscreen mode was not defined.
  ACTIVE,       // Fullscreen is activated for an app.
  NON_ACTIVE,   // Fullscreen was not activated for an app.
};

arc::mojom::OrientationLock GetCurrentOrientation() {
  if (!display::Display::HasInternalDisplay())
    return arc::mojom::OrientationLock::NONE;
  display::Display internal_display =
      ash::Shell::GetInstance()->display_manager()->GetDisplayForId(
          display::Display::InternalDisplayId());

  // ChromeOS currently assumes that the internal panel is always
  // landscape (ROTATE_0 == landscape).
  switch (internal_display.rotation()) {
    case display::Display::ROTATE_0:
    case display::Display::ROTATE_180:
      return arc::mojom::OrientationLock::LANDSCAPE;
    case display::Display::ROTATE_90:
    case display::Display::ROTATE_270:
      return arc::mojom::OrientationLock::PORTRAIT;
  }
  return arc::mojom::OrientationLock::NONE;
}

blink::WebScreenOrientationLockType BlinkOrientationLockFromMojom(
    arc::mojom::OrientationLock orientation_lock) {
  DCHECK_NE(arc::mojom::OrientationLock::CURRENT, orientation_lock);
  if (orientation_lock == arc::mojom::OrientationLock::PORTRAIT) {
    return blink::WebScreenOrientationLockPortrait;
  } else if (orientation_lock == arc::mojom::OrientationLock::LANDSCAPE) {
    return blink::WebScreenOrientationLockLandscape;
  } else {
    return blink::WebScreenOrientationLockAny;
  }
}

}  // namespace

class ArcAppWindowLauncherController::AppWindow : public ui::BaseWindow {
 public:
  AppWindow(int task_id,
            const std::string app_id,
            ArcAppWindowLauncherController* owner)
      : task_id_(task_id), app_id_(app_id), owner_(owner) {}
  ~AppWindow() {}

  void SetController(ArcAppWindowLauncherItemController* controller) {
    DCHECK(!controller_ && controller);
    controller_ = controller;
  }

  void ResetController() { controller_ = nullptr; }

  void SetFullscreenMode(FullScreenMode mode) {
    DCHECK(mode != FullScreenMode::NOT_DEFINED);
    fullscreen_mode_ = mode;
  }

  FullScreenMode fullscreen_mode() const { return fullscreen_mode_; }

  int task_id() const { return task_id_; }

  ash::ShelfID shelf_id() const { return shelf_id_; }

  void set_shelf_id(ash::ShelfID shelf_id) { shelf_id_ = shelf_id; }

  views::Widget* widget() const { return widget_; }

  void set_widget(views::Widget* widget) { widget_ = widget; }

  ArcAppWindowLauncherItemController* controller() { return controller_; }

  const std::string app_id() { return app_id_; }

  // ui::BaseWindow:
  bool IsActive() const override {
    return widget_ && widget_->IsActive() &&
           owner_->active_task_id_ == task_id_;
  }

  bool IsMaximized() const override {
    NOTREACHED();
    return false;
  }

  bool IsMinimized() const override {
    NOTREACHED();
    return false;
  }

  bool IsFullscreen() const override {
    NOTREACHED();
    return false;
  }

  gfx::NativeWindow GetNativeWindow() const override { return nullptr; }

  gfx::Rect GetRestoredBounds() const override {
    NOTREACHED();
    return gfx::Rect();
  }

  ui::WindowShowState GetRestoredState() const override {
    NOTREACHED();
    return ui::SHOW_STATE_NORMAL;
  }

  gfx::Rect GetBounds() const override {
    NOTREACHED();
    return gfx::Rect();
  }

  void Show() override {
    // TODO(khmel): support window minimizing.
  }

  void ShowInactive() override { NOTREACHED(); }

  void Hide() override { NOTREACHED(); }

  void Close() override {
    arc::mojom::AppInstance* app_instance = GetAppInstance();
    if (!app_instance)
      return;
    app_instance->CloseTask(task_id_);
  }

  void Activate() override {
    arc::mojom::AppInstance* app_instance = GetAppInstance();
    if (!app_instance)
      return;
    app_instance->SetTaskActive(task_id_);
    if (widget_)
      widget_->Activate();
  }

  void Deactivate() override { NOTREACHED(); }

  void Maximize() override { NOTREACHED(); }

  void Minimize() override {
    if (widget_)
      widget_->Minimize();
  }

  void Restore() override { NOTREACHED(); }

  void SetBounds(const gfx::Rect& bounds) override { NOTREACHED(); }

  void FlashFrame(bool flash) override { NOTREACHED(); }

  bool IsAlwaysOnTop() const override {
    NOTREACHED();
    return false;
  }

  void SetAlwaysOnTop(bool always_on_top) override { NOTREACHED(); }

  arc::mojom::OrientationLock requested_orientation_lock() const {
    return requested_orientation_lock_;
  }

  void set_requested_orientation_lock(arc::mojom::OrientationLock lock) {
    has_requested_orientation_lock_ = true;
    requested_orientation_lock_ = lock;
  }

  bool has_requested_orientation_lock() const {
    return has_requested_orientation_lock_;
  }

 private:
  arc::mojom::AppInstance* GetAppInstance() {
    arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
    arc::mojom::AppInstance* app_instance =
        bridge_service ? bridge_service->app()->instance() : nullptr;
    if (!app_instance) {
      VLOG(2) << "Arc Bridge is not available.";
      return nullptr;
    }

    if (bridge_service->app()->version() < 3) {
      VLOG(2) << "Arc Bridge has old version for apps."
              << bridge_service->app()->version();
      return nullptr;
    }
    return app_instance;
  }

  int task_id_;
  ash::ShelfID shelf_id_ = 0;
  std::string app_id_;
  FullScreenMode fullscreen_mode_ = FullScreenMode::NOT_DEFINED;
  // Unowned pointers
  ArcAppWindowLauncherController* owner_;
  ArcAppWindowLauncherItemController* controller_ = nullptr;
  // Unowned pointer, represents host Arc window.
  views::Widget* widget_ = nullptr;

  arc::mojom::OrientationLock requested_orientation_lock_ =
      arc::mojom::OrientationLock::NONE;
  bool has_requested_orientation_lock_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppWindow);
};

ArcAppWindowLauncherController::ArcAppWindowLauncherController(
    ChromeLauncherController* owner,
    ash::ShelfDelegate* shelf_delegate)
    : AppWindowLauncherController(owner), shelf_delegate_(shelf_delegate) {
  if (arc::ArcAuthService::IsAllowedForProfile(owner->GetProfile())) {
    observed_profile_ = owner->GetProfile();
    StartObserving(observed_profile_);
  }
}

ArcAppWindowLauncherController::~ArcAppWindowLauncherController() {
  if (observed_profile_)
    StopObserving(observed_profile_);
  if (observing_shell_)
    ash::WmShell::Get()->RemoveShellObserver(this);
}

// static
std::string ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(
    const std::string& arc_app_id) {
  return arc_app_id == arc::kPlayStoreAppId ? ArcSupportHost::kHostAppId
                                            : arc_app_id;
}

// static
std::string ArcAppWindowLauncherController::GetArcAppIdFromShelfAppId(
    const std::string& shelf_app_id) {
  return shelf_app_id == ArcSupportHost::kHostAppId ? arc::kPlayStoreAppId
                                                    : shelf_app_id;
}

void ArcAppWindowLauncherController::ActiveUserChanged(
    const std::string& user_email) {
  for (auto& it : task_id_to_app_window_) {
    AppWindow* app_window = it.second.get();
    if (user_email ==
        user_manager::UserManager::Get()
            ->GetPrimaryUser()
            ->GetAccountId()
            .GetUserEmail()) {
      RegisterApp(app_window);
    } else {
      UnregisterApp(app_window);
    }
  }
}

void ArcAppWindowLauncherController::AdditionalUserAddedToSession(
    Profile* profile) {
  DCHECK(!arc::ArcAuthService::IsAllowedForProfile(profile));
}

void ArcAppWindowLauncherController::OnWindowInitialized(aura::Window* window) {
  // Arc windows has type WINDOW_TYPE_NORMAL.
  if (window->type() != ui::wm::WINDOW_TYPE_NORMAL)
    return;
  observed_windows_.push_back(window);
  window->AddObserver(this);
}

void ArcAppWindowLauncherController::OnWindowVisibilityChanging(
    aura::Window* window,
    bool visible) {
  // The application id property should be set at this time.
  if (visible)
    CheckForAppWindowWidget(window);
}

void ArcAppWindowLauncherController::OnWindowDestroying(aura::Window* window) {
  auto it =
      std::find(observed_windows_.begin(), observed_windows_.end(), window);
  DCHECK(it != observed_windows_.end());
  observed_windows_.erase(it);
  window->RemoveObserver(this);

  for (auto& it : task_id_to_app_window_) {
    if (it.second->widget() == views::Widget::GetWidgetForNativeWindow(window))
      it.second->set_widget(nullptr);
  }
}

ArcAppWindowLauncherController::AppWindow*
ArcAppWindowLauncherController::GetAppWindowForTask(int task_id) {
  TaskIdToAppWindow::iterator it = task_id_to_app_window_.find(task_id);
  if (it == task_id_to_app_window_.end())
    return nullptr;
  return it->second.get();
}

void ArcAppWindowLauncherController::CheckForAppWindowWidget(
    aura::Window* window) {
  const std::string app_id = exo::ShellSurface::GetApplicationId(window);
  if (app_id.empty())
    return;

  int task_id = -1;
  if (sscanf(app_id.c_str(), "org.chromium.arc.%d", &task_id) != 1)
    return;

  if (task_id) {
    // We need to add the observer after exo started observing shell
    // because we want to update the orientation after exo send
    // the layout switch information.
    if (!observing_shell_) {
      observing_shell_ = true;
      ash::WmShell::Get()->AddShellObserver(this);
    }

    AppWindow* app_window = GetAppWindowForTask(task_id);
    if (app_window) {
      app_window->set_widget(views::Widget::GetWidgetForNativeWindow(window));
      ash::SetShelfIDForWindow(app_window->shelf_id(), window);
      chrome::MultiUserWindowManager::GetInstance()->SetWindowOwner(
          window,
          user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
      if (ash::WmShell::Get()
              ->maximize_mode_controller()
              ->IsMaximizeModeWindowManagerEnabled()) {
        SetOrientationLockForAppWindow(app_window);
      }
    }
  }
}

void ArcAppWindowLauncherController::OnAppReadyChanged(
    const std::string& app_id,
    bool ready) {
  if (!ready)
    OnAppRemoved(app_id);
}

void ArcAppWindowLauncherController::OnAppRemoved(const std::string& app_id) {
  const std::string shelf_app_id = GetShelfAppIdFromArcAppId(app_id);

  AppControllerMap::const_iterator it = app_controller_map_.find(shelf_app_id);
  if (it == app_controller_map_.end())
    return;

  const ArcAppWindowLauncherItemController* controller = it->second;

  std::vector<int> task_ids_to_remove;
  for (auto* window : controller->windows()) {
    AppWindow* app_window = static_cast<AppWindow*>(window);
    task_ids_to_remove.push_back(app_window->task_id());
  }

  for (const auto task_id : task_ids_to_remove)
    OnTaskDestroyed(task_id);

  DCHECK(app_controller_map_.find(shelf_app_id) == app_controller_map_.end());
}

void ArcAppWindowLauncherController::OnTaskCreated(
    int task_id,
    const std::string& package_name,
    const std::string& activity_name) {
  DCHECK(!GetAppWindowForTask(task_id));

  const std::string app_id = GetShelfAppIdFromArcAppId(
      ArcAppListPrefs::GetAppId(package_name, activity_name));

  std::unique_ptr<AppWindow> app_window(new AppWindow(task_id, app_id, this));
  RegisterApp(app_window.get());

  task_id_to_app_window_[task_id] = std::move(app_window);

  for (auto* window : observed_windows_)
    CheckForAppWindowWidget(window);
}

void ArcAppWindowLauncherController::OnTaskDestroyed(int task_id) {
  TaskIdToAppWindow::iterator it = task_id_to_app_window_.find(task_id);
  if (it == task_id_to_app_window_.end())
    return;

  AppWindow* app_window = it->second.get();
  UnregisterApp(app_window);

  task_id_to_app_window_.erase(it);
}

void ArcAppWindowLauncherController::OnTaskSetActive(int32_t task_id) {
  if (observed_profile_ != owner()->GetProfile())
    return;

  TaskIdToAppWindow::iterator previous_active_app_it =
      task_id_to_app_window_.find(active_task_id_);
  if (previous_active_app_it != task_id_to_app_window_.end()) {
    owner()->SetItemStatus(previous_active_app_it->second->shelf_id(),
                           ash::STATUS_RUNNING);
    previous_active_app_it->second->SetFullscreenMode(
        previous_active_app_it->second->widget() &&
                previous_active_app_it->second->widget()->IsFullscreen()
            ? FullScreenMode::ACTIVE
            : FullScreenMode::NON_ACTIVE);
  }

  active_task_id_ = task_id;

  TaskIdToAppWindow::iterator new_active_app_it =
      task_id_to_app_window_.find(active_task_id_);
  if (new_active_app_it != task_id_to_app_window_.end()) {
    owner()->SetItemStatus(
        new_active_app_it->second->shelf_id(),
        new_active_app_it->second->widget() &&
                new_active_app_it->second->widget()->IsActive()
            ? ash::STATUS_ACTIVE
            : ash::STATUS_RUNNING);
    // TODO(reveman): Figure out how to support fullscreen in interleaved
    // window mode.
    // if (new_active_app_it->second->widget()) {
    //   new_active_app_it->second->widget()->SetFullscreen(
    //       new_active_app_it->second->fullscreen_mode() ==
    //       FullScreenMode::ACTIVE);
    // }
  }
}

void ArcAppWindowLauncherController::OnTaskOrientationLockRequested(
    int32_t task_id,
    const arc::mojom::OrientationLock orientation_lock) {
  // Don't save to AppInfo because this is requested in runtime.
  TaskIdToAppWindow::iterator app_it = task_id_to_app_window_.find(task_id);
  if (app_it == task_id_to_app_window_.end())
    return;
  AppWindow* app_window = app_it->second.get();
  app_window->set_requested_orientation_lock(orientation_lock);

  if (ash::WmShell::Get()
          ->maximize_mode_controller()
          ->IsMaximizeModeWindowManagerEnabled()) {
    SetOrientationLockForAppWindow(app_window);
  }
}

AppWindowLauncherItemController*
ArcAppWindowLauncherController::ControllerForWindow(aura::Window* window) {
  AppWindow* app_window = GetAppWindowForTask(active_task_id_);
  if (app_window &&
      app_window->widget() == views::Widget::GetWidgetForNativeWindow(window)) {
    return app_window->controller();
  }

  for (auto& it : task_id_to_app_window_) {
    if (it.second->widget() == views::Widget::GetWidgetForNativeWindow(window))
      return it.second->controller();
  }

  return nullptr;
}

void ArcAppWindowLauncherController::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  OnTaskSetActive(active_task_id_);
}

void ArcAppWindowLauncherController::OnMaximizeModeStarted() {
  for (auto& it : task_id_to_app_window_)
    SetOrientationLockForAppWindow(it.second.get());
}

void ArcAppWindowLauncherController::OnMaximizeModeEnded() {
  ash::ScreenOrientationController* orientation_controller =
      ash::Shell::GetInstance()->screen_orientation_controller();
  // Don't unlock one by one because it'll switch to next rotation.
  orientation_controller->UnlockAll();
}

void ArcAppWindowLauncherController::StartObserving(Profile* profile) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  DCHECK(prefs);
  prefs->AddObserver(this);
}

void ArcAppWindowLauncherController::StopObserving(Profile* profile) {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  prefs->RemoveObserver(this);
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);
}

void ArcAppWindowLauncherController::RegisterApp(AppWindow* app_window) {
  const std::string app_id = app_window->app_id();
  DCHECK(!app_id.empty());

  ArcAppWindowLauncherItemController* controller;
  AppControllerMap::iterator it = app_controller_map_.find(app_id);
  ash::ShelfID shelf_id = 0;
  if (it != app_controller_map_.end()) {
    controller = it->second;
    DCHECK_EQ(controller->app_id(), app_id);
    shelf_id = controller->shelf_id();
  } else {
    controller = new ArcAppWindowLauncherItemController(app_id, owner());
    shelf_id = shelf_delegate_->GetShelfIDForAppID(app_id);
    if (shelf_id == 0) {
      // Map Play Store shelf icon to Arc Support host, to share one entry.
      shelf_id = owner()->CreateAppLauncherItem(controller, app_id,
                                                ash::STATUS_RUNNING);
    } else {
      owner()->SetItemController(shelf_id, controller);
    }
    app_controller_map_[app_id] = controller;
  }
  controller->AddWindow(app_window);
  owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  app_window->SetController(controller);
  app_window->set_shelf_id(shelf_id);
}

void ArcAppWindowLauncherController::UnregisterApp(AppWindow* app_window) {
  const std::string app_id = app_window->app_id();
  DCHECK(!app_id.empty());
  AppControllerMap::iterator it = app_controller_map_.find(app_id);
  DCHECK(it != app_controller_map_.end());

  ArcAppWindowLauncherItemController* controller = it->second;
  controller->RemoveWindow(app_window);
  if (!controller->window_count()) {
    ash::ShelfID shelf_id = app_window->shelf_id();
    owner()->CloseLauncherItem(shelf_id);
    app_controller_map_.erase(it);
  }
  app_window->ResetController();
}

void ArcAppWindowLauncherController::SetOrientationLockForAppWindow(
    AppWindow* app_window) {
  ash::WmWindow* window =
      ash::WmLookup::Get()->GetWindowForWidget(app_window->widget());
  if (!window)
    return;
  arc::mojom::OrientationLock orientation_lock;
  if (app_window->has_requested_orientation_lock()) {
    orientation_lock = app_window->requested_orientation_lock();
  } else {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(observed_profile_);
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        prefs->GetApp(app_window->app_id());
    if (!app_info)
      return;
    orientation_lock = app_info->orientation_lock;
  }

  if (orientation_lock == arc::mojom::OrientationLock::CURRENT) {
    // Resolve the orientation when it first resolved.
    orientation_lock = GetCurrentOrientation();
    app_window->set_requested_orientation_lock(orientation_lock);
  }

  ash::Shell* shell = ash::Shell::GetInstance();
  shell->screen_orientation_controller()->LockOrientationForWindow(
      window, BlinkOrientationLockFromMojom(orientation_lock));
}
