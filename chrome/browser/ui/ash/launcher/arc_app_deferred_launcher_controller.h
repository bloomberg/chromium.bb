// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_CONTROLLER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

class ArcAppDeferredLauncherItemController;
class ChromeLauncherController;

class ArcAppDeferredLauncherController : public ArcAppListPrefs::Observer,
                                         public ArcAppIconLoader::PostEffect {
 public:
  explicit ArcAppDeferredLauncherController(ChromeLauncherController* owner);
  ~ArcAppDeferredLauncherController() override;

  base::TimeDelta GetActiveTime(const std::string& app_id) const;

  // Register deferred Arc app launch.
  void RegisterDeferredLaunch(const std::string& app_id);

  // ArcAppIconLoader::PostEffect:
  void Apply(const std::string& app_id, gfx::ImageSkia* image) override;

  // ArcAppListPrefs::Observer:
  void OnAppReadyChanged(const std::string& app_id, bool ready) override;
  void OnAppRemoved(const std::string& app_id) override;

  // Close Shelf item if it has ArcAppDeferredLauncherItemController controller
  // and remove entry from the list of tracking items.
  void Close(const std::string& app_id);

 private:
  using AppControllerMap =
      std::map<std::string, ArcAppDeferredLauncherItemController*>;

  void UpdateApps();
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
