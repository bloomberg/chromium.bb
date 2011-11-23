// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/conflict_resolver.h"

#include <algorithm>
#include <map>
#include <set>

#include "base/location.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/sessions/status_controller.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"

using std::map;
using std::set;
using syncable::BaseTransaction;
using syncable::Directory;
using syncable::Entry;
using syncable::Id;
using syncable::MutableEntry;
using syncable::ScopedDirLookup;
using syncable::WriteTransaction;

namespace browser_sync {

using sessions::ConflictProgress;
using sessions::StatusController;

namespace {

const int SYNC_CYCLES_BEFORE_ADMITTING_DEFEAT = 8;

// Enumeration of different conflict resolutions. Used for histogramming.
enum SimpleConflictResolutions {
  OVERWRITE_LOCAL,           // Resolved by overwriting local changes.
  OVERWRITE_SERVER,          // Resolved by overwriting server changes.
  UNDELETE,                  // Resolved by undeleting local item.
  IGNORE_ENCRYPTION,         // Resolved by ignoring an encryption-only server
                             // change. TODO(zea): implement and use this.
  CONFLICT_RESOLUTION_SIZE,
};

}  // namespace

ConflictResolver::ConflictResolver() {
}

ConflictResolver::~ConflictResolver() {
}

void ConflictResolver::IgnoreLocalChanges(MutableEntry* entry) {
  // An update matches local actions, merge the changes.
  // This is a little fishy because we don't actually merge them.
  // In the future we should do a 3-way merge.
  VLOG(1) << "Resolving conflict by ignoring local changes:" << entry;
  // With IS_UNSYNCED false, changes should be merged.
  entry->Put(syncable::IS_UNSYNCED, false);
}

void ConflictResolver::OverwriteServerChanges(WriteTransaction* trans,
                                              MutableEntry * entry) {
  // This is similar to an overwrite from the old client.
  // This is equivalent to a scenario where we got the update before we'd
  // made our local client changes.
  // TODO(chron): This is really a general property clobber. We clobber
  // the server side property. Perhaps we should actually do property merging.
  VLOG(1) << "Resolving conflict by ignoring server changes:" << entry;
  entry->Put(syncable::BASE_VERSION, entry->Get(syncable::SERVER_VERSION));
  entry->Put(syncable::IS_UNAPPLIED_UPDATE, false);
}

ConflictResolver::ProcessSimpleConflictResult
ConflictResolver::ProcessSimpleConflict(WriteTransaction* trans,
                                        const Id& id,
                                        StatusController* status) {
  MutableEntry entry(trans, syncable::GET_BY_ID, id);
  // Must be good as the entry won't have been cleaned up.
  CHECK(entry.good());
  // If an update fails, locally we have to be in a set or unsynced. We're not
  // in a set here, so we must be unsynced.
  if (!entry.Get(syncable::IS_UNSYNCED)) {
    return NO_SYNC_PROGRESS;
  }

  if (!entry.Get(syncable::IS_UNAPPLIED_UPDATE)) {
    if (!entry.Get(syncable::PARENT_ID).ServerKnows()) {
      VLOG(1) << "Item conflicting because its parent not yet committed. Id: "
              << id;
    } else {
      VLOG(1) << "No set for conflicting entry id " << id << ". There should "
                 "be an update/commit that will fix this soon. This message "
                 "should not repeat.";
    }
    return NO_SYNC_PROGRESS;
  }

  if (entry.Get(syncable::IS_DEL) && entry.Get(syncable::SERVER_IS_DEL)) {
    // we've both deleted it, so lets just drop the need to commit/update this
    // entry.
    entry.Put(syncable::IS_UNSYNCED, false);
    entry.Put(syncable::IS_UNAPPLIED_UPDATE, false);
    // we've made changes, but they won't help syncing progress.
    // METRIC simple conflict resolved by merge.
    return NO_SYNC_PROGRESS;
  }

  if (!entry.Get(syncable::SERVER_IS_DEL)) {
    // This logic determines "client wins" vs. "server wins" strategy picking.
    // TODO(nick): The current logic is arbitrary; instead, it ought to be made
    // consistent with the ModelAssociator behavior for a datatype.  It would
    // be nice if we could route this back to ModelAssociator code to pick one
    // of three options: CLIENT, SERVER, or MERGE.  Some datatypes (autofill)
    // are easily mergeable.
    // See http://crbug.com/77339.
    bool name_matches = entry.Get(syncable::NON_UNIQUE_NAME) ==
                        entry.Get(syncable::SERVER_NON_UNIQUE_NAME);
    bool parent_matches = entry.Get(syncable::PARENT_ID) ==
                                    entry.Get(syncable::SERVER_PARENT_ID);
    bool entry_deleted = entry.Get(syncable::IS_DEL);

    if (!entry_deleted && name_matches && parent_matches) {
      // TODO(zea): We may prefer to choose the local changes over the server
      // if we know the local changes happened before (or vice versa).
      // See http://crbug.com/76596
      VLOG(1) << "Resolving simple conflict, ignoring local changes for:"
              << entry;
      IgnoreLocalChanges(&entry);
      status->increment_num_local_overwrites();
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                                OVERWRITE_LOCAL,
                                CONFLICT_RESOLUTION_SIZE);
    } else {
      VLOG(1) << "Resolving simple conflict, overwriting server changes for:"
              << entry;
      OverwriteServerChanges(trans, &entry);
      status->increment_num_server_overwrites();
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                                OVERWRITE_SERVER,
                                CONFLICT_RESOLUTION_SIZE);
    }
    return SYNC_PROGRESS;
  } else {  // SERVER_IS_DEL is true
    // If a server deleted folder has local contents we should be in a set.
    if (entry.Get(syncable::IS_DIR)) {
      Directory::ChildHandles children;
      trans->directory()->GetChildHandlesById(trans,
                                              entry.Get(syncable::ID),
                                              &children);
      if (0 != children.size()) {
        VLOG(1) << "Entry is a server deleted directory with local contents, "
                   "should be in a set. (race condition).";
        return NO_SYNC_PROGRESS;
      }
    }

    // The entry is deleted on the server but still exists locally.
    if (!entry.Get(syncable::UNIQUE_CLIENT_TAG).empty()) {
      // If we've got a client-unique tag, we can undelete while retaining
      // our present ID.
      DCHECK_EQ(entry.Get(syncable::SERVER_VERSION), 0) << "For the server to "
          "know to re-create, client-tagged items should revert to version 0 "
          "when server-deleted.";
      OverwriteServerChanges(trans, &entry);
      status->increment_num_server_overwrites();
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                                OVERWRITE_SERVER,
                                CONFLICT_RESOLUTION_SIZE);
      // Clobber the versions, just in case the above DCHECK is violated.
      entry.Put(syncable::SERVER_VERSION, 0);
      entry.Put(syncable::BASE_VERSION, 0);
    } else {
      // Otherwise, we've got to undelete by creating a new locally
      // uncommitted entry.
      SyncerUtil::SplitServerInformationIntoNewEntry(trans, &entry);

      MutableEntry server_update(trans, syncable::GET_BY_ID, id);
      CHECK(server_update.good());
      CHECK(server_update.Get(syncable::META_HANDLE) !=
            entry.Get(syncable::META_HANDLE))
          << server_update << entry;
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                                UNDELETE,
                                CONFLICT_RESOLUTION_SIZE);
    }
    return SYNC_PROGRESS;
  }
}

