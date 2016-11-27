// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_PROVIDER_H_
#define CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_PROVIDER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "components/drive/drive_app_registry_observer.h"
#include "extensions/browser/extension_registry_observer.h"

namespace drive {
struct DriveAppInfo;
}

class BrowserContextKeyedServiceFactory;
class DriveAppConverter;
class DriveAppMapping;
class DriveAppUninstallSyncService;
class DriveServiceBridge;
class Profile;

// DriveAppProvider is the integration point for Drive apps. It ensures each
// Drive app has a corresponding Chrome app in the extension system. If there
// is no matching Chrome app, a local URL app would be created. The class
// processes app changes from both DriveAppRegistry and extension system to
// keep the two in sync.
class DriveAppProvider : public drive::DriveAppRegistryObserver,
                         public extensions::ExtensionRegistryObserver {
 public:
  DriveAppProvider(Profile* profile,
                   DriveAppUninstallSyncService* uninstall_sync_service);
  ~DriveAppProvider() override;

  // Appends PKS factories this class depends on.
  static void AppendDependsOnFactories(
      std::set<BrowserContextKeyedServiceFactory*>* factories);

  void SetDriveServiceBridgeForTest(
      std::unique_ptr<DriveServiceBridge> test_bridge);

  // Adds/removes uninstalled Drive app id from DriveAppUninstallSyncService.
  // If a Drive app id is added as uninstalled Drive app, DriveAppProvider
  // would not auto create the local URL app for it until the uninstall record
  // is removed.
  void AddUninstalledDriveAppFromSync(const std::string& drive_app_id);
  void RemoveUninstalledDriveAppFromSync(const std::string& drive_app_id);

 private:
  friend class DriveAppProviderTest;

  typedef std::set<std::string> IdSet;
  typedef std::vector<drive::DriveAppInfo> DriveAppInfos;

  // Maps |drive_app_id| to |new_app|'s id in mapping. Auto uninstall existing
  // mapped app if it is an URL app.
  void UpdateMappingAndExtensionSystem(const std::string& drive_app_id,
                                       const extensions::Extension* new_app,
                                       bool is_new_app_generated);

  // Deferred processing of relevant extension installed message.
  void ProcessDeferredOnExtensionInstalled(const std::string drive_app_id,
                                           const std::string chrome_app_id);

  void SchedulePendingConverters();
  void OnLocalAppConverted(const DriveAppConverter* converter, bool success);

  bool IsMappedUrlAppUpToDate(const drive::DriveAppInfo& drive_app) const;

  void AddOrUpdateDriveApp(const drive::DriveAppInfo& drive_app);
  void ProcessRemovedDriveApp(const std::string& drive_app_id);

  void UpdateDriveApps();

  // drive::DriveAppRegistryObserver overrides:
  void OnDriveAppRegistryUpdated() override;

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  Profile* profile_;
  DriveAppUninstallSyncService* uninstall_sync_service_;

  std::unique_ptr<DriveServiceBridge> service_bridge_;
  std::unique_ptr<DriveAppMapping> mapping_;
  DriveAppInfos drive_apps_;
  bool drive_app_registry_updated_;

  // Tracks the pending web app convertions.
  ScopedVector<DriveAppConverter> pending_converters_;

  base::WeakPtrFactory<DriveAppProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppProvider);
};

#endif  // CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_PROVIDER_H_
