// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_syncable_factory.h"

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_value_store.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "services/preferences/public/cpp/pref_store_adapter.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

#if !defined(OS_IOS)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/policy_service.h"  // nogncheck
#include "components/policy/core/common/policy_types.h"    // nogncheck
#endif

namespace sync_preferences {

PrefServiceSyncableFactory::PrefServiceSyncableFactory() {}

PrefServiceSyncableFactory::~PrefServiceSyncableFactory() {}

void PrefServiceSyncableFactory::SetManagedPolicies(
    policy::PolicyService* service,
    policy::BrowserPolicyConnector* connector) {
#if !defined(OS_IOS)
  set_managed_prefs(new policy::ConfigurationPolicyPrefStore(
      service, connector->GetHandlerList(), policy::POLICY_LEVEL_MANDATORY));
#else
  NOTREACHED();
#endif
}

void PrefServiceSyncableFactory::SetRecommendedPolicies(
    policy::PolicyService* service,
    policy::BrowserPolicyConnector* connector) {
#if !defined(OS_IOS)
  set_recommended_prefs(new policy::ConfigurationPolicyPrefStore(
      service, connector->GetHandlerList(), policy::POLICY_LEVEL_RECOMMENDED));
#else
  NOTREACHED();
#endif
}

void PrefServiceSyncableFactory::SetPrefModelAssociatorClient(
    PrefModelAssociatorClient* pref_model_associator_client) {
  pref_model_associator_client_ = pref_model_associator_client;
}

namespace {

// Expose the |backing_pref_store| through the prefs service.
scoped_refptr<::PrefStore> CreateRegisteredPrefStore(
    service_manager::Connector* connector,
    scoped_refptr<::PrefStore> backing_pref_store,
    PrefValueStore::PrefStoreType type) {
  // If we're testing or if the prefs service feature flag is off we don't
  // register.
  if (!connector || !backing_pref_store)
    return backing_pref_store;

  prefs::mojom::PrefStoreRegistryPtr registry_ptr;
  connector->BindInterface(prefs::mojom::kServiceName, &registry_ptr);
  return make_scoped_refptr(new prefs::PrefStoreAdapter(
      backing_pref_store, prefs::PrefStoreImpl::Create(
                              registry_ptr.get(), backing_pref_store, type)));
}

}  // namespace

std::unique_ptr<PrefServiceSyncable> PrefServiceSyncableFactory::CreateSyncable(
    user_prefs::PrefRegistrySyncable* pref_registry,
    service_manager::Connector* connector) {
  TRACE_EVENT0("browser", "PrefServiceSyncableFactory::CreateSyncable");
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();

  // Expose all read-only stores through the prefs service.
  auto managed = CreateRegisteredPrefStore(connector, managed_prefs_,
                                           PrefValueStore::MANAGED_STORE);
  auto supervised = CreateRegisteredPrefStore(
      connector, supervised_user_prefs_, PrefValueStore::SUPERVISED_USER_STORE);
  auto extension = CreateRegisteredPrefStore(connector, extension_prefs_,
                                             PrefValueStore::EXTENSION_STORE);
  auto command_line = CreateRegisteredPrefStore(
      connector, command_line_prefs_, PrefValueStore::COMMAND_LINE_STORE);
  auto recommended = CreateRegisteredPrefStore(
      connector, recommended_prefs_, PrefValueStore::RECOMMENDED_STORE);

  // TODO(sammc): Register Mojo user pref store once implemented.
  std::unique_ptr<PrefServiceSyncable> pref_service(new PrefServiceSyncable(
      pref_notifier,
      new PrefValueStore(managed.get(), supervised.get(), extension.get(),
                         command_line.get(), user_prefs_.get(),
                         recommended.get(), pref_registry->defaults().get(),
                         pref_notifier),
      user_prefs_.get(), pref_registry, pref_model_associator_client_,
      read_error_callback_, async_));
  return pref_service;
}

}  // namespace sync_preferences