ConflictResolver::ConflictSetCountMapKey ConflictResolver::GetSetKey(
    ConflictSet* set) {
  // TODO(sync): Come up with a better scheme for set hashing. This scheme
  // will make debugging easy.
  // If this call to sort is removed, we need to add one before we use
  // binary_search in ProcessConflictSet.
  sort(set->begin(), set->end());
  std::stringstream rv;
  for (ConflictSet::iterator i = set->begin() ; i != set->end() ; ++i )
    rv << *i << ".";
  return rv.str();
}

namespace {

bool AttemptToFixCircularConflict(WriteTransaction* trans,
                                  ConflictSet* conflict_set) {
  UMA_HISTOGRAM_COUNTS("Sync.ConflictFixCircularity", 1);
  ConflictSet::const_iterator i;
  for (i = conflict_set->begin() ; i != conflict_set->end() ; ++i) {
    MutableEntry entryi(trans, syncable::GET_BY_ID, *i);
    if (entryi.Get(syncable::PARENT_ID) ==
            entryi.Get(syncable::SERVER_PARENT_ID) ||
        !entryi.Get(syncable::IS_UNAPPLIED_UPDATE) ||
        !entryi.Get(syncable::IS_DIR)) {
      continue;
    }
    Id parentid = entryi.Get(syncable::SERVER_PARENT_ID);
    // Create the entry here as it's the only place we could ever get a parentid
    // that doesn't correspond to a real entry.
    Entry parent(trans, syncable::GET_BY_ID, parentid);
    if (!parent.good())  // server parent update not received yet
      continue;
    // This loop walks upwards from the server parent. If we hit the root (0)
    // all is well. If we hit the entry we're examining it means applying the
    // parent id would cause a loop. We don't need more general loop detection
    // because we know our local tree is valid.
    while (!parentid.IsRoot()) {
      Entry parent(trans, syncable::GET_BY_ID, parentid);
      CHECK(parent.good());
      if (parentid == *i)
        break;  // It's a loop.
      parentid = parent.Get(syncable::PARENT_ID);
    }
    if (parentid.IsRoot())
      continue;
    VLOG(1) << "Overwriting server changes to avoid loop: " << entryi;
    entryi.Put(syncable::BASE_VERSION, entryi.Get(syncable::SERVER_VERSION));
    entryi.Put(syncable::IS_UNSYNCED, true);
    entryi.Put(syncable::IS_UNAPPLIED_UPDATE, false);
    // METRIC conflict resolved by breaking dir loop.
    return true;
  }
  return false;
}

bool AttemptToFixUnsyncedEntryInDeletedServerTree(WriteTransaction* trans,
                                                  ConflictSet* conflict_set,
                                                  const Entry& entry) {
  if (!entry.Get(syncable::IS_UNSYNCED) || entry.Get(syncable::IS_DEL))
    return false;
  Id parentid = entry.Get(syncable::PARENT_ID);
  MutableEntry parent(trans, syncable::GET_BY_ID, parentid);
  if (!parent.good() || !parent.Get(syncable::IS_UNAPPLIED_UPDATE) ||
      !parent.Get(syncable::SERVER_IS_DEL) ||
      !binary_search(conflict_set->begin(), conflict_set->end(), parentid))
    return false;
  // We're trying to commit into a directory tree that's been deleted. To
  // solve this we recreate the directory tree.
  //
  // We do this in two parts, first we ensure the tree is unaltered since the
  // conflict was detected.
  Id id = parentid;
  while (!id.IsRoot()) {
    if (!binary_search(conflict_set->begin(), conflict_set->end(), id))
      break;
    Entry parent(trans, syncable::GET_BY_ID, id);
    if (!parent.good() || !parent.Get(syncable::IS_UNAPPLIED_UPDATE) ||
        !parent.Get(syncable::SERVER_IS_DEL))
      return false;
    id = parent.Get(syncable::PARENT_ID);
  }
  // Now we fix up the entries.
  id = parentid;
  while (!id.IsRoot()) {
    MutableEntry parent(trans, syncable::GET_BY_ID, id);
    if (!binary_search(conflict_set->begin(), conflict_set->end(), id))
      break;
    VLOG(1) << "Giving directory a new id so we can undelete it " << parent;
    ClearServerData(&parent);
    SyncerUtil::ChangeEntryIDAndUpdateChildren(trans, &parent,
        trans->directory()->NextId());
    parent.Put(syncable::BASE_VERSION, 0);
    parent.Put(syncable::IS_UNSYNCED, true);
    id = parent.Get(syncable::PARENT_ID);
    // METRIC conflict resolved by recreating dir tree.
  }
  return true;
}

// TODO(chron): needs unit test badly
bool AttemptToFixUpdateEntryInDeletedLocalTree(WriteTransaction* trans,
                                               ConflictSet* conflict_set,
                                               const Entry& entry) {
  if (!entry.Get(syncable::IS_UNAPPLIED_UPDATE) ||
      entry.Get(syncable::SERVER_IS_DEL))
    return false;
  Id parent_id = entry.Get(syncable::SERVER_PARENT_ID);
  MutableEntry parent(trans, syncable::GET_BY_ID, parent_id);
  if (!parent.good() || !parent.Get(syncable::IS_DEL) ||
    !binary_search(conflict_set->begin(), conflict_set->end(), parent_id)) {
    return false;
  }
  // We've deleted a directory tree that's got contents on the server. We
  // recreate the directory to solve the problem.
  //
  // We do this in two parts, first we ensure the tree is unaltered since
  // the conflict was detected.
  Id id = parent_id;
  // As we will be crawling the path of deleted entries there's a chance we'll
  // end up having to reparent an item as there will be an invalid parent.
  Id reroot_id = syncable::GetNullId();
  // Similarly crawling deleted items means we risk loops.
  int loop_detection = conflict_set->size();
  while (!id.IsRoot() && --loop_detection >= 0) {
    Entry parent(trans, syncable::GET_BY_ID, id);
    // If we get a bad parent, or a parent that's deleted on client and server
    // we recreate the hierarchy in the root.
    if (!parent.good()) {
      reroot_id = id;
      break;
    }
    CHECK(parent.Get(syncable::IS_DIR));
    if (!binary_search(conflict_set->begin(), conflict_set->end(), id)) {
      // We've got to an entry that's not in the set. If it has been deleted
      // between set building and this point in time we return false. If it had
      // been deleted earlier it would have been in the set.
      // TODO(sync): Revisit syncer code organization to see if conflict
      // resolution can be done in the same transaction as set building.
      if (parent.Get(syncable::IS_DEL))
        return false;
      break;
    }
    if (!parent.Get(syncable::IS_DEL) ||
        parent.Get(syncable::SERVER_IS_DEL) ||
        !parent.Get(syncable::IS_UNSYNCED)) {
      return false;
    }
    id = parent.Get(syncable::PARENT_ID);
  }
  // If we find we've been looping we re-root the hierarchy.
  if (loop_detection < 0) {
    if (id == entry.Get(syncable::ID))
      reroot_id = entry.Get(syncable::PARENT_ID);
    else
      reroot_id = id;
  }
  // Now we fix things up by undeleting all the folders in the item's path.
  id = parent_id;
  while (!id.IsRoot() && id != reroot_id) {
    if (!binary_search(conflict_set->begin(), conflict_set->end(), id)) {
      break;
    }
    MutableEntry entry(trans, syncable::GET_BY_ID, id);

    VLOG(1) << "Undoing our deletion of " << entry
            << ", will have name " << entry.Get(syncable::NON_UNIQUE_NAME);

    Id parent_id = entry.Get(syncable::PARENT_ID);
    if (parent_id == reroot_id) {
      parent_id = trans->root_id();
    }
    entry.Put(syncable::PARENT_ID, parent_id);
    entry.Put(syncable::IS_DEL, false);
    id = entry.Get(syncable::PARENT_ID);
    // METRIC conflict resolved by recreating dir tree.
  }
  return true;
}

bool AttemptToFixRemovedDirectoriesWithContent(WriteTransaction* trans,
                                               ConflictSet* conflict_set) {
  UMA_HISTOGRAM_COUNTS("Sync.ConflictFixRemovedDirectoriesWithContent", 1);
  ConflictSet::const_iterator i;
  for (i = conflict_set->begin() ; i != conflict_set->end() ; ++i) {
    Entry entry(trans, syncable::GET_BY_ID, *i);
    if (AttemptToFixUnsyncedEntryInDeletedServerTree(trans,
        conflict_set, entry)) {
      return true;
    }
    if (AttemptToFixUpdateEntryInDeletedLocalTree(trans, conflict_set, entry))
      return true;
  }
  return false;
}

}  // namespace

