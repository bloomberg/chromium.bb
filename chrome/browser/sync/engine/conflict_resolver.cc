// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.

#include "chrome/browser/sync/engine/conflict_resolver.h"

#include <map>
#include <set>

#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/path_helpers.h"

using std::map;
using std::set;
using syncable::BaseTransaction;
using syncable::Directory;
using syncable::Entry;
using syncable::Id;
using syncable::MutableEntry;
using syncable::Name;
using syncable::ScopedDirLookup;
using syncable::SyncName;
using syncable::WriteTransaction;

namespace browser_sync {

const int SYNC_CYCLES_BEFORE_ADMITTING_DEFEAT = 8;

ConflictResolver::ConflictResolver() {
}

ConflictResolver::~ConflictResolver() {
}

namespace {
// TODO(ncarter): Remove title/path conflicts and the code to resolve them.
// This is historical cruft that seems to be actually reached by some users.
inline PathString GetConflictPathnameBase(PathString base) {
  time_t time_since = time(NULL);
  struct tm* now = localtime(&time_since);
  // Use a fixed format as the locale's format may include '/' characters or
  // other illegal characters.
  PathString date = IntToPathString(now->tm_year + 1900);
  date.append(PSTR("-"));
  ++now->tm_mon;  // tm_mon is 0-based.
  if (now->tm_mon < 10)
    date.append(PSTR("0"));
  date.append(IntToPathString(now->tm_mon));
  date.append(PSTR("-"));
  if (now->tm_mday < 10)
    date.append(PSTR("0"));
  date.append(IntToPathString(now->tm_mday));
  return base + PSTR(" (Edited on ") + date + PSTR(")");
}

// TODO(ncarter): Remove title/path conflicts and the code to resolve them.
Name FindNewName(BaseTransaction* trans,
                 Id parent_id,
                 const SyncName& original_name) {
  const PathString name = original_name.value();
  // 255 is defined in our spec.
  const size_t allowed_length = kSyncProtocolMaxNameLengthBytes;
  // TODO(sync): How do we get length on other platforms? The limit is
  // checked in java on the server, so it's not the number of glyphs its the
  // number of 16 bit characters in the UTF-16 representation.

  // 10 characters for 32 bit numbers + 2 characters for brackets means 12
  // characters should be more than enough for the name. Doubling this ensures
  // that we will have enough space.
  COMPILE_ASSERT(kSyncProtocolMaxNameLengthBytes >= 24,
                 maximum_name_too_short);
  CHECK(name.length() <= allowed_length);

  if (!Entry(trans,
             syncable::GET_BY_PARENTID_AND_DBNAME,
             parent_id,
             name).good())
    return Name::FromSyncName(original_name);
  PathString base = name;
  PathString ext;
  PathString::size_type ext_index = name.rfind('.');
  if (PathString::npos != ext_index && 0 != ext_index &&
      name.length() - ext_index < allowed_length / 2) {
    base = name.substr(0, ext_index);
    ext = name.substr(ext_index);
  }

  PathString name_base = GetConflictPathnameBase(base);
  if (name_base.length() + ext.length() > allowed_length) {
    name_base.resize(allowed_length - ext.length());
    TrimPathStringToValidCharacter(&name_base);
  }
  PathString new_name = name_base + ext;
  int n = 2;
  while (Entry(trans,
               syncable::GET_BY_PARENTID_AND_DBNAME,
               parent_id,
               new_name).good()) {
    PathString local_ext = PSTR("(");
    local_ext.append(IntToPathString(n));
    local_ext.append(PSTR(")"));
    local_ext.append(ext);
    if (name_base.length() + local_ext.length() > allowed_length) {
      name_base.resize(allowed_length - local_ext.length());
      TrimPathStringToValidCharacter(&name_base);
    }
    new_name = name_base + local_ext;
    n++;
  }

  CHECK(new_name.length() <= kSyncProtocolMaxNameLengthBytes);
  return Name(new_name);
}

}  // namespace

void ConflictResolver::IgnoreLocalChanges(MutableEntry* entry) {
  // An update matches local actions, merge the changes.
  // This is a little fishy because we don't actually merge them.
  // In the future we should do a 3-way merge.
  LOG(INFO) << "Server and local changes match, merging:" << entry;
  // With IS_UNSYNCED false, changes should be merged.
  // METRIC simple conflict resolved by merge.
  entry->Put(syncable::IS_UNSYNCED, false);
}

void ConflictResolver::OverwriteServerChanges(WriteTransaction* trans,
                                              MutableEntry * entry) {
  // This is similar to an overwrite from the old client.
  // This is equivalent to a scenario where we got the update before we'd
  // made our local client changes.
  // TODO(chron): This is really a general property clobber. We clobber
  // the server side property. Perhaps we should actually do property merging.
  entry->Put(syncable::BASE_VERSION, entry->Get(syncable::SERVER_VERSION));
  entry->Put(syncable::IS_UNAPPLIED_UPDATE, false);
  // METRIC conflict resolved by overwrite.
}

ConflictResolver::ProcessSimpleConflictResult
ConflictResolver::ProcessSimpleConflict(WriteTransaction* trans,
                                        Id id,
                                        SyncerSession* session) {
  MutableEntry entry(trans, syncable::GET_BY_ID, id);
  // Must be good as the entry won't have been cleaned up.
  CHECK(entry.good());
  // If an update fails, locally we have to be in a set or unsynced. We're not
  // in a set here, so we must be unsynced.
  if (!entry.Get(syncable::IS_UNSYNCED))
    return NO_SYNC_PROGRESS;
  if (!entry.Get(syncable::IS_UNAPPLIED_UPDATE)) {
    if (!entry.Get(syncable::PARENT_ID).ServerKnows()) {
      LOG(INFO) << "Item conflicting because its parent not yet committed. "
          "Id: "<< id;
    } else {
      LOG(INFO) << "No set for conflicting entry id " << id << ". There should "
          "be an update/commit that will fix this soon. This message should "
          "not repeat.";
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
    // TODO(chron): Should we check more fields? Since IS_UNSYNCED is
    // turned on, this is really probably enough as fields will be overwritten.
    // Check if there's no changes.

    // Verbose but easier to debug.
    bool name_matches = entry.SyncNameMatchesServerName();
    bool parent_matches = entry.Get(syncable::PARENT_ID) ==
                                    entry.Get(syncable::SERVER_PARENT_ID);
    bool entry_deleted = entry.Get(syncable::IS_DEL);

    if (!entry_deleted && name_matches && parent_matches) {
      LOG(INFO) << "Resolving simple conflict, ignoring local changes for:"
                << entry;
      IgnoreLocalChanges(&entry);
    } else {
      LOG(INFO) << "Resolving simple conflict, overwriting server"
          " changes for:" << entry;
      OverwriteServerChanges(trans, &entry);
    }
    return SYNC_PROGRESS;
  } else {  // SERVER_IS_DEL is true
    // If a server deleted folder has local contents we should be in a set.
    if (entry.Get(syncable::IS_DIR)) {
      Directory::ChildHandles children;
      trans->directory()->GetChildHandles(trans,
                                          entry.Get(syncable::ID),
                                          &children);
      if (0 != children.size()) {
        LOG(INFO) << "Entry is a server deleted directory with local contents, "
          "should be in a set. (race condition).";
        return NO_SYNC_PROGRESS;
      }
    }
    // METRIC conflict resolved by entry split;

    // If the entry's deleted on the server, we can have a directory here.
    entry.Put(syncable::IS_UNSYNCED, true);

    SyncerUtil::SplitServerInformationIntoNewEntry(trans, &entry);

    MutableEntry server_update(trans, syncable::GET_BY_ID, id);
    CHECK(server_update.good());
    CHECK(server_update.Get(syncable::META_HANDLE) !=
          entry.Get(syncable::META_HANDLE))
        << server_update << entry;

    return SYNC_PROGRESS;
  }
}

namespace {

bool NamesCollideWithChildrenOfFolder(BaseTransaction* trans,
                                      const Directory::ChildHandles& children,
                                      Id folder_id) {
  Directory::ChildHandles::const_iterator i = children.begin();
  while (i != children.end()) {
    Entry child(trans, syncable::GET_BY_HANDLE, *i);
    CHECK(child.good());
    if (Entry(trans,
              syncable::GET_BY_PARENTID_AND_DBNAME,
              folder_id,
              child.GetName().db_value()).good())
      return true;
    ++i;
  }
  return false;
}

void GiveEntryNewName(WriteTransaction* trans, MutableEntry* entry) {
  using namespace syncable;
  Name new_name =
      FindNewName(trans, entry->Get(syncable::PARENT_ID), entry->GetName());
  LOG(INFO) << "Resolving name clash, renaming " << *entry << " to "
      << new_name.db_value();
  entry->PutName(new_name);
  CHECK(entry->Get(syncable::IS_UNSYNCED));
}

}  // namespace

bool ConflictResolver::AttemptItemMerge(WriteTransaction* trans,
                                        MutableEntry* locally_named,
                                        MutableEntry* server_named) {
  // To avoid complications we only merge new entries with server entries.
  if (locally_named->Get(syncable::IS_DIR) !=
          server_named->Get(syncable::SERVER_IS_DIR) ||
      locally_named->Get(syncable::ID).ServerKnows() ||
      locally_named->Get(syncable::IS_UNAPPLIED_UPDATE) ||
      server_named->Get(syncable::IS_UNSYNCED))
    return false;
  Id local_id = locally_named->Get(syncable::ID);
  Id desired_id = server_named->Get(syncable::ID);
  if (locally_named->Get(syncable::IS_DIR)) {
    // Extra work for directory name clash. We have to make sure we don't have
    // clashing child items, and update the parent id the children of the new
    // entry.
    Directory::ChildHandles children;
    trans->directory()->GetChildHandles(trans, local_id, &children);
    if (NamesCollideWithChildrenOfFolder(trans, children, desired_id))
      return false;

    LOG(INFO) << "Merging local changes to: " << desired_id << ". "
        << *locally_named;

    server_named->Put(syncable::ID, trans->directory()->NextId());
    Directory::ChildHandles::iterator i;
    for (i = children.begin() ; i != children.end() ; ++i) {
      MutableEntry child_entry(trans, syncable::GET_BY_HANDLE, *i);
      CHECK(child_entry.good());
      CHECK(child_entry.Put(syncable::PARENT_ID, desired_id));
      CHECK(child_entry.Put(syncable::IS_UNSYNCED, true));
      Id id = child_entry.Get(syncable::ID);
      // We only note new entries for quicker merging next round.
      if (!id.ServerKnows())
        children_of_merged_dirs_.insert(id);
    }
  } else {
    if (!server_named->Get(syncable::IS_DEL))
      return false;
  }

  LOG(INFO) << "Identical client and server items merging server changes. " <<
      *locally_named << " server: " << *server_named;

  // Clear server_named's server data and mark it deleted so it goes away
  // quietly because it's now identical to a deleted local entry.
  // locally_named takes on the ID of the server entry.
  server_named->Put(syncable::ID, trans->directory()->NextId());
  locally_named->Put(syncable::ID, desired_id);
  locally_named->Put(syncable::IS_UNSYNCED, false);
  CopyServerFields(server_named, locally_named);
  ClearServerData(server_named);
  server_named->Put(syncable::IS_DEL, true);
  server_named->Put(syncable::BASE_VERSION, 0);
  CHECK(SUCCESS ==
        SyncerUtil::AttemptToUpdateEntryWithoutMerge(
            trans, locally_named, NULL));
  return true;
}

ConflictResolver::ServerClientNameClashReturn
ConflictResolver::ProcessServerClientNameClash(WriteTransaction* trans,
                                               MutableEntry* locally_named,
                                               MutableEntry* server_named,
                                               SyncerSession* session) {
  if (!locally_named->ExistsOnClientBecauseDatabaseNameIsNonEmpty())
    return NO_CLASH;  // Locally_named is a server update.
  if (locally_named->Get(syncable::IS_DEL) ||
      server_named->Get(syncable::SERVER_IS_DEL)) {
    return NO_CLASH;
  }
  if (locally_named->Get(syncable::PARENT_ID) !=
      server_named->Get(syncable::SERVER_PARENT_ID)) {
    return NO_CLASH;  // Different parents.
  }

  PathString name = locally_named->GetSyncNameValue();
  if (0 != syncable::ComparePathNames(name,
      server_named->Get(syncable::SERVER_NAME))) {
    return NO_CLASH;  // Different names.
  }

  // First try to merge.
  if (AttemptItemMerge(trans, locally_named, server_named)) {
    // METRIC conflict resolved by merge
    return SOLVED;
  }
  // We need to rename.
  if (!locally_named->Get(syncable::IS_UNSYNCED)) {
    LOG(ERROR) << "Locally named part of a name conflict not unsynced?";
    locally_named->Put(syncable::IS_UNSYNCED, true);
  }
  if (!server_named->Get(syncable::IS_UNAPPLIED_UPDATE)) {
    LOG(ERROR) << "Server named part of a name conflict not an update?";
  }
  GiveEntryNewName(trans, locally_named);

  // METRIC conflict resolved by rename
  return SOLVED;
}

ConflictResolver::ServerClientNameClashReturn
ConflictResolver::ProcessNameClashesInSet(WriteTransaction* trans,
                                          ConflictSet* conflict_set,
                                          SyncerSession* session) {
  ConflictSet::const_iterator i,j;
  for (i = conflict_set->begin() ; i != conflict_set->end() ; ++i) {
    MutableEntry entryi(trans, syncable::GET_BY_ID, *i);
    if (!entryi.Get(syncable::IS_UNSYNCED) &&
        !entryi.Get(syncable::IS_UNAPPLIED_UPDATE))
      // This set is broken / doesn't make sense, this may be transient.
      return BOGUS_SET;
    for (j = conflict_set->begin() ; *i != *j ; ++j) {
      MutableEntry entryj(trans, syncable::GET_BY_ID, *j);
      ServerClientNameClashReturn rv =
        ProcessServerClientNameClash(trans, &entryi, &entryj, session);
      if (NO_CLASH == rv)
        rv = ProcessServerClientNameClash(trans, &entryj, &entryi, session);
      if (NO_CLASH != rv)
        return rv;
    }
  }
  return NO_CLASH;
}

ConflictResolver::ConflictSetCountMapKey ConflictResolver::GetSetKey(
    ConflictSet* set) {
  // TODO(sync): Come up with a better scheme for set hashing. This scheme
  // will make debugging easy.
  // If this call to sort is removed, we need to add one before we use
  // binary_search in ProcessConflictSet.
  sort(set->begin(), set->end());
  std::stringstream rv;
  for(ConflictSet::iterator i = set->begin() ; i != set->end() ; ++i )
    rv << *i << ".";
  return rv.str();
}

namespace {

bool AttemptToFixCircularConflict(WriteTransaction* trans,
                                  ConflictSet* conflict_set) {
  ConflictSet::const_iterator i, j;
  for(i = conflict_set->begin() ; i != conflict_set->end() ; ++i) {
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
    LOG(INFO) << "Overwriting server changes to avoid loop: " << entryi;
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
    LOG(INFO) << "Giving directory a new id so we can undelete it "
              << parent;
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
  Id reroot_id = syncable::kNullId;
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
    if (!binary_search(conflict_set->begin(), conflict_set->end(), id))
      break;
    MutableEntry entry(trans, syncable::GET_BY_ID, id);
    Id parent_id = entry.Get(syncable::PARENT_ID);
    if (parent_id == reroot_id)
      parent_id = trans->root_id();
    Name old_name = entry.GetName();
    Name new_name = FindNewName(trans, parent_id, old_name);
    LOG(INFO) << "Undoing our deletion of " << entry <<
        ", will have name " << new_name.db_value();
    if (new_name != old_name || parent_id != entry.Get(syncable::PARENT_ID))
      CHECK(entry.PutParentIdAndName(parent_id, new_name));
    entry.Put(syncable::IS_DEL, false);
    id = entry.Get(syncable::PARENT_ID);
    // METRIC conflict resolved by recreating dir tree.
  }
  return true;
}

bool AttemptToFixRemovedDirectoriesWithContent(WriteTransaction* trans,
                                               ConflictSet* conflict_set) {
  ConflictSet::const_iterator i,j;
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

bool ConflictResolver::ProcessConflictSet(WriteTransaction* trans,
                                          ConflictSet* conflict_set,
                                          int conflict_count,
                                          SyncerSession* session) {
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

  LOG(INFO) << "Fixing a set containing " << set_size << " items";

  ServerClientNameClashReturn rv = ProcessNameClashesInSet(trans, conflict_set,
                                                           session);
  if (SOLVED == rv)
    return true;
  if (NO_CLASH != rv)
    return false;

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
    ConflictResolutionView* view) {
  if (attempt_count < SYNC_CYCLES_BEFORE_ADMITTING_DEFEAT) {
    return false;
  }

  // Don't signal stuck if we're not up to date.
  if (view->num_server_changes_remaining() > 0) {
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

  view->set_syncer_stuck(true);

  return true;
  // TODO(sync): If we're stuck for a while we need to alert the user, clear
  // cache or reset syncing. At the very least we should stop trying something
  // that's obviously not working.
}

bool ConflictResolver::ResolveSimpleConflicts(const ScopedDirLookup& dir,
                                              ConflictResolutionView* view,
                                              SyncerSession* session) {
  WriteTransaction trans(dir, syncable::SYNCER, __FILE__, __LINE__);
  bool forward_progress = false;
  // First iterate over simple conflict items (those that belong to no set).
  set<Id>::const_iterator conflicting_item_it;
  for (conflicting_item_it = view->CommitConflictsBegin();
       conflicting_item_it != view->CommitConflictsEnd() ;
       ++conflicting_item_it) {
    Id id = *conflicting_item_it;
    map<Id, ConflictSet*>::const_iterator item_set_it =
        view->IdToConflictSetFind(id);
    if (item_set_it == view->IdToConflictSetEnd() ||
        0 == item_set_it->second) {
      // We have a simple conflict.
      switch(ProcessSimpleConflict(&trans, id, session)) {
        case NO_SYNC_PROGRESS:
          {
            int conflict_count = (simple_conflict_count_map_[id] += 2);
            LogAndSignalIfConflictStuck(&trans, conflict_count,
                                        &id, &id + 1, view);
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
                                        ConflictResolutionView* view,
                                        SyncerSession* session) {
  bool rv = false;
  if (ResolveSimpleConflicts(dir, view, session))
    rv = true;
  WriteTransaction trans(dir, syncable::SYNCER, __FILE__, __LINE__);
  set<Id> children_of_dirs_merged_last_round;
  std::swap(children_of_merged_dirs_, children_of_dirs_merged_last_round);
  set<ConflictSet*>::const_iterator set_it;
  for (set_it = view->ConflictSetsBegin();
       set_it != view->ConflictSetsEnd();
       set_it++) {
    ConflictSet* conflict_set = *set_it;
    ConflictSetCountMapKey key = GetSetKey(conflict_set);
    conflict_set_count_map_[key] += 2;
    int conflict_count = conflict_set_count_map_[key];
    // Keep a metric for new sets.
    if (2 == conflict_count) {
      // METRIC conflict sets seen ++
    }
    // See if this set contains entries whose parents were merged last round.
    if (SortedCollectionsIntersect(children_of_dirs_merged_last_round.begin(),
                                   children_of_dirs_merged_last_round.end(),
                                   conflict_set->begin(),
                                   conflict_set->end())) {
      LOG(INFO) << "Accelerating resolution for hierarchical merge.";
      conflict_count += 2;
    }
    // See if we should process this set.
    if (ProcessConflictSet(&trans, conflict_set, conflict_count, session)) {
      rv = true;
    }
    LogAndSignalIfConflictStuck(&trans, conflict_count,
                                conflict_set->begin(),
                                conflict_set->end(), view);
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
