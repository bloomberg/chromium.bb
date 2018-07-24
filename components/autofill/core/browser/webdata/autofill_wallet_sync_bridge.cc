// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"

#include <utility>

#include "base/logging.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace autofill {

namespace {

// Address to this variable used as the user data key.
static int kAutofillWalletSyncBridgeUserDataKey = 0;

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
              /*dump_stack=*/base::RepeatingClosure())));
}

// static
syncer::ModelTypeSyncBridge* AutofillWalletSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletSyncBridgeUserDataKey));
}

AutofillWalletSyncBridge::AutofillWalletSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {}

AutofillWalletSyncBridge::~AutofillWalletSyncBridge() {}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
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
  NOTIMPLEMENTED();
  return "";
}

std::string AutofillWalletSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

}  // namespace autofill
