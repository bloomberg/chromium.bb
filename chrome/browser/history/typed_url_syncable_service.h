// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TYPED_URL_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_HISTORY_TYPED_URL_SYNCABLE_SERVICE_H_

#include <set>
#include <vector>

#include "chrome/browser/history/history_notifications.h"
#include "components/history/core/browser/history_types.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"
#include "ui/base/page_transition_types.h"

class GURL;
class TypedUrlSyncableServiceTest;

namespace base {
class MessageLoop;
};

namespace sync_pb {
class TypedUrlSpecifics;
};

namespace history {

class HistoryBackend;
class URLRow;

extern const char kTypedUrlTag[];

class TypedUrlSyncableService : public syncer::SyncableService {
 public:
  explicit TypedUrlSyncableService(HistoryBackend* history_backend);
  virtual ~TypedUrlSyncableService();

  static syncer::ModelType model_type() { return syncer::TYPED_URLS; }

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // Called directly by HistoryBackend when local url data changes.
  void OnUrlsModified(URLRows* changed_urls);
  void OnUrlVisited(ui::PageTransition transition, URLRow* row);
  void OnUrlsDeleted(bool all_history, bool expired, URLRows* rows);

 protected:
  void GetSyncedUrls(std::set<GURL>* urls) {
    urls->insert(synced_typed_urls_.begin(), synced_typed_urls_.end());
  }

 private:
  typedef std::vector<std::pair<URLID, URLRow> > TypedUrlUpdateVector;
  typedef std::vector<std::pair<GURL, std::vector<VisitInfo> > >
      TypedUrlVisitVector;

  // This is a helper map used only in Merge/Process* functions. The lifetime
  // of the iterator is longer than the map object.
  typedef std::map<GURL, std::pair<syncer::SyncChange::SyncChangeType,
                                   URLRows::iterator> > TypedUrlMap;
  // This is a helper map used to associate visit vectors from the history db
  // to the typed urls in the above map.
  typedef std::map<GURL, VisitVector> UrlVisitVectorMap;

  // Helper function that determines if we should ignore a URL for the purposes
  // of sync, because it contains invalid data.
  bool ShouldIgnoreUrl(const GURL& url);

  // Returns true if the caller should sync as a result of the passed visit
  // notification. We use this to throttle the number of sync changes we send
  // to the server so we don't hit the server for every
  // single typed URL visit.
  bool ShouldSyncVisit(ui::PageTransition transition, URLRow* row);

  // Utility routine that either updates an existing sync node or creates a
  // new one for the passed |typed_url| if one does not already exist. Returns
  // false and sets an unrecoverable error if the operation failed.
  bool CreateOrUpdateSyncNode(URLRow typed_url,
                              syncer::SyncChangeList* changes);

  void AddTypedUrlToChangeList(
    syncer::SyncChange::SyncChangeType change_type,
    const URLRow& row,
    const VisitVector& visits,
    std::string title,
    syncer::SyncChangeList* change_list);

  // Converts the passed URL information to a TypedUrlSpecifics structure for
  // writing to the sync DB.
  static void WriteToTypedUrlSpecifics(const URLRow& url,
                                       const VisitVector& visits,
                                       sync_pb::TypedUrlSpecifics* specifics);

  // Fetches visits from the history DB corresponding to the passed URL. This
  // function compensates for the fact that the history DB has rather poor data
  // integrity (duplicate visits, visit timestamps that don't match the
  // last_visit timestamp, huge data sets that exhaust memory when fetched,
  // etc) by modifying the passed |url| object and |visits| vector.
  // Returns false if we could not fetch the visits for the passed URL, and
  // tracks DB error statistics internally for reporting via UMA.
  virtual bool FixupURLAndGetVisits(URLRow* url,
                                    VisitVector* visits);

  // TODO(sync): Consider using "delete all" sync logic instead of in-memory
  // cache of typed urls. See http://crbug.com/231689.
  std::set<GURL> synced_typed_urls_;

  HistoryBackend* const history_backend_;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local url changes, since we triggered them.
  bool processing_syncer_changes_;

  // We receive ownership of |sync_processor_| and |error_handler_| in
  // MergeDataAndStartSyncing() and destroy them in StopSyncing().
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> sync_error_handler_;

  // Statistics for the purposes of tracking the percentage of DB accesses that
  // fail for each client via UMA.
  int num_db_accesses_;
  int num_db_errors_;

  base::MessageLoop* expected_loop_;

  FRIEND_TEST_ALL_PREFIXES(TypedUrlSyncableServiceTest,
                           AddLocalTypedUrlAndSync);
  FRIEND_TEST_ALL_PREFIXES(TypedUrlSyncableServiceTest,
                           UpdateLocalTypedUrlAndSync);
  FRIEND_TEST_ALL_PREFIXES(TypedUrlSyncableServiceTest,
                           LinkVisitLocalTypedUrlAndSync);
  FRIEND_TEST_ALL_PREFIXES(TypedUrlSyncableServiceTest,
                           TypedVisitLocalTypedUrlAndSync);
  FRIEND_TEST_ALL_PREFIXES(TypedUrlSyncableServiceTest,
                           DeleteLocalTypedUrlAndSync);
  FRIEND_TEST_ALL_PREFIXES(TypedUrlSyncableServiceTest,
                           DeleteAllLocalTypedUrlAndSync);
  FRIEND_TEST_ALL_PREFIXES(TypedUrlSyncableServiceTest,
                           MaxVisitLocalTypedUrlAndSync);
  FRIEND_TEST_ALL_PREFIXES(TypedUrlSyncableServiceTest,
                           ThrottleVisitLocalTypedUrlSync);

  DISALLOW_COPY_AND_ASSIGN(TypedUrlSyncableService);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TYPED_URL_SYNCABLE_SERVICE_H_
