// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"

#include <unordered_map>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/pickle.h"
#include "components/autofill/core/browser/autofill_metadata.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

namespace autofill {

namespace {

using sync_pb::WalletMetadataSpecifics;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::MetadataChangeList;

// Address to this variable used as the user data key.
static int kAutofillWalletMetadataSyncBridgeUserDataKey = 0;

std::string GetClientTagForSpecificsId(WalletMetadataSpecifics::Type type,
                                       const std::string& specifics_id) {
  switch (type) {
    case WalletMetadataSpecifics::ADDRESS:
      return "address-" + specifics_id;
    case WalletMetadataSpecifics::CARD:
      return "card-" + specifics_id;
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED();
      return "";
  }
}

// Returns the wallet metadata specifics id for the specified |metadata_id|.
std::string GetSpecificsIdForMetadataId(const std::string& metadata_id) {
  // Metadata id is in the raw format (like profiles/cards from WalletData)
  // whereas the specifics id is base64-encoded.
  return GetBase64EncodedId(metadata_id);
}

// Returns the wallet metadata id for the specified |specifics_id|.
std::string GetMetadataIdForSpecificsId(const std::string& specifics_id) {
  // The specifics id is base64-encoded whereas the metadata id is in the raw
  // format (like profiles/cards from WalletData).
  return GetBase64DecodedId(specifics_id);
}

// Returns the wallet metadata specifics storage key for the specified |type|
// and |metadata_id|.
std::string GetStorageKeyForWalletMetadataTypeAndId(
    WalletMetadataSpecifics::Type type,
    const std::string& metadata_id) {
  return GetStorageKeyForWalletMetadataTypeAndSpecificsId(
      type, GetSpecificsIdForMetadataId(metadata_id));
}

struct TypeAndMetadataId {
  WalletMetadataSpecifics::Type type;
  std::string metadata_id;
};

TypeAndMetadataId ParseWalletMetadataStorageKey(
    const std::string& storage_key) {
  TypeAndMetadataId parsed;

  base::Pickle pickle(storage_key.data(), storage_key.size());
  base::PickleIterator iterator(pickle);
  int type_int;
  if (!iterator.ReadInt(&type_int) ||
      !iterator.ReadString(&parsed.metadata_id)) {
    NOTREACHED() << "Unsupported storage_key provided " << storage_key;
  }
  parsed.type = static_cast<WalletMetadataSpecifics::Type>(type_int);
  return parsed;
}

// Returns EntityData for wallet_metadata for |local_metadata| and |type|.
std::unique_ptr<EntityData> CreateEntityDataFromAutofillMetadata(
    WalletMetadataSpecifics::Type type,
    const AutofillMetadata& local_metadata) {
  auto entity_data = std::make_unique<EntityData>();
  std::string specifics_id = GetSpecificsIdForMetadataId(local_metadata.id);
  entity_data->non_unique_name = GetClientTagForSpecificsId(type, specifics_id);

  WalletMetadataSpecifics* remote_metadata =
      entity_data->specifics.mutable_wallet_metadata();
  remote_metadata->set_type(type);
  remote_metadata->set_id(specifics_id);
  remote_metadata->set_use_count(local_metadata.use_count);
  remote_metadata->set_use_date(
      local_metadata.use_date.ToDeltaSinceWindowsEpoch().InMicroseconds());

  switch (type) {
    case WalletMetadataSpecifics::ADDRESS: {
      remote_metadata->set_address_has_converted(local_metadata.has_converted);
      break;
    }
    case WalletMetadataSpecifics::CARD: {
      // The strings must be in valid UTF-8 to sync.
      remote_metadata->set_card_billing_address_id(
          GetBase64EncodedId(local_metadata.billing_address_id));
      break;
    }
    case WalletMetadataSpecifics::UNKNOWN: {
      NOTREACHED();
      break;
    }
  }

  return entity_data;
}

// Returns AutofillMetadata for |specifics|.
AutofillMetadata CreateAutofillMetadataFromWalletMetadataSpecifics(
    const WalletMetadataSpecifics& specifics) {
  AutofillMetadata metadata;
  metadata.id = GetMetadataIdForSpecificsId(specifics.id());
  metadata.use_count = specifics.use_count();
  metadata.use_date = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(specifics.use_date()));

  switch (specifics.type()) {
    case WalletMetadataSpecifics::ADDRESS:
      metadata.has_converted = specifics.address_has_converted();
      break;
    case WalletMetadataSpecifics::CARD:
      metadata.billing_address_id =
          GetBase64DecodedId(specifics.card_billing_address_id());
      break;
    case WalletMetadataSpecifics::UNKNOWN:
      break;
  }

