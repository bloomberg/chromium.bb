// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "extensions/browser/extension_registry_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

class Profile;

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
  void Launch(const std::string& app_id, int32_t event_flags) override;

  // extensions::ExtensionRegistryObserver overrides.
  // TODO(crbug.com/826982): implement.

  // Checks if extension is disabled and if enable flow should be started.
  // Returns true if extension enable flow is started or there is already one
  // running.
  bool RunExtensionEnableFlow(const std::string& app_id);

  apps::mojom::AppPtr Convert(const extensions::Extension* extension);

  mojo::Binding<apps::mojom::Publisher> binding_;
  Profile* profile_;
  mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_;

  // |next_u_key_| is incremented every time Convert returns a valid AppPtr, so
  // that when an extension's icon has changed, this apps::mojom::Publisher
  // sends a different IconKey even though the IconKey's s_key hasn't changed.
  uint64_t next_u_key_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
