// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_util.h"

#include <set>
#include <string>
#include <vector>

#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_changes_version.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/sync_types.h"

using syncable::BASE_VERSION;
using syncable::BOOKMARK_FAVICON;
using syncable::BOOKMARK_URL;
using syncable::Blob;
using syncable::CHANGES_VERSION;
using syncable::CREATE;
using syncable::CREATE_NEW_UPDATE_ITEM;
using syncable::CTIME;
using syncable::ComparePathNames;
using syncable::Directory;
using syncable::Entry;
using syncable::ExtendedAttributeKey;
using syncable::GET_BY_HANDLE;
using syncable::GET_BY_ID;
using syncable::ID;
using syncable::IS_BOOKMARK_OBJECT;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::Id;
using syncable::META_HANDLE;
using syncable::MTIME;
using syncable::MutableEntry;
using syncable::MutableExtendedAttribute;
using syncable::NEXT_ID;
using syncable::NON_UNIQUE_NAME;
using syncable::PARENT_ID;
using syncable::PREV_ID;
using syncable::ReadTransaction;
using syncable::SERVER_BOOKMARK_FAVICON;
using syncable::SERVER_BOOKMARK_URL;
using syncable::SERVER_CTIME;
using syncable::SERVER_IS_BOOKMARK_OBJECT;
using syncable::SERVER_IS_DEL;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_MTIME;
using syncable::SERVER_NON_UNIQUE_NAME;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_POSITION_IN_PARENT;
using syncable::SERVER_VERSION;
using syncable::SINGLETON_TAG;
using syncable::SYNCER;
using syncable::WriteTransaction;

