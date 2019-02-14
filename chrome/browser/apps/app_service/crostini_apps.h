// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_CROSTINI_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_CROSTINI_APPS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

class Profile;

namespace apps {

// An app publisher (in the App Service sense) of Crostini apps,
//
// See chrome/services/app_service/README.md.
class CrostiniApps : public KeyedService,
                     public apps::mojom::Publisher,
                     public crostini::CrostiniRegistryService::Observer {
 public:
  CrostiniApps();
  ~CrostiniApps() override;

  void Initialize(const apps::mojom::AppServicePtr& app_service,
                  Profile* profile);

 private:
  enum class PublishAppIDType {
    kInstall,
    kUninstall,
    kUpdate,
  };

  // apps::mojom::Publisher overrides.
  void Connect(apps::mojom::SubscriberPtr subscriber,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;

  // CrostiniRegistryService::Observer overrides.
  void OnRegistryUpdated(
      crostini::CrostiniRegistryService* registry_service,
      const std::vector<std::string>& updated_apps,
      const std::vector<std::string>& removed_apps,
      const std::vector<std::string>& inserted_apps) override;
  void OnAppIconUpdated(const std::string& app_id,
                        ui::ScaleFactor scale_factor) override;

  void LoadIconFromVM(const std::string icon_key_s_key,
                      apps::mojom::IconCompression icon_compression,
                      int32_t size_hint_in_dip,
                      bool allow_placeholder_icon,
                      ui::ScaleFactor scale_factor,
                      LoadIconCallback callback);

  apps::mojom::AppPtr Convert(
      const std::string& app_id,
      const crostini::CrostiniRegistryService::Registration& registration,
      bool new_icon_key);
  apps::mojom::IconKeyPtr NewIconKey(const std::string& app_id);
  void PublishAppID(const std::string& app_id, PublishAppIDType type);
  void Publish(apps::mojom::AppPtr app);

  mojo::Binding<apps::mojom::Publisher> binding_;
  mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_;

  crostini::CrostiniRegistryService* registry_;

  // |next_u_key_| is incremented every time Convert returns a valid AppPtr, so
  // that when an app's icon has changed, this apps::mojom::Publisher sends a
  // different IconKey even though the IconKey's s_key hasn't changed.
  uint64_t next_u_key_;

  base::WeakPtrFactory<CrostiniApps> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_CROSTINI_APPS_H_
