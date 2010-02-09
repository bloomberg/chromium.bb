// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/string_util.h"
#include "chrome/browser/sync/engine/process_commit_response_command.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/test/sync/engine/syncer_command_test.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

using sessions::SyncSession;
using std::string;
using syncable::BASE_VERSION;
using syncable::Entry;
using syncable::IS_DIR;
using syncable::IS_UNSYNCED;
using syncable::Id;
using syncable::MutableEntry;
using syncable::NON_UNIQUE_NAME;
using syncable::ReadTransaction;
using syncable::ScopedDirLookup;
using syncable::UNITTEST;
using syncable::WriteTransaction;

// A test fixture for tests exercising ProcessCommitResponseCommand.
class ProcessCommitResponseCommandTest : public SyncerCommandTest {
 public:
 protected:
  ProcessCommitResponseCommandTest()
      : next_old_revision_(1),
        next_new_revision_(4000),
        next_server_position_(10000) {
  }

  // Create an unsynced item in the database.  If item_id is a local ID, it
  // will be treated as a create-new.  Otherwise, if it's a server ID, we'll
  // fake the server data so that it looks like it exists on the server.
  void CreateUnsyncedItem(const Id& item_id,
                          const Id& parent_id,
                          const string& name,
                          bool is_folder) {
    ScopedDirLookup dir(syncdb().manager(), syncdb().name());
    ASSERT_TRUE(dir.good());
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    Id predecessor_id = dir->GetLastChildId(&trans, parent_id);
    MutableEntry entry(&trans, syncable::CREATE, parent_id, name);
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::ID, item_id);
    entry.Put(syncable::BASE_VERSION,
        item_id.ServerKnows() ? next_old_revision_++ : 0);
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::IS_DIR, is_folder);
    entry.Put(syncable::IS_DEL, false);
    entry.Put(syncable::PARENT_ID, parent_id);
    entry.PutPredecessor(predecessor_id);
    sync_pb::EntitySpecifics default_bookmark_specifics;
    default_bookmark_specifics.MutableExtension(sync_pb::bookmark);
    entry.Put(syncable::SPECIFICS, default_bookmark_specifics);
    if (item_id.ServerKnows()) {
      entry.Put(syncable::SERVER_SPECIFICS, default_bookmark_specifics);
      entry.Put(syncable::SERVER_IS_DIR, is_folder);
      entry.Put(syncable::SERVER_PARENT_ID, parent_id);
      entry.Put(syncable::SERVER_IS_DEL, false);
    }
  }

  // Create a new unsynced item in the database, and synthesize a commit
  // record and a commit response for it in the syncer session.  If item_id
  // is a local ID, the item will be a create operation.  Otherwise, it
  // will be an edit.
  void CreateUnprocessedCommitResult(const Id& item_id,
                                     const Id& parent_id,
                                     const string& name) {
    sessions::StatusController* sync_state = session()->status_controller();
    bool is_folder = true;
    CreateUnsyncedItem(item_id, parent_id, name, is_folder);

    // ProcessCommitResponseCommand consumes commit_ids from the session
    // state, so we need to update that.  O(n^2) because it's a test.
    std::vector<Id> commit_ids = sync_state->commit_ids();
    commit_ids.push_back(item_id);
    sync_state->set_commit_ids(commit_ids);

    ScopedDirLookup dir(syncdb().manager(), syncdb().name());
    ASSERT_TRUE(dir.good());
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, syncable::GET_BY_ID, item_id);
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::SYNCING, true);

    // ProcessCommitResponseCommand looks at both the commit message as well
    // as the commit response, so we need to synthesize both here.
    sync_pb::ClientToServerMessage* commit =
        sync_state->mutable_commit_message();
    commit->set_message_contents(ClientToServerMessage::COMMIT);
    SyncEntity* entity = static_cast<SyncEntity*>(
        commit->mutable_commit()->add_entries());
    entity->set_non_unique_name(name);
    entity->set_folder(is_folder);
    entity->set_parent_id(parent_id);
    entity->set_version(entry.Get(syncable::BASE_VERSION));
    entity->mutable_specifics()->CopyFrom(entry.Get(syncable::SPECIFICS));
    entity->set_id(item_id);

    sync_pb::ClientToServerResponse* response =
        sync_state->mutable_commit_response();
    response->set_error_code(sync_pb::ClientToServerResponse::SUCCESS);
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response->mutable_commit()->add_entryresponse();
    entry_response->set_response_type(CommitResponse::SUCCESS);
    entry_response->set_name("Garbage.");
    entry_response->set_non_unique_name(entity->name());
    if (item_id.ServerKnows())
      entry_response->set_id_string(entity->id_string());
    else
      entry_response->set_id_string(id_factory_.NewServerId().GetServerId());
    entry_response->set_version(next_new_revision_++);
    entry_response->set_position_in_parent(next_server_position_++);

    // If the ID of our parent item committed earlier in the batch was
    // rewritten, rewrite it in the entry response.  This matches
    // the server behavior.
    entry_response->set_parent_id_string(entity->parent_id_string());
    for (int i = 0; i < commit->commit().entries_size(); ++i) {
      if (commit->commit().entries(i).id_string() ==
          entity->parent_id_string()) {
        entry_response->set_parent_id_string(
            response->commit().entryresponse(i).id_string());
      }
    }
  }

  ProcessCommitResponseCommand command_;
  TestIdFactory id_factory_;

 private:
  int64 next_old_revision_;
  int64 next_new_revision_;
  int64 next_server_position_;
  DISALLOW_COPY_AND_ASSIGN(ProcessCommitResponseCommandTest);
};

