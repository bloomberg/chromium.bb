// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_EPHEMERAL_APP_SERVICE_H_
#define CHROME_BROWSER_APPS_EPHEMERAL_APP_SERVICE_H_

#include <map>
#include <set>

#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionRegistry;
}  // namespace extensions

// Performs the background garbage collection of ephemeral apps.
class EphemeralAppService : public KeyedService,
                            public content::NotificationObserver,
                            public extensions::ExtensionRegistryObserver {
 public:
  // Returns the instance for the given profile. This is a convenience wrapper
  // around EphemeralAppServiceFactory::GetForProfile.
  static EphemeralAppService* Get(Profile* profile);

  explicit EphemeralAppService(Profile* profile);
  virtual ~EphemeralAppService();

  // Clears the ephemeral app cache. Removes all idle ephemeral apps.
  void ClearCachedApps();

  int ephemeral_app_count() const { return ephemeral_app_count_; }

  // Constants exposed for testing purposes:

  // The number of days of inactivity before an ephemeral app will be removed.
  static const int kAppInactiveThreshold;
  // If the ephemeral app has been launched within this number of days, it will
  // definitely not be garbage collected.
  static const int kAppKeepThreshold;
  // The maximum number of ephemeral apps to keep cached. Excess may be removed.
  static const int kMaxEphemeralAppsCount;

 private:
  // A map used to order the ephemeral apps by their last launch time.
  typedef std::multimap<base::Time, std::string> LaunchTimeAppMap;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // extensions::ExtensionRegistryObserver.
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  void Init();
  void InitEphemeralAppCount();

  // Garbage collect ephemeral apps.
  void TriggerGarbageCollect(const base::TimeDelta& delay);
  void GarbageCollectApps();
  static void GetAppsToRemove(int app_count,
                              const LaunchTimeAppMap& app_launch_times,
                              std::set<std::string>* remove_app_ids);

  Profile* profile_;

  content::NotificationRegistrar registrar_;
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  base::OneShotTimer<EphemeralAppService> garbage_collect_apps_timer_;

  // The count of cached ephemeral apps.
  int ephemeral_app_count_;

  friend class EphemeralAppServiceTest;
  friend class EphemeralAppServiceBrowserTest;

  DISALLOW_COPY_AND_ASSIGN(EphemeralAppService);
};

#endif  // CHROME_BROWSER_APPS_EPHEMERAL_APP_SERVICE_H_
