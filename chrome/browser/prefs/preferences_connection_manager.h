// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREFERENCES_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_PREFS_PREFERENCES_CONNECTION_MANAGER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

// Handles all incoming prefs::mojom::PreferenceManagerRequest, providing a
// separate PreferencesManager per connection request.
//
// Additionally monitors system shutdown to clean up connections to PrefService.
//
// TODO(jonross): Observe profile switching and update PreferenceManager
// connections.
class PreferencesConnectionManager
    : public NON_EXPORTED_BASE(prefs::mojom::PreferencesFactory),
      public NON_EXPORTED_BASE(
          service_manager::InterfaceFactory<prefs::mojom::PreferencesFactory>),
      public NON_EXPORTED_BASE(service_manager::Service) {
 public:
  PreferencesConnectionManager();
  ~PreferencesConnectionManager() override;

 private:
  // mojo::StrongBinding callback:
  void OnConnectionError(
      mojo::StrongBindingPtr<prefs::mojom::PreferencesManager> binding);

  // KeyedServiceShutdownNotifier::Subscription callback. Used to cleanup when
  // the active PrefService is being destroyed.
  void OnProfileDestroyed();

  // prefs::mojom::PreferencesFactory:
  void Create(prefs::mojom::PreferencesObserverPtr observer,
              prefs::mojom::PreferencesManagerRequest manager) override;

  // service_manager::InterfaceFactory<PreferencesFactory>:
  void Create(const service_manager::Identity& remote_identity,
              prefs::mojom::PreferencesFactoryRequest request) override;

  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  mojo::BindingSet<prefs::mojom::PreferencesFactory> factory_bindings_;

  // Bindings that automatically cleanup during connection errors.
  std::vector<mojo::StrongBindingPtr<prefs::mojom::PreferencesManager>>
      manager_bindings_;

  // Observes shutdown, when PrefService is being destroyed.
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      profile_shutdown_notification_;

  DISALLOW_COPY_AND_ASSIGN(PreferencesConnectionManager);
};

#endif  // CHROME_BROWSER_PREFS_PREFERENCES_CONNECTION_MANAGER_H_
