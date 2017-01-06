// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/proto/autofill_sync.pb.h"
#include "components/autofill/core/browser/webdata/autofill_metadata_change_list.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_error.h"
#include "net/base/escape.h"

namespace autofill {

namespace {

const char kAutocompleteEntryNamespaceTag[] = "autofill_entry|";
const char kAutocompleteTagDelimiter[] = "|";

void* UserDataKey() {
  // Use the address of a static that COMDAT folding won't ever collide
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const AutofillEntry& entry) {
  auto entity_data = base::MakeUnique<syncer::EntityData>();
  entity_data->non_unique_name = base::UTF16ToUTF8(entry.key().name());
  sync_pb::AutofillSpecifics* autofill =
      entity_data->specifics.mutable_autofill();
  autofill->set_name(base::UTF16ToUTF8(entry.key().name()));
  autofill->set_value(base::UTF16ToUTF8(entry.key().value()));
  autofill->add_usage_timestamp(entry.date_created().ToInternalValue());
  if (entry.date_created() != entry.date_last_used())
    autofill->add_usage_timestamp(entry.date_last_used().ToInternalValue());
  return entity_data;
}

std::string BuildSerializedStorageKey(const std::string& name,
                                      const std::string& value) {
  AutofillSyncStorageKey proto;
  proto.set_name(name);
  proto.set_value(value);
  return proto.SerializeAsString();
}

std::string GetStorageKeyFromModel(const AutofillKey& key) {
  return BuildSerializedStorageKey(base::UTF16ToUTF8(key.name()),
                                   base::UTF16ToUTF8(key.value()));
}

}  // namespace

// static
void AutocompleteSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* web_data_backend) {
  web_data_service->GetDBUserData()->SetUserData(
      UserDataKey(),
      new AutocompleteSyncBridge(
          web_data_backend,
          base::Bind(&syncer::ModelTypeChangeProcessor::Create)));
}

// static
AutocompleteSyncBridge* AutocompleteSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutocompleteSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(UserDataKey()));
}

AutocompleteSyncBridge::AutocompleteSyncBridge(
    AutofillWebDataBackend* backend,
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeSyncBridge(change_processor_factory, syncer::AUTOFILL),
      web_data_backend_(backend),
      scoped_observer_(this) {
  DCHECK(web_data_backend_);

  scoped_observer_.Add(web_data_backend_);
}

AutocompleteSyncBridge::~AutocompleteSyncBridge() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

std::unique_ptr<syncer::MetadataChangeList>
AutocompleteSyncBridge::CreateMetadataChangeList() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::MakeUnique<AutofillMetadataChangeList>(GetAutofillTable(),
                                                      syncer::AUTOFILL);
}

syncer::SyncError AutocompleteSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityDataMap entity_data_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return syncer::SyncError();
}

syncer::SyncError AutocompleteSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return syncer::SyncError();
}

void AutocompleteSyncBridge::AutocompleteSyncBridge::GetData(
    StorageKeyList storage_keys,
    DataCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unordered_set<std::string> keys_set;
  for (const auto& key : storage_keys) {
    keys_set.insert(key);
  }

  auto batch = base::MakeUnique<syncer::MutableDataBatch>();
  std::vector<AutofillEntry> entries;
  GetAutofillTable()->GetAllAutofillEntries(&entries);
  for (const AutofillEntry& entry : entries) {
    std::string key = GetStorageKeyFromModel(entry.key());
    if (keys_set.find(key) != keys_set.end()) {
      batch->Put(key, CreateEntityData(entry));
    }
  }
  callback.Run(syncer::SyncError(), std::move(batch));
}

void AutocompleteSyncBridge::GetAllData(DataCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto batch = base::MakeUnique<syncer::MutableDataBatch>();
  std::vector<AutofillEntry> entries;
  GetAutofillTable()->GetAllAutofillEntries(&entries);
  for (const AutofillEntry& entry : entries) {
    batch->Put(GetStorageKeyFromModel(entry.key()), CreateEntityData(entry));
  }
  callback.Run(syncer::SyncError(), std::move(batch));
}

std::string AutocompleteSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill());
  const sync_pb::AutofillSpecifics specifics = entity_data.specifics.autofill();
  return std::string(kAutocompleteEntryNamespaceTag) +
         net::EscapePath(specifics.name()) +
         std::string(kAutocompleteTagDelimiter) +
         net::EscapePath(specifics.value());
}

std::string AutocompleteSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill());
  const sync_pb::AutofillSpecifics specifics = entity_data.specifics.autofill();
  return BuildSerializedStorageKey(specifics.name(), specifics.value());
}

// AutofillWebDataServiceObserverOnDBThread implementation.
void AutocompleteSyncBridge::AutofillEntriesChanged(
    const AutofillChangeList& changes) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
AutofillEntry AutocompleteSyncBridge::CreateAutofillEntry(
    const sync_pb::AutofillSpecifics& autofill_specifics) {
  AutofillKey key(base::UTF8ToUTF16(autofill_specifics.name()),
                  base::UTF8ToUTF16(autofill_specifics.value()));
  base::Time date_created, date_last_used;
  const google::protobuf::RepeatedField<int64_t>& timestamps =
      autofill_specifics.usage_timestamp();
  if (!timestamps.empty()) {
    date_created = base::Time::FromInternalValue(*timestamps.begin());
    date_last_used = base::Time::FromInternalValue(*timestamps.rbegin());
  }
  return AutofillEntry(key, date_created, date_last_used);
}

AutofillTable* AutocompleteSyncBridge::GetAutofillTable() const {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

}  // namespace autofill
