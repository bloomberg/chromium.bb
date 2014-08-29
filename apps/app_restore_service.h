// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_RESTORE_SERVICE_H_
#define APPS_APP_RESTORE_SERVICE_H_

#include <string>
#include <vector>

#include "apps/app_lifetime_monitor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace extensions {
class Extension;
}

class Profile;

namespace apps {

// Tracks what apps need to be restarted when the browser restarts.
class AppRestoreService : public KeyedService,
                          public AppLifetimeMonitor::Observer {
 public:
  // Returns true if apps should be restored on the current platform, given
  // whether this new browser process launched due to a restart.
  static bool ShouldRestoreApps(bool is_browser_restart);

  explicit AppRestoreService(Profile* profile);

  // Restart apps that need to be restarted and clear the "running" preference
  // from apps to prevent them being restarted in subsequent restarts.
  void HandleStartup(bool should_restore_apps);

  // Returns whether this extension is running or, at startup, whether it was
  // running when Chrome was last terminated.
  bool IsAppRestorable(const std::string& extension_id);

  static AppRestoreService* Get(Profile* profile);

 private:
  // AppLifetimeMonitor::Observer.
  virtual void OnAppStart(Profile* profile, const std::string& app_id) OVERRIDE;
  virtual void OnAppActivated(Profile* profile,
                              const std::string& app_id) OVERRIDE;
  virtual void OnAppDeactivated(Profile* profile,
                                const std::string& app_id) OVERRIDE;
  virtual void OnAppStop(Profile* profile, const std::string& app_id) OVERRIDE;
  virtual void OnChromeTerminating() OVERRIDE;

  // KeyedService.
  virtual void Shutdown() OVERRIDE;

  void RecordAppStart(const std::string& extension_id);
  void RecordAppStop(const std::string& extension_id);
  void RecordAppActiveState(const std::string& id, bool is_active);

  void RestoreApp(const extensions::Extension* extension);

  void StartObservingAppLifetime();
  void StopObservingAppLifetime();

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AppRestoreService);
};

}  // namespace apps

#endif  // APPS_APP_RESTORE_SERVICE_H_
