// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions manipulating syncable::Entries, intended for use by the
// syncer.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_UTIL_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_UTIL_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_id.h"

namespace browser_sync {

class Cryptographer;
class SyncEntity;

class SyncerUtil {
 public:
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

  // If the server sent down a client-tagged entry, or an entry whose
  // commit response was lost, it is necessary to update a local entry
  // with an ID that doesn't match the ID of the update.  Here, we
  // find the ID of such an entry, if it exists.  This function may
  // determine that |server_entry| should be dropped; if so, it returns
  // the null ID -- callers must handle this case.  When update application
  // should proceed normally with a new local entry, this function will
  // return server_entry.id(); the caller must create an entry with that
  // ID.  This function does not alter the database.
  static syncable::Id FindLocalIdToUpdate(
      syncable::BaseTransaction* trans,
      const SyncEntity& server_entry);

  static UpdateAttemptResponse AttemptToUpdateEntry(
      syncable::WriteTransaction* const trans,
      syncable::MutableEntry* const entry,
      ConflictResolver* resolver,
      Cryptographer* cryptographer);

  // Pass in name to avoid redundant UTF8 conversion.
  static void UpdateServerFieldsFromUpdate(
      syncable::MutableEntry* local_entry,
      const SyncEntity& server_entry,
      const std::string& name);

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

  static VerifyCommitResult ValidateCommitEntry(syncable::Entry* entry);

  static VerifyResult VerifyNewEntry(const SyncEntity& update,
                                     syncable::Entry* target,
                                     const bool deleted);

  // Assumes we have an existing entry; check here for updates that break
  // consistency rules.
  static VerifyResult VerifyUpdateConsistency(syncable::WriteTransaction* trans,
                                              const SyncEntity& update,
                                              syncable::MutableEntry* target,
                                              const bool deleted,
                                              const bool is_directory,
                                              syncable::ModelType model_type);

  // Assumes we have an existing entry; verify an update that seems to be
  // expressing an 'undelete'
  static VerifyResult VerifyUndelete(syncable::WriteTransaction* trans,
                                     const SyncEntity& update,
                                     syncable::MutableEntry* target);

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
  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncerUtil);
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
// TODO(sync): Fix this. No need to use two timescales.
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
