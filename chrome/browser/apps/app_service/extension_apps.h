// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_

#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "extensions/browser/extension_registry_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

class Profile;

namespace extensions {
class ExtensionSet;
}

namespace apps {

// An app publisher (in the App Service sense) of extension-backed apps,
// including Chrome Apps (platform apps and legacy packaged apps) and hosted
// apps (including desktop PWAs).
//
// In the future, desktop PWAs will be migrated to a new system.
//
// See chrome/services/app_service/README.md.
class ExtensionApps : public apps::mojom::Publisher,
                      public extensions::ExtensionRegistryObserver {
 public:
  ExtensionApps();
  ~ExtensionApps() override;

  void Initialize(const apps::mojom::AppServicePtr& app_service,
                  Profile* profile);

 private:
  // apps::mojom::Publisher overrides.
  void Connect(apps::mojom::SubscriberPtr subscriber,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // Checks if extension is disabled and if enable flow should be started.
  // Returns true if extension enable flow is started or there is already one
  // running.
  bool RunExtensionEnableFlow(const std::string& app_id);

  static bool IsBlacklisted(const std::string& app_id);
  apps::mojom::AppPtr Convert(const extensions::Extension* extension,
                              apps::mojom::Readiness readiness);
  void ConvertVector(const extensions::ExtensionSet& extensions,
                     apps::mojom::Readiness readiness,
                     std::vector<apps::mojom::AppPtr>* apps_out);

  mojo::Binding<apps::mojom::Publisher> binding_;
  mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_;

  Profile* profile_;

  // |next_u_key_| is incremented every time Convert returns a valid AppPtr, so
  // that when an app's icon has changed, this apps::mojom::Publisher sends a
  // different IconKey even though the IconKey's s_key hasn't changed.
  uint64_t next_u_key_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
