// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/active_profile_pref_service.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"

ActiveProfilePrefService::ActiveProfilePrefService() = default;

ActiveProfilePrefService::~ActiveProfilePrefService() = default;

void ActiveProfilePrefService::Connect(
    prefs::mojom::PrefRegistryPtr pref_registry,
    const std::vector<PrefValueStore::PrefStoreType>& already_connected_types,
    const ConnectCallback& callback) {
  auto* connector = content::BrowserContext::GetConnectorFor(
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile());
  connector->BindInterface(prefs::mojom::kServiceName, &connector_ptr_);
  connector_ptr_.set_connection_error_handler(base::Bind(
      &ActiveProfilePrefService::OnConnectError, base::Unretained(this)));
  connector_ptr_->Connect(std::move(pref_registry), already_connected_types,
                          callback);
}

void ActiveProfilePrefService::Create(
    const service_manager::Identity& remote_identity,
    prefs::mojom::PrefStoreConnectorRequest request) {
  connector_bindings_.AddBinding(this, std::move(request));
}

void ActiveProfilePrefService::OnStart() {}

bool ActiveProfilePrefService::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  // N.B. This check is important as not doing it would allow one user to read
  // another user's prefs.
  if (remote_info.identity.user_id() != service_manager::mojom::kRootUserID) {
    LOG(WARNING) << "Blocked service instance="
                 << remote_info.identity.instance()
                 << ", name=" << remote_info.identity.name()
                 << ", user_id=" << remote_info.identity.user_id()
                 << " from connecting to the active profile's pref service.";
    return false;
  }
  registry->AddInterface<prefs::mojom::PrefStoreConnector>(this);
  return true;
}

void ActiveProfilePrefService::OnConnectError() {
  connector_bindings_.CloseAllBindings();
}
