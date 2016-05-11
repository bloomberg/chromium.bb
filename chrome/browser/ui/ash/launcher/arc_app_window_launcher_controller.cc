// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"

#include "ash/shelf/shelf_util.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "components/arc/arc_bridge_service.h"
#include "components/exo/shell_surface.h"
#include "ui/aura/env.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"

namespace {

enum class FullScreenMode {
  NOT_DEFINED,  // Fullscreen mode was not defined.
  ACTIVE,       // Fullscreen is activated for an app.
  NON_ACTIVE,   // Fullscreen was not activated for an app.
};
}

class ArcAppWindowLauncherController::AppWindow : public ui::BaseWindow {
 public:
  AppWindow(int task_id,
            std::string package_name,
            ArcAppWindowLauncherController* owner)
      : task_id_(task_id), package_name_(package_name), owner_(owner) {}
  ~AppWindow() {}

  void SetController(ArcAppWindowLauncherItemController* controller) {
    DCHECK(!controller_ && controller);
    controller_ = controller;
  }

  void SetFullscreenMode(FullScreenMode mode) {
    DCHECK(mode != FullScreenMode::NOT_DEFINED);
    fullscreen_mode_ = mode;
  }

  FullScreenMode fullscreen_mode() const { return fullscreen_mode_; }

  int task_id() const { return task_id_; }

  const std::string& package_name() const { return package_name_; }

  ash::ShelfID shelf_id() const { return shelf_id_; }

  void set_shelf_id(ash::ShelfID shelf_id) { shelf_id_ = shelf_id; }

  views::Widget* widget() const { return widget_; }

  void set_widget(views::Widget* widget) { widget_ = widget; }

  ArcAppWindowLauncherItemController* controller() { return controller_; }

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
    // TODO(khmel): support window minimizing.
  }

  void Restore() override { NOTREACHED(); }

  void SetBounds(const gfx::Rect& bounds) override { NOTREACHED(); }

  void FlashFrame(bool flash) override { NOTREACHED(); }

  bool IsAlwaysOnTop() const override {
    NOTREACHED();
    return false;
  }

  void SetAlwaysOnTop(bool always_on_top) override { NOTREACHED(); }

 private:
  arc::mojom::AppInstance* GetAppInstance() {
    arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
    arc::mojom::AppInstance* app_instance =
        bridge_service ? bridge_service->app_instance() : nullptr;
    if (!app_instance) {
      VLOG(2) << "Arc Bridge is not available.";
      return nullptr;
    }

    if (bridge_service->app_version() < 3) {
      VLOG(2) << "Arc Bridge has old version for apps."
              << bridge_service->app_version();
      return nullptr;
    }
    return app_instance;
  }

  int task_id_;
  std::string package_name_;
  ash::ShelfID shelf_id_ = 0;
  FullScreenMode fullscreen_mode_ = FullScreenMode::NOT_DEFINED;
  // Unowned pointers
  ArcAppWindowLauncherController* owner_;
  ArcAppWindowLauncherItemController* controller_ = nullptr;
  // Unowned pointer, represents host Arc window.
  views::Widget* widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppWindow);
};

ArcAppWindowLauncherController::ArcAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
  ArcAppListPrefs::Get(owner->profile())->AddObserver(this);
}

ArcAppWindowLauncherController::~ArcAppWindowLauncherController() {
  for (auto window : observed_windows_)
    window->RemoveObserver(this);
  ArcAppListPrefs::Get(owner()->profile())->RemoveObserver(this);
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);
}

void ArcAppWindowLauncherController::OnWindowInitialized(aura::Window* window) {
  // Arc windows has type WINDOW_TYPE_NORMAL.
  if (window->type() != ui::wm::WINDOW_TYPE_NORMAL)
    return;
  observed_windows_.push_back(window);
  window->AddObserver(this);
  CheckForAppWindowWidget(window);
}

