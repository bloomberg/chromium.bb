// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"

#include <string>

#include "ash/aura/wm_window_aura.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window_property.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/shared/app_types.h"
#include "ash/shell.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
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
#include "ui/display/manager/display_manager.h"
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
      return arc::mojom::OrientationLock::LANDSCAPE_PRIMARY;
    case display::Display::ROTATE_90:
      return arc::mojom::OrientationLock::PORTRAIT_PRIMARY;
    case display::Display::ROTATE_180:
      return arc::mojom::OrientationLock::LANDSCAPE_SECONDARY;
    case display::Display::ROTATE_270:
      return arc::mojom::OrientationLock::PORTRAIT_SECONDARY;
  }
  return arc::mojom::OrientationLock::NONE;
}

blink::WebScreenOrientationLockType BlinkOrientationLockFromMojom(
    arc::mojom::OrientationLock orientation_lock) {
  DCHECK_NE(arc::mojom::OrientationLock::CURRENT, orientation_lock);
  switch (orientation_lock) {
    case arc::mojom::OrientationLock::PORTRAIT:
      return blink::WebScreenOrientationLockPortrait;
    case arc::mojom::OrientationLock::LANDSCAPE:
      return blink::WebScreenOrientationLockLandscape;
    case arc::mojom::OrientationLock::PORTRAIT_PRIMARY:
      return blink::WebScreenOrientationLockPortraitPrimary;
    case arc::mojom::OrientationLock::LANDSCAPE_PRIMARY:
      return blink::WebScreenOrientationLockLandscapePrimary;
    case arc::mojom::OrientationLock::PORTRAIT_SECONDARY:
      return blink::WebScreenOrientationLockPortraitSecondary;
    case arc::mojom::OrientationLock::LANDSCAPE_SECONDARY:
      return blink::WebScreenOrientationLockLandscapeSecondary;
    default:
      return blink::WebScreenOrientationLockAny;
  }
}

}  // namespace

// The information about the arc application window which has to be kept
// even when its AppWindow is not present.
class ArcAppWindowLauncherController::AppWindowInfo {
 public:
  explicit AppWindowInfo(const std::string& shelf_app_id)
      : shelf_app_id_(shelf_app_id) {}
  ~AppWindowInfo() {}

  const std::string& shelf_app_id() const { return shelf_app_id_; }

  bool has_requested_orientation_lock() const {
    return has_requested_orientation_lock_;
  }

  void set_requested_orientation_lock(arc::mojom::OrientationLock lock) {
    has_requested_orientation_lock_ = true;
    requested_orientation_lock_ = lock;
  }

  arc::mojom::OrientationLock requested_orientation_lock() const {
    return requested_orientation_lock_;
  }

  void set_app_window(std::unique_ptr<AppWindow> window) {
    app_window_ = std::move(window);
  }

  AppWindow* app_window() { return app_window_.get(); }

 private:
  std::string shelf_app_id_;
  bool has_requested_orientation_lock_ = false;
  arc::mojom::OrientationLock requested_orientation_lock_ =
      arc::mojom::OrientationLock::NONE;
  std::unique_ptr<AppWindow> app_window_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowInfo);
};

// A ui::BaseWindow for a chromeos launcher to control ARC applications.
class ArcAppWindowLauncherController::AppWindow : public ui::BaseWindow {
 public:
  AppWindow(int task_id,
            views::Widget* widget,
            ArcAppWindowLauncherController* owner)
      : task_id_(task_id), widget_(widget), owner_(owner) {}
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

  ArcAppWindowLauncherItemController* controller() { return controller_; }

