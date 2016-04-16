// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"

#include "ash/shelf/shelf_util.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "components/arc/arc_bridge_service.h"
#include "components/exo/shell_surface.h"
#include "ui/aura/env.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"

class ArcAppWindowLauncherController::AppWindow : public ui::BaseWindow {
 public:
  AppWindow(int task_id, views::Widget* widget)
      : task_id_(task_id), widget_(widget) {}
  ~AppWindow() {}

  void SetController(ArcAppWindowLauncherItemController* controller) {
    DCHECK(!controller_ && controller);
    controller_ = controller;
  }

  int task_id() const { return task_id_; }

  ArcAppWindowLauncherItemController* controller() { return controller_; }

  // ui::BaseWindow:
  bool IsActive() const override { return widget_->IsActive(); }

  bool IsMaximized() const override { return widget_->IsMaximized(); }

  bool IsMinimized() const override { return widget_->IsMinimized(); }

  bool IsFullscreen() const override { return widget_->IsFullscreen(); }

  gfx::NativeWindow GetNativeWindow() const override {
    return widget_->GetNativeWindow();
  }

  gfx::Rect GetRestoredBounds() const override {
    return widget_->GetRestoredBounds();
  }

  ui::WindowShowState GetRestoredState() const override {
    // Stub implementation. See also ChromeNativeAppWindowViews.
    if (IsMaximized())
      return ui::SHOW_STATE_MAXIMIZED;
    if (IsFullscreen())
      return ui::SHOW_STATE_FULLSCREEN;
    return ui::SHOW_STATE_NORMAL;
  }

  gfx::Rect GetBounds() const override {
    return widget_->GetWindowBoundsInScreen();
  }

  void Show() override {
    if (widget_->IsVisible()) {
      widget_->Activate();
      return;
    }
    widget_->Show();
  }

  void ShowInactive() override {
    if (widget_->IsVisible())
      return;

    widget_->ShowInactive();
  }

  void Hide() override { widget_->Hide(); }

  void Close() override { widget_->Close(); }

  void Activate() override { widget_->Activate(); }

  void Deactivate() override { widget_->Deactivate(); }

  void Maximize() override { widget_->Maximize(); }

  void Minimize() override { widget_->Minimize(); }

  void Restore() override { widget_->Restore(); }

  void SetBounds(const gfx::Rect& bounds) override {
    widget_->SetBounds(bounds);
  }

  void FlashFrame(bool flash) override { widget_->FlashFrame(flash); }

  bool IsAlwaysOnTop() const override { return widget_->IsAlwaysOnTop(); }

  void SetAlwaysOnTop(bool always_on_top) override {
    widget_->SetAlwaysOnTop(always_on_top);
  }

 private:
  int task_id_;
  // Unowned pointers
  views::Widget* widget_;
  ArcAppWindowLauncherItemController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppWindow);
};

ArcAppWindowLauncherController::ArcAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner),
      observed_windows_(this),
      weak_ptr_factory_(this) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
}

ArcAppWindowLauncherController::~ArcAppWindowLauncherController() {
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
  if (window_to_app_window_.find(window) != window_to_app_window_.end())
    return;

  const std::string app_id = exo::ShellSurface::GetApplicationId(window);
  if (app_id.empty())
    return;

  int task_id = 0;
  if (sscanf(app_id.c_str(), "org.chromium.arc.%d", &task_id) != 1 || !task_id)
    return;

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  window_to_app_window_[window].reset(new AppWindow(task_id, widget));

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    NOTREACHED();
    return;
  }

  arc::mojom::AppInstance* app_instance = bridge_service->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to resolve task when bridge service is not ready.";
    return;
  }

  // Resolve information about task.
  app_instance->GetTaskInfo(
      task_id, base::Bind(&ArcAppWindowLauncherController::OnGetTaskInfo,
                          weak_ptr_factory_.GetWeakPtr(), task_id));
}

void ArcAppWindowLauncherController::OnWindowDestroying(aura::Window* window) {
  DCHECK(observed_windows_.IsObserving(window));
  observed_windows_.Remove(window);

  WindowToAppWindow::iterator it = window_to_app_window_.find(window);
  if (it == window_to_app_window_.end())
    return;

  ArcAppWindowLauncherItemController* controller = it->second->controller();
  if (controller) {
    const std::string app_id = controller->app_id();
    controller->RemoveWindowForNativeWindow(window);
    if (!controller->window_count()) {
      ash::ShelfID shelf_id = ash::GetShelfIDForWindow(window);
      DCHECK(shelf_id);
      owner()->CloseLauncherItem(shelf_id);
      AppControllerMap::iterator it2 = app_controller_map_.find(app_id);
      DCHECK(it2 != app_controller_map_.end());
      app_controller_map_.erase(it2);
    }
  }
  window_to_app_window_.erase(it);
}

ArcAppWindowLauncherController::AppWindow*
ArcAppWindowLauncherController::GetAppWindowForTask(int task_id) {
  for (auto& it : window_to_app_window_) {
    if (it.second->task_id() == task_id)
      return it.second.get();
  }
  return nullptr;
}

void ArcAppWindowLauncherController::OnGetTaskInfo(int task_id,
                                                   mojo::String package_name,
                                                   mojo::String activity_name) {
  if (package_name.get().empty() || activity_name.get().empty())
    return;

  AppWindow* app_window = GetAppWindowForTask(task_id);
  if (!app_window)
    return;

  DCHECK(app_window && !app_window->controller());
  ash::ShelfItemStatus status =
      ash::wm::IsActiveWindow(app_window->GetNativeWindow())
          ? ash::STATUS_ACTIVE
          : ash::STATUS_RUNNING;

  const std::string app_id =
      ArcAppListPrefs::GetAppId(package_name, activity_name);

  ArcAppWindowLauncherItemController* controller;
  AppControllerMap::iterator it = app_controller_map_.find(app_id);
  ash::ShelfID shelf_id = 0;
  if (it != app_controller_map_.end()) {
    controller = it->second;
    DCHECK(controller->app_id() == app_id);
    shelf_id = controller->shelf_id();
    controller->AddWindow(app_window);
  } else {
    controller = new ArcAppWindowLauncherItemController(app_id, owner());
    controller->AddWindow(app_window);
    shelf_id = owner()->GetShelfIDForAppID(app_id);
    if (shelf_id == 0)
      shelf_id = owner()->CreateAppLauncherItem(controller, app_id, status);
    else
      owner()->SetItemController(shelf_id, controller);
    app_controller_map_[app_id] = controller;
  }
  app_window->SetController(controller);
  owner()->SetItemStatus(shelf_id, status);
  ash::SetShelfIDForWindow(shelf_id, app_window->GetNativeWindow());
}

AppWindowLauncherItemController*
ArcAppWindowLauncherController::ControllerForWindow(aura::Window* window) {
  WindowToAppWindow::iterator it = window_to_app_window_.find(window);
  if (it == window_to_app_window_.end())
    return nullptr;
  return it->second->controller();
}
