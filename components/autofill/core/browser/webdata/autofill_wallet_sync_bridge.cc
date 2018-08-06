// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

using sync_pb::AutofillWalletSpecifics;

namespace autofill {
namespace {

// Address to this variable used as the user data key.
static int kAutofillWalletSyncBridgeUserDataKey = 0;

std::string GetStorageKeyFromAutofillWalletSpecifics(
    const AutofillWalletSpecifics& specifics) {
  switch (specifics.type()) {
    case AutofillWalletSpecifics::MASKED_CREDIT_CARD:
      return specifics.masked_card().id();
    case AutofillWalletSpecifics::POSTAL_ADDRESS:
      return specifics.address().id();
    case AutofillWalletSpecifics::CUSTOMER_DATA:
      return specifics.customer_data().id();
    case AutofillWalletSpecifics::UNKNOWN:
      NOTREACHED();
      return std::string();
  }
  return std::string();
}

}  // namespace

// static
void AutofillWalletSyncBridge::CreateForWebDataServiceAndBackend(
    const std::string& app_locale,
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_WALLET_DATA,
              /*dump_stack=*/base::RepeatingClosure()),
          web_data_backend));
}

// static
syncer::ModelTypeSyncBridge* AutofillWalletSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletSyncBridgeUserDataKey));
}

AutofillWalletSyncBridge::AutofillWalletSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend) {}

AutofillWalletSyncBridge::~AutofillWalletSyncBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_WALLET_DATA);
}

base::Optional<syncer::ModelError> AutofillWalletSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  return ApplySyncChanges(std::move(metadata_change_list),
                          std::move(entity_data));
}

base::Optional<syncer::ModelError> AutofillWalletSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

void AutofillWalletSyncBridge::GetData(StorageKeyList storage_keys,
                                       DataCallback callback) {
  NOTIMPLEMENTED();
}

void AutofillWalletSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string AutofillWalletSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet());

  switch (entity_data.specifics.autofill_wallet().type()) {
    case sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS:
      return "address-" + GetStorageKey(entity_data);
    case sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD:
      return "card-" + GetStorageKey(entity_data);
    case sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA:
      return "customer-" + GetStorageKey(entity_data);
    case sync_pb::AutofillWalletSpecifics::UNKNOWN:
      NOTREACHED();
      return std::string();
  }
  return std::string();
}

std::string AutofillWalletSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet());
  return GetStorageKeyFromAutofillWalletSpecifics(
      entity_data.specifics.autofill_wallet());
}

AutofillTable* AutofillWalletSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

}  // namespace autofill