namespace browser_sync {

using std::string;
using std::vector;

// Returns the number of unsynced entries.
// static
int SyncerUtil::GetUnsyncedEntries(syncable::BaseTransaction* trans,
                                   vector<int64> *handles) {
  trans->directory()->GetUnsyncedMetaHandles(trans, handles);
  LOG_IF(INFO, handles->size() > 0)
      << "Have " << handles->size() << " unsynced items.";
  return handles->size();
}

// static
void SyncerUtil::ChangeEntryIDAndUpdateChildren(
    syncable::WriteTransaction* trans,
    syncable::MutableEntry* entry,
    const syncable::Id& new_id,
    syncable::Directory::ChildHandles* children) {
  syncable::Id old_id = entry->Get(ID);
  if (!entry->Put(ID, new_id)) {
    Entry old_entry(trans, GET_BY_ID, new_id);
    CHECK(old_entry.good());
    LOG(FATAL) << "Attempt to change ID to " << new_id
               << " conflicts with existing entry.\n\n"
               << *entry << "\n\n" << old_entry;
  }
  if (entry->Get(IS_DIR)) {
    // Get all child entries of the old id.
    trans->directory()->GetChildHandles(trans, old_id, children);
    Directory::ChildHandles::iterator i = children->begin();
    while (i != children->end()) {
      MutableEntry child_entry(trans, GET_BY_HANDLE, *i++);
      CHECK(child_entry.good());
      CHECK(child_entry.Put(PARENT_ID, new_id));
    }
  }
  // Update Id references on the previous and next nodes in the sibling
  // order.  Do this by reinserting into the linked list; the first
  // step in PutPredecessor is to Unlink from the existing order, which
  // will overwrite the stale Id value from the adjacent nodes.
  if (entry->Get(PREV_ID) == entry->Get(NEXT_ID) &&
      entry->Get(PREV_ID) == old_id) {
    // We just need a shallow update to |entry|'s fields since it is already
    // self looped.
    entry->Put(NEXT_ID, new_id);
    entry->Put(PREV_ID, new_id);
  } else {
    entry->PutPredecessor(entry->Get(PREV_ID));
  }
}

// static
void SyncerUtil::ChangeEntryIDAndUpdateChildren(
    syncable::WriteTransaction* trans,
    syncable::MutableEntry* entry,
    const syncable::Id& new_id) {
  syncable::Directory::ChildHandles children;
  ChangeEntryIDAndUpdateChildren(trans, entry, new_id, &children);
}

// static
void SyncerUtil::AttemptReuniteLostCommitResponses(
    syncable::WriteTransaction* trans,
    const SyncEntity& server_entry,
    const string& client_id) {
  // If a commit succeeds, but the response does not come back fast enough
  // then the syncer might assume that it was never committed.
  // The server will track the client that sent up the original commit and
  // return this in a get updates response. When this matches a local
  // uncommitted item, we must mutate our local item and version to pick up
  // the committed version of the same item whose commit response was lost.
  // There is however still a race condition if the server has not
  // completed the commit by the time the syncer tries to get updates
  // again. To mitigate this, we need to have the server time out in
  // a reasonable span, our commit batches have to be small enough
  // to process within our HTTP response "assumed alive" time.

  // We need to check if we have a  that didn't get its server
  // id updated correctly. The server sends down a client ID
  // and a local (negative) id. If we have a entry by that
  // description, we should update the ID and version to the
  // server side ones to avoid multiple commits to the same name.
  if (server_entry.has_originator_cache_guid() &&
      server_entry.originator_cache_guid() == client_id) {
    syncable::Id server_id = syncable::Id::CreateFromClientString(
        server_entry.originator_client_item_id());
    CHECK(!server_id.ServerKnows());
    syncable::MutableEntry local_entry(trans, GET_BY_ID, server_id);

    // If it exists, then our local client lost a commit response.
    if (local_entry.good() && !local_entry.Get(IS_DEL)) {
      int64 old_version = local_entry.Get(BASE_VERSION);
      int64 new_version = server_entry.version();
      CHECK(old_version <= 0);
      CHECK(new_version > 0);
      // Otherwise setting the base version could cause a consistency failure.
      // An entry should never be version 0 and SYNCED.
      CHECK(local_entry.Get(IS_UNSYNCED));

      // Just a quick sanity check.
      CHECK(!local_entry.Get(ID).ServerKnows());

      LOG(INFO) << "Reuniting lost commit response IDs" <<
        " server id: " << server_entry.id() << " local id: " <<
        local_entry.Get(ID) << " new version: " << new_version;

      local_entry.Put(BASE_VERSION, new_version);

      ChangeEntryIDAndUpdateChildren(trans, &local_entry, server_entry.id());

      // We need to continue normal processing on this update after we
      // reunited its ID.
    }
    // !local_entry.Good() means we don't have a left behind entry for this
    // ID. We successfully committed before. In the future we should get rid
    // of this system and just have client side generated IDs as a whole.
  }
}

// static
UpdateAttemptResponse SyncerUtil::AttemptToUpdateEntry(
    syncable::WriteTransaction* const trans,
    syncable::MutableEntry* const entry,
    ConflictResolver* resolver) {

  CHECK(entry->good());
  if (!entry->Get(IS_UNAPPLIED_UPDATE))
    return SUCCESS;  // No work to do.
  syncable::Id id = entry->Get(ID);

  if (entry->Get(IS_UNSYNCED)) {
    LOG(INFO) << "Skipping update, returning conflict for: " << id
              << " ; it's unsynced.";
    return CONFLICT;
  }
  if (!entry->Get(SERVER_IS_DEL)) {
    syncable::Id new_parent = entry->Get(SERVER_PARENT_ID);
    Entry parent(trans, GET_BY_ID,  new_parent);
    // A note on non-directory parents:
    // We catch most unfixable tree invariant errors at update receipt time,
    // however we deal with this case here because we may receive the child
    // first then the illegal parent. Instead of dealing with it twice in
    // different ways we deal with it once here to reduce the amount of code and
    // potential errors.
    if (!parent.good() || parent.Get(IS_DEL) || !parent.Get(IS_DIR)) {
      return CONFLICT;
    }
    if (entry->Get(PARENT_ID) != new_parent) {
      if (!entry->Get(IS_DEL) && !IsLegalNewParent(trans, id, new_parent)) {
        LOG(INFO) << "Not updating item " << id << ", illegal new parent "
          "(would cause loop).";
        return CONFLICT;
      }
    }
  } else if (entry->Get(IS_DIR)) {
    Directory::ChildHandles handles;
    trans->directory()->GetChildHandles(trans, id, &handles);
    if (!handles.empty()) {
      // If we have still-existing children, then we need to deal with
      // them before we can process this change.
      LOG(INFO) << "Not deleting directory; it's not empty " << *entry;
      return CONFLICT;
    }
  }

  SyncerUtil::UpdateLocalDataFromServerData(trans, entry);

  return SUCCESS;
}

// Pass in name and checksum because of UTF8 conversion.
// static
void SyncerUtil::UpdateServerFieldsFromUpdate(
    MutableEntry* local_entry,
    const SyncEntity& server_entry,
    const PathString& name) {
  if (server_entry.deleted()) {
    // The server returns very lightweight replies for deletions, so we don't
    // clobber a bunch of fields on delete.
    local_entry->Put(SERVER_IS_DEL, true);
    local_entry->Put(SERVER_VERSION,
        std::max(local_entry->Get(SERVER_VERSION),
          local_entry->Get(BASE_VERSION)) + 1L);
    local_entry->Put(IS_UNAPPLIED_UPDATE, true);
    return;
  }

  CHECK(local_entry->Get(ID) == server_entry.id())
      << "ID Changing not supported here";
  local_entry->Put(SERVER_PARENT_ID, server_entry.parent_id());
  local_entry->Put(SERVER_NON_UNIQUE_NAME, name);
  local_entry->Put(SERVER_VERSION, server_entry.version());
  local_entry->Put(SERVER_CTIME,
      ServerTimeToClientTime(server_entry.ctime()));
  local_entry->Put(SERVER_MTIME,
      ServerTimeToClientTime(server_entry.mtime()));
  local_entry->Put(SERVER_IS_BOOKMARK_OBJECT, server_entry.has_bookmarkdata());
  local_entry->Put(SERVER_IS_DIR, server_entry.IsFolder());
  if (server_entry.has_singleton_tag()) {
    const PathString& tag = server_entry.singleton_tag();
    local_entry->Put(SINGLETON_TAG, tag);
  }
  if (server_entry.has_bookmarkdata() && !server_entry.deleted()) {
    const SyncEntity::BookmarkData& bookmark = server_entry.bookmarkdata();
    if (bookmark.has_bookmark_url()) {
      const PathString& url = bookmark.bookmark_url();
      local_entry->Put(SERVER_BOOKMARK_URL, url);
    }
    if (bookmark.has_bookmark_favicon()) {
      Blob favicon_blob;
      SyncerProtoUtil::CopyProtoBytesIntoBlob(bookmark.bookmark_favicon(),
                                              &favicon_blob);
      local_entry->Put(SERVER_BOOKMARK_FAVICON, favicon_blob);
    }
  }
  if (server_entry.has_position_in_parent()) {
    local_entry->Put(SERVER_POSITION_IN_PARENT,
                     server_entry.position_in_parent());
  }

  local_entry->Put(SERVER_IS_DEL, server_entry.deleted());
  // We only mark the entry as unapplied if its version is greater than the
  // local data. If we're processing the update that corresponds to one of our
  // commit we don't apply it as time differences may occur.
  if (server_entry.version() > local_entry->Get(BASE_VERSION)) {
    local_entry->Put(IS_UNAPPLIED_UPDATE, true);
  }
  ApplyExtendedAttributes(local_entry, server_entry);
}

// static
void SyncerUtil::ApplyExtendedAttributes(
    syncable::MutableEntry* local_entry,
    const SyncEntity& server_entry) {
  local_entry->DeleteAllExtendedAttributes(local_entry->write_transaction());
  if (server_entry.has_extended_attributes()) {
    const sync_pb::ExtendedAttributes & extended_attributes =
      server_entry.extended_attributes();
    for (int i = 0; i < extended_attributes.extendedattribute_size(); i++) {
      const PathString& pathstring_key =
          extended_attributes.extendedattribute(i).key();
      ExtendedAttributeKey key(local_entry->Get(META_HANDLE), pathstring_key);
      MutableExtendedAttribute local_attribute(local_entry->write_transaction(),
          CREATE, key);
      SyncerProtoUtil::CopyProtoBytesIntoBlob(
          extended_attributes.extendedattribute(i).value(),
          local_attribute.mutable_value());
    }
  }
}

// Creates a new Entry iff no Entry exists with the given id.
// static
void SyncerUtil::CreateNewEntry(syncable::WriteTransaction *trans,
                                const syncable::Id& id) {
  syncable::MutableEntry entry(trans, syncable::GET_BY_ID, id);
  if (!entry.good()) {
    syncable::MutableEntry new_entry(trans, syncable::CREATE_NEW_UPDATE_ITEM,
                                     id);
  }
}

// static
bool SyncerUtil::ServerAndLocalOrdersMatch(syncable::Entry* entry) {
  // Find the closest up-to-date local sibling by walking the linked list.
  syncable::Id local_up_to_date_predecessor = entry->Get(PREV_ID);
  while (!local_up_to_date_predecessor.IsRoot()) {
    Entry local_prev(entry->trans(), GET_BY_ID, local_up_to_date_predecessor);
    if (!local_prev.good() || local_prev.Get(IS_DEL))
      return false;
    if (!local_prev.Get(IS_UNAPPLIED_UPDATE) && !local_prev.Get(IS_UNSYNCED))
      break;
    local_up_to_date_predecessor = local_prev.Get(PREV_ID);
  }

  // Now find the closest up-to-date sibling in the server order.
  syncable::Id server_up_to_date_predecessor =
      ComputePrevIdFromServerPosition(entry->trans(), entry,
                                      entry->Get(SERVER_PARENT_ID));
  return server_up_to_date_predecessor == local_up_to_date_predecessor;
}

// static
bool SyncerUtil::ServerAndLocalEntriesMatch(syncable::Entry* entry) {
  if (!ClientAndServerTimeMatch(
        entry->Get(CTIME), ClientTimeToServerTime(entry->Get(SERVER_CTIME)))) {
    LOG(WARNING) << "Client and server time mismatch";
    return false;
  }
  if (entry->Get(IS_DEL) && entry->Get(SERVER_IS_DEL))
    return true;
  // Name should exactly match here.
  if (!(entry->Get(NON_UNIQUE_NAME) == entry->Get(SERVER_NON_UNIQUE_NAME))) {
    LOG(WARNING) << "Unsanitized name mismatch";
    return false;
  }

  if (entry->Get(PARENT_ID) != entry->Get(SERVER_PARENT_ID) ||
      entry->Get(IS_DIR) != entry->Get(SERVER_IS_DIR) ||
      entry->Get(IS_DEL) != entry->Get(SERVER_IS_DEL)) {
    LOG(WARNING) << "Metabit mismatch";
    return false;
  }

  if (!ServerAndLocalOrdersMatch(entry)) {
    LOG(WARNING) << "Server/local ordering mismatch";
    return false;
  }

  if (entry->Get(IS_BOOKMARK_OBJECT)) {
    if (!entry->Get(IS_DIR)) {
      if (entry->Get(BOOKMARK_URL) != entry->Get(SERVER_BOOKMARK_URL)) {
        LOG(WARNING) << "Bookmark URL mismatch";
        return false;
      }
    }
  }
  if (entry->Get(IS_DIR))
    return true;
  // For historical reasons, a folder's MTIME changes when its contents change.
  // TODO(ncarter): Remove the special casing of MTIME.
  bool time_match = ClientAndServerTimeMatch(entry->Get(MTIME),
      ClientTimeToServerTime(entry->Get(SERVER_MTIME)));
  if (!time_match) {
    LOG(WARNING) << "Time mismatch";
  }
  return time_match;
}

// static
void SyncerUtil::SplitServerInformationIntoNewEntry(
    syncable::WriteTransaction* trans,
    syncable::MutableEntry* entry) {
  syncable::Id id = entry->Get(ID);
  ChangeEntryIDAndUpdateChildren(trans, entry, trans->directory()->NextId());
  entry->Put(BASE_VERSION, 0);

  MutableEntry new_entry(trans, CREATE_NEW_UPDATE_ITEM, id);
  CopyServerFields(entry, &new_entry);
  ClearServerData(entry);

  LOG(INFO) << "Splitting server information, local entry: " << *entry <<
    " server entry: " << new_entry;
}

// This function is called on an entry when we can update the user-facing data
// from the server data.
// static
void SyncerUtil::UpdateLocalDataFromServerData(
    syncable::WriteTransaction* trans,
    syncable::MutableEntry* entry) {
  CHECK(!entry->Get(IS_UNSYNCED));
  CHECK(entry->Get(IS_UNAPPLIED_UPDATE));
  LOG(INFO) << "Updating entry : " << *entry;
  entry->Put(IS_BOOKMARK_OBJECT, entry->Get(SERVER_IS_BOOKMARK_OBJECT));
  // This strange dance around the IS_DEL flag avoids problems when setting
  // the name.
  // TODO(chron): Is this still an issue? Unit test this codepath.
  if (entry->Get(SERVER_IS_DEL)) {
    entry->Put(IS_DEL, true);
  } else {
    entry->Put(NON_UNIQUE_NAME, entry->Get(SERVER_NON_UNIQUE_NAME));
    entry->Put(PARENT_ID, entry->Get(SERVER_PARENT_ID));
    CHECK(entry->Put(IS_DEL, false));
    Id new_predecessor = ComputePrevIdFromServerPosition(trans, entry,
        entry->Get(SERVER_PARENT_ID));
    CHECK(entry->PutPredecessor(new_predecessor))
        << " Illegal predecessor after converting from server position.";
  }

  entry->Put(CTIME, entry->Get(SERVER_CTIME));
  entry->Put(MTIME, entry->Get(SERVER_MTIME));
  entry->Put(BASE_VERSION, entry->Get(SERVER_VERSION));
  entry->Put(IS_DIR, entry->Get(SERVER_IS_DIR));
  entry->Put(IS_DEL, entry->Get(SERVER_IS_DEL));
  entry->Put(BOOKMARK_URL, entry->Get(SERVER_BOOKMARK_URL));
  entry->Put(BOOKMARK_FAVICON, entry->Get(SERVER_BOOKMARK_FAVICON));
  entry->Put(IS_UNAPPLIED_UPDATE, false);
}

// static
VerifyCommitResult SyncerUtil::ValidateCommitEntry(
    syncable::MutableEntry* entry) {
  syncable::Id id = entry->Get(ID);
  if (id == entry->Get(PARENT_ID)) {
    CHECK(id.IsRoot()) << "Non-root item is self parenting." << *entry;
    // If the root becomes unsynced it can cause us problems.
    LOG(ERROR) << "Root item became unsynced " << *entry;
    return VERIFY_UNSYNCABLE;
  }
  if (entry->IsRoot()) {
    LOG(ERROR) << "Permanent item became unsynced " << *entry;
    return VERIFY_UNSYNCABLE;
  }
  if (entry->Get(IS_DEL) && !entry->Get(ID).ServerKnows()) {
    // Drop deleted uncommitted entries.
    return VERIFY_UNSYNCABLE;
  }
  return VERIFY_OK;
}

// static
bool SyncerUtil::AddItemThenPredecessors(
    syncable::BaseTransaction* trans,
    syncable::Entry* item,
    syncable::IndexedBitField inclusion_filter,
    syncable::MetahandleSet* inserted_items,
    vector<syncable::Id>* commit_ids) {

  if (!inserted_items->insert(item->Get(META_HANDLE)).second)
    return false;
  commit_ids->push_back(item->Get(ID));
  if (item->Get(IS_DEL))
    return true;  // Deleted items have no predecessors.

  Id prev_id = item->Get(PREV_ID);
  while (!prev_id.IsRoot()) {
    Entry prev(trans, GET_BY_ID, prev_id);
    CHECK(prev.good()) << "Bad id when walking predecessors.";
    if (!prev.Get(inclusion_filter))
      break;
    if (!inserted_items->insert(prev.Get(META_HANDLE)).second)
      break;
    commit_ids->push_back(prev_id);
    prev_id = prev.Get(PREV_ID);
  }
  return true;
}

// static
void SyncerUtil::AddPredecessorsThenItem(
    syncable::BaseTransaction* trans,
    syncable::Entry* item,
    syncable::IndexedBitField inclusion_filter,
    syncable::MetahandleSet* inserted_items,
    vector<syncable::Id>* commit_ids) {
  vector<syncable::Id>::size_type initial_size = commit_ids->size();
  if (!AddItemThenPredecessors(trans, item, inclusion_filter, inserted_items,
                               commit_ids))
    return;
  // Reverse what we added to get the correct order.
  std::reverse(commit_ids->begin() + initial_size, commit_ids->end());
}

// TODO(ncarter): This is redundant to some code in GetCommitIdsCommand.  Unify
// them.
// static
void SyncerUtil::AddUncommittedParentsAndTheirPredecessors(
    syncable::BaseTransaction* trans,
    syncable::MetahandleSet* inserted_items,
    vector<syncable::Id>* commit_ids,
    syncable::Id parent_id) {
  vector<syncable::Id>::size_type intial_commit_ids_size = commit_ids->size();
  // Climb the tree adding entries leaf -> root.
  while (!parent_id.ServerKnows()) {
    Entry parent(trans, GET_BY_ID, parent_id);
    CHECK(parent.good()) << "Bad user-only parent in item path.";
    if (!AddItemThenPredecessors(trans, &parent, IS_UNSYNCED, inserted_items,
                                 commit_ids))
      break;  // Parent was already present in |inserted_items|.
    parent_id = parent.Get(PARENT_ID);
  }
  // Reverse what we added to get the correct order.
  std::reverse(commit_ids->begin() + intial_commit_ids_size, commit_ids->end());
}

// static
void SyncerUtil::MarkDeletedChildrenSynced(
    const syncable::ScopedDirLookup &dir,
    std::set<syncable::Id>* deleted_folders) {
  // There's two options here.
  // 1. Scan deleted unsynced entries looking up their pre-delete tree for any
  //    of the deleted folders.
  // 2. Take each folder and do a tree walk of all entries underneath it.
  // #2 has a lower big O cost, but writing code to limit the time spent inside
  // the transaction during each step is simpler with 1. Changing this decision
  // may be sensible if this code shows up in profiling.
  if (deleted_folders->empty())
    return;
  Directory::UnsyncedMetaHandles handles;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    dir->GetUnsyncedMetaHandles(&trans, &handles);
  }
  if (handles.empty())
    return;
  Directory::UnsyncedMetaHandles::iterator it;
  for (it = handles.begin() ; it != handles.end() ; ++it) {
    // Single transaction / entry we deal with.
    WriteTransaction trans(dir, SYNCER, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_HANDLE, *it);
    if (!entry.Get(IS_UNSYNCED) || !entry.Get(IS_DEL))
      continue;
    syncable::Id id = entry.Get(PARENT_ID);
    while (id != trans.root_id()) {
      if (deleted_folders->find(id) != deleted_folders->end()) {
        // We've synced the deletion of this deleted entries parent.
        entry.Put(IS_UNSYNCED, false);
        break;
      }
      Entry parent(&trans, GET_BY_ID, id);
      if (!parent.good() || !parent.Get(IS_DEL))
        break;
      id = parent.Get(PARENT_ID);
    }
  }
}

