// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TYPED_URL_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_TYPED_URL_MODEL_ASSOCIATOR_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "base/task.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"

class GURL;
class MessageLoop;
class ProfileSyncService;

namespace history {
class HistoryBackend;
class URLRow;
};

namespace sync_api {
class WriteNode;
class WriteTransaction;
};

namespace browser_sync {

extern const char kTypedUrlTag[];

// Contains all model association related logic:
// * Algorithm to associate typed_url model and sync model.
// * Persisting model associations and loading them back.
// We do not check if we have local data before this run; we always
// merge and sync.
class TypedUrlModelAssociator
  : public AbortablePerDataTypeAssociatorInterface<std::string, std::string> {
 public:
  typedef std::vector<history::URLRow> TypedUrlVector;
  typedef std::vector<std::pair<history::URLID, history::URLRow> >
      TypedUrlUpdateVector;
  typedef std::vector<std::pair<GURL, std::vector<history::VisitInfo> > >
      TypedUrlVisitVector;

  static syncable::ModelType model_type() { return syncable::TYPED_URLS; }
  TypedUrlModelAssociator(ProfileSyncService* sync_service,
                          history::HistoryBackend* history_backend);
  virtual ~TypedUrlModelAssociator();

  // PerDataTypeAssociatorInterface implementation.
  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual bool AssociateModels(SyncError* error) OVERRIDE;

  // Delete all typed url nodes.
  bool DeleteAllNodes(sync_api::WriteTransaction* trans);

  // Clears all associations.
  virtual bool DisassociateModels(SyncError* error) OVERRIDE;

  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) OVERRIDE;

  virtual bool CryptoReadyIfNecessary() OVERRIDE;

  // Not implemented.
  virtual const std::string* GetChromeNodeFromSyncId(int64 sync_id) OVERRIDE;

  // Not implemented.
  virtual bool InitSyncNodeFromChromeId(const std::string& node_id,
                                        sync_api::BaseNode* sync_node) OVERRIDE;

  // Returns the sync id for the given typed_url name, or sync_api::kInvalidId
  // if the typed_url name is not associated to any sync id.
  virtual int64 GetSyncIdFromChromeId(const std::string& node_id) OVERRIDE;

  // Associates the given typed_url name with the given sync id.
  virtual void Associate(const std::string* node, int64 sync_id) OVERRIDE;

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id) OVERRIDE;

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

  bool WriteToHistoryBackend(const TypedUrlVector* new_urls,
                             const TypedUrlUpdateVector* updated_urls,
                             const TypedUrlVisitVector* new_visits,
                             const history::VisitVector* deleted_visits);

  // Given a typed URL in the sync DB, looks for an existing entry in the
  // local history DB and generates a list of visits to add to the
  // history DB to bring it up to date (avoiding duplicates).
  // Updates the passed |visits_to_add| and |visits_to_remove| vectors with the
  // visits to add to/remove from the history DB, and adds a new entry to either
  // |updated_urls| or |new_urls| depending on whether the URL already existed
  // in the history DB.
  // Returns false if we encountered an error trying to access the history DB.
  bool UpdateFromSyncDB(const sync_pb::TypedUrlSpecifics& typed_url,
                        TypedUrlVisitVector* visits_to_add,
                        history::VisitVector* visits_to_remove,
                        TypedUrlUpdateVector* updated_urls,
                        TypedUrlVector* new_urls);

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
                              sync_api::WriteNode* node);

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

  // Helper function that fetches visits from the history DB corresponding to
  // the passed URL. This function compensates for the fact that the history DB
  // has rather poor data integrity (duplicate visits, visit timestamps that
  // don't match the last_visit timestamp, huge data sets that exhaust memory
  // when fetched, etc) by modifying the passed |url| object and |visits|
  // vector.
  // Returns false if we could not fetch the visits for the passed URL.
  static bool FixupURLAndGetVisits(history::HistoryBackend* backend,
                                   history::URLRow* url,
                                   history::VisitVector* visits);

  // Updates the passed |url_row| based on the values in |specifics|. Fields
  // that are not contained in |specifics| (such as typed_count) are left
  // unchanged.
  static void UpdateURLRowFromTypedUrlSpecifics(
      const sync_pb::TypedUrlSpecifics& specifics, history::URLRow* url_row);

 private:
  typedef std::map<std::string, int64> TypedUrlToSyncIdMap;
  typedef std::map<int64, std::string> SyncIdToTypedUrlMap;

  // Makes sure that the node with the specified tag is already in our
  // association map.
  bool IsAssociated(const std::string& node_tag);

  // Helper function that determines if we should ignore a URL for the purposes
  // of sync, because it's import-only.
  bool ShouldIgnoreUrl(const history::URLRow& url,
                       const history::VisitVector& visits);

  ProfileSyncService* sync_service_;
  history::HistoryBackend* history_backend_;
  int64 typed_url_node_id_;

  MessageLoop* expected_loop_;

  TypedUrlToSyncIdMap id_map_;
  SyncIdToTypedUrlMap id_map_inverse_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TYPED_URL_MODEL_ASSOCIATOR_H_