// TODO(sync): Eliminate conflict sets. They're not necessary.
bool ConflictResolver::ProcessConflictSet(WriteTransaction* trans,
                                          ConflictSet* conflict_set,
                                          int conflict_count) {
  int set_size = conflict_set->size();
  if (set_size < 2) {
    LOG(WARNING) << "Skipping conflict set because it has size " << set_size;
    // We can end up with sets of size one if we have a new item in a set that
    // we tried to commit transactionally. This should not be a persistent
    // situation.
    return false;
  }
  if (conflict_count < 3) {
    // Avoid resolving sets that could be the result of transient conflicts.
    // Transient conflicts can occur because the client or server can be
    // slightly out of date.
    return false;
  }

  VLOG(1) << "Fixing a set containing " << set_size << " items";

  // Fix circular conflicts.
  if (AttemptToFixCircularConflict(trans, conflict_set))
    return true;
  // Check for problems involving contents of removed folders.
  if (AttemptToFixRemovedDirectoriesWithContent(trans, conflict_set))
    return true;
  return false;
}

template <typename InputIt>
bool ConflictResolver::LogAndSignalIfConflictStuck(
    BaseTransaction* trans,
    int attempt_count,
    InputIt begin,
    InputIt end,
    StatusController* status) {
  if (attempt_count < SYNC_CYCLES_BEFORE_ADMITTING_DEFEAT) {
    return false;
  }

  // Don't signal stuck if we're not up to date.
  if (status->num_server_changes_remaining() > 0) {
    return false;
  }

  LOG(ERROR) << "[BUG] Conflict set cannot be resolved, has "
             << end - begin << " items:";
  for (InputIt i = begin ; i != end ; ++i) {
    Entry e(trans, syncable::GET_BY_ID, *i);
    if (e.good())
      LOG(ERROR) << "  " << e;
    else
      LOG(ERROR) << "  Bad ID:" << *i;
  }

  status->set_syncer_stuck(true);
  UMA_HISTOGRAM_COUNTS("Sync.SyncerConflictStuck", 1);

  return true;
  // TODO(sync): If we're stuck for a while we need to alert the user, clear
  // cache or reset syncing. At the very least we should stop trying something
  // that's obviously not working.
}