  return metadata;
}

bool HasLocalBillingAddress(const AutofillMetadata& metadata) {
  return metadata.billing_address_id.size() == kLocalGuidSize;
}

bool IsRemoteBillingAddressEqualOrBetter(const AutofillMetadata& local,
                                         const AutofillMetadata& remote) {
  // If local is empty, remote is better (or equal). Otherwise, if remote is
  // empty, local is better.
  if (local.billing_address_id.empty()) {
    return true;
  } else if (remote.billing_address_id.empty()) {
    return false;
  }
  // Now we need to decide between non-empty profiles. Prefer id's pointing to
  // local profiles over ids of non-local profiles.
  if (HasLocalBillingAddress(local) != HasLocalBillingAddress(remote)) {
    return HasLocalBillingAddress(remote);
  }
  // For both local / both remote, we prefer the more recently used.
  return remote.use_date >= local.use_date;
}

AutofillMetadata MergeMetadata(WalletMetadataSpecifics::Type type,
                               const AutofillMetadata& local,
                               const AutofillMetadata& remote) {
  AutofillMetadata merged;
  DCHECK_EQ(local.id, remote.id);
  merged.id = local.id;

  switch (type) {
    case WalletMetadataSpecifics::ADDRESS:
      merged.has_converted = local.has_converted || remote.has_converted;
      break;
    case WalletMetadataSpecifics::CARD:
      if (IsRemoteBillingAddressEqualOrBetter(local, remote)) {
        merged.billing_address_id = remote.billing_address_id;
      } else {
        merged.billing_address_id = local.billing_address_id;
      }
      break;
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED();
      break;
  }

  // Special case for local models with a use_count of one. This means the local
  // model was only created, never used. The remote model should always be
  // preferred.
  // This situation can happen for new Chromium instances where there is no data
  // yet on disk, making the use_date artificially high. Once the metadata sync
  // kicks in, we should use that value.
  if (local.use_count == 1) {
    merged.use_count = remote.use_count;
    merged.use_date = remote.use_date;
  } else {
    merged.use_count = std::max(local.use_count, remote.use_count);
    merged.use_date = std::max(local.use_date, remote.use_date);
  }
  return merged;
}

// Metadata is worth updating if its value is "newer" then before; here "newer"
// is the ordering of legal state transitions that metadata can take that is
// defined below.
bool IsMetadataWorthUpdating(AutofillMetadata existing_entry,
                             AutofillMetadata new_entry) {
  if (existing_entry.use_count < new_entry.use_count &&
      existing_entry.use_date < new_entry.use_date) {
    return true;
  }
  // For the following type-specific fields, we don't have to distinguish the
  // type of metadata as both entries must be of the same type and therefore
  // irrelevant values are default, thus equal.

  // It is only legal to move from non-converted to converted. Do not accept
  // the other transition.
  if (!existing_entry.has_converted && new_entry.has_converted) {
    return true;
  }
  if (existing_entry.billing_address_id != new_entry.billing_address_id) {
    return true;
  }
  return false;
}

bool AddServerMetadata(AutofillTable* table,
                       WalletMetadataSpecifics::Type type,
                       const AutofillMetadata& metadata) {
  switch (type) {
    case WalletMetadataSpecifics::ADDRESS:
      return table->AddServerAddressMetadata(metadata);
    case WalletMetadataSpecifics::CARD:
      return table->AddServerCardMetadata(metadata);
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED();
      return false;
  }
}

bool RemoveServerMetadata(AutofillTable* table,
                          WalletMetadataSpecifics::Type type,
                          const std::string& id) {
  switch (type) {
    case WalletMetadataSpecifics::ADDRESS:
      return table->RemoveServerAddressMetadata(id);
    case WalletMetadataSpecifics::CARD:
      return table->RemoveServerCardMetadata(id);
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED();
      return false;
  }
}

bool UpdateServerMetadata(AutofillTable* table,
                          WalletMetadataSpecifics::Type type,
                          const AutofillMetadata& metadata) {
  // TODO: Create UpdateServerAddressMetadata() that takes metadata as arg.
  switch (type) {
    case WalletMetadataSpecifics::ADDRESS:
      return table->UpdateServerAddressMetadata(metadata);
    case WalletMetadataSpecifics::CARD:
      return table->UpdateServerCardMetadata(metadata);
    case WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED();
      return false;
  }
}

}  // namespace

// static
void AutofillWalletMetadataSyncBridge::CreateForWebDataServiceAndBackend(
    const std::string& app_locale,
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletMetadataSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletMetadataSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_WALLET_METADATA,
              /*dump_stack=*/base::RepeatingClosure()),
          web_data_backend));
}

// static
AutofillWalletMetadataSyncBridge*
AutofillWalletMetadataSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletMetadataSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletMetadataSyncBridgeUserDataKey));
}

