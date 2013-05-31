// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_LOAD_SERVICE_H_
#define APPS_APP_LOAD_SERVICE_H_

#include <map>
#include <string>

#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace apps {

// Monitors apps being reloaded and performs app specific actions (like launch
// or restart) on them. Also provides an interface to schedule these actions.
class AppLoadService : public BrowserContextKeyedService,
                       public content::NotificationObserver {
 public:
  explicit AppLoadService(Profile* profile);
  virtual ~AppLoadService();

  // Reload the application with the given id and then send it the OnRestarted
  // event.
  void RestartApplication(const std::string& extension_id);

  // Schedule a launch to happen for the extension with id |extension_id|
  // when it loads next. This does not cause the extension to be reloaded.
  void ScheduleLaunchOnLoad(const std::string& extension_id);

  static AppLoadService* Get(Profile* profile);

 private:
  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Map of extension id to reload action. Absence from the map implies
  // no action.
  std::map<std::string, int> reload_actions_;
  content::NotificationRegistrar registrar_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AppLoadService);
};

}  // namespace apps

#endif  // APPS_APP_LOAD_SERVICE_H_
