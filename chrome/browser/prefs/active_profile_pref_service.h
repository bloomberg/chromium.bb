// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_ACTIVE_PROFILE_PREF_SERVICE_H_
#define CHROME_BROWSER_PREFS_ACTIVE_PROFILE_PREF_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

// A |prefs::mojom::PrefStoreConnector| implementation that forwards connection
// calls from the root service to the active profile. Used by mash, which runs
// as the root user, to talk to prefs.
//
// TODO(http://crbug.com/705347): Once mash supports several profiles, remove
// this class and the forwarder service.
class ActiveProfilePrefService : public prefs::mojom::PrefStoreConnector,
                                 public service_manager::Service {
 public:
  ActiveProfilePrefService();
  ~ActiveProfilePrefService() override;

 private:
  // prefs::mojom::PrefStoreConnector:
  void Connect(prefs::mojom::PrefRegistryPtr pref_registry,
               ConnectCallback callback) override;

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void Create(prefs::mojom::PrefStoreConnectorRequest request);

  // Called if forwarding the connection request to the per-profile service
  // instance failed.
  void OnConnectError();

  prefs::mojom::PrefStoreConnector& GetPrefStoreConnector();

  prefs::mojom::PrefStoreConnectorPtr connector_ptr_;
  service_manager::BinderRegistry registry_;
  mojo::Binding<prefs::mojom::PrefStoreConnector> connector_binding_;

  DISALLOW_COPY_AND_ASSIGN(ActiveProfilePrefService);
};

#endif  // CHROME_BROWSER_PREFS_ACTIVE_PROFILE_PREF_SERVICE_H_