bool ConflictResolver::ResolveSimpleConflicts(
    const ScopedDirLookup& dir,
    const ConflictProgress& progress,
    sessions::StatusController* status) {
  WriteTransaction trans(FROM_HERE, syncable::SYNCER, dir);
  bool forward_progress = false;
  // First iterate over simple conflict items (those that belong to no set).
  set<Id>::const_iterator conflicting_item_it;
  for (conflicting_item_it = progress.ConflictingItemsBegin();
       conflicting_item_it != progress.ConflictingItemsEnd();
       ++conflicting_item_it) {
    Id id = *conflicting_item_it;
    map<Id, ConflictSet*>::const_iterator item_set_it =
        progress.IdToConflictSetFind(id);
    if (item_set_it == progress.IdToConflictSetEnd() ||
        0 == item_set_it->second) {
      // We have a simple conflict.
      switch (ProcessSimpleConflict(&trans, id, status)) {
        case NO_SYNC_PROGRESS:
          {
            int conflict_count = (simple_conflict_count_map_[id] += 2);
            LogAndSignalIfConflictStuck(&trans, conflict_count,
                                        &id, &id + 1, status);
            break;
          }
        case SYNC_PROGRESS:
          forward_progress = true;
          break;
      }
    }
  }
  // Reduce the simple_conflict_count for each item currently tracked.
  SimpleConflictCountMap::iterator i = simple_conflict_count_map_.begin();
  while (i != simple_conflict_count_map_.end()) {
    if (0 == --(i->second))
      simple_conflict_count_map_.erase(i++);
    else
      ++i;
  }
  return forward_progress;
}