AutofillWalletMetadataSyncBridge::AutofillWalletMetadataSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend),
      scoped_observer_(this),
      track_wallet_data_(false),
      weak_ptr_factory_(this) {
  DCHECK(web_data_backend_);
  scoped_observer_.Add(web_data_backend_);

  LoadDataCacheAndMetadata();
}

AutofillWalletMetadataSyncBridge::~AutofillWalletMetadataSyncBridge() {}

void AutofillWalletMetadataSyncBridge::OnWalletDataTrackingStateChanged(
    bool is_tracking) {
  track_wallet_data_ = is_tracking;
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletMetadataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_WALLET_METADATA);
}

base::Optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // First upload local entities that are not mentioned in |entity_data|.
  UploadInitialLocalData(metadata_change_list.get(), entity_data);

  return MergeRemoteChanges(std::move(metadata_change_list),
                            std::move(entity_data));
}

base::Optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  return MergeRemoteChanges(std::move(metadata_change_list),
                            std::move(entity_data));
}

void AutofillWalletMetadataSyncBridge::GetData(StorageKeyList storage_keys,
                                               DataCallback callback) {
  // Build a set out of the list to allow quick lookup.
  std::unordered_set<std::string> storage_keys_set(storage_keys.begin(),
                                                   storage_keys.end());
  GetDataImpl(std::move(storage_keys_set), std::move(callback));
}

void AutofillWalletMetadataSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  // Get all data by not providing any |storage_keys| filter.
  GetDataImpl(/*storage_keys=*/base::nullopt, std::move(callback));
}

std::string AutofillWalletMetadataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  const WalletMetadataSpecifics& remote_metadata =
      entity_data.specifics.wallet_metadata();
  return GetClientTagForSpecificsId(remote_metadata.type(),
                                    remote_metadata.id());
}

std::string AutofillWalletMetadataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetStorageKeyForWalletMetadataTypeAndSpecificsId(
      entity_data.specifics.wallet_metadata().type(),
      entity_data.specifics.wallet_metadata().id());
}

void AutofillWalletMetadataSyncBridge::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  // Skip local profiles (if possible, i.e. if it is not a deletion where
  // data_model() is not set).
  if (change.data_model() &&
      change.data_model()->record_type() != AutofillProfile::SERVER_PROFILE) {
    return;
  }
  LocalMetadataChanged(WalletMetadataSpecifics::ADDRESS, change);
}

void AutofillWalletMetadataSyncBridge::CreditCardChanged(
    const CreditCardChange& change) {
  LocalMetadataChanged(WalletMetadataSpecifics::CARD, change);
}

AutofillTable* AutofillWalletMetadataSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

void AutofillWalletMetadataSyncBridge::LoadDataCacheAndMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  // Load the data cache (both addresses and cards into the same cache, the keys
  // in the cache never overlap).
  std::map<std::string, AutofillMetadata> addresses_metadata;
  std::map<std::string, AutofillMetadata> cards_metadata;
  if (!GetAutofillTable()->GetServerAddressesMetadata(&addresses_metadata) ||
      !GetAutofillTable()->GetServerCardsMetadata(&cards_metadata)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill data from WebDatabase."});
    return;
  }
  for (const auto& it : addresses_metadata) {
    cache_[GetStorageKeyForWalletMetadataTypeAndId(
        WalletMetadataSpecifics::ADDRESS, it.first)] = it.second;
  }
  for (const auto& it : cards_metadata) {
    cache_[GetStorageKeyForWalletMetadataTypeAndId(
        WalletMetadataSpecifics::CARD, it.first)] = it.second;
  }

  // Load the metadata and send to the processor.
  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(syncer::AUTOFILL_WALLET_METADATA,
                                              batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill metadata from WebDatabase."});
    return;
  }

  change_processor()->ModelReadyToSync(std::move(batch));
}

void AutofillWalletMetadataSyncBridge::GetDataImpl(
    base::Optional<std::unordered_set<std::string>> storage_keys_set,
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const auto& pair : cache_) {
    const std::string& storage_key = pair.first;
    const AutofillMetadata& metadata = pair.second;
    TypeAndMetadataId parsed_storage_key =
        ParseWalletMetadataStorageKey(storage_key);
    if (!storage_keys_set ||
        base::ContainsKey(*storage_keys_set, storage_key)) {
      batch->Put(storage_key, CreateEntityDataFromAutofillMetadata(
                                  parsed_storage_key.type, metadata));
    }
  }

  std::move(callback).Run(std::move(batch));
}

