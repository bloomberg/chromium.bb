// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

class ArcAppDeferredLauncherItemController;
class ChromeLauncherController;

// ArcAppDeferredLauncherController displays visual feedback that the ARC
// application the user has just activated is waiting for ARC to be ready, and
// will be asynchronously launched as soon as it can.
class ArcAppDeferredLauncherController
    : public ArcAppListPrefs::Observer,
      public arc::ArcSessionManager::Observer {
 public:
  explicit ArcAppDeferredLauncherController(ChromeLauncherController* owner);
  ~ArcAppDeferredLauncherController() override;

  bool HasApp(const std::string& app_id) const;

  base::TimeDelta GetActiveTime(const std::string& app_id) const;

  // Registers deferred ARC app launch. |app_id| is the app to be launched, and
  // |event_flags| describes the original event flags that triggered the app's
  // activation.
  void RegisterDeferredLaunch(const std::string& app_id,
                              int event_flags,
                              int64_t display_id);

  // Applies spinning effect if requested app is handled by deferred controller.
  void MaybeApplySpinningEffect(const std::string& app_id,
                                gfx::ImageSkia* image);

  // ArcAppListPrefs::Observer:
  void OnAppReadyChanged(const std::string& app_id, bool ready) override;
  void OnAppRemoved(const std::string& app_id) override;

  // arc::ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // Removes entry from the list of tracking items.
  void Remove(const std::string& app_id);

  // Closes Shelf item if it has ArcAppDeferredLauncherItemController controller
  // and removes entry from the list of tracking items.
  void Close(const std::string& app_id);

 private:
  // Defines mapping of a shelf app id to a corresponded controller. Shelf app
  // id is optional mapping (for example, Play Store to ARC Host Support).
  using AppControllerMap =
      std::map<std::string, ArcAppDeferredLauncherItemController*>;

  void UpdateApps();
  void UpdateShelfItemIcon(const std::string& app_id);
  void RegisterNextUpdate();

  // Unowned pointers.
  ChromeLauncherController* owner_;
  Profile* observed_profile_ = nullptr;

  AppControllerMap app_controller_map_;

  // Always keep this the last member of this class.
  base::WeakPtrFactory<ArcAppDeferredLauncherController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppDeferredLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_CONTROLLER_H_
