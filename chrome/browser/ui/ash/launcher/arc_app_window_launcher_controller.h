// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

class ArcAppWindowLauncherItemController;
class ChromeLauncherController;

class ArcAppWindowLauncherController : public AppWindowLauncherController,
                                       public aura::EnvObserver,
                                       public aura::WindowObserver {
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

 private:
  class AppWindow;

  using WindowToAppWindow = std::map<aura::Window*, std::unique_ptr<AppWindow>>;
  using AppControllerMap =
      std::map<std::string, ArcAppWindowLauncherItemController*>;

  AppWindow* GetAppWindowForTask(int task_id);

  void OnGetTaskInfo(int task_id,
                     mojo::String package_name,
                     mojo::String activity_name);

  // AppWindowLauncherController:
  AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) override;

  WindowToAppWindow window_to_app_window_;
  AppControllerMap app_controller_map_;
  ScopedObserver<aura::Window, aura::WindowObserver> observed_windows_;
  base::WeakPtrFactory<ArcAppWindowLauncherController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_