void AutofillWalletMetadataSyncBridge::UploadInitialLocalData(
    syncer::MetadataChangeList* metadata_change_list,
    const syncer::EntityChangeList& entity_data) {
  // First, make a copy of all local storage keys.
  std::set<std::string> local_keys_to_upload;
  for (const auto& it : cache_) {
    local_keys_to_upload.insert(it.first);
  }
  // Strip |local_keys_to_upload| of the keys of data provided by the server.
  for (const EntityChange& change : entity_data) {
    DCHECK_EQ(change.type(), EntityChange::ACTION_ADD)
        << "Illegal change; can only be called during initial MergeSyncData()";
    local_keys_to_upload.erase(change.storage_key());
  }
  // Upload the remaining storage keys
  for (const std::string& storage_key : local_keys_to_upload) {
    TypeAndMetadataId parsed_storage_key =
        ParseWalletMetadataStorageKey(storage_key);
    change_processor()->Put(storage_key,
                            CreateEntityDataFromAutofillMetadata(
                                parsed_storage_key.type, cache_[storage_key]),
                            metadata_change_list);
  }
}

base::Optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::MergeRemoteChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  bool is_any_local_modified = false;

  AutofillTable* table = GetAutofillTable();

  for (const EntityChange& change : entity_data) {
    TypeAndMetadataId parsed_storage_key =
        ParseWalletMetadataStorageKey(change.storage_key());
    switch (change.type()) {
      case EntityChange::ACTION_ADD:
      case EntityChange::ACTION_UPDATE: {
        const WalletMetadataSpecifics& specifics =
            change.data().specifics.wallet_metadata();
        AutofillMetadata remote =
            CreateAutofillMetadataFromWalletMetadataSpecifics(specifics);
        auto it = cache_.find(change.storage_key());
        base::Optional<AutofillMetadata> local = base::nullopt;
        if (it != cache_.end()) {
          local = it->second;
        }

        if (!local) {
          cache_[change.storage_key()] = remote;
          is_any_local_modified |= AddServerMetadata(
              GetAutofillTable(), parsed_storage_key.type, remote);
          continue;
        }

        // Resolve the conflict between the local and the newly received remote.
        AutofillMetadata merged =
            MergeMetadata(parsed_storage_key.type, *local, remote);
        if (merged != *local) {
          cache_[change.storage_key()] = merged;
          is_any_local_modified |=
              UpdateServerMetadata(table, parsed_storage_key.type, merged);
        }
        if (merged != remote) {
          change_processor()->Put(change.storage_key(),
                                  CreateEntityDataFromAutofillMetadata(
                                      parsed_storage_key.type, merged),
                                  metadata_change_list.get());
        }
        break;
      }
      case EntityChange::ACTION_DELETE: {
        cache_.erase(change.storage_key());
        is_any_local_modified |= RemoveServerMetadata(
            table, parsed_storage_key.type, parsed_storage_key.metadata_id);
        break;
      }
    }
  }

  if (is_any_local_modified) {
    web_data_backend_->NotifyOfMultipleAutofillChanges();
  }
  return base::nullopt;
}

template <class DataType>
void AutofillWalletMetadataSyncBridge::LocalMetadataChanged(
    WalletMetadataSpecifics::Type type,
    AutofillDataModelChange<DataType> change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string& metadata_id = change.key();
  std::string storage_key =
      GetStorageKeyForWalletMetadataTypeAndId(type, metadata_id);
  std::unique_ptr<MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  switch (change.type()) {
    case AutofillProfileChange::EXPIRE:
      NOTREACHED() << "EXPIRE change is not allowed for wallet entities";
      return;
    case AutofillProfileChange::REMOVE:
      if (RemoveServerMetadata(GetAutofillTable(), type, metadata_id)) {
        cache_.erase(storage_key);
        // Send up deletion only if we had this entry in the DB. It is not there
        // if (i) it was previously deleted by a remote deletion or (ii) this is
        // notification for a LOCAL_PROFILE (which have non-overlapping
        // storage_keys).
        change_processor()->Delete(storage_key, metadata_change_list.get());
      }
      return;
    case AutofillProfileChange::ADD:
    case AutofillProfileChange::UPDATE:
      DCHECK(change.data_model());

      AutofillMetadata new_entry = change.data_model()->GetMetadata();
      auto it = cache_.find(storage_key);
      base::Optional<AutofillMetadata> existing_entry = base::nullopt;
      if (it != cache_.end()) {
        existing_entry = it->second;
      }

      if (existing_entry &&
          !IsMetadataWorthUpdating(*existing_entry, new_entry)) {
        // Skip changes that are outdated, etc. (changes that would result in
        // inferior metadata compared to what we have now).
        return;
      }

      cache_[storage_key] = new_entry;
      if (existing_entry) {
        UpdateServerMetadata(GetAutofillTable(), type, new_entry);
      } else {
        AddServerMetadata(GetAutofillTable(), type, new_entry);
      }

      change_processor()->Put(
          storage_key, CreateEntityDataFromAutofillMetadata(type, new_entry),
          metadata_change_list.get());
      return;
  }
}

}  // namespace autofill
