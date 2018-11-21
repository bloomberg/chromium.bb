// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "chrome/services/app_service/app_registry/app_registry.h"
#include "chrome/services/app_service/app_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/pref_service_factory.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace apps {

AppService::AppService() = default;

AppService::~AppService() = default;

void AppService::OnStart() {
  binder_registry_.AddInterface<apps::mojom::AppRegistry>(base::BindRepeating(
      &AppService::BindAppRegistryRequest, base::Unretained(this)));
  binder_registry_.AddInterface<apps::mojom::AppService>(base::BindRepeating(
      &AppServiceImpl::BindRequest, base::Unretained(&impl_)));

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  AppRegistry::RegisterPrefs(pref_registry.get());

  prefs::ConnectToPrefService(
      context()->connector(), std::move(pref_registry),
      base::BindRepeating(&AppService::OnPrefServiceConnected,
                          base::Unretained(this)));
}

void AppService::OnBindInterface(const service_manager::BindSourceInfo& source,
                                 const std::string& interface_name,
                                 mojo::ScopedMessagePipeHandle interface_pipe) {
  binder_registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void AppService::BindAppRegistryRequest(
    apps::mojom::AppRegistryRequest request) {
  if (!IsInitializationComplete()) {
    pending_app_registry_requests_.push_back(std::move(request));
    return;
  }

  app_registry_->BindRequest(std::move(request));
}

void AppService::OnPrefServiceConnected(
    std::unique_ptr<::PrefService> pref_service) {
  // Connecting to the pref service is required for the app_registry_ to
  // function.
  DCHECK(pref_service);
  app_registry_ = std::make_unique<AppRegistry>(std::move(pref_service));

  DCHECK(IsInitializationComplete());
  for (auto& request : pending_app_registry_requests_)
    BindAppRegistryRequest(std::move(request));

  pending_app_registry_requests_.clear();
}

}  // namespace apps
