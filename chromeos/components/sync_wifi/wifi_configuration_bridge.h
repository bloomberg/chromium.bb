// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/network/network_configuration_observer.h"
#include "chromeos/network/network_metadata_observer.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace chromeos {

class NetworkConfigurationHandler;
class NetworkMetadataStore;

namespace sync_wifi {

class LocalNetworkCollector;
class SyncedNetworkMetricsLogger;
class SyncedNetworkUpdater;

// Receives updates to network configurations from the Chrome sync back end and
// from the system network stack and keeps both lists in sync.
class WifiConfigurationBridge : public syncer::ModelTypeSyncBridge,
                                public NetworkConfigurationObserver,
                                public NetworkMetadataObserver {
 public:
  WifiConfigurationBridge(
      SyncedNetworkUpdater* synced_network_updater,
      LocalNetworkCollector* local_network_collector,
      NetworkConfigurationHandler* network_configuration_handler,
      SyncedNetworkMetricsLogger* metrics_recorder,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory create_store_callback);
  ~WifiConfigurationBridge() override;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // NetworkMetadataObserver:
  void OnFirstConnectionToNetwork(const std::string& guid) override;
  void OnNetworkUpdate(const std::string& guid,
                       base::DictionaryValue* set_properties) override;

  // NetworkConfigurationObserver::
  void OnBeforeConfigurationRemoved(const std::string& service_path,
                                    const std::string& guid) override;
  void OnConfigurationRemoved(const std::string& service_path,
                              const std::string& guid) override;

  // Comes from |entries_| the in-memory map.
  std::vector<NetworkIdentifier> GetAllIdsForTesting();

  void SetNetworkMetadataStore(
      base::WeakPtr<NetworkMetadataStore> network_metadata_store);

 private:
  void Commit(std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch);

  // Callbacks for ModelTypeStore.
  void OnStoreCreated(const base::Optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllData(
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> records);
  void OnReadAllMetadata(const base::Optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const base::Optional<syncer::ModelError>& error);

  void OnGetAllSyncableNetworksResult(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList change_list,
      std::vector<sync_pb::WifiConfigurationSpecifics> local_network_list);

  void SaveNetworkToSync(
      base::Optional<sync_pb::WifiConfigurationSpecifics> proto);
  void RemoveNetworkFromSync(
      base::Optional<sync_pb::WifiConfigurationSpecifics> proto);

  // An in-memory list of the proto's that mirrors what is on the sync server.
  // This gets updated when changes are received from the server and after local
  // changes have been committed.  On initialization of this class, it is
  // populated with the contents of |store_|.
  base::flat_map<std::string, sync_pb::WifiConfigurationSpecifics> entries_;

  // Map of network |guid| to |storage_key|.  After a network is deleted, we
  // no longer have access to its metadata so this stores the necessary
  // information to delete it from sync.
  base::flat_map<std::string, std::string> pending_deletes_;

  // The on disk store of WifiConfigurationSpecifics protos that mirrors what
  // is on the sync server.  This gets updated when changes are received from
  // the server and after local changes have been committed to the server.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  SyncedNetworkUpdater* synced_network_updater_;
  LocalNetworkCollector* local_network_collector_;
  NetworkConfigurationHandler* network_configuration_handler_;
  SyncedNetworkMetricsLogger* metrics_recorder_;
  base::WeakPtr<NetworkMetadataStore> network_metadata_store_;

  base::WeakPtrFactory<WifiConfigurationBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WifiConfigurationBridge);
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_BRIDGE_H_
