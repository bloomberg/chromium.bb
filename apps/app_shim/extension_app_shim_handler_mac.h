// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_H_
#define APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_H_

#include <map>
#include <string>

#include "apps/app_lifetime_monitor.h"
#include "apps/app_shim/app_shim_handler_mac.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace apps {

// This app shim handler that handles events for app shims that correspond to an
// extension.
class ExtensionAppShimHandler : public AppShimHandler,
                                public content::NotificationObserver,
                                public AppLifetimeMonitor::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual bool ProfileExistsForPath(const base::FilePath& path);
    virtual Profile* ProfileForPath(const base::FilePath& path);

    virtual extensions::ShellWindowRegistry::ShellWindowList GetWindows(
        Profile* profile, const std::string& extension_id);

    virtual const extensions::Extension* GetAppExtension(
        Profile* profile, const std::string& extension_id);
    virtual void LaunchApp(Profile* profile,
                           const extensions::Extension* extension);
    virtual void LaunchShim(Profile* profile,
                            const extensions::Extension* extension);

  };

  ExtensionAppShimHandler();
  virtual ~ExtensionAppShimHandler();

  // AppShimHandler overrides:
  virtual bool OnShimLaunch(Host* host, AppShimLaunchType launch_type) OVERRIDE;
  virtual void OnShimClose(Host* host) OVERRIDE;
  virtual void OnShimFocus(Host* host) OVERRIDE;
  virtual void OnShimQuit(Host* host) OVERRIDE;

  // AppLifetimeMonitor::Observer overrides:
  virtual void OnAppStart(Profile* profile, const std::string& app_id) OVERRIDE;
  virtual void OnAppActivated(Profile* profile,
                              const std::string& app_id) OVERRIDE;
  virtual void OnAppDeactivated(Profile* profile,
                                const std::string& app_id) OVERRIDE;
  virtual void OnAppStop(Profile* profile, const std::string& app_id) OVERRIDE;
  virtual void OnChromeTerminating() OVERRIDE;

 protected:
  typedef std::map<std::pair<Profile*, std::string>, AppShimHandler::Host*>
      HostMap;

  // Exposed for testing.
  void set_delegate(Delegate* delegate);
  HostMap& hosts() { return hosts_; }
  content::NotificationRegistrar& registrar() { return registrar_; }

 private:
  // Listen to the NOTIFICATION_EXTENSION_HOST_DESTROYED message to detect when
  // an app closes. When that happens, call OnAppClosed on the relevant
  // AppShimHandler::Host which causes the app shim process to quit.
  // The host will call OnShimClose on this.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  scoped_ptr<Delegate> delegate_;

  HostMap hosts_;

  content::NotificationRegistrar registrar_;
};

}  // namespace apps

#endif  // APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_H_