  // ui::BaseWindow:
  bool IsActive() const override {
    return widget_->IsActive() && owner_->active_task_id_ == task_id_;
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

  gfx::NativeWindow GetNativeWindow() const override {
    return widget_->GetNativeWindow();
  }

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

  void Show() override { widget_->Show(); }

  void ShowInactive() override { NOTREACHED(); }

  void Hide() override { NOTREACHED(); }

  void Close() override { arc::CloseTask(task_id_); }

  void Activate() override { widget_->Activate(); }

  void Deactivate() override { NOTREACHED(); }

  void Maximize() override { NOTREACHED(); }

  void Minimize() override { widget_->Minimize(); }

  void Restore() override { NOTREACHED(); }

  void SetBounds(const gfx::Rect& bounds) override { NOTREACHED(); }

  void FlashFrame(bool flash) override { NOTREACHED(); }

  bool IsAlwaysOnTop() const override {
    NOTREACHED();
    return false;
  }

  void SetAlwaysOnTop(bool always_on_top) override { NOTREACHED(); }

 private:
  int task_id_;
  ash::ShelfID shelf_id_ = 0;
  FullScreenMode fullscreen_mode_ = FullScreenMode::NOT_DEFINED;
  // Unowned pointers
  views::Widget* const widget_;
  ArcAppWindowLauncherController* owner_;
  ArcAppWindowLauncherItemController* controller_ = nullptr;
  // Unowned pointer, represents host Arc window.

  DISALLOW_COPY_AND_ASSIGN(AppWindow);
};

ArcAppWindowLauncherController::ArcAppWindowLauncherController(
    ChromeLauncherController* owner,
    ash::ShelfDelegate* shelf_delegate)
    : AppWindowLauncherController(owner), shelf_delegate_(shelf_delegate) {
  if (arc::ArcSessionManager::IsAllowedForProfile(owner->profile())) {
    observed_profile_ = owner->profile();
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
  const std::string& primary_user_email = user_manager::UserManager::Get()
                                              ->GetPrimaryUser()
                                              ->GetAccountId()
                                              .GetUserEmail();
  if (user_email == primary_user_email) {
    // Restore existing Arc window and create controllers for them.
    AttachControllerToWindowsIfNeeded();

    // Make sure that we created items for all apps, not only which have a
    // window.
    for (const auto& info : task_id_to_app_window_info_)
      AttachControllerToTask(info.second->shelf_app_id(), info.first);

    // Update active status.
    OnTaskSetActive(active_task_id_);
  } else {
    // Remove all Arc apps and destroy its controllers. There is no mapping
    // task id to app window because it is not safe when controller is missing.
    for (auto& it : task_id_to_app_window_info_)
      UnregisterApp(it.second.get(), true);

    // Some controllers might have no windows attached, for example background
    // task when foreground tasks is in full screen.
    for (const auto& it : app_controller_map_)
      owner()->CloseLauncherItem(it.second->shelf_id());
    app_controller_map_.clear();
  }
}

void ArcAppWindowLauncherController::AdditionalUserAddedToSession(
    Profile* profile) {
  DCHECK(!arc::ArcSessionManager::IsAllowedForProfile(profile));
}

void ArcAppWindowLauncherController::OnWindowInitialized(aura::Window* window) {
  // An arc window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget.
  if (window->type() != ui::wm::WINDOW_TYPE_NORMAL || !window->delegate())
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level())
    return;
  observed_windows_.push_back(window);
  window->AddObserver(this);
}

void ArcAppWindowLauncherController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  // Attach window to multi-user manager now to let it manage visibility state
  // of the Arc window correctly.
  if (GetWindowTaskId(window) > 0) {
    chrome::MultiUserWindowManager::GetInstance()->SetWindowOwner(
        window,
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  }

  // The application id property should be set at this time. It is important to
  // have window->IsVisible set to true before attaching to a controller because
  // the window is registered in multi-user manager and this manager may
  // consider this new window as hidden for current profile. Multi-user manager
  // uses OnWindowVisibilityChanging event to update window state.
  if (visible && observed_profile_ == owner()->profile())
    AttachControllerToWindowIfNeeded(window);
}

void ArcAppWindowLauncherController::OnWindowDestroying(aura::Window* window) {
  auto it =
      std::find(observed_windows_.begin(), observed_windows_.end(), window);
  DCHECK(it != observed_windows_.end());
  observed_windows_.erase(it);
  window->RemoveObserver(this);

  auto info_it = std::find_if(
      task_id_to_app_window_info_.begin(), task_id_to_app_window_info_.end(),
      [window](TaskIdToAppWindowInfo::value_type& pair) {
        return pair.second->app_window() &&
               pair.second->app_window()->GetNativeWindow() == window;
      });
  if (info_it != task_id_to_app_window_info_.end()) {
    // Note, window may be recreated in some cases, so do not close controller
    // on window destroying. Controller will be closed onTaskDestroyed event
    // which is generated when actual task is destroyed.
    UnregisterApp(info_it->second.get(), false);
  }
}

ArcAppWindowLauncherController::AppWindowInfo*
ArcAppWindowLauncherController::GetAppWindowInfoForTask(int task_id) {
  const auto it = task_id_to_app_window_info_.find(task_id);
  return it == task_id_to_app_window_info_.end() ? nullptr : it->second.get();
}

ArcAppWindowLauncherController::AppWindow*
ArcAppWindowLauncherController::GetAppWindowForTask(int task_id) {
  AppWindowInfo* info = GetAppWindowInfoForTask(task_id);
  return info ? info->app_window() : nullptr;
}

void ArcAppWindowLauncherController::AttachControllerToWindowsIfNeeded() {
  for (auto* window : observed_windows_)
    AttachControllerToWindowIfNeeded(window);
}

void ArcAppWindowLauncherController::AttachControllerToWindowIfNeeded(
    aura::Window* window) {
  const int task_id = GetWindowTaskId(window);
  if (task_id <= 0)
    return;

  // We need to add the observer after exo started observing shell
  // because we want to update the orientation after exo send
  // the layout switch information.
  if (!observing_shell_) {
    observing_shell_ = true;
    ash::WmShell::Get()->AddShellObserver(this);
  }

  // Check if we have controller for this task.
  if (GetAppWindowForTask(task_id))
    return;

  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::ARC_APP));

  // Create controller if we have task info.
  AppWindowInfo* info = GetAppWindowInfoForTask(task_id);
  if (!info) {
    VLOG(1) << "Could not find AppWindowInfo for task:" << task_id;
    return;
  }

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  DCHECK(!info->app_window());
  info->set_app_window(base::MakeUnique<AppWindow>(task_id, widget, this));
  RegisterApp(info);
  DCHECK(info->app_window()->controller());
  ash::WmWindowAura::Get(window)->SetIntProperty(
      ash::WmWindowProperty::SHELF_ID, info->app_window()->shelf_id());
  if (ash::WmShell::Get()
          ->maximize_mode_controller()
          ->IsMaximizeModeWindowManagerEnabled()) {
    SetOrientationLockForAppWindow(info->app_window());
  }
}

