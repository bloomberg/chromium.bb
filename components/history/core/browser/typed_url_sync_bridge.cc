// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_sync_bridge.h"

#include "base/memory/ptr_util.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

namespace history {

TypedURLSyncBridge::TypedURLSyncBridge(
    HistoryBackend* history_backend,
    syncer::SyncMetadataStore* sync_metadata_store,
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeSyncBridge(change_processor_factory, syncer::TYPED_URLS),
      history_backend_(history_backend),
      sync_metadata_store_(sync_metadata_store) {
  DCHECK(history_backend_);
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(sync_metadata_store_);
  NOTIMPLEMENTED();
}

TypedURLSyncBridge::~TypedURLSyncBridge() {
  // TODO(gangwu): unregister as HistoryBackendObserver, can use  ScopedObserver
  // to do it.
}

std::unique_ptr<syncer::MetadataChangeList>
TypedURLSyncBridge::CreateMetadataChangeList() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return base::MakeUnique<syncer::SyncMetadataStoreChangeList>(
      sync_metadata_store_, syncer::TYPED_URLS);
}

base::Optional<syncer::ModelError> TypedURLSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityDataMap entity_data_map) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
  return {};
}

base::Optional<syncer::ModelError> TypedURLSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
  return {};
}

void TypedURLSyncBridge::GetData(StorageKeyList storage_keys,
                                 DataCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
}

void TypedURLSyncBridge::GetAllData(DataCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
}

// Must be exactly the value of GURL::spec() for backwards comparability with
// the previous (Directory + SyncableService) iteration of sync integration.
// This can be large but it is assumed that this is not held in memory at steady
// state.
std::string TypedURLSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(entity_data.specifics.has_typed_url())
      << "EntityData does not have typed urls specifics.";

  return entity_data.specifics.typed_url().url();
}

// Prefer to use URLRow::id() to uniquely identify entities when coordinating
// with sync because it has a significantly low memory cost than a URL.
std::string TypedURLSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
  return std::string();
}

void TypedURLSyncBridge::OnURLVisited(history::HistoryBackend* history_backend,
                                      ui::PageTransition transition,
                                      const history::URLRow& row,
                                      const history::RedirectList& redirects,
                                      base::Time visit_time) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
}

void TypedURLSyncBridge::OnURLsModified(
    history::HistoryBackend* history_backend,
    const history::URLRows& changed_urls) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
}

void TypedURLSyncBridge::OnURLsDeleted(history::HistoryBackend* history_backend,
                                       bool all_history,
                                       bool expired,
                                       const history::URLRows& deleted_rows,
                                       const std::set<GURL>& favicon_urls) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
}

}  // namespace history
