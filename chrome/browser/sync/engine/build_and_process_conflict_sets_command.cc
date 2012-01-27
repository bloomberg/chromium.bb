// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/build_and_process_conflict_sets_command.h"

#include <set>
#include <string>
#include <sstream>
#include <vector>

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/update_applicator.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"

namespace browser_sync {

using sessions::ConflictProgress;
using sessions::StatusController;
using sessions::SyncSession;
using sessions::UpdateProgress;
using std::set;
using std::string;
using std::vector;

BuildAndProcessConflictSetsCommand::BuildAndProcessConflictSetsCommand() {}
BuildAndProcessConflictSetsCommand::~BuildAndProcessConflictSetsCommand() {}

std::set<ModelSafeGroup> BuildAndProcessConflictSetsCommand::GetGroupsToChange(
    const sessions::SyncSession& session) const {
  return session.GetEnabledGroupsWithConflicts();
}

SyncerError BuildAndProcessConflictSetsCommand::ModelChangingExecuteImpl(
    SyncSession* session) {
  syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                session->context()->account_name());
  if (!dir.good())
    return DIRECTORY_LOOKUP_FAILED;

  syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER, dir);
  BuildConflictSets(&trans,
      session->mutable_status_controller()->mutable_conflict_progress());

  return SYNCER_OK;
}

void BuildAndProcessConflictSetsCommand::BuildConflictSets(
    syncable::BaseTransaction* trans,
    ConflictProgress* conflict_progress) {
  conflict_progress->CleanupSets();
  set<syncable::Id>::const_iterator i =
      conflict_progress->ConflictingItemsBegin();
  while (i != conflict_progress->ConflictingItemsEnd()) {
    syncable::Entry entry(trans, syncable::GET_BY_ID, *i);
    if (!entry.good() ||
        (!entry.Get(syncable::IS_UNSYNCED) &&
         !entry.Get(syncable::IS_UNAPPLIED_UPDATE))) {
      // This can happen very rarely. It means we had a simply conflicting item
      // that randomly committed; its ID could have changed during the commit.
      // We drop the entry as it's no longer conflicting.
      conflict_progress->EraseConflictingItemById(*(i++));
      continue;
    }
    if (entry.ExistsOnClientBecauseNameIsNonEmpty() &&
       (entry.Get(syncable::IS_DEL) || entry.Get(syncable::SERVER_IS_DEL))) {
       // If we're deleted on client or server we can't be in a complex set.
      ++i;
      continue;
    }
    bool new_parent =
        entry.Get(syncable::PARENT_ID) != entry.Get(syncable::SERVER_PARENT_ID);
    if (new_parent)
      MergeSetsForIntroducedLoops(trans, &entry, conflict_progress);
    MergeSetsForNonEmptyDirectories(trans, &entry, conflict_progress);
    ++i;
  }
}

void BuildAndProcessConflictSetsCommand::MergeSetsForIntroducedLoops(
    syncable::BaseTransaction* trans, syncable::Entry* entry,
    ConflictProgress* conflict_progress) {
  // This code crawls up from the item in question until it gets to the root
  // or itself. If it gets to the root it does nothing. If it finds a loop all
  // moved unsynced entries in the list of crawled entries have their sets
  // merged with the entry.
  // TODO(sync): Build test cases to cover this function when the argument list
  // has settled.
  syncable::Id parent_id = entry->Get(syncable::SERVER_PARENT_ID);
  syncable::Entry parent(trans, syncable::GET_BY_ID, parent_id);
  if (!parent.good()) {
    return;
  }
  // Don't check for loop if the server parent is deleted.
  if (parent.Get(syncable::IS_DEL))
    return;
  vector<syncable::Id> conflicting_entries;
  while (!parent_id.IsRoot()) {
    syncable::Entry parent(trans, syncable::GET_BY_ID, parent_id);
    if (!parent.good()) {
      DVLOG(1) << "Bad parent in loop check, skipping. Bad parent id: "
               << parent_id << " entry: " << *entry;
      return;
    }
    if (parent.Get(syncable::IS_UNSYNCED) &&
        entry->Get(syncable::PARENT_ID) !=
            entry->Get(syncable::SERVER_PARENT_ID))
      conflicting_entries.push_back(parent_id);
    parent_id = parent.Get(syncable::PARENT_ID);
    if (parent_id == entry->Get(syncable::ID))
      break;
  }
  if (parent_id.IsRoot())
    return;
  for (size_t i = 0; i < conflicting_entries.size(); i++) {
    conflict_progress->MergeSets(entry->Get(syncable::ID),
                                 conflicting_entries[i]);
  }
}

