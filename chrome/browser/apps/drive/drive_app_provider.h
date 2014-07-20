// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_PROVIDER_H_
#define CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/drive/drive_app_registry_observer.h"
#include "extensions/browser/extension_registry_observer.h"

namespace drive {
struct DriveAppInfo;
}

class BrowserContextKeyedServiceFactory;
class DriveAppConverter;
class DriveAppMapping;
class DriveServiceBridge;
class ExtensionService;
class Profile;

// DriveAppProvider is the integration point for Drive apps. It ensures each
// Drive app has a corresponding Chrome app in the extension system. If there
// is no matching Chrome app, a local URL app would be created. The class
// processes app changes from both DriveAppRegistry and extension system to
// keep the two in sync.
class DriveAppProvider : public drive::DriveAppRegistryObserver,
                         public extensions::ExtensionRegistryObserver {
 public:
  explicit DriveAppProvider(Profile* profile);
  virtual ~DriveAppProvider();

  // Appends PKS factories this class depends on.
  static void AppendDependsOnFactories(
      std::set<BrowserContextKeyedServiceFactory*>* factories);

  void SetDriveServiceBridgeForTest(scoped_ptr<DriveServiceBridge> test_bridge);

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

  // drive::DriveAppRegistryObserver overrides:
  virtual void OnDriveAppRegistryUpdated() OVERRIDE;

  // extensions::ExtensionRegistryObserver overrides:
  virtual void OnExtensionInstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      bool is_update) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  Profile* profile_;

  scoped_ptr<DriveServiceBridge> service_bridge_;
  scoped_ptr<DriveAppMapping> mapping_;
  DriveAppInfos drive_apps_;

  // Tracks the pending web app convertions.
  ScopedVector<DriveAppConverter> pending_converters_;

  base::WeakPtrFactory<DriveAppProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppProvider);
};

#endif  // CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_PROVIDER_H_
