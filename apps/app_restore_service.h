// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_RESTORE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_APP_RESTORE_SERVICE_H_

#include <string>

#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {
class Extension;
}

namespace apps {

// Tracks what apps need to be restarted when the browser restarts.
class AppRestoreService : public ProfileKeyedService,
                          public content::NotificationObserver {
 public:
  explicit AppRestoreService(Profile* profile);

  // Restart apps that need to be restarted and clear the "running" preference
  // from apps to prevent them being restarted in subsequent restarts.
  void HandleStartup(bool should_restore_apps);

 private:
  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void RecordAppStart(const std::string& extension_id);
  void RecordAppStop(const std::string& extension_id);
  void RestoreApp(const extensions::Extension* extension);

  content::NotificationRegistrar registrar_;
  Profile* profile_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_EXTENSIONS_APP_RESTORE_SERVICE_H_