namespace {

class ServerDeletedPathChecker {
 public:
  static bool CausingConflict(const syncable::Entry& e,
                              const syncable::Entry& log_entry) {
    CHECK(e.good()) << "Missing parent in path of: " << log_entry;
    if (e.Get(syncable::IS_UNAPPLIED_UPDATE) &&
        e.Get(syncable::SERVER_IS_DEL)) {
      CHECK(!e.Get(syncable::IS_DEL)) << " Inconsistency in local tree. "
                            "syncable::Entry: " << e << " Leaf: " << log_entry;
      return true;
    } else {
      CHECK(!e.Get(syncable::IS_DEL)) << " Deleted entry has children. "
                            "syncable::Entry: " << e << " Leaf: " << log_entry;
      return false;
    }
  }

  // returns 0 if we should stop investigating the path.
  static syncable::Id GetAndExamineParent(syncable::BaseTransaction* trans,
                                          const syncable::Id& id,
                                          const syncable::Id& check_id,
                                          const syncable::Entry& log_entry) {
    syncable::Entry parent(trans, syncable::GET_BY_ID, id);
    CHECK(parent.good()) << "Tree inconsitency, missing id" << id << " "
        << log_entry;
    syncable::Id parent_id = parent.Get(syncable::PARENT_ID);
    CHECK(parent_id != check_id) << "Loop in dir tree! "
      << log_entry << " " << parent;
    return parent_id;
  }
};

class LocallyDeletedPathChecker {
 public:
  static bool CausingConflict(const syncable::Entry& e,
                              const syncable::Entry& log_entry) {
    return e.good() && e.Get(syncable::IS_DEL) && e.Get(syncable::IS_UNSYNCED);
  }

  // returns 0 if we should stop investigating the path.
  static syncable::Id GetAndExamineParent(syncable::BaseTransaction* trans,
                                          const syncable::Id& id,
                                          const syncable::Id& check_id,
                                          const syncable::Entry& log_entry) {
    syncable::Entry parent(trans, syncable::GET_BY_ID, id);
    if (!parent.good())
      return syncable::GetNullId();
    syncable::Id parent_id = parent.Get(syncable::PARENT_ID);
    if (parent_id == check_id)
      return syncable::GetNullId();
    return parent_id;
  }
};

template <typename Checker>
void CrawlDeletedTreeMergingSets(syncable::BaseTransaction* trans,
                                 const syncable::Entry& entry,
                                 ConflictProgress* conflict_progress,
                                 Checker checker) {
  syncable::Id parent_id = entry.Get(syncable::PARENT_ID);
  syncable::Id double_step_parent_id = parent_id;
  // This block builds sets where we've got an entry in a directory the server
  // wants to delete.
  //
  // Here we're walking up the tree to find all entries that the pass checks
  // deleted. We can be extremely strict here as anything unexpected means
  // invariants in the local hierarchy have been broken.
  while (!parent_id.IsRoot()) {
    if (!double_step_parent_id.IsRoot()) {
      // Checks to ensure we don't loop.
      double_step_parent_id = checker.GetAndExamineParent(
          trans, double_step_parent_id, parent_id, entry);
      double_step_parent_id = checker.GetAndExamineParent(
          trans, double_step_parent_id, parent_id, entry);
    }
    syncable::Entry parent(trans, syncable::GET_BY_ID, parent_id);
    if (checker.CausingConflict(parent, entry)) {
      conflict_progress->MergeSets(entry.Get(syncable::ID),
                                   parent.Get(syncable::ID));
    } else {
      break;
    }
    parent_id = parent.Get(syncable::PARENT_ID);
  }
}

}  // namespace

void BuildAndProcessConflictSetsCommand::MergeSetsForNonEmptyDirectories(
    syncable::BaseTransaction* trans, syncable::Entry* entry,
    ConflictProgress* conflict_progress) {
  if (entry->Get(syncable::IS_UNSYNCED) && !entry->Get(syncable::IS_DEL)) {
    ServerDeletedPathChecker checker;
    CrawlDeletedTreeMergingSets(trans, *entry, conflict_progress, checker);
  }
  if (entry->Get(syncable::IS_UNAPPLIED_UPDATE) &&
      !entry->Get(syncable::SERVER_IS_DEL)) {
    syncable::Entry parent(trans, syncable::GET_BY_ID,
                           entry->Get(syncable::SERVER_PARENT_ID));
    syncable::Id parent_id = entry->Get(syncable::SERVER_PARENT_ID);
    if (!parent.good())
      return;
    LocallyDeletedPathChecker checker;
    if (!checker.CausingConflict(parent, *entry))
      return;
    conflict_progress->MergeSets(entry->Get(syncable::ID),
                                 parent.Get(syncable::ID));
    CrawlDeletedTreeMergingSets(trans, parent, conflict_progress, checker);
  }
}

}  // namespace browser_sync
