// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/autofill_sync.pb.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

using base::Optional;
using base::UTF16ToUTF8;
using sync_pb::AutofillProfileSpecifics;
using syncer::EntityData;
using syncer::MetadataChangeList;
using syncer::ModelError;

namespace autofill {

namespace {

// Simplify checking for optional errors and returning only when present.
#define RETURN_IF_ERROR(x)                \
  if (Optional<ModelError> ret_val = x) { \
    return ret_val;                       \
  }

// Address to this variable used as the user data key.
static int kAutofillProfileSyncBridgeUserDataKey = 0;

}  // namespace

// static
void AutofillProfileSyncBridge::CreateForWebDataServiceAndBackend(
    const std::string& app_locale,
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillProfileSyncBridgeUserDataKey,
      std::make_unique<AutofillProfileSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_PROFILE,
              /*dump_stack=*/base::RepeatingClosure()),
          app_locale, web_data_backend));
}

// static
base::WeakPtr<syncer::ModelTypeSyncBridge>
AutofillProfileSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillProfileSyncBridge*>(
             web_data_service->GetDBUserData()->GetUserData(
                 &kAutofillProfileSyncBridgeUserDataKey))
      ->AsWeakPtr();
}

AutofillProfileSyncBridge::AutofillProfileSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    const std::string& app_locale,
    AutofillWebDataBackend* backend)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)),
      app_locale_(app_locale),
      web_data_backend_(backend),
      scoped_observer_(this) {
  DCHECK(web_data_backend_);

  scoped_observer_.Add(web_data_backend_);

  LoadMetadata();
}

AutofillProfileSyncBridge::~AutofillProfileSyncBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

std::unique_ptr<MetadataChangeList>
AutofillProfileSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_PROFILE);
}

Optional<syncer::ModelError> AutofillProfileSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(entity_data.empty());
  // TODO(jkrcal): Implement non-empty initial merge.
  return base::nullopt;
}

Optional<ModelError> AutofillProfileSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(entity_changes.empty());
  // TODO(jkrcal): Implement non-empty sync changes.
  return base::nullopt;
}

void AutofillProfileSyncBridge::GetData(StorageKeyList storage_keys,
                                        DataCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<std::unique_ptr<AutofillProfile>> entries;
  if (!GetAutofillTable()->GetAutofillProfiles(&entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  std::unordered_set<std::string> keys_set(storage_keys.begin(),
                                           storage_keys.end());
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<AutofillProfile>& entry : entries) {
    std::string key = GetStorageKeyFromAutofillProfile(*entry);
    if (base::ContainsKey(keys_set, key)) {
      batch->Put(key, CreateEntityDataFromAutofillProfile(*entry));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void AutofillProfileSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<std::unique_ptr<AutofillProfile>> entries;
  if (!GetAutofillTable()->GetAutofillProfiles(&entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<AutofillProfile>& entry : entries) {
    batch->Put(GetStorageKeyFromAutofillProfile(*entry),
               CreateEntityDataFromAutofillProfile(*entry));
  }
  std::move(callback).Run(std::move(batch));
}

void AutofillProfileSyncBridge::ActOnLocalChange(
    const AutofillProfileChange& change) {
  DCHECK((change.type() == AutofillProfileChange::REMOVE) ==
         (change.data_model() == nullptr));
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }
  if (change.data_model() &&
      change.data_model()->record_type() != AutofillProfile::LOCAL_PROFILE) {
    return;
  }

  auto metadata_change_list =
      std::make_unique<syncer::SyncMetadataStoreChangeList>(
          GetAutofillTable(), syncer::AUTOFILL_PROFILE);

  switch (change.type()) {
    case AutofillChange::ADD:
    case AutofillChange::UPDATE:
      change_processor()->Put(
          change.key(),
          CreateEntityDataFromAutofillProfile(*change.data_model()),
          metadata_change_list.get());
      break;
    case AutofillChange::REMOVE:
      change_processor()->Delete(change.key(), metadata_change_list.get());
      break;
  }

  if (Optional<ModelError> error = metadata_change_list->TakeError()) {
    change_processor()->ReportError(*error);
  }
}

void AutofillProfileSyncBridge::LoadMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(syncer::AUTOFILL_PROFILE,
                                              batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill metadata from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

std::string AutofillProfileSyncBridge::GetClientTag(
    const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_profile());
  // Must equal to guid of the entry. This is to maintain compatibility with the
  // previous sync integration (Directory and SyncableService).
  return entity_data.specifics.autofill_profile().guid();
}

std::string AutofillProfileSyncBridge::GetStorageKey(
    const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_profile());
  return GetStorageKeyFromAutofillProfileSpecifics(
      entity_data.specifics.autofill_profile());
}

void AutofillProfileSyncBridge::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ActOnLocalChange(change);
}

AutofillTable* AutofillProfileSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

}  // namespace autofill
