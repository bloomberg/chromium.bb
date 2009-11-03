// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions manipulating syncable::Entries, intended for use by the
// syncer.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_UTIL_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/sync_types.h"

namespace browser_sync {

class SyncEntity;

class SyncerUtil {
 public:
  // TODO(ncarter): Remove unique-in-parent title support and name conflicts.
  static syncable::Id GetNameConflictingItemId(
      syncable::BaseTransaction* trans,
      const syncable::Id& parent_id,
      const PathString& server_name);

  static void ChangeEntryIDAndUpdateChildren(
      syncable::WriteTransaction* trans,
      syncable::MutableEntry* entry,
      const syncable::Id& new_id,
      syncable::Directory::ChildHandles* children);

  // Returns the number of unsynced entries.
  static int GetUnsyncedEntries(syncable::BaseTransaction* trans,
                                std::vector<int64> *handles);

  static void ChangeEntryIDAndUpdateChildren(syncable::WriteTransaction* trans,
                                             syncable::MutableEntry* entry,
                                             const syncable::Id& new_id);

  static void AttemptReuniteLostCommitResponses(
      syncable::WriteTransaction* trans,
      const SyncEntity& server_entry,
      const std::string& client_id);

  static UpdateAttemptResponse AttemptToUpdateEntry(
      syncable::WriteTransaction* const trans,
      syncable::MutableEntry* const entry,
      ConflictResolver* resolver);

  static UpdateAttemptResponse AttemptToUpdateEntryWithoutMerge(
      syncable::WriteTransaction* const trans,
      syncable::MutableEntry* const entry,
      syncable::Id* const conflicting_id);

  // Pass in name to avoid redundant UTF8 conversion.
  static void UpdateServerFieldsFromUpdate(
      syncable::MutableEntry* local_entry,
      const SyncEntity& server_entry,
      const syncable::SyncName& name);

  static void ApplyExtendedAttributes(
      syncable::MutableEntry* local_entry,
      const SyncEntity& server_entry);

  // Creates a new Entry iff no Entry exists with the given id.
  static void CreateNewEntry(syncable::WriteTransaction *trans,
                             const syncable::Id& id);

  static bool ServerAndLocalEntriesMatch(syncable::Entry* entry);

  static void SplitServerInformationIntoNewEntry(
      syncable::WriteTransaction* trans,
      syncable::MutableEntry* entry);

  // This function is called on an entry when we can update the user-facing data
  // from the server data.
  static void UpdateLocalDataFromServerData(syncable::WriteTransaction* trans,
                                            syncable::MutableEntry* entry);

  static VerifyCommitResult ValidateCommitEntry(syncable::MutableEntry* entry);

  static VerifyResult VerifyNewEntry(const SyncEntity& entry,
                                     syncable::MutableEntry* same_id,
                                     const bool deleted);

  // Assumes we have an existing entry; check here for updates that break
  // consistency rules.
  static VerifyResult VerifyUpdateConsistency(syncable::WriteTransaction* trans,
                                              const SyncEntity& entry,
                                              syncable::MutableEntry* same_id,
                                              const bool deleted,
                                              const bool is_directory,
                                              const bool is_bookmark);

  // Assumes we have an existing entry; verify an update that seems to be
  // expressing an 'undelete'
  static VerifyResult VerifyUndelete(syncable::WriteTransaction* trans,
                                     const SyncEntity& entry,
                                     syncable::MutableEntry* same_id);

  // Compute a local predecessor position for |update_item|.  The position
  // is determined by the SERVER_POSITION_IN_PARENT value of |update_item|,
  // as well as the SERVER_POSITION_IN_PARENT values of any up-to-date
  // children of |parent_id|.
  static syncable::Id ComputePrevIdFromServerPosition(
      syncable::BaseTransaction* trans,
      syncable::Entry* update_item,
      const syncable::Id& parent_id);

  // Append |item|, followed by a chain of its predecessors selected by
  // |inclusion_filter|, to the |commit_ids| vector and tag them as included by
  // storing in the set |inserted_items|.  |inclusion_filter| (typically one of
  // IS_UNAPPLIED_UPDATE or IS_UNSYNCED) selects which type of predecessors to
  // include.  Returns true if |item| was added, and false if it was already in
  // the list.
  //
  // Use AddPredecessorsThenItem instead of this method if you want the
  // item to be the last, rather than first, item appended.
  static bool AddItemThenPredecessors(
      syncable::BaseTransaction* trans,
      syncable::Entry* item,
      syncable::IndexedBitField inclusion_filter,
      syncable::MetahandleSet* inserted_items,
      std::vector<syncable::Id>* commit_ids);

  // Exactly like AddItemThenPredecessors, except items are appended in the
  // reverse (and generally more useful) order: a chain of predecessors from
  // far to near, and finally the item.
  static void AddPredecessorsThenItem(
      syncable::BaseTransaction* trans,
      syncable::Entry* item,
      syncable::IndexedBitField inclusion_filter,
      syncable::MetahandleSet* inserted_items,
      std::vector<syncable::Id>* commit_ids);

  static void AddUncommittedParentsAndTheirPredecessors(
      syncable::BaseTransaction* trans,
      syncable::MetahandleSet* inserted_items,
      std::vector<syncable::Id>* commit_ids,
      syncable::Id parent_id);

  static void MarkDeletedChildrenSynced(
      const syncable::ScopedDirLookup &dir,
      std::set<syncable::Id>* deleted_folders);

  // Examine the up-to-date predecessors of this item according to the server
  // position, and then again according to the local position.  Return true
  // if they match.  For an up-to-date item, this should be the case.
  static bool ServerAndLocalOrdersMatch(syncable::Entry* entry);

 private:
  // Private ctor/dtor since this class shouldn't be instantiated.
  SyncerUtil() {}
  virtual ~SyncerUtil() {}
  DISALLOW_COPY_AND_ASSIGN(SyncerUtil);
};

#ifndef OS_WIN

// time.h on Linux and Mac both return seconds since the epoch, this should
// be converted to milliseconds.
inline int64 ServerTimeToClientTime(int64 server_time) {
  return server_time / GG_LONGLONG(1000);
}

inline int64 ClientTimeToServerTime(int64 client_time) {
  return client_time * GG_LONGLONG(1000);
}

// As we truncate server times on the client for posix and on the server for
// windows we need two ClientAndServerTimeMatch fucntions.
inline bool ClientAndServerTimeMatch(int64 client_time, int64 server_time) {
  // Compare at the coarser timescale (client)
  return client_time == ServerTimeToClientTime(server_time);
}
#else
// The sync server uses Java Times (ms since 1970)
// and the client uses FILETIMEs (ns since 1601) so we need to convert
// between the timescales.
inline int64 ServerTimeToClientTime(int64 server_time) {
  return server_time * GG_LONGLONG(10000) + GG_LONGLONG(116444736000000000);
}

inline int64 ClientTimeToServerTime(int64 client_time) {
  return (client_time - GG_LONGLONG(116444736000000000)) / GG_LONGLONG(10000);
}

inline bool ClientAndServerTimeMatch(int64 client_time, int64 server_time) {
  // Compare at the coarser timescale (server)
  return ClientTimeToServerTime(client_time) == server_time;
}
#endif

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_UTIL_H_
