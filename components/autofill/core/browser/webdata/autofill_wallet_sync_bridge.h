// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_SYNC_BRIDGE_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace autofill {

class AutofillProfile;
class AutofillTable;
class AutofillWebDataBackend;
class AutofillWebDataService;
class CreditCard;

// Sync bridge responsible for propagating local changes to the processor and
// applying remote changes to the local database.
class AutofillWalletSyncBridge : public base::SupportsUserData::Data,
                                 public syncer::ModelTypeSyncBridge {
 public:
  // Factory method that hides dealing with change_processor and also stores the
  // created bridge within |web_data_service|. This method should only be
  // called on |web_data_service|'s DB thread.
  static void CreateForWebDataServiceAndBackend(
      const std::string& app_locale,
      AutofillWebDataBackend* webdata_backend,
      AutofillWebDataService* web_data_service);

  static syncer::ModelTypeSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  explicit AutofillWalletSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      AutofillWebDataBackend* web_data_backend);
  ~AutofillWalletSyncBridge() override;

  // ModelTypeSyncBridge implementation.
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

 private:
  struct AutofillWalletDiff {
    int items_added = 0;
    int items_removed = 0;

    bool IsEmpty() const { return items_added == 0 && items_removed == 0; }
  };

  // Sets the wallet data from |entity_data| to this client. Records metrics
  // about added/deleted data if not the |is_initial_data|.
  void SetSyncData(const syncer::EntityChangeList& entity_data,
                   bool is_initial_data);

  // Computes a "diff" (items added, items removed) of two vectors of items,
  // which should be either CreditCard or AutofillProfile. This is used for two
  // purposes:
  // 1) Detecting if anything has changed, so that we don't write to disk in the
  //    common case where nothing has changed.
  // 2) Recording metrics on the number of added/removed items.
  // This is exposed as a static method so that it can be tested.
  template <class Item>
  static AutofillWalletDiff ComputeAutofillWalletDiff(
      const std::vector<std::unique_ptr<Item>>& old_data,
      const std::vector<Item>& new_data);

  // Returns the table associated with the |web_data_backend_|.
  AutofillTable* GetAutofillTable();

  // AutofillProfileSyncBridge is owned by |web_data_backend_| through
  // SupportsUserData, so it's guaranteed to outlive |this|.
  AutofillWebDataBackend* const web_data_backend_;

  // Synchronously load sync metadata from the autofill table and pass it to the
  // processor so that it can start tracking changes.
  void LoadMetadata();

  // The bridge should be used on the same sequence where it is constructed.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletSyncBridge);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_SYNC_BRIDGE_H_
