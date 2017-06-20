// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_syncable.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/value_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/overlay_user_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "services/preferences/public/cpp/persistent_pref_store_client.h"
#include "services/preferences/public/cpp/pref_registry_serializer.h"
#include "services/preferences/public/cpp/pref_store_impl.h"
#include "services/preferences/public/cpp/registering_delegate.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace sync_preferences {

namespace {

// This only needs to be called once per incognito profile.
void InitIncognitoService(service_manager::Connector* incognito_connector,
                          service_manager::Connector* user_connector) {
  prefs::mojom::PrefStoreConnectorPtr connector;
  user_connector->BindInterface(prefs::mojom::kServiceName, &connector);
  auto config = prefs::mojom::PersistentPrefStoreConfiguration::New();
  config->set_incognito_configuration(
      prefs::mojom::IncognitoPersistentPrefStoreConfiguration::New(
          std::move(connector)));
  prefs::mojom::PrefServiceControlPtr control;
  incognito_connector->BindInterface(prefs::mojom::kServiceName, &control);
  control->Init(std::move(config));
}

}  // namespace

PrefServiceSyncable::PrefServiceSyncable(
    PrefNotifierImpl* pref_notifier,
    PrefValueStore* pref_value_store,
    PersistentPrefStore* user_prefs,
    user_prefs::PrefRegistrySyncable* pref_registry,
    const PrefModelAssociatorClient* pref_model_associator_client,
    base::Callback<void(PersistentPrefStore::PrefReadError)>
        read_error_callback,
    bool async)
    : PrefService(pref_notifier,
                  pref_value_store,
                  user_prefs,
                  pref_registry,
                  read_error_callback,
                  async),
      pref_service_forked_(false),
      pref_sync_associator_(pref_model_associator_client, syncer::PREFERENCES),
      priority_pref_sync_associator_(pref_model_associator_client,
                                     syncer::PRIORITY_PREFERENCES) {
  pref_sync_associator_.SetPrefService(this);
  priority_pref_sync_associator_.SetPrefService(this);

  // Let PrefModelAssociators know about changes to preference values.
  pref_value_store->set_callback(base::Bind(
      &PrefServiceSyncable::ProcessPrefChange, base::Unretained(this)));

  // Add already-registered syncable preferences to PrefModelAssociator.
  for (PrefRegistry::const_iterator it = pref_registry->begin();
       it != pref_registry->end(); ++it) {
    const std::string& path = it->first;
    AddRegisteredSyncablePreference(path,
                                    pref_registry_->GetRegistrationFlags(path));
  }

  // Watch for syncable preferences registered after this point.
  pref_registry->SetSyncableRegistrationCallback(
      base::Bind(&PrefServiceSyncable::AddRegisteredSyncablePreference,
                 base::Unretained(this)));
}

PrefServiceSyncable::~PrefServiceSyncable() {
  // Remove our callback from the registry, since it may outlive us.
  user_prefs::PrefRegistrySyncable* registry =
      static_cast<user_prefs::PrefRegistrySyncable*>(pref_registry_.get());
  registry->SetSyncableRegistrationCallback(
      user_prefs::PrefRegistrySyncable::SyncableRegistrationCallback());
}

PrefServiceSyncable* PrefServiceSyncable::CreateIncognitoPrefService(
    PrefStore* incognito_extension_pref_store,
    const std::vector<const char*>& overlay_pref_names,
    std::set<PrefValueStore::PrefStoreType> already_connected_types,
    service_manager::Connector* incognito_connector,
    service_manager::Connector* user_connector) {
  pref_service_forked_ = true;
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();

  scoped_refptr<user_prefs::PrefRegistrySyncable> forked_registry =
      static_cast<user_prefs::PrefRegistrySyncable*>(pref_registry_.get())
          ->ForkForIncognito();

  scoped_refptr<OverlayUserPrefStore> incognito_pref_store;
  std::unique_ptr<RegisteringDelegate> delegate;
  if (incognito_connector) {
    DCHECK(user_connector);
    incognito_pref_store = CreateOverlayUsingPrefService(
        forked_registry.get(), std::move(already_connected_types),
        incognito_connector, user_connector);
    prefs::mojom::PrefStoreRegistryPtr registry;
    incognito_connector->BindInterface(prefs::mojom::kServiceName, &registry);
    delegate = base::MakeUnique<RegisteringDelegate>(std::move(registry));
  } else {
    incognito_pref_store = new OverlayUserPrefStore(user_pref_store_.get());
  }

  for (const char* overlay_pref_name : overlay_pref_names)
    incognito_pref_store->RegisterOverlayPref(overlay_pref_name);

  PrefServiceSyncable* incognito_service = new PrefServiceSyncable(
      pref_notifier,
      pref_value_store_->CloneAndSpecialize(NULL,  // managed
                                            NULL,  // supervised_user
                                            incognito_extension_pref_store,
                                            NULL,  // command_line_prefs
                                            incognito_pref_store.get(),
                                            NULL,  // recommended
                                            forked_registry->defaults().get(),
                                            pref_notifier, std::move(delegate)),
      incognito_pref_store.get(), forked_registry.get(),
      pref_sync_associator_.client(), read_error_callback_, false);
  return incognito_service;
}

