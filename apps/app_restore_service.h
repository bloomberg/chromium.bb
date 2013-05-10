// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_RESTORE_SERVICE_H_
#define APPS_APP_RESTORE_SERVICE_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {
class Extension;

namespace app_file_handler_util {
struct SavedFileEntry;
}

}

class Profile;

using extensions::app_file_handler_util::SavedFileEntry;

namespace apps {

// Tracks what apps need to be restarted when the browser restarts.
class AppRestoreService : public ProfileKeyedService,
                          public content::NotificationObserver,
                          public extensions::ShellWindowRegistry::Observer {
 public:
  // Returns true if apps should be restored on the current platform, given
  // whether this new browser process launched due to a restart.
  static bool ShouldRestoreApps(bool is_browser_restart);

  explicit AppRestoreService(Profile* profile);

  // Restart apps that need to be restarted and clear the "running" preference
  // from apps to prevent them being restarted in subsequent restarts.
  void HandleStartup(bool should_restore_apps);

 private:
  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // extensions::ShellWindowRegistry::Observer.
  virtual void OnShellWindowAdded(ShellWindow* shell_window) OVERRIDE;
  virtual void OnShellWindowIconChanged(ShellWindow* shell_window) OVERRIDE;
  virtual void OnShellWindowRemoved(ShellWindow* shell_window) OVERRIDE;

  // ProfileKeyedService.
  virtual void Shutdown() OVERRIDE;

  void RecordAppStart(const std::string& extension_id);
  void RecordAppStop(const std::string& extension_id);
  void RecordIfAppHasWindows(const std::string& id);

  void RestoreApp(
      const extensions::Extension* extension,
      const std::vector<SavedFileEntry>& file_entries);

  content::NotificationRegistrar registrar_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AppRestoreService);
};

}  // namespace apps

#endif  // APPS_APP_RESTORE_SERVICE_H_
