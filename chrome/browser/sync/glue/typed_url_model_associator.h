// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TYPED_URL_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_TYPED_URL_MODEL_ASSOCIATOR_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync_driver/data_type_error_handler.h"
#include "components/sync_driver/model_associator.h"
#include "sync/protocol/typed_url_specifics.pb.h"

class GURL;
class ProfileSyncService;

namespace base {
class MessageLoop;
}

namespace history {
class HistoryBackend;
class URLRow;
};

namespace syncer {
class WriteNode;
class WriteTransaction;
};

namespace browser_sync {

// Contains all model association related logic:
// * Algorithm to associate typed_url model and sync model.
// * Persisting model associations and loading them back.
// We do not check if we have local data before this run; we always
// merge and sync.
class TypedUrlModelAssociator : public sync_driver::AssociatorInterface {
 public:
  typedef std::vector<std::pair<GURL, std::vector<history::VisitInfo> > >
      TypedUrlVisitVector;

  static syncer::ModelType model_type() { return syncer::TYPED_URLS; }
  TypedUrlModelAssociator(ProfileSyncService* sync_service,
                          history::HistoryBackend* history_backend,
                          sync_driver::DataTypeErrorHandler* error_handler);
  virtual ~TypedUrlModelAssociator();

  // AssociatorInterface implementation.
  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual syncer::SyncError AssociateModels(
      syncer::SyncMergeResult* local_merge_result,
      syncer::SyncMergeResult* syncer_merge_result) OVERRIDE;

  // Clears all associations.
  virtual syncer::SyncError DisassociateModels() OVERRIDE;

  // Called from the main thread, to abort the currently active model
  // association (for example, if we are shutting down).
  virtual void AbortAssociation() OVERRIDE;

  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) OVERRIDE;

  virtual bool CryptoReadyIfNecessary() OVERRIDE;

  // Delete all typed url nodes.
  bool DeleteAllNodes(syncer::WriteTransaction* trans);

  void WriteToHistoryBackend(const history::URLRows* new_urls,
                             const history::URLRows* updated_urls,
                             const TypedUrlVisitVector* new_visits,
                             const history::VisitVector* deleted_visits);

  // Given a typed URL in the sync DB, looks for an existing entry in the
  // local history DB and generates a list of visits to add to the
  // history DB to bring it up to date (avoiding duplicates).
  // Updates the passed |visits_to_add| and |visits_to_remove| vectors with the
  // visits to add to/remove from the history DB, and adds a new entry to either
  // |updated_urls| or |new_urls| depending on whether the URL already existed
  // in the history DB.
  void UpdateFromSyncDB(const sync_pb::TypedUrlSpecifics& typed_url,
                        TypedUrlVisitVector* visits_to_add,
                        history::VisitVector* visits_to_remove,
                        history::URLRows* updated_urls,
                        history::URLRows* new_urls);

  // Given a TypedUrlSpecifics object, removes all visits that are older than
  // the current expiration time. Note that this can result in having no visits
  // at all.
  sync_pb::TypedUrlSpecifics FilterExpiredVisits(
      const sync_pb::TypedUrlSpecifics& specifics);

  // Returns the percentage of DB accesses that have resulted in an error.
  int GetErrorPercentage() const;

  // Bitfield returned from MergeUrls to specify the result of the merge.
  typedef uint32 MergeResult;
  static const MergeResult DIFF_NONE                = 0;
  static const MergeResult DIFF_UPDATE_NODE         = 1 << 0;
  static const MergeResult DIFF_LOCAL_ROW_CHANGED   = 1 << 1;
  static const MergeResult DIFF_LOCAL_VISITS_ADDED  = 1 << 2;

  // Merges the URL information in |typed_url| with the URL information from the
  // history database in |url| and |visits|, and returns a bitmask with the
  // results of the merge:
  // DIFF_UPDATE_NODE - changes have been made to |new_url| and |visits| which
  //   should be persisted to the sync node.
  // DIFF_LOCAL_ROW_CHANGED - The history data in |new_url| should be persisted
  //   to the history DB.
  // DIFF_LOCAL_VISITS_ADDED - |new_visits| contains a list of visits that
  //   should be written to the history DB for this URL. Deletions are not
  //   written to the DB - each client is left to age out visits on their own.
  static MergeResult MergeUrls(const sync_pb::TypedUrlSpecifics& typed_url,
                               const history::URLRow& url,
                               history::VisitVector* visits,
                               history::URLRow* new_url,
                               std::vector<history::VisitInfo>* new_visits);
  static void WriteToSyncNode(const history::URLRow& url,
                              const history::VisitVector& visits,
                              syncer::WriteNode* node);

  // Diffs the set of visits between the history DB and the sync DB, using the
  // sync DB as the canonical copy. Result is the set of |new_visits| and
  // |removed_visits| that can be applied to the history DB to make it match
  // the sync DB version. |removed_visits| can be null if the caller does not
  // care about which visits to remove.
  static void DiffVisits(const history::VisitVector& old_visits,
                         const sync_pb::TypedUrlSpecifics& new_url,
                         std::vector<history::VisitInfo>* new_visits,
                         history::VisitVector* removed_visits);

  // Converts the passed URL information to a TypedUrlSpecifics structure for
  // writing to the sync DB
  static void WriteToTypedUrlSpecifics(const history::URLRow& url,
                                       const history::VisitVector& visits,
                                       sync_pb::TypedUrlSpecifics* specifics);

  // Fetches visits from the history DB corresponding to the passed URL. This
  // function compensates for the fact that the history DB has rather poor data
  // integrity (duplicate visits, visit timestamps that don't match the
  // last_visit timestamp, huge data sets that exhaust memory when fetched,
  // etc) by modifying the passed |url| object and |visits| vector.
  // Returns false if we could not fetch the visits for the passed URL, and
  // tracks DB error statistics internally for reporting via UMA.
  bool FixupURLAndGetVisits(history::URLRow* url,
                            history::VisitVector* visits);

  // Updates the passed |url_row| based on the values in |specifics|. Fields
  // that are not contained in |specifics| (such as typed_count) are left
  // unchanged.
  static void UpdateURLRowFromTypedUrlSpecifics(
      const sync_pb::TypedUrlSpecifics& specifics, history::URLRow* url_row);

  // Helper function that determines if we should ignore a URL for the purposes
  // of sync, because it contains invalid data.
  bool ShouldIgnoreUrl(const GURL& url);

 protected:
  // Helper function that clears our error counters (used to reset stats after
  // model association so we can track model association errors separately).
  // Overridden by tests.
  virtual void ClearErrorStats();

 private:

  // Helper routine that actually does the work of associating models.
  syncer::SyncError DoAssociateModels();

  // Helper function that determines if we should ignore a URL for the purposes
  // of sync, based on the visits the URL had.
  bool ShouldIgnoreVisits(const history::VisitVector& visits);

  ProfileSyncService* sync_service_;
  history::HistoryBackend* history_backend_;

  base::MessageLoop* expected_loop_;

  bool abort_requested_;
  base::Lock abort_lock_;

  // Guaranteed to outlive datatypes.
  sync_driver::DataTypeErrorHandler* error_handler_;

  // Statistics for the purposes of tracking the percentage of DB accesses that
  // fail for each client via UMA.
  int num_db_accesses_;
  int num_db_errors_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TYPED_URL_MODEL_ASSOCIATOR_H_
