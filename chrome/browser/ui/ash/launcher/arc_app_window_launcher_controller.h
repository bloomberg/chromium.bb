// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/common/shell_observer.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace ash {
class ShelfDelegate;
}

namespace aura {
class Window;
}

class ArcAppWindowLauncherItemController;
class ChromeLauncherController;

class Profile;

class ArcAppWindowLauncherController : public AppWindowLauncherController,
                                       public aura::EnvObserver,
                                       public aura::WindowObserver,
                                       public ash::ShellObserver,
                                       public ArcAppListPrefs::Observer {
 public:
  ArcAppWindowLauncherController(ChromeLauncherController* owner,
                                 ash::ShelfDelegate* shelf_delegate);
  ~ArcAppWindowLauncherController() override;

  // Returns shelf app id. Play Store app is mapped to Arc platform host app.
  static std::string GetShelfAppIdFromArcAppId(const std::string& arc_app_id);

  // Returns Arc app id. Arc platform host app is mapped to Play Store app.
  static std::string GetArcAppIdFromShelfAppId(const std::string& shelf_app_id);

  // Returns Arc task id for the window.
  static int GetWindowTaskId(aura::Window* window);

  // AppWindowLauncherController:
  void ActiveUserChanged(const std::string& user_email) override;
  void AdditionalUserAddedToSession(Profile* profile) override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // aura::client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // ash::ShellObserver:
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

  // ArcAppListPrefs::Observer:
  void OnAppReadyChanged(const std::string& app_id, bool ready) override;
  void OnAppRemoved(const std::string& app_id) override;
  void OnTaskCreated(int task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent) override;
  void OnTaskDestroyed(int task_id) override;
  void OnTaskSetActive(int32_t task_id) override;
  void OnTaskOrientationLockRequested(
      int32_t task_id,
      const arc::mojom::OrientationLock orientation_lock) override;

 private:
  class AppWindow;
  class AppWindowInfo;

  using TaskIdToAppWindowInfo = std::map<int, std::unique_ptr<AppWindowInfo>>;
  using ShelfAppIdToAppControllerMap =
      std::map<std::string, ArcAppWindowLauncherItemController*>;

  void StartObserving(Profile* profile);
  void StopObserving(Profile* profile);

  void RegisterApp(AppWindowInfo* app_window_info);
  void UnregisterApp(AppWindowInfo* app_window_info, bool close_controller);

  AppWindowInfo* GetAppWindowInfoForTask(int task_id);
  AppWindow* GetAppWindowForTask(int task_id);

  void AttachControllerToWindowIfNeeded(aura::Window* window);
  void AttachControllerToWindowsIfNeeded();
  ArcAppWindowLauncherItemController* AttachControllerToTask(
      const std::string& shelf_app_id,
      int taskId);

  void SetOrientationLockForAppWindow(AppWindow* app_window);

  // AppWindowLauncherController:
  AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) override;

  // Not owned
  ash::ShelfDelegate* shelf_delegate_;
  int active_task_id_ = -1;
  TaskIdToAppWindowInfo task_id_to_app_window_info_;
  ShelfAppIdToAppControllerMap app_controller_map_;
  std::vector<aura::Window*> observed_windows_;
  Profile* observed_profile_ = nullptr;
  bool observing_shell_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcAppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_
