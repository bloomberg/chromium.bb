// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_H_
#define APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_H_

#include <map>
#include <string>

#include "apps/app_shim/app_shim_handler_mac.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class FilePath;
}

namespace extensions {
class Extension;
}

namespace apps {

// This app shim handler that handles events for app shims that correspond to an
// extension.
class ExtensionAppShimHandler : public AppShimHandler,
                                public content::NotificationObserver {
 public:
  class ProfileManagerFacade {
   public:
    virtual ~ProfileManagerFacade() {}
    virtual bool ProfileExistsForPath(const base::FilePath& path);
    virtual Profile* ProfileForPath(const base::FilePath& path);
  };

  ExtensionAppShimHandler();
  virtual ~ExtensionAppShimHandler();

  // AppShimHandler overrides:
  virtual bool OnShimLaunch(Host* host, AppShimLaunchType launch_type) OVERRIDE;
  virtual void OnShimClose(Host* host) OVERRIDE;
  virtual void OnShimFocus(Host* host) OVERRIDE;
  virtual void OnShimQuit(Host* host) OVERRIDE;

 protected:
  typedef std::map<std::pair<Profile*, std::string>, AppShimHandler::Host*>
      HostMap;

  // Exposed for testing.
  void set_profile_manager_facade(ProfileManagerFacade* profile_manager_facade);
  HostMap& hosts() { return hosts_; }
  content::NotificationRegistrar& registrar() { return registrar_; }

 private:
  virtual bool LaunchApp(Profile* profile,
                         const std::string& app_id,
                         AppShimLaunchType launch_type);

  // Listen to the NOTIFICATION_EXTENSION_HOST_DESTROYED message to detect when
  // an app closes. When that happens, call OnAppClosed on the relevant
  // AppShimHandler::Host which causes the app shim process to quit.
  // The host will call OnShimClose on this.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void StartShim(Profile* profile, const extensions::Extension* extension);

  void CloseShim(Profile* profile, const std::string& app_id);

  scoped_ptr<ProfileManagerFacade> profile_manager_facade_;
  HostMap hosts_;

  content::NotificationRegistrar registrar_;
};

}  // namespace apps

#endif  // APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_H_