bool PrefServiceSyncable::IsSyncing() {
  return pref_sync_associator_.models_associated();
}

bool PrefServiceSyncable::IsPrioritySyncing() {
  return priority_pref_sync_associator_.models_associated();
}

bool PrefServiceSyncable::IsPrefSynced(const std::string& name) const {
  return pref_sync_associator_.IsPrefSynced(name) ||
         priority_pref_sync_associator_.IsPrefSynced(name);
}

void PrefServiceSyncable::AddObserver(PrefServiceSyncableObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PrefServiceSyncable::RemoveObserver(
    PrefServiceSyncableObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

syncer::SyncableService* PrefServiceSyncable::GetSyncableService(
    const syncer::ModelType& type) {
  if (type == syncer::PREFERENCES) {
    return &pref_sync_associator_;
  } else if (type == syncer::PRIORITY_PREFERENCES) {
    return &priority_pref_sync_associator_;
  } else {
    NOTREACHED() << "invalid model type: " << type;
    return NULL;
  }
}

void PrefServiceSyncable::UpdateCommandLinePrefStore(
    PrefStore* cmd_line_store) {
  // If |pref_service_forked_| is true, then this PrefService and the forked
  // copies will be out of sync.
  DCHECK(!pref_service_forked_);
  PrefService::UpdateCommandLinePrefStore(cmd_line_store);
}

void PrefServiceSyncable::AddSyncedPrefObserver(const std::string& name,
                                                SyncedPrefObserver* observer) {
  pref_sync_associator_.AddSyncedPrefObserver(name, observer);
  priority_pref_sync_associator_.AddSyncedPrefObserver(name, observer);
}

void PrefServiceSyncable::RemoveSyncedPrefObserver(
    const std::string& name,
    SyncedPrefObserver* observer) {
  pref_sync_associator_.RemoveSyncedPrefObserver(name, observer);
  priority_pref_sync_associator_.RemoveSyncedPrefObserver(name, observer);
}

void PrefServiceSyncable::RegisterMergeDataFinishedCallback(
    const base::Closure& callback) {
  pref_sync_associator_.RegisterMergeDataFinishedCallback(callback);
}

// Set the PrefModelAssociatorClient to use for that object during tests.
void PrefServiceSyncable::SetPrefModelAssociatorClientForTesting(
    const PrefModelAssociatorClient* pref_model_associator_client) {
  pref_sync_associator_.SetPrefModelAssociatorClientForTesting(
      pref_model_associator_client);
  priority_pref_sync_associator_.SetPrefModelAssociatorClientForTesting(
      pref_model_associator_client);
}

void PrefServiceSyncable::AddRegisteredSyncablePreference(
    const std::string& path,
    uint32_t flags) {
  DCHECK(FindPreference(path));
  if (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_PREF) {
    pref_sync_associator_.RegisterPref(path.c_str());
  } else if (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF) {
    priority_pref_sync_associator_.RegisterPref(path.c_str());
  }
}

void PrefServiceSyncable::OnIsSyncingChanged() {
  for (auto& observer : observer_list_)
    observer.OnIsSyncingChanged();
}

void PrefServiceSyncable::ProcessPrefChange(const std::string& name) {
  pref_sync_associator_.ProcessPrefChange(name);
  priority_pref_sync_associator_.ProcessPrefChange(name);
}

OverlayUserPrefStore* PrefServiceSyncable::CreateOverlayUsingPrefService(
    user_prefs::PrefRegistrySyncable* pref_registry,
    std::set<PrefValueStore::PrefStoreType> already_connected_types,
    service_manager::Connector* incognito_connector,
    service_manager::Connector* user_connector) const {
  InitIncognitoService(incognito_connector, user_connector);

  prefs::mojom::PrefStoreConnectorPtr pref_connector;
  incognito_connector->BindInterface(prefs::mojom::kServiceName,
                                     &pref_connector);
  std::vector<PrefValueStore::PrefStoreType> in_process_types(
      already_connected_types.begin(), already_connected_types.end());

  // Connect to the writable, in-memory incognito pref store.
  prefs::mojom::PersistentPrefStoreConnectionPtr connection;
  prefs::mojom::PersistentPrefStoreConnectionPtr incognito_connection;
  std::vector<prefs::mojom::PrefRegistrationPtr> remote_defaults;
  std::unordered_map<PrefValueStore::PrefStoreType,
                     prefs::mojom::PrefStoreConnectionPtr>
      other_pref_stores;
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_calls;
  bool success = pref_connector->Connect(
      prefs::SerializePrefRegistry(*pref_registry), in_process_types,
      &incognito_connection, &connection, &remote_defaults, &other_pref_stores);
  DCHECK(success);

  return new OverlayUserPrefStore(
      new prefs::PersistentPrefStoreClient(std::move(incognito_connection)),
      user_pref_store_.get());
}

}  // namespace sync_preferences
