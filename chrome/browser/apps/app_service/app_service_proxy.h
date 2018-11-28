// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_

#include "base/macros.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding_set.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"
#endif  // OS_CHROMEOS

class Profile;

namespace apps {

// Singleton (per Profile) proxy and cache of an App Service's apps.
//
// Singleton-ness means that //chrome/browser code (e.g UI code) can find *the*
// proxy for a given Profile, and therefore share the cache.
//
// See chrome/services/app_service/README.md.
class AppServiceProxy : public KeyedService, public apps::mojom::Subscriber {
 public:
  static AppServiceProxy* Get(Profile* profile);

  explicit AppServiceProxy(Profile* profile);

  ~AppServiceProxy() override;

  AppRegistryCache& Cache();

  void LoadIcon(const std::string& app_id,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                apps::mojom::Publisher::LoadIconCallback callback);

 private:
  // apps::mojom::Subscriber overrides.
  void OnApps(std::vector<apps::mojom::AppPtr> deltas) override;
  void Clone(apps::mojom::SubscriberRequest request) override;

  apps::mojom::AppServicePtr app_service_;
  mojo::BindingSet<apps::mojom::Subscriber> bindings_;
  AppRegistryCache cache_;

#if defined(OS_CHROMEOS)
  BuiltInChromeOsApps built_in_chrome_os_apps_;
#endif  // OS_CHROMEOS

  DISALLOW_COPY_AND_ASSIGN(AppServiceProxy);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
