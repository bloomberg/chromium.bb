// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/active_profile_pref_service.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"

ActiveProfilePrefService::ActiveProfilePrefService()
    : connector_binding_(this) {
  registry_.AddInterface<prefs::mojom::PrefStoreConnector>(
      base::Bind(&ActiveProfilePrefService::Create, base::Unretained(this)));
}

ActiveProfilePrefService::~ActiveProfilePrefService() = default;

void ActiveProfilePrefService::Connect(
    prefs::mojom::PrefRegistryPtr pref_registry,
    ConnectCallback callback) {
  GetPrefStoreConnector().Connect(std::move(pref_registry),
                                  std::move(callback));
}

void ActiveProfilePrefService::Create(
    prefs::mojom::PrefStoreConnectorRequest request) {
  connector_binding_.Close();
  connector_binding_.Bind(std::move(request));
}

void ActiveProfilePrefService::OnStart() {}

void ActiveProfilePrefService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // N.B. This check is important as not doing it would allow one user to read
  // another user's prefs.
  // TODO(beng): This should be obsoleted by Service Manager user id routing.
  if (context()->identity().user_id() != source_info.identity.user_id()) {
    LOG(WARNING) << "Blocked service instance="
                 << source_info.identity.instance()
                 << ", name=" << source_info.identity.name()
                 << ", user_id=" << source_info.identity.user_id()
                 << " from connecting to the active profile's pref service.";
    return;
  }
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void ActiveProfilePrefService::OnConnectError() {
  connector_binding_.Close();
}

prefs::mojom::PrefStoreConnector&
ActiveProfilePrefService::GetPrefStoreConnector() {
  // The pref service enforces that each service always registers the same set
  // of prefs. If the ActiveProfilePrefService connects directly using
  // content::BrowserContext::GetConnectorFor(), the pref service will receive
  // two connections from |content::mojom::kBrowserServiceName|: one for the
  // browser and one from mash, each registering different prefs.
  //
  // Instead, the prefs::mojom::kForwarderServiceName running as root connects
  // to the prefs::mojom::kForwarderServiceName running as the user for the
  // active user profile, which then connects to the pref service. Thus, mash
  // appears to be |prefs::mojom::kForwarderServiceName| instead of
  // |content::mojom::kBrowserServiceName|.
  if (context()->identity().user_id() == service_manager::mojom::kRootUserID) {
    content::BrowserContext::GetConnectorFor(
        ProfileManager::GetActiveUserProfile()->GetOriginalProfile())
        ->BindInterface(prefs::mojom::kForwarderServiceName, &connector_ptr_);
  } else if (!connector_ptr_) {
    context()->connector()->BindInterface(prefs::mojom::kServiceName,
                                          &connector_ptr_);
  }
  connector_ptr_.set_connection_error_handler(base::Bind(
      &ActiveProfilePrefService::OnConnectError, base::Unretained(this)));
  return *connector_ptr_;
}
