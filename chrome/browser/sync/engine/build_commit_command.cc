// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/build_commit_command.h"

#include <set>
#include <string>
#include <vector>

#include "base/string_util.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_changes_version.h"

using std::set;
using std::string;
using std::vector;
using syncable::IS_DEL;
using syncable::Id;
using syncable::MutableEntry;
using syncable::SPECIFICS;
using syncable::UNSPECIFIED;

namespace browser_sync {

using sessions::SyncSession;

BuildCommitCommand::BuildCommitCommand() {}
BuildCommitCommand::~BuildCommitCommand() {}

void BuildCommitCommand::AddExtensionsActivityToMessage(
    SyncSession* session, CommitMessage* message) {
  // We only send ExtensionsActivity to the server if bookmarks are being
  // committed.
  ExtensionsActivityMonitor* monitor = session->context()->extensions_monitor();
  if (!session->status_controller()->HasBookmarkCommitActivity()) {
    // Return the records to the activity monitor.
    monitor->PutRecords(session->extensions_activity());
    session->mutable_extensions_activity()->clear();
    return;
  }
  const ExtensionsActivityMonitor::Records& records =
      session->extensions_activity();
  for (ExtensionsActivityMonitor::Records::const_iterator it = records.begin();
       it != records.end(); ++it) {
    sync_pb::ChromiumExtensionsActivity* activity_message =
        message->add_extensions_activity();
    activity_message->set_extension_id(it->second.extension_id);
    activity_message->set_bookmark_writes_since_last_commit(
        it->second.bookmark_write_count);
  }
}

namespace {
void SetEntrySpecifics(MutableEntry* meta_entry, SyncEntity* sync_entry) {
  // Add the new style extension and the folder bit.
  sync_entry->mutable_specifics()->CopyFrom(meta_entry->Get(SPECIFICS));
  sync_entry->set_folder(meta_entry->Get(syncable::IS_DIR));

  DCHECK(meta_entry->GetModelType() == sync_entry->GetModelType());
}

void SetOldStyleBookmarkData(MutableEntry* meta_entry, SyncEntity* sync_entry) {
  DCHECK(meta_entry->Get(SPECIFICS).HasExtension(sync_pb::bookmark));

  // Old-style inlined bookmark data.
  sync_pb::SyncEntity_BookmarkData* bookmark =
      sync_entry->mutable_bookmarkdata();

  if (!meta_entry->Get(syncable::IS_DIR)) {
    const sync_pb::BookmarkSpecifics& bookmark_specifics =
        meta_entry->Get(SPECIFICS).GetExtension(sync_pb::bookmark);
    bookmark->set_bookmark_url(bookmark_specifics.url());
    bookmark->set_bookmark_favicon(bookmark_specifics.favicon());
    bookmark->set_bookmark_folder(false);
  } else {
    bookmark->set_bookmark_folder(true);
  }
}
}  // namespace

void BuildCommitCommand::ExecuteImpl(SyncSession* session) {
  ClientToServerMessage message;
  message.set_share(session->context()->account_name());
  message.set_message_contents(ClientToServerMessage::COMMIT);

  CommitMessage* commit_message = message.mutable_commit();
  commit_message->set_cache_guid(
      session->write_transaction()->directory()->cache_guid());
  AddExtensionsActivityToMessage(session, commit_message);
  SyncerProtoUtil::AddRequestBirthday(
      session->write_transaction()->directory(), &message);

  const vector<Id>& commit_ids = session->status_controller()->commit_ids();
  for (size_t i = 0; i < commit_ids.size(); i++) {
    Id id = commit_ids[i];
    SyncEntity* sync_entry =
        static_cast<SyncEntity*>(commit_message->add_entries());
    sync_entry->set_id(id);
    MutableEntry meta_entry(session->write_transaction(),
                            syncable::GET_BY_ID,
                            id);
    CHECK(meta_entry.good());
    // This is the only change we make to the entry in this function.
    meta_entry.Put(syncable::SYNCING, true);

    DCHECK(0 != session->routing_info().count(meta_entry.GetModelType()))
        << "Committing change to datatype that's not actively enabled.";

    string name = meta_entry.Get(syncable::NON_UNIQUE_NAME);
    CHECK(!name.empty());  // Make sure this isn't an update.
    TruncateUTF8ToByteSize(name, 255, &name);
    sync_entry->set_name(name);

    // Set the non_unique_name.  If we do, the server ignores
    // the |name| value (using |non_unique_name| instead), and will return
    // in the CommitResponse a unique name if one is generated.
    // We send both because it may aid in logging.
    sync_entry->set_non_unique_name(name);

    if (!meta_entry.Get(syncable::UNIQUE_CLIENT_TAG).empty()) {
      sync_entry->set_client_defined_unique_tag(
          meta_entry.Get(syncable::UNIQUE_CLIENT_TAG));
    }

    // Deleted items with server-unknown parent ids can be a problem so we set
    // the parent to 0. (TODO(sync): Still true in protocol?).
    Id new_parent_id;
    if (meta_entry.Get(syncable::IS_DEL) &&
        !meta_entry.Get(syncable::PARENT_ID).ServerKnows()) {
      new_parent_id = session->write_transaction()->root_id();
    } else {
      new_parent_id = meta_entry.Get(syncable::PARENT_ID);
    }
    sync_entry->set_parent_id(new_parent_id);

    // If our parent has changed, send up the old one so the server
    // can correctly deal with multiple parents.
    // TODO(nick): With the server keeping track of the primary sync parent,
    // it should not be necessary to provide the old_parent_id: the version
    // number should suffice.
    if (new_parent_id != meta_entry.Get(syncable::SERVER_PARENT_ID) &&
        0 != meta_entry.Get(syncable::BASE_VERSION) &&
        syncable::CHANGES_VERSION != meta_entry.Get(syncable::BASE_VERSION)) {
      sync_entry->set_old_parent_id(meta_entry.Get(syncable::SERVER_PARENT_ID));
    }

    int64 version = meta_entry.Get(syncable::BASE_VERSION);
    if (syncable::CHANGES_VERSION == version || 0 == version) {
      // Undeletions are only supported for items that have a client tag.
      DCHECK(!id.ServerKnows() ||
             !meta_entry.Get(syncable::UNIQUE_CLIENT_TAG).empty())
          << meta_entry;

      // Version 0 means to create or undelete an object.
      sync_entry->set_version(0);
    } else {
      DCHECK(id.ServerKnows()) << meta_entry;
      sync_entry->set_version(meta_entry.Get(syncable::BASE_VERSION));
    }
    sync_entry->set_ctime(ClientTimeToServerTime(
        meta_entry.Get(syncable::CTIME)));
    sync_entry->set_mtime(ClientTimeToServerTime(
        meta_entry.Get(syncable::MTIME)));

    // Deletion is final on the server, let's move things and then delete them.
    if (meta_entry.Get(IS_DEL)) {
      sync_entry->set_deleted(true);
    } else {
      if (meta_entry.Get(SPECIFICS).HasExtension(sync_pb::bookmark)) {
        // Common data in both new and old protocol.
        const Id& prev_id = meta_entry.Get(syncable::PREV_ID);
        string prev_id_string =
            prev_id.IsRoot() ? string() : prev_id.GetServerId();
        sync_entry->set_insert_after_item_id(prev_id_string);

        // TODO(ncarter): In practice we won't want to send this data twice
        // over the wire; instead, when deployed servers are able to accept
        // the new-style scheme, we should abandon the old way.
        SetOldStyleBookmarkData(&meta_entry, sync_entry);
      }
      SetEntrySpecifics(&meta_entry, sync_entry);
    }
  }
  session->status_controller()->mutable_commit_message()->CopyFrom(message);
}

}  // namespace browser_sync
