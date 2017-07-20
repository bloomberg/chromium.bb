// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

class ArcAppWindow;
class ArcAppWindowLauncherItemController;
class ChromeLauncherController;

class Profile;

class ArcAppWindowLauncherController : public AppWindowLauncherController,
                                       public aura::EnvObserver,
                                       public aura::WindowObserver,
                                       public ash::TabletModeObserver,
                                       public ArcAppListPrefs::Observer,
                                       public arc::ArcSessionManager::Observer {
 public:
  explicit ArcAppWindowLauncherController(ChromeLauncherController* owner);
  ~ArcAppWindowLauncherController() override;

  // Returns ARC task id for the window.
  static int GetWindowTaskId(aura::Window* window);

  // AppWindowLauncherController:
  void ActiveUserChanged(const std::string& user_email) override;
  void AdditionalUserAddedToSession(Profile* profile) override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ash::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ArcAppListPrefs::Observer:
  void OnAppReadyChanged(const std::string& app_id, bool ready) override;
  void OnAppRemoved(const std::string& app_id) override;
  void OnTaskCreated(int task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent) override;
  void OnTaskDescriptionUpdated(
      int32_t task_id,
      const std::string& label,
      const std::vector<uint8_t>& icon_png_data) override;
  void OnTaskDestroyed(int task_id) override;
  void OnTaskOrientationLockRequested(
      int32_t task_id,
      const arc::mojom::OrientationLock orientation_lock) override;
  void OnTaskSetActive(int32_t task_id) override;

  int active_task_id() const { return active_task_id_; }

 private:
  class AppWindowInfo;

  using TaskIdToAppWindowInfo = std::map<int, std::unique_ptr<AppWindowInfo>>;

  // Maps shelf group id to controller. Shelf group id is optional parameter for
  // the Android task. If it is not set, app id is used instead.
  using ShelfGroupToAppControllerMap =
      std::map<arc::ArcAppShelfId, ArcAppWindowLauncherItemController*>;

  void StartObserving(Profile* profile);
  void StopObserving(Profile* profile);

  void RegisterApp(AppWindowInfo* app_window_info);
  void UnregisterApp(AppWindowInfo* app_window_info);

  AppWindowInfo* GetAppWindowInfoForTask(int task_id);
  ArcAppWindow* GetAppWindowForTask(int task_id);

  void AttachControllerToWindowIfNeeded(aura::Window* window);
  void AttachControllerToWindowsIfNeeded();
  ArcAppWindowLauncherItemController* AttachControllerToTask(
      int taskId,
      const AppWindowInfo& app_window_info);

  void SetOrientationLockForAppWindow(ArcAppWindow* app_window);

  std::vector<int> GetTaskIdsForApp(const std::string& arc_app_id) const;

  // AppWindowLauncherController:
  AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) override;

  // arc::ArcSessionManager::Observer:
  void OnArcOptInManagementCheckStarted() override;
  void OnArcSessionStopped(arc::ArcStopReason stop_reason) override;

  int active_task_id_ = -1;
  TaskIdToAppWindowInfo task_id_to_app_window_info_;
  ShelfGroupToAppControllerMap app_shelf_group_to_controller_map_;
  std::vector<aura::Window*> observed_windows_;
  Profile* observed_profile_ = nullptr;
  bool observing_shell_ = false;

  // The time when the ARC OptIn management check was started. This happens
  // right after user agrees the ToS or in some cases for managed user when ARC
  // starts for the first time. OptIn management check is preceding step before
  // ARC container is actually started.
  base::Time opt_in_management_check_start_time_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_LAUNCHER_CONTROLLER_H_