bool ConflictResolver::ResolveConflicts(const ScopedDirLookup& dir,
                                        const ConflictProgress& progress,
                                        sessions::StatusController* status) {
  bool rv = false;
  if (ResolveSimpleConflicts(dir, progress, status))
    rv = true;
  WriteTransaction trans(FROM_HERE, syncable::SYNCER, dir);
  set<ConflictSet*>::const_iterator set_it;
  for (set_it = progress.ConflictSetsBegin();
       set_it != progress.ConflictSetsEnd();
       set_it++) {
    ConflictSet* conflict_set = *set_it;
    ConflictSetCountMapKey key = GetSetKey(conflict_set);
    conflict_set_count_map_[key] += 2;
    int conflict_count = conflict_set_count_map_[key];
    // Keep a metric for new sets.
    if (2 == conflict_count) {
      // METRIC conflict sets seen ++
    }
    // See if we should process this set.
    if (ProcessConflictSet(&trans, conflict_set, conflict_count)) {
      rv = true;
    }
    LogAndSignalIfConflictStuck(&trans, conflict_count,
                                conflict_set->begin(),
                                conflict_set->end(), status);
  }
  if (rv) {
    // This code means we don't signal that syncing is stuck when any conflict
    // resolution has occured.
    // TODO(sync): As this will also reduce our sensitivity to problem
    // conditions and increase the time for cascading resolutions we may have to
    // revisit this code later, doing something more intelligent.
    conflict_set_count_map_.clear();
    simple_conflict_count_map_.clear();
  }
  ConflictSetCountMap::iterator i = conflict_set_count_map_.begin();
  while (i != conflict_set_count_map_.end()) {
    if (0 == --i->second) {
      conflict_set_count_map_.erase(i++);
      // METRIC self resolved conflict sets ++.
    } else {
      ++i;
    }
  }
  return rv;
}

}  // namespace browser_sync
