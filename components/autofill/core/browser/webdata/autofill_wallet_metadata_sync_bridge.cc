// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"

#include <utility>

#include "base/logging.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace autofill {

namespace {

// Address to this variable used as the user data key.
static int kAutofillWalletMetadataSyncBridgeUserDataKey = 0;

std::string GetClientTagForSpecificsId(
    sync_pb::WalletMetadataSpecifics::Type type,
    const std::string& specifics_id) {
  switch (type) {
    case sync_pb::WalletMetadataSpecifics::ADDRESS:
      return "address-" + specifics_id;
    case sync_pb::WalletMetadataSpecifics::CARD:
      return "card-" + specifics_id;
    case sync_pb::WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED();
      return "";
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
              syncer::AUTOFILL_WALLET_DATA,
              /*dump_stack=*/base::RepeatingClosure())));
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
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {}

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
  NOTIMPLEMENTED();
}

void AutofillWalletMetadataSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string AutofillWalletMetadataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  const sync_pb::WalletMetadataSpecifics& remote_metadata =
      entity_data.specifics.wallet_metadata();
  return GetClientTagForSpecificsId(remote_metadata.type(),
                                    remote_metadata.id());
}

std::string AutofillWalletMetadataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.wallet_metadata().id();
}

}  // namespace autofill