void ArcAppWindowLauncherController::OnAppReadyChanged(
    const std::string& arc_app_id,
    bool ready) {
  if (!ready)
    OnAppRemoved(arc_app_id);
}

void ArcAppWindowLauncherController::OnAppRemoved(
    const std::string& arc_app_id) {
  const std::string shelf_app_id = GetShelfAppIdFromArcAppId(arc_app_id);

  const auto it = app_controller_map_.find(shelf_app_id);
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
    const std::string& activity_name,
    const std::string& intent) {
  DCHECK(!GetAppWindowForTask(task_id));
  const std::string arc_app_id =
      ArcAppListPrefs::GetAppId(package_name, activity_name);
  const std::string shelf_app_id = GetShelfAppIdFromArcAppId(arc_app_id);
  task_id_to_app_window_info_[task_id] =
      base::MakeUnique<AppWindowInfo>(shelf_app_id);
  // Don't create shelf icon for non-primary user.
  if (observed_profile_ != owner()->profile())
    return;

  AttachControllerToWindowsIfNeeded();

  // Some tasks can be started in background and might have no window until
  // pushed to the front. We need its representation on the shelf to give a user
  // control over it.
  AttachControllerToTask(shelf_app_id, task_id);
}

void ArcAppWindowLauncherController::OnTaskDestroyed(int task_id) {
  auto it = task_id_to_app_window_info_.find(task_id);
  if (it == task_id_to_app_window_info_.end())
    return;
  UnregisterApp(it->second.get(), true);

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  const std::string& shelf_app_id = it->second->shelf_app_id();

  const auto it_controller = app_controller_map_.find(shelf_app_id);
  if (it_controller != app_controller_map_.end()) {
    ArcAppWindowLauncherItemController* controller = it_controller->second;
    controller->RemoveTaskId(task_id);
    if (!controller->window_count()) {
      owner()->CloseLauncherItem(controller->shelf_id());
      app_controller_map_.erase(it_controller);
    }
  }

  task_id_to_app_window_info_.erase(it);
}