// static
VerifyResult SyncerUtil::VerifyNewEntry(
    const SyncEntity& entry,
    syncable::MutableEntry* same_id,
    const bool deleted) {
  if (same_id->good()) {
    // Not a new entry.
    return VERIFY_UNDECIDED;
  }
  if (deleted) {
    // Deletion of an item we've never seen can be ignored.
    return VERIFY_SKIP;
  }

  return VERIFY_SUCCESS;
}

// Assumes we have an existing entry; check here for updates that break
// consistency rules.
// static
VerifyResult SyncerUtil::VerifyUpdateConsistency(
    syncable::WriteTransaction* trans,
    const SyncEntity& entry,
    syncable::MutableEntry* same_id,
    const bool deleted,
    const bool is_directory,
    const bool has_bookmark_data) {

  CHECK(same_id->good());

  // If the entry is a delete, we don't really need to worry at this stage.
  if (deleted)
    return VERIFY_SUCCESS;

  if (same_id->Get(SERVER_VERSION) > 0) {
    // Then we've had an update for this entry before.
    if (is_directory != same_id->Get(SERVER_IS_DIR) ||
        has_bookmark_data != same_id->Get(SERVER_IS_BOOKMARK_OBJECT)) {
      if (same_id->Get(IS_DEL)) {  // If we've deleted the item, we don't care.
        return VERIFY_SKIP;
      } else {
        LOG(ERROR) << "Server update doesn't agree with previous updates. ";
        LOG(ERROR) << " Entry: " << *same_id;
        LOG(ERROR) << " Update: " << SyncEntityDebugString(entry);
        return VERIFY_FAIL;
      }
    }

    if (!deleted &&
        (same_id->Get(SERVER_IS_DEL) ||
         (!same_id->Get(IS_UNSYNCED) && same_id->Get(IS_DEL) &&
          same_id->Get(BASE_VERSION) > 0))) {
      // An undelete. The latter case in the above condition is for
      // when the server does not give us an update following the
      // commit of a delete, before undeleting.  Undeletion is possible
      // in the server's storage backend, so it's possible on the client,
      // though not expected to be something that is commonly possible.
      VerifyResult result =
          SyncerUtil::VerifyUndelete(trans, entry, same_id);
      if (VERIFY_UNDECIDED != result)
        return result;
    }
  }
  if (same_id->Get(BASE_VERSION) > 0) {
    // We've committed this entry in the past.
    if (is_directory != same_id->Get(IS_DIR) ||
        has_bookmark_data != same_id->Get(IS_BOOKMARK_OBJECT)) {
      LOG(ERROR) << "Server update doesn't agree with committed item. ";
      LOG(ERROR) << " Entry: " << *same_id;
      LOG(ERROR) << " Update: " << SyncEntityDebugString(entry);
      return VERIFY_FAIL;
    }
    if (same_id->Get(BASE_VERSION) == entry.version() &&
        !same_id->Get(IS_UNSYNCED) &&
        !SyncerProtoUtil::Compare(*same_id, entry)) {
      // TODO(sync): This constraint needs to be relaxed. For now it's OK to
      // fail the verification and deal with it when we ApplyUpdates.
      LOG(ERROR) << "Server update doesn't match local data with same "
          "version. A bug should be filed. Entry: " << *same_id <<
          "Update: " << SyncEntityDebugString(entry);
      return VERIFY_FAIL;
    }
    if (same_id->Get(SERVER_VERSION) > entry.version()) {
      LOG(WARNING) << "We've already seen a more recent update from the server";
      LOG(WARNING) << " Entry: " << *same_id;
      LOG(WARNING) << " Update: " << SyncEntityDebugString(entry);
      return VERIFY_SKIP;
    }
  }
  return VERIFY_SUCCESS;
}

