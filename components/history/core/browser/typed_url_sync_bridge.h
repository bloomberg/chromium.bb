// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNC_BRIDGE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNC_BRIDGE_H_

#include "components/history/core/browser/history_backend_observer.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/sync_error.h"

namespace history {

class TypedURLSyncBridge : public syncer::ModelTypeSyncBridge,
                           public history::HistoryBackendObserver {
 public:
  TypedURLSyncBridge(HistoryBackend* history_backend,
                     const ChangeProcessorFactory& change_processor_factory);
  ~TypedURLSyncBridge() override;

  // syncer::ModelTypeService implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityDataMap entity_data_map) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllData(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // history::HistoryBackendObserver:
  void OnURLVisited(history::HistoryBackend* history_backend,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsModified(history::HistoryBackend* history_backend,
                      const history::URLRows& changed_urls) override;
  void OnURLsDeleted(history::HistoryBackend* history_backend,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

 private:
  // The backend we're syncing local changes from and sync changes to.
  HistoryBackend* const history_backend_;

  // Since HistoryBackend use SequencedTaskRunner, so should use SequenceChecker
  // here.
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(TypedURLSyncBridge);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNC_BRIDGE_H_