void ArcAppWindowLauncherController::OnTaskSetActive(int32_t task_id) {
  if (observed_profile_ != owner()->profile()) {
    active_task_id_ = task_id;
    return;
  }

  AppWindow* previous_app_window = GetAppWindowForTask(active_task_id_);
  if (previous_app_window) {
    owner()->SetItemStatus(previous_app_window->shelf_id(),
                           ash::STATUS_RUNNING);
    previous_app_window->SetFullscreenMode(
        previous_app_window->widget() &&
                previous_app_window->widget()->IsFullscreen()
            ? FullScreenMode::ACTIVE
            : FullScreenMode::NON_ACTIVE);
  }

  active_task_id_ = task_id;

  AppWindow* current_app_window = GetAppWindowForTask(task_id);
  if (current_app_window) {
    owner()->SetItemStatus(
        current_app_window->shelf_id(),
        current_app_window->widget() && current_app_window->IsActive()
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
  // Don't save to AppInfo in prefs because this is requested in runtime.
  AppWindowInfo* info = GetAppWindowInfoForTask(task_id);
  DCHECK(info);
  if (!info)
    return;
  info->set_requested_orientation_lock(orientation_lock);

  if (ash::WmShell::Get()
          ->maximize_mode_controller()
          ->IsMaximizeModeWindowManagerEnabled()) {
    AppWindow* app_window = info->app_window();
    if (app_window)
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

  for (auto& it : task_id_to_app_window_info_) {
    AppWindow* app_window = it.second->app_window();
    if (app_window &&
        app_window->widget() ==
            views::Widget::GetWidgetForNativeWindow(window)) {
      return it.second->app_window()->controller();
    }
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
  for (auto& it : task_id_to_app_window_info_) {
    AppWindow* app_window = it.second->app_window();
    if (app_window)
      SetOrientationLockForAppWindow(app_window);
  }
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

ArcAppWindowLauncherItemController*
ArcAppWindowLauncherController::AttachControllerToTask(
    const std::string& shelf_app_id,
    int task_id) {
  const auto it = app_controller_map_.find(shelf_app_id);
  if (it != app_controller_map_.end()) {
    DCHECK_EQ(it->second->app_id(), shelf_app_id);
    it->second->AddTaskId(task_id);
    return it->second;
  }

  ArcAppWindowLauncherItemController* controller =
      new ArcAppWindowLauncherItemController(shelf_app_id, owner());
  const ash::ShelfID shelf_id =
      shelf_delegate_->GetShelfIDForAppID(shelf_app_id);
  if (!shelf_id) {
    owner()->CreateAppLauncherItem(controller, shelf_app_id,
                                   ash::STATUS_RUNNING);
  } else {
    owner()->SetItemController(shelf_id, controller);
    owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  }
  controller->AddTaskId(task_id);
  app_controller_map_[shelf_app_id] = controller;
  return controller;
}

void ArcAppWindowLauncherController::RegisterApp(
    AppWindowInfo* app_window_info) {
  const std::string shelf_app_id = app_window_info->shelf_app_id();
  DCHECK(!shelf_app_id.empty());
  AppWindow* app_window = app_window_info->app_window();
  ArcAppWindowLauncherItemController* controller =
      AttachControllerToTask(shelf_app_id, app_window->task_id());
  DCHECK(controller);

  const ash::ShelfID shelf_id =
      shelf_delegate_->GetShelfIDForAppID(shelf_app_id);
  DCHECK(shelf_id);

  controller->AddWindow(app_window);
  owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  app_window->SetController(controller);
  app_window->set_shelf_id(shelf_id);
}

void ArcAppWindowLauncherController::UnregisterApp(
    AppWindowInfo* app_window_info,
    bool close_controller) {
  AppWindow* app_window = app_window_info->app_window();
  if (!app_window)
    return;
  const std::string& shelf_app_id = app_window_info->shelf_app_id();
  DCHECK(app_window);
  DCHECK(!shelf_app_id.empty());
  const auto it = app_controller_map_.find(shelf_app_id);
  CHECK(it != app_controller_map_.end());

  ArcAppWindowLauncherItemController* controller = it->second;
  controller->RemoveWindow(app_window);
  if (close_controller && !controller->window_count()) {
    ash::ShelfID shelf_id = app_window->shelf_id();
    owner()->CloseLauncherItem(shelf_id);
    app_controller_map_.erase(it);
  }
  app_window->ResetController();
  app_window_info->set_app_window(nullptr);
}

void ArcAppWindowLauncherController::SetOrientationLockForAppWindow(
    AppWindow* app_window) {
  ash::WmWindow* window =
      ash::WmLookup::Get()->GetWindowForWidget(app_window->widget());
  if (!window)
    return;
  AppWindowInfo* info = GetAppWindowInfoForTask(app_window->task_id());
  arc::mojom::OrientationLock orientation_lock;

  if (info->has_requested_orientation_lock()) {
    orientation_lock = info->requested_orientation_lock();
  } else {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(observed_profile_);
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        prefs->GetApp(info->shelf_app_id());
    if (!app_info)
      return;
    orientation_lock = app_info->orientation_lock;
  }

  if (orientation_lock == arc::mojom::OrientationLock::CURRENT) {
    // Resolve the orientation when it first resolved.
    orientation_lock = GetCurrentOrientation();
    info->set_requested_orientation_lock(orientation_lock);
  }
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->screen_orientation_controller()->LockOrientationForWindow(
      window, BlinkOrientationLockFromMojom(orientation_lock));
}

// static
int ArcAppWindowLauncherController::GetWindowTaskId(aura::Window* window) {
  const std::string arc_app_id = exo::ShellSurface::GetApplicationId(window);
  if (arc_app_id.empty())
    return -1;

  int task_id = -1;
  if (sscanf(arc_app_id.c_str(), "org.chromium.arc.%d", &task_id) != 1)
    return -1;

  return task_id;
}
