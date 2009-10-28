// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/build_commit_command.h"

#include <set>
#include <string>
#include <vector>

#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_changes_version.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/sync_types.h"

using std::set;
using std::string;
using std::vector;
using syncable::ExtendedAttribute;
using syncable::Id;
using syncable::MutableEntry;
using syncable::Name;

namespace browser_sync {

BuildCommitCommand::BuildCommitCommand() {}
BuildCommitCommand::~BuildCommitCommand() {}

void BuildCommitCommand::AddExtensionsActivityToMessage(
    SyncerSession* session, CommitMessage* message) {
  const ExtensionsActivityMonitor::Records& records =
      session->extensions_activity();
  for (ExtensionsActivityMonitor::Records::const_iterator it = records.begin();
       it != records.end(); ++it) {
    sync_pb::CommitMessage_ChromiumExtensionsActivity* activity_message =
        message->add_extensions_activity();
    activity_message->set_extension_id(it->second.extension_id);
    activity_message->set_bookmark_writes_since_last_commit(
        it->second.bookmark_write_count);
  }
}

void BuildCommitCommand::ExecuteImpl(SyncerSession* session) {
  ClientToServerMessage message;
  message.set_share(ToUTF8(session->account_name()).get_string());
  message.set_message_contents(ClientToServerMessage::COMMIT);

  CommitMessage* commit_message = message.mutable_commit();
  commit_message->set_cache_guid(
      session->write_transaction()->directory()->cache_guid());
  AddExtensionsActivityToMessage(session, commit_message);

  const vector<Id>& commit_ids = session->commit_ids();
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

    Name name = meta_entry.GetName();
    CHECK(!name.value().empty());  // Make sure this isn't an update.
    sync_entry->set_name(ToUTF8(name.value()).get_string());
    // Set the non_unique_name if we have one.  If we do, the server ignores
    // the |name| value (using |non_unique_name| instead), and will return
    // in the CommitResponse a unique name if one is generated.  Even though
    // we could get away with only sending |name|, we send both because it
    // may aid in logging.
    if (name.value() != name.non_unique_value()) {
      sync_entry->set_non_unique_name(
        ToUTF8(name.non_unique_value()).get_string());
    }
    // Deleted items with negative parent ids can be a problem so we set the
    // parent to 0. (TODO(sync): Still true in protocol?).
    Id new_parent_id;
    if (meta_entry.Get(syncable::IS_DEL) &&
        !meta_entry.Get(syncable::PARENT_ID).ServerKnows()) {
      new_parent_id = session->write_transaction()->root_id();
    } else {
      new_parent_id = meta_entry.Get(syncable::PARENT_ID);
    }
    sync_entry->set_parent_id(new_parent_id);
    // TODO(sync): Investigate all places that think transactional commits
    // actually exist.
    //
    // This is the only logic we'll need when transactional commits are moved
    // to the server. If our parent has changes, send up the old one so the
    // server can correctly deal with multiple parents.
    if (new_parent_id != meta_entry.Get(syncable::SERVER_PARENT_ID) &&
        0 != meta_entry.Get(syncable::BASE_VERSION) &&
        syncable::CHANGES_VERSION != meta_entry.Get(syncable::BASE_VERSION)) {
      sync_entry->set_old_parent_id(meta_entry.Get(syncable::SERVER_PARENT_ID));
    }

    int64 version = meta_entry.Get(syncable::BASE_VERSION);
    if (syncable::CHANGES_VERSION == version || 0 == version) {
      // If this CHECK triggers during unit testing, check that we haven't
      // altered an item that's an unapplied update.
      CHECK(!id.ServerKnows()) << meta_entry;
      sync_entry->set_version(0);
    } else {
      CHECK(id.ServerKnows()) << meta_entry;
      sync_entry->set_version(meta_entry.Get(syncable::BASE_VERSION));
    }
    sync_entry->set_ctime(ClientTimeToServerTime(
        meta_entry.Get(syncable::CTIME)));
    sync_entry->set_mtime(ClientTimeToServerTime(
        meta_entry.Get(syncable::MTIME)));

    set<ExtendedAttribute> extended_attributes;
    meta_entry.GetAllExtendedAttributes(
        session->write_transaction(), &extended_attributes);
    set<ExtendedAttribute>::iterator iter;
    sync_pb::ExtendedAttributes* mutable_extended_attributes =
        sync_entry->mutable_extended_attributes();
    for (iter = extended_attributes.begin(); iter != extended_attributes.end();
        ++iter) {
      sync_pb::ExtendedAttributes_ExtendedAttribute *extended_attribute =
          mutable_extended_attributes->add_extendedattribute();
      extended_attribute->set_key(ToUTF8(iter->key()).get_string());
      SyncerProtoUtil::CopyBlobIntoProtoBytes(iter->value(),
          extended_attribute->mutable_value());
    }

    // Deletion is final on the server, let's move things and then delete them.
    if (meta_entry.Get(syncable::IS_DEL)) {
      sync_entry->set_deleted(true);
    } else if (meta_entry.Get(syncable::IS_BOOKMARK_OBJECT)) {
      sync_pb::SyncEntity_BookmarkData* bookmark =
          sync_entry->mutable_bookmarkdata();
      bookmark->set_bookmark_folder(meta_entry.Get(syncable::IS_DIR));
      const Id& prev_id = meta_entry.Get(syncable::PREV_ID);
      string prev_string = prev_id.IsRoot() ? string() : prev_id.GetServerId();
      sync_entry->set_insert_after_item_id(prev_string);

      if (!meta_entry.Get(syncable::IS_DIR)) {
        string bookmark_url = ToUTF8(meta_entry.Get(syncable::BOOKMARK_URL));
        bookmark->set_bookmark_url(bookmark_url);
        SyncerProtoUtil::CopyBlobIntoProtoBytes(
            meta_entry.Get(syncable::BOOKMARK_FAVICON),
            bookmark->mutable_bookmark_favicon());
      }
    }
  }
  session->set_commit_message(message);
}

}  // namespace browser_sync