void ArcAppWindowLauncherController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
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
  // exo::ShellSurface::GetApplicationId() will return the package name
  // if available and org.chromium.arc.TASKID otherwise. org.chromium.arc.0
  // is the default window and never used by real applications, only by
  // system dialogs and effects.
  const std::string app_id = exo::ShellSurface::GetApplicationId(window);
  if (app_id.empty())
    return;

  // Check if we have any app windows with matching package names.
  // TODO(reveman): Revisit this if we need to have different icons for
  // different sub intents.
  for (auto& it : task_id_to_app_window_) {
    if (it.second->package_name() != app_id)
      continue;
    it.second->set_widget(views::Widget::GetWidgetForNativeWindow(window));
  }

  int task_id = -1;
  if (sscanf(app_id.c_str(), "org.chromium.arc.%d", &task_id) != 1)
    return;

  if (!task_id) {
    // task_id=0 is the default window. It will not contain any real
    // apps so best if it's ignored by the shelf for purposes of darkening.
    ash::wm::GetWindowState(window)->set_ignored_by_shelf(true);
    return;
  }

  AppWindow* app_window = GetAppWindowForTask(task_id);
  if (app_window)
    app_window->set_widget(views::Widget::GetWidgetForNativeWindow(window));
}

void ArcAppWindowLauncherController::OnTaskCreated(
    int task_id,
    const std::string& package_name,
    const std::string& activity_name) {
  DCHECK(!GetAppWindowForTask(task_id));

  std::unique_ptr<AppWindow> app_window(
      new AppWindow(task_id, package_name, this));

  const std::string app_id =
      ArcAppListPrefs::GetAppId(package_name, activity_name);

  ArcAppWindowLauncherItemController* controller;
  AppControllerMap::iterator it = app_controller_map_.find(app_id);
  ash::ShelfID shelf_id = 0;
  if (it != app_controller_map_.end()) {
    controller = it->second;
    DCHECK_EQ(controller->app_id(), app_id);
    shelf_id = controller->shelf_id();
  } else {
    controller = new ArcAppWindowLauncherItemController(app_id, owner());
    shelf_id = owner()->GetShelfIDForAppID(app_id);
    if (shelf_id == 0) {
      shelf_id = owner()->CreateAppLauncherItem(controller, app_id,
                                                ash::STATUS_RUNNING);
    } else {
      owner()->SetItemController(shelf_id, controller);
    }
    app_controller_map_[app_id] = controller;
  }
  controller->AddWindow(app_window.get());
  owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  app_window->SetController(controller);
  app_window->set_shelf_id(shelf_id);

  task_id_to_app_window_[task_id] = std::move(app_window);

  for (auto window : observed_windows_)
    CheckForAppWindowWidget(window);
}

void ArcAppWindowLauncherController::OnTaskDestroyed(int task_id) {
  TaskIdToAppWindow::iterator it = task_id_to_app_window_.find(task_id);
  if (it == task_id_to_app_window_.end())
    return;

  AppWindow* app_window = it->second.get();
  ArcAppWindowLauncherItemController* controller = app_window->controller();
  DCHECK(controller);
  const std::string app_id = controller->app_id();
  controller->RemoveWindow(app_window);
  if (!controller->window_count()) {
    ash::ShelfID shelf_id = app_window->shelf_id();
    DCHECK(shelf_id);
    owner()->CloseLauncherItem(shelf_id);
    AppControllerMap::iterator it2 = app_controller_map_.find(app_id);
    DCHECK(it2 != app_controller_map_.end());
    app_controller_map_.erase(it2);
  }

  task_id_to_app_window_.erase(it);
}

void ArcAppWindowLauncherController::OnTaskSetActive(int32_t task_id) {
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
    // Activate Arc widget if active task has been changed. This can be
    // due creating of the new Arc app or bringing an existing app to the front.
    if (new_active_app_it != previous_active_app_it &&
        new_active_app_it->second->widget()) {
      new_active_app_it->second->widget()->Activate();
    }

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
