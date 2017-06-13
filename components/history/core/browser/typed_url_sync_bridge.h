// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNC_BRIDGE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNC_BRIDGE_H_

#include "base/scoped_observer.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/history/core/browser/typed_url_sync_metadata_database.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/sync_error.h"

namespace history {

class TypedURLSyncBridge : public syncer::ModelTypeSyncBridge,
                           public history::HistoryBackendObserver {
 public:
  // |sync_metadata_store| is owned by |history_backend|, and must outlive
  // TypedURLSyncBridge.
  TypedURLSyncBridge(HistoryBackend* history_backend,
                     TypedURLSyncMetadataDatabase* sync_metadata_store,
                     const ChangeProcessorFactory& change_processor_factory);
  ~TypedURLSyncBridge() override;

  // syncer::ModelTypeService implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
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

  // Must be called after creation and before any operations.
  void Init();

  // Returns the percentage of DB accesses that have resulted in an error.
  int GetErrorPercentage() const;

  // Return true if this function successfully converts the passed URL
  // information to a TypedUrlSpecifics structure for writing to the sync DB.
  static bool WriteToTypedUrlSpecifics(const URLRow& url,
                                       const VisitVector& visits,
                                       sync_pb::TypedUrlSpecifics* specifics)
      WARN_UNUSED_RESULT;

 private:
  friend class TypedURLSyncBridgeTest;

  // Synchronously load sync metadata from the TypedURLSyncMetadataDatabase and
  // pass it to the processor so that it can start tracking changes.
  void LoadMetadata();

  // Helper function that clears our error counters (used to reset stats after
  // merge so we can track merge errors separately).
  void ClearErrorStats();

  // Fetches visits from the history DB corresponding to the passed URL. This
  // function compensates for the fact that the history DB has rather poor data
  // integrity (duplicate visits, visit timestamps that don't match the
  // last_visit timestamp, huge data sets that exhaust memory when fetched,
  // expired visits that are not deleted by |ExpireHistoryBackend|, etc) by
  // modifying the passed |url| object and |visits| vector. The order of
  // |visits| will be from the oldest to the newest order.
  // Returns false in two cases.
  // 1. we could not fetch the visits for the passed URL, DB error.
  // 2. No visits for the passed url, or all the visits are expired.
  bool FixupURLAndGetVisits(URLRow* url, VisitVector* visits);

  // Create an EntityData by URL |row| and its visits |visits|.
  std::unique_ptr<syncer::EntityData> CreateEntityData(
      const URLRow& row,
      const VisitVector& visits);

  // A non-owning pointer to the backend, which we're syncing local changes from
  // and sync changes to.
  HistoryBackend* const history_backend_;

  // A non-owning pointer to the database, which is for storing typed urls sync
  // metadata and state.
  TypedURLSyncMetadataDatabase* const sync_metadata_database_;

  // Statistics for the purposes of tracking the percentage of DB accesses that
  // fail for each client via UMA.
  int num_db_accesses_;
  int num_db_errors_;

  // Since HistoryBackend use SequencedTaskRunner, so should use SequenceChecker
  // here.
  base::SequenceChecker sequence_checker_;

  // Tracks observed history backend, for receiving updates from history
  // backend.
  ScopedObserver<history::HistoryBackend, history::HistoryBackendObserver>
      history_backend_observer_;

  DISALLOW_COPY_AND_ASSIGN(TypedURLSyncBridge);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNC_BRIDGE_H_
