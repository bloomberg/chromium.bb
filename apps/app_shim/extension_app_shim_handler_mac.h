// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_MAC_H_
#define APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_MAC_H_

#include <map>
#include <string>
#include <vector>

#include "apps/app_lifetime_monitor.h"
#include "apps/app_shim/app_shim_handler_mac.h"
#include "apps/app_window_registry.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
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

class AppWindow;

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
    virtual void LoadProfileAsync(const base::FilePath& path,
                                  base::Callback<void(Profile*)> callback);

    virtual AppWindowRegistry::AppWindowList GetWindows(
        Profile* profile,
        const std::string& extension_id);

    virtual const extensions::Extension* GetAppExtension(
        Profile* profile, const std::string& extension_id);
    virtual void EnableExtension(Profile* profile,
                                 const std::string& extension_id,
                                 const base::Callback<void()>& callback);
    virtual void LaunchApp(Profile* profile,
                           const extensions::Extension* extension,
                           const std::vector<base::FilePath>& files);
    virtual void LaunchShim(Profile* profile,
                            const extensions::Extension* extension);

    virtual void MaybeTerminate();
  };

  ExtensionAppShimHandler();
  virtual ~ExtensionAppShimHandler();

  AppShimHandler::Host* FindHost(Profile* profile, const std::string& app_id);

  static void QuitAppForWindow(AppWindow* app_window);

  static void HideAppForWindow(AppWindow* app_window);

  static void FocusAppForWindow(AppWindow* app_window);

  // Brings the window to the front without showing it and instructs the shim to
  // request user attention. Returns false if there is no shim for this window.
  static bool RequestUserAttentionForWindow(AppWindow* app_window);

  // Called by AppControllerMac when Chrome hides.
  static void OnChromeWillHide();

  // AppShimHandler overrides:
  virtual void OnShimLaunch(Host* host,
                            AppShimLaunchType launch_type,
                            const std::vector<base::FilePath>& files) OVERRIDE;
  virtual void OnShimClose(Host* host) OVERRIDE;
  virtual void OnShimFocus(Host* host,
                           AppShimFocusType focus_type,
                           const std::vector<base::FilePath>& files) OVERRIDE;
  virtual void OnShimSetHidden(Host* host, bool hidden) OVERRIDE;
  virtual void OnShimQuit(Host* host) OVERRIDE;

  // AppLifetimeMonitor::Observer overrides:
  virtual void OnAppStart(Profile* profile, const std::string& app_id) OVERRIDE;
  virtual void OnAppActivated(Profile* profile,
                              const std::string& app_id) OVERRIDE;
  virtual void OnAppDeactivated(Profile* profile,
                                const std::string& app_id) OVERRIDE;
  virtual void OnAppStop(Profile* profile, const std::string& app_id) OVERRIDE;
  virtual void OnChromeTerminating() OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  typedef std::map<std::pair<Profile*, std::string>, AppShimHandler::Host*>
      HostMap;

  // Exposed for testing.
  void set_delegate(Delegate* delegate);
  HostMap& hosts() { return hosts_; }
  content::NotificationRegistrar& registrar() { return registrar_; }

 private:
  // Helper function to get the instance on the browser process.
  static ExtensionAppShimHandler* GetInstance();

  // This is passed to Delegate::LoadProfileAsync for shim-initiated launches
  // where the profile was not yet loaded.
  void OnProfileLoaded(Host* host,
                       AppShimLaunchType launch_type,
                       const std::vector<base::FilePath>& files,
                       Profile* profile);

  // This is passed to Delegate::EnableViaPrompt for shim-initiated launches
  // where the extension is disabled.
  void OnExtensionEnabled(const base::FilePath& profile_path,
                          const std::string& app_id,
                          const std::vector<base::FilePath>& files);

  scoped_ptr<Delegate> delegate_;

  HostMap hosts_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ExtensionAppShimHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppShimHandler);
};

}  // namespace apps

#endif  // APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_MAC_H_
