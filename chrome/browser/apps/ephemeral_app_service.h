// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_EPHEMERAL_APP_SERVICE_H_
#define CHROME_BROWSER_APPS_EPHEMERAL_APP_SERVICE_H_

#include <set>

#include "apps/app_lifetime_monitor.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionRegistry;
}  // namespace extensions

// Delete cached ephemeral apps at startup.
// TODO(benwells): Remove this system.  https://crbug.com/517735.
class EphemeralAppService : public KeyedService,
                            public extensions::ExtensionRegistryObserver,
                            public apps::AppLifetimeMonitor::Observer {
 public:
  // Returns the instance for the given profile. This is a convenience wrapper
  // around EphemeralAppServiceFactory::GetForProfile.
  static EphemeralAppService* Get(Profile* profile);

  explicit EphemeralAppService(Profile* profile);
  ~EphemeralAppService() override;

  // Clears the ephemeral app cache. Removes all idle ephemeral apps.
  void ClearCachedApps();

  void set_disable_delay_for_test(int delay) {
    disable_idle_app_delay_ = delay;
  }

 private:
  // extensions::ExtensionRegistryObserver.
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const extensions::Extension* extension,
                                  bool is_update,
                                  bool from_ephemeral,
                                  const std::string& old_name) override;

  // apps::AppLifetimeMonitor::Observer implementation.
  void OnAppStop(Profile* profile, const std::string& app_id) override;
  void OnChromeTerminating() override;

  void Init();

  void DisableEphemeralApp(const std::string& app_id);

  void HandleEphemeralAppPromoted(const extensions::Extension* app);

  Profile* profile_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;
  ScopedObserver<apps::AppLifetimeMonitor, apps::AppLifetimeMonitor::Observer>
      app_lifetime_monitor_observer_;

  // Number of seconds before disabling idle ephemeral apps.
  // Overridden in tests.
  int disable_idle_app_delay_;

  base::WeakPtrFactory<EphemeralAppService> weak_ptr_factory_;

  friend class EphemeralAppServiceBrowserTest;

  DISALLOW_COPY_AND_ASSIGN(EphemeralAppService);
};

#endif  // CHROME_BROWSER_APPS_EPHEMERAL_APP_SERVICE_H_
