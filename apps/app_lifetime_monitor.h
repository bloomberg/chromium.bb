// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_LIFETIME_MONITOR_H_
#define APPS_APP_LIFETIME_MONITOR_H_

#include <string>
#include <vector>

#include "apps/app_window_registry.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {
class Extension;
}

class Profile;

namespace apps {

// Observes startup of apps and their windows and notifies observers of these
// events.
class AppLifetimeMonitor : public KeyedService,
                           public content::NotificationObserver,
                           public AppWindowRegistry::Observer {
 public:
  class Observer {
   public:
    // Called when the app starts running.
    virtual void OnAppStart(Profile* profile, const std::string& app_id) = 0;
    // Called when the app becomes active to the user, i.e. it opens a window.
    virtual void OnAppActivated(Profile* profile,
                                const std::string& app_id) = 0;
    // Called when the app becomes inactive to the user.
    virtual void OnAppDeactivated(Profile* profile,
                                  const std::string& app_id) = 0;
    // Called when the app stops running.
    virtual void OnAppStop(Profile* profile, const std::string& app_id) = 0;
    // Called when chrome is about to terminate. This gives observers a chance
    // to do something before the apps shut down. This is a system-wide event
    // so there is no associated profile and app id.
    virtual void OnChromeTerminating() = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit AppLifetimeMonitor(Profile* profile);
  virtual ~AppLifetimeMonitor();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // AppWindowRegistry::Observer overrides:
  virtual void OnAppWindowRemoved(AppWindow* app_window) OVERRIDE;
  virtual void OnAppWindowHidden(apps::AppWindow* app_window) OVERRIDE;
  virtual void OnAppWindowShown(apps::AppWindow* app_window) OVERRIDE;

  // KeyedService overrides:
  virtual void Shutdown() OVERRIDE;

  bool HasVisibleAppWindows(apps::AppWindow* app_window) const;

  void NotifyAppStart(const std::string& app_id);
  void NotifyAppActivated(const std::string& app_id);
  void NotifyAppDeactivated(const std::string& app_id);
  void NotifyAppStop(const std::string& app_id);
  void NotifyChromeTerminating();

  content::NotificationRegistrar registrar_;
  Profile* profile_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppLifetimeMonitor);
};

}  // namespace apps

#endif  // APPS_APP_LIFETIME_MONITOR_H_
