// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_APP_REGISTRY_APP_REGISTRY_H_
#define CHROME_SERVICES_APP_SERVICE_APP_REGISTRY_APP_REGISTRY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/app_registry.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefRegistrySimple;
class PrefService;

namespace apps {

// The app registry maintains metadata on installed apps.
//
// TODO: rename this class? See the TODO in app_registry.mojom.
class AppRegistry : public apps::mojom::AppRegistry {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit AppRegistry(std::unique_ptr<PrefService> pref_service);
  ~AppRegistry() override;

  void BindRequest(apps::mojom::AppRegistryRequest request);

  // Returns |app_id|'s preferred state.
  apps::mojom::AppPreferred GetIfAppPreferredForTesting(
      const std::string& app_id) const;

  // mojom::apps::AppRegistry overrides.
  void SetAppPreferred(const std::string& app_id,
                       apps::mojom::AppPreferred state) override;

 private:
  std::unique_ptr<PrefService> pref_service_;
  mojo::BindingSet<apps::mojom::AppRegistry> bindings_;

  DISALLOW_COPY_AND_ASSIGN(AppRegistry);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_APP_REGISTRY_APP_REGISTRY_H_
