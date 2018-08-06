// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"

#include <unordered_map>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/optional.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace autofill {

namespace {

using sync_pb::WalletMetadataSpecifics;
using syncer::EntityData;

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

// Returns EntityData with common fields set based on |local_data_model|.
std::unique_ptr<EntityData> CreateEntityDataFromAutofillDataModel(
    const AutofillDataModel& local_data_model,
    WalletMetadataSpecifics::Type type,
    const std::string& specifics_id) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->non_unique_name = GetClientTagForSpecificsId(type, specifics_id);

  WalletMetadataSpecifics* metadata =
      entity_data->specifics.mutable_wallet_metadata();
  metadata->set_type(type);
  metadata->set_id(specifics_id);
  metadata->set_use_count(local_data_model.use_count());
  metadata->set_use_date(
      local_data_model.use_date().ToDeltaSinceWindowsEpoch().InMicroseconds());
  return entity_data;
}

// Returns EntityData for wallet_metadata for |local_profile|.
std::unique_ptr<EntityData> CreateWalletMetadataEntityDataFromAutofillProfile(
    const AutofillProfile& local_profile) {
  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillDataModel(
          local_profile, WalletMetadataSpecifics::ADDRESS,
          GetSpecificsIdForEntryServerId(local_profile.server_id()));

  WalletMetadataSpecifics* metadata =
      entity_data->specifics.mutable_wallet_metadata();
  metadata->set_address_has_converted(local_profile.has_converted());
  return entity_data;
}

// Returns EntityData for wallet_metadata for |local_card|.
std::unique_ptr<EntityData> CreateEntityDataFromCreditCard(
    const CreditCard& local_card) {
  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillDataModel(
          local_card, WalletMetadataSpecifics::CARD,
          GetSpecificsIdForEntryServerId(local_card.server_id()));

  WalletMetadataSpecifics* metadata =
      entity_data->specifics.mutable_wallet_metadata();
  // The strings must be in valid UTF-8 to sync.
  std::string billing_address_id;
  base::Base64Encode(local_card.billing_address_id(), &billing_address_id);
  metadata->set_card_billing_address_id(billing_address_id);
  return entity_data;
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
              syncer::AUTOFILL_WALLET_DATA,
              /*dump_stack=*/base::RepeatingClosure()),
          web_data_backend));
}

// static
syncer::ModelTypeSyncBridge*
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
      web_data_backend_(web_data_backend) {}

AutofillWalletMetadataSyncBridge::~AutofillWalletMetadataSyncBridge() {}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletMetadataSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
}

base::Optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<syncer::ModelError>
AutofillWalletMetadataSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
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
  return GetStorageKeyForSpecificsId(
      entity_data.specifics.wallet_metadata().id());
}

AutofillTable* AutofillWalletMetadataSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

void AutofillWalletMetadataSyncBridge::GetDataImpl(
    base::Optional<std::unordered_set<std::string>> storage_keys_set,
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> cards;
  if (!GetAutofillTable()->GetServerProfiles(&profiles) ||
      !GetAutofillTable()->GetServerCreditCards(&cards)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::unique_ptr<AutofillProfile>& entry : profiles) {
    std::string key = GetStorageKeyForEntryServerId(entry->server_id());
    if (!storage_keys_set || base::ContainsKey(*storage_keys_set, key)) {
      batch->Put(key,
                 CreateWalletMetadataEntityDataFromAutofillProfile(*entry));
    }
  }
  for (const std::unique_ptr<CreditCard>& entry : cards) {
    std::string key = GetStorageKeyForEntryServerId(entry->server_id());
    if (!storage_keys_set || base::ContainsKey(*storage_keys_set, key)) {
      batch->Put(GetStorageKeyForEntryServerId(entry->server_id()),
                 CreateEntityDataFromCreditCard(*entry));
    }
  }

  std::move(callback).Run(std::move(batch));
}

}  // namespace autofill
