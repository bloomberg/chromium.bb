// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/sync_error.h"
#include "net/base/escape.h"

namespace {

const char kAutocompleteEntryNamespaceTag[] = "autofill_entry|";

void* UserDataKey() {
  // Use the address of a static that COMDAT folding won't ever collide
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

const std::string FormatStorageKey(const std::string name,
                                   const std::string value) {
  return net::EscapePath(name) + "|" + net::EscapePath(value);
}

}  // namespace

namespace autofill {

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

// syncer::ModelTypeService implementation.
std::unique_ptr<syncer::MetadataChangeList>
AutocompleteSyncBridge::CreateMetadataChangeList() {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return nullptr;
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
  NOTIMPLEMENTED();
}

void AutocompleteSyncBridge::GetAllData(DataCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
}

std::string AutocompleteSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill());

  const sync_pb::AutofillSpecifics specifics = entity_data.specifics.autofill();
  std::string storage_key =
      FormatStorageKey(specifics.name(), specifics.value());
  std::string prefix(kAutocompleteEntryNamespaceTag);
  return prefix + storage_key;
}

std::string AutocompleteSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  const sync_pb::AutofillSpecifics specifics = entity_data.specifics.autofill();
  return FormatStorageKey(specifics.name(), specifics.value());
}

// AutofillWebDataServiceObserverOnDBThread implementation.
void AutocompleteSyncBridge::AutofillEntriesChanged(
    const AutofillChangeList& changes) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

}  // namespace autofill