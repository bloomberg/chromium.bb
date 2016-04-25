// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"

#include "ash/shelf/shelf_util.h"
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

class ArcAppWindowLauncherController::AppWindow : public ui::BaseWindow {
 public:
  AppWindow(int task_id, ArcAppWindowLauncherController* owner)
      : task_id_(task_id), owner_(owner) {}
  ~AppWindow() {}

  void SetController(ArcAppWindowLauncherItemController* controller) {
    DCHECK(!controller_ && controller);
    controller_ = controller;
  }

  int task_id() const { return task_id_; }

  ash::ShelfID shelf_id() const { return shelf_id_; }

  void set_shelf_id(ash::ShelfID shelf_id) { shelf_id_ = shelf_id; }

  ArcAppWindowLauncherItemController* controller() { return controller_; }

  // ui::BaseWindow:
  bool IsActive() const override {
    return owner_->root_widget_ && owner_->root_widget_->IsActive() &&
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
    if (owner_->root_widget_)
      owner_->root_widget_->Activate();
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
  ash::ShelfID shelf_id_ = 0;
  // Unowned pointers
  ArcAppWindowLauncherController* owner_;
  ArcAppWindowLauncherItemController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppWindow);
};

ArcAppWindowLauncherController::ArcAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner), observed_windows_(this) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
  ArcAppListPrefs::Get(owner->profile())->AddObserver(this);
}

ArcAppWindowLauncherController::~ArcAppWindowLauncherController() {
  ArcAppListPrefs::Get(owner()->profile())->RemoveObserver(this);
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);
}

void ArcAppWindowLauncherController::OnWindowInitialized(aura::Window* window) {
  DCHECK(!observed_windows_.IsObserving(window));

  // Root Arc window has type WINDOW_TYPE_NORMAL.
  if (window->type() != ui::wm::WINDOW_TYPE_NORMAL)
    return;

  observed_windows_.Add(window);
}

void ArcAppWindowLauncherController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  const std::string app_id = exo::ShellSurface::GetApplicationId(window);
  if (app_id.empty())
    return;

  int task_id = 0;
  if (sscanf(app_id.c_str(), "org.chromium.arc.%d", &task_id) != 1 ||
      task_id != 0)
    return;

  DCHECK(!root_widget_);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);

  DCHECK(widget);
  root_widget_ = widget;
}

void ArcAppWindowLauncherController::OnWindowDestroying(aura::Window* window) {
  DCHECK(observed_windows_.IsObserving(window));
  observed_windows_.Remove(window);

  if (!root_widget_ || root_widget_->GetNativeWindow() != window)
    return;

  root_widget_ = nullptr;
}

ArcAppWindowLauncherController::AppWindow*
ArcAppWindowLauncherController::GetAppWindowForTask(int task_id) {
  TaskIdToAppWindow::iterator it = task_id_to_app_window_.find(task_id);
  if (it == task_id_to_app_window_.end())
    return nullptr;
  return it->second.get();
}

void ArcAppWindowLauncherController::OnTaskCreated(
    int task_id,
    const std::string& package_name,
    const std::string& activity_name) {
  DCHECK(!GetAppWindowForTask(task_id));

  std::unique_ptr<AppWindow> app_window(new AppWindow(task_id, this));

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
  }

  active_task_id_ = task_id;

  TaskIdToAppWindow::iterator new_active_app_it =
      task_id_to_app_window_.find(active_task_id_);
  if (new_active_app_it != task_id_to_app_window_.end()) {
    owner()->SetItemStatus(new_active_app_it->second->shelf_id(),
                           root_widget_ && root_widget_->IsActive()
                               ? ash::STATUS_ACTIVE
                               : ash::STATUS_RUNNING);
  }
}

AppWindowLauncherItemController*
ArcAppWindowLauncherController::ControllerForWindow(aura::Window* window) {
  if (!root_widget_ || root_widget_->GetNativeWindow() != window)
    return nullptr;

  AppWindow* app_window = GetAppWindowForTask(active_task_id_);
  if (!app_window)
    return nullptr;

  return app_window->controller();
}

void ArcAppWindowLauncherController::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  OnTaskSetActive(active_task_id_);
}