// Assumes we have an existing entry; verify an update that seems to be
// expressing an 'undelete'
// static
VerifyResult SyncerUtil::VerifyUndelete(syncable::WriteTransaction* trans,
                                        const SyncEntity& entry,
                                        syncable::MutableEntry* same_id) {
  CHECK(same_id->good());
  LOG(INFO) << "Server update is attempting undelete. " << *same_id
            << "Update:" << SyncEntityDebugString(entry);
  // Move the old one aside and start over.  It's too tricky to get the old one
  // back into a state that would pass CheckTreeInvariants().
  if (same_id->Get(IS_DEL)) {
    same_id->Put(ID, trans->directory()->NextId());
    same_id->Put(BASE_VERSION, CHANGES_VERSION);
    same_id->Put(SERVER_VERSION, 0);
    return VERIFY_SUCCESS;
  }
  if (entry.version() < same_id->Get(SERVER_VERSION)) {
    LOG(WARNING) << "Update older than current server version for" <<
        *same_id << "Update:" << SyncEntityDebugString(entry);
    return VERIFY_SUCCESS;  // Expected in new sync protocol.
  }
  return VERIFY_UNDECIDED;
}

// static
syncable::Id SyncerUtil::ComputePrevIdFromServerPosition(
    syncable::BaseTransaction* trans,
    syncable::Entry* update_item,
    const syncable::Id& parent_id) {
  const int64 position_in_parent = update_item->Get(SERVER_POSITION_IN_PARENT);

  // TODO(ncarter): This computation is linear in the number of children, but
  // we could make it logarithmic if we kept an index on server position.
  syncable::Id closest_sibling;
  syncable::Id next_id = trans->directory()->GetFirstChildId(trans, parent_id);
  while (!next_id.IsRoot()) {
    syncable::Entry candidate(trans, GET_BY_ID, next_id);
    if (!candidate.good()) {
      LOG(WARNING) << "Should not happen";
      return closest_sibling;
    }
    next_id = candidate.Get(NEXT_ID);

    // Defensively prevent self-comparison.
    if (candidate.Get(META_HANDLE) == update_item->Get(META_HANDLE)) {
      continue;
    }

    // Ignore unapplied updates -- they might not even be server-siblings.
    if (candidate.Get(IS_UNAPPLIED_UPDATE)) {
      continue;
    }

    // Unsynced items don't have a valid server position.
    if (!candidate.Get(IS_UNSYNCED)) {
      // If |candidate| is after |update_entry| according to the server
      // ordering, then we're done.  ID is the tiebreaker.
      if ((candidate.Get(SERVER_POSITION_IN_PARENT) > position_in_parent) ||
          ((candidate.Get(SERVER_POSITION_IN_PARENT) == position_in_parent) &&
           (candidate.Get(ID) > update_item->Get(ID)))) {
          return closest_sibling;
      }
    }

    // We can't trust the SERVER_ fields of unsynced items, but they are
    // potentially legitimate local predecessors.  In the case where
    // |update_item| and an unsynced item wind up in the same insertion
    // position, we need to choose how to order them.  The following check puts
    // the unapplied update first; removing it would put the unsynced item(s)
    // first.
    if (candidate.Get(IS_UNSYNCED)) {
      continue;
    }

    // |update_entry| is considered to be somewhere after |candidate|, so store
    // it as the upper bound.
    closest_sibling = candidate.Get(ID);
  }

  return closest_sibling;
}

}  // namespace browser_sync
