// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_SYNC_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_SYNC_BRIDGE_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class MetadataChangeList;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace password_manager {

class PasswordStoreSync;

// Sync bridge implementation for PASSWORDS model type. Takes care of
// propagating local passwords to other clients and vice versa.
//
// This is achieved by implementing the interface ModelTypeSyncBridge, which
// ClientTagBasedModelTypeProcessor will use to interact, ultimately, with the
// sync server. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/sync/model_api.md#Implementing-ModelTypeSyncBridge
// for details.
class PasswordSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  // |password_store_sync| must not be null and must outlive this object.
  PasswordSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      PasswordStoreSync* password_store_sync);
  ~PasswordSyncBridge() override;

  // Notifies the bridge of changes to the password database. Callers are
  // responsible for calling this function within the very same transaction as
  // the data changes.
  void ActOnPasswordStoreChanges(const PasswordStoreChangeList& changes);

  // ModelTypeSyncBridge implementation.
  void OnSyncStarting(
      const syncer::DataTypeActivationRequest& request) override;
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
  bool SupportsGetStorageKey() const override;
  ModelTypeSyncBridge::StopSyncResponse ApplyStopSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list)
      override;

 private:
  // Password store responsible for persistence.
  PasswordStoreSync* const password_store_sync_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PasswordSyncBridge);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_SYNC_BRIDGE_H_
