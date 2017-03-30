// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_ACTIVE_PROFILE_PREF_SERVICE_H_
#define CHROME_BROWSER_PREFS_ACTIVE_PROFILE_PREF_SERVICE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

// A |prefs::mojom::PrefStoreConnector| implementation that forwards connection
// calls from the root service to the active profile. Used by mash, which runs
// as the root user, to talk to prefs.
//
// TODO(http://crbug.com/705347): Once mash supports several profiles, remove
// this class and the forwarder service.
class ActiveProfilePrefService : public prefs::mojom::PrefStoreConnector,
                                 public service_manager::InterfaceFactory<
                                     prefs::mojom::PrefStoreConnector>,
                                 public service_manager::Service {
 public:
  ActiveProfilePrefService();
  ~ActiveProfilePrefService() override;

 private:
  // prefs::mojom::PrefStoreConnector:
  void Connect(prefs::mojom::PrefRegistryPtr pref_registry,
               const ConnectCallback& callback) override;

  // service_manager::InterfaceFactory<PrefStoreConnector>:
  void Create(const service_manager::Identity& remote_identity,
              prefs::mojom::PrefStoreConnectorRequest request) override;

  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // Called if forwarding the connection request to the per-profile service
  // instance failed.
  void OnConnectError();

  prefs::mojom::PrefStoreConnectorPtr connector_ptr_;
  mojo::BindingSet<prefs::mojom::PrefStoreConnector> connector_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ActiveProfilePrefService);
};

#endif  // CHROME_BROWSER_PREFS_ACTIVE_PROFILE_PREF_SERVICE_H_
