// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

class ArcAppWindowLauncherItemController;
class ChromeLauncherController;

class ArcAppWindowLauncherController : public AppWindowLauncherController,
                                       public aura::EnvObserver,
                                       public aura::WindowObserver,
                                       public ArcAppListPrefs::Observer {
 public:
  explicit ArcAppWindowLauncherController(ChromeLauncherController* owner);
  ~ArcAppWindowLauncherController() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // aura::client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // ArcAppListPrefs::Observer:
  void OnTaskCreated(int task_id,
                     const std::string& package_name,
                     const std::string& activity) override;
  void OnTaskDestroyed(int task_id) override;
  void OnTaskSetActive(int32_t task_id) override;

 private:
  class AppWindow;

  using TaskIdToAppWindow = std::map<int, std::unique_ptr<AppWindow>>;
  using AppControllerMap =
      std::map<std::string, ArcAppWindowLauncherItemController*>;

  AppWindow* GetAppWindowForTask(int task_id);

  // AppWindowLauncherController:
  AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) override;

  int active_task_id_ = -1;
  TaskIdToAppWindow task_id_to_app_window_;
  AppControllerMap app_controller_map_;
  ScopedObserver<aura::Window, aura::WindowObserver> observed_windows_;
  // Unowned pointer, represents host Arc window.
  views::Widget* root_widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcAppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_
