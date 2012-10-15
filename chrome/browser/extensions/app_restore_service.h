// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_RESTORE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_APP_RESTORE_SERVICE_H_

#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#include <string>

class Profile;

namespace extensions {

class Extension;

// Tracks what apps need to be restarted when the browser restarts.
class AppRestoreService : public ProfileKeyedService,
                          public content::NotificationObserver {
 public:
  explicit AppRestoreService(Profile* profile);

  // Dispatches the app.runtime.onRestarted event to apps that were running in
  // the last session.
  void RestoreApps();

 private:
  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void RecordAppStart(const std::string& extension_id);
  void RecordAppStop(const std::string& extension_id);
  void RestoreApp(const Extension* extension);

  content::NotificationRegistrar registrar_;
  Profile* profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_RESTORE_SERVICE_H_