// In this test, we test processing a commit response for a commit batch that
// includes a newly created folder and some (but not all) of its children.
// In particular, the folder has 50 children, which alternate between being
// new items and preexisting items.  This mixture of new and old is meant to
// be a torture test of the code in ProcessCommitResponseCommand that changes
// an item's ID from a local ID to a server-generated ID on the first commit.
// We commit only the first 25 children in the sibling order, leaving the
// second 25 children as unsynced items.  http://crbug.com/33081 describes
// how this scenario used to fail, reversing the order for the second half
// of the children.
TEST_F(ProcessCommitResponseCommandTest, NewFolderCommitKeepsChildOrder) {
  // Create the parent folder, a new item whose ID will change on commit.
  Id folder_id = id_factory_.NewLocalId();
  CreateUnprocessedCommitResult(folder_id, id_factory_.root(), "A");

  // Verify that the item is reachable.
  {
    ScopedDirLookup dir(syncdb().manager(), syncdb().name());
    ASSERT_TRUE(dir.good());
    ReadTransaction trans(dir, __FILE__, __LINE__);
    ASSERT_EQ(folder_id, dir->GetFirstChildId(&trans, id_factory_.root()));
  }

  // The first 25 children of the parent folder will be part of the commit
  // batch.
  int batch_size = 25;
  int i = 0;
  for (; i < batch_size; ++i) {
    // Alternate between new and old child items, just for kicks.
    Id id = (i % 4 < 2) ? id_factory_.NewLocalId() : id_factory_.NewServerId();
    CreateUnprocessedCommitResult(id, folder_id, StringPrintf("Item %d", i));
  }
  // The second 25 children will be unsynced items but NOT part of the commit
  // batch.  When the ID of the parent folder changes during the commit,
  // these items PARENT_ID should be updated, and their ordering should be
  // preserved.
  for (; i < 2*batch_size; ++i) {
    // Alternate between new and old child items, just for kicks.
    Id id = (i % 4 < 2) ? id_factory_.NewLocalId() : id_factory_.NewServerId();
    CreateUnsyncedItem(id, folder_id, StringPrintf("Item %d", i), false);
  }

  // Process the commit response for the parent folder and the first
  // 25 items.  This should apply the values indicated by
  // each CommitResponse_EntryResponse to the syncable Entries.  All new
  // items in the commit batch should have their IDs changed to server IDs.
  command_.ModelChangingExecuteImpl(session());

  ScopedDirLookup dir(syncdb().manager(), syncdb().name());
  ASSERT_TRUE(dir.good());
  ReadTransaction trans(dir, __FILE__, __LINE__);
  // Lookup the parent folder by finding a child of the root.  We can't use
  // folder_id here, because it changed during the commit.
  Id new_fid = dir->GetFirstChildId(&trans, id_factory_.root());
  ASSERT_FALSE(new_fid.IsRoot());
  EXPECT_TRUE(new_fid.ServerKnows());
  EXPECT_FALSE(folder_id.ServerKnows());
  EXPECT_TRUE(new_fid != folder_id);
  Entry parent(&trans, syncable::GET_BY_ID, new_fid);
  ASSERT_TRUE(parent.good());
  ASSERT_EQ("A", parent.Get(NON_UNIQUE_NAME))
      << "Name of parent folder should not change.";
  ASSERT_LT(0, parent.Get(BASE_VERSION))
      << "Parent should have a valid (positive) server base revision";

  Id cid = dir->GetFirstChildId(&trans, new_fid);
  int child_count = 0;
  // Now loop over all the children of the parent folder, verifying
  // that they are in their original order by checking to see that their
  // names are still sequential.
  while (!cid.IsRoot()) {
    SCOPED_TRACE(::testing::Message("Examining item #") << child_count);
    Entry c(&trans, syncable::GET_BY_ID, cid);
    DCHECK(c.good());
    ASSERT_EQ(StringPrintf("Item %d", child_count), c.Get(NON_UNIQUE_NAME));
    ASSERT_EQ(new_fid, c.Get(syncable::PARENT_ID));
    if (child_count < batch_size) {
      ASSERT_FALSE(c.Get(IS_UNSYNCED)) << "Item should be committed";
      ASSERT_TRUE(cid.ServerKnows());
      ASSERT_LT(0, c.Get(BASE_VERSION));
    } else {
      ASSERT_TRUE(c.Get(IS_UNSYNCED)) << "Item should be uncommitted";
      // We alternated between creates and edits; double check that these items
      // have been preserved.
      if (child_count % 4 < 2) {
        ASSERT_FALSE(cid.ServerKnows());
        ASSERT_GE(0, c.Get(BASE_VERSION));
      } else {
        ASSERT_TRUE(cid.ServerKnows());
        ASSERT_LT(0, c.Get(BASE_VERSION));
      }
    }
    cid = c.Get(syncable::NEXT_ID);
    child_count++;
  }
  ASSERT_EQ(batch_size*2, child_count)
      << "Too few or too many children in parent folder after commit.";
}

}  // namespace browser_sync
