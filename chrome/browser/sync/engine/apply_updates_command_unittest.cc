// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/format_macros.h"
#include "base/location.h"
#include "base/stringprintf.h"
#include "chrome/browser/sync/engine/apply_updates_command.h"
#include "chrome/browser/sync/engine/nigori_util.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/test/engine/fake_model_worker.h"
#include "chrome/browser/sync/test/engine/syncer_command_test.h"
#include "chrome/browser/sync/test/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

using sessions::SyncSession;
using std::string;
using syncable::Entry;
using syncable::GetAllRealModelTypes;
using syncable::Id;
using syncable::MutableEntry;
using syncable::ReadTransaction;
using syncable::ScopedDirLookup;
using syncable::UNITTEST;
using syncable::WriteTransaction;

namespace {
sync_pb::EntitySpecifics DefaultBookmarkSpecifics() {
  sync_pb::EntitySpecifics result;
  AddDefaultExtensionValue(syncable::BOOKMARKS, &result);
  return result;
}
} // namespace

// A test fixture for tests exercising ApplyUpdatesCommand.
class ApplyUpdatesCommandTest : public SyncerCommandTest {
 public:
 protected:
  ApplyUpdatesCommandTest() : next_revision_(1) {}
  virtual ~ApplyUpdatesCommandTest() {}

  virtual void SetUp() {
    workers()->clear();
    mutable_routing_info()->clear();
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_PASSWORD)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_PASSIVE)));
    (*mutable_routing_info())[syncable::BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[syncable::PASSWORDS] = GROUP_PASSWORD;
    (*mutable_routing_info())[syncable::NIGORI] = GROUP_PASSIVE;
    SyncerCommandTest::SetUp();
  }

  // Create a new unapplied folder node with a parent.
  void CreateUnappliedNewItemWithParent(
      const string& item_id,
      const sync_pb::EntitySpecifics& specifics,
      const string& parent_id) {
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    WriteTransaction trans(FROM_HERE, UNITTEST, dir);
    MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
        Id::CreateFromServerId(item_id));
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::SERVER_VERSION, next_revision_++);
    entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);

    entry.Put(syncable::SERVER_NON_UNIQUE_NAME, item_id);
    entry.Put(syncable::SERVER_PARENT_ID, Id::CreateFromServerId(parent_id));
    entry.Put(syncable::SERVER_IS_DIR, true);
    entry.Put(syncable::SERVER_SPECIFICS, specifics);
  }

  // Create a new unapplied update without a parent.
  void CreateUnappliedNewItem(const string& item_id,
                              const sync_pb::EntitySpecifics& specifics,
                              bool is_unique) {
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    WriteTransaction trans(FROM_HERE, UNITTEST, dir);
    MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
        Id::CreateFromServerId(item_id));
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::SERVER_VERSION, next_revision_++);
    entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
    entry.Put(syncable::SERVER_NON_UNIQUE_NAME, item_id);
    entry.Put(syncable::SERVER_PARENT_ID, syncable::GetNullId());
    entry.Put(syncable::SERVER_IS_DIR, false);
    entry.Put(syncable::SERVER_SPECIFICS, specifics);
    if (is_unique)  // For top-level nodes.
      entry.Put(syncable::UNIQUE_SERVER_TAG, item_id);
  }

  // Create an unsynced item in the database.  If item_id is a local ID, it
  // will be treated as a create-new.  Otherwise, if it's a server ID, we'll
  // fake the server data so that it looks like it exists on the server.
  // Returns the methandle of the created item in |metahandle_out| if not NULL.
  void CreateUnsyncedItem(const Id& item_id,
                          const Id& parent_id,
                          const string& name,
                          bool is_folder,
                          syncable::ModelType model_type,
                          int64* metahandle_out) {
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    WriteTransaction trans(FROM_HERE, UNITTEST, dir);
    Id predecessor_id;
    ASSERT_TRUE(
        dir->GetLastChildIdForTest(&trans, parent_id, &predecessor_id));
    MutableEntry entry(&trans, syncable::CREATE, parent_id, name);
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::ID, item_id);
    entry.Put(syncable::BASE_VERSION,
        item_id.ServerKnows() ? next_revision_++ : 0);
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::IS_DIR, is_folder);
    entry.Put(syncable::IS_DEL, false);
    entry.Put(syncable::PARENT_ID, parent_id);
    CHECK(entry.PutPredecessor(predecessor_id));
    sync_pb::EntitySpecifics default_specifics;
    syncable::AddDefaultExtensionValue(model_type, &default_specifics);
    entry.Put(syncable::SPECIFICS, default_specifics);
    if (item_id.ServerKnows()) {
      entry.Put(syncable::SERVER_SPECIFICS, default_specifics);
      entry.Put(syncable::SERVER_IS_DIR, is_folder);
      entry.Put(syncable::SERVER_PARENT_ID, parent_id);
      entry.Put(syncable::SERVER_IS_DEL, false);
    }
    if (metahandle_out)
      *metahandle_out = entry.Get(syncable::META_HANDLE);
  }

  ApplyUpdatesCommand apply_updates_command_;
  TestIdFactory id_factory_;
 private:
  int64 next_revision_;
  DISALLOW_COPY_AND_ASSIGN(ApplyUpdatesCommandTest);
};

TEST_F(ApplyUpdatesCommandTest, Simple) {
  string root_server_id = syncable::GetNullId().GetServerId();
  CreateUnappliedNewItemWithParent("parent",
                                   DefaultBookmarkSpecifics(),
                                   root_server_id);
  CreateUnappliedNewItemWithParent("child",
                                   DefaultBookmarkSpecifics(),
                                   "parent");

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();

  sessions::ScopedModelSafeGroupRestriction r(status, GROUP_UI);
  ASSERT_TRUE(status->update_progress());
  EXPECT_EQ(2, status->update_progress()->AppliedUpdatesSize())
      << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
  EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
      << "Simple update shouldn't result in conflicts";
  EXPECT_EQ(2, status->update_progress()->SuccessfullyAppliedUpdateCount())
      << "All items should have been successfully applied";
}

TEST_F(ApplyUpdatesCommandTest, UpdateWithChildrenBeforeParents) {
  // Set a bunch of updates which are difficult to apply in the order
  // they're received due to dependencies on other unseen items.
  string root_server_id = syncable::GetNullId().GetServerId();
  CreateUnappliedNewItemWithParent("a_child_created_first",
                                   DefaultBookmarkSpecifics(),
                                   "parent");
  CreateUnappliedNewItemWithParent("x_child_created_first",
                                   DefaultBookmarkSpecifics(),
                                   "parent");
  CreateUnappliedNewItemWithParent("parent",
                                   DefaultBookmarkSpecifics(),
                                   root_server_id);
  CreateUnappliedNewItemWithParent("a_child_created_second",
                                   DefaultBookmarkSpecifics(),
                                   "parent");
  CreateUnappliedNewItemWithParent("x_child_created_second",
                                   DefaultBookmarkSpecifics(),
                                   "parent");

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  sessions::ScopedModelSafeGroupRestriction r(status, GROUP_UI);
  ASSERT_TRUE(status->update_progress());
  EXPECT_EQ(5, status->update_progress()->AppliedUpdatesSize())
      << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
  EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
      << "Simple update shouldn't result in conflicts, even if out-of-order";
  EXPECT_EQ(5, status->update_progress()->SuccessfullyAppliedUpdateCount())
      << "All updates should have been successfully applied";
}

TEST_F(ApplyUpdatesCommandTest, NestedItemsWithUnknownParent) {
  // We shouldn't be able to do anything with either of these items.
  CreateUnappliedNewItemWithParent("some_item",
                                   DefaultBookmarkSpecifics(),
                                   "unknown_parent");
  CreateUnappliedNewItemWithParent("some_other_item",
                                   DefaultBookmarkSpecifics(),
                                   "some_item");

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  sessions::ScopedModelSafeGroupRestriction r(status, GROUP_UI);
  ASSERT_TRUE(status->update_progress());
  EXPECT_EQ(2, status->update_progress()->AppliedUpdatesSize())
      << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
  EXPECT_EQ(2, status->conflict_progress()->ConflictingItemsSize())
      << "All updates with an unknown ancestors should be in conflict";
  EXPECT_EQ(0, status->update_progress()->SuccessfullyAppliedUpdateCount())
      << "No item with an unknown ancestor should be applied";
}

TEST_F(ApplyUpdatesCommandTest, ItemsBothKnownAndUnknown) {
  // See what happens when there's a mixture of good and bad updates.
  string root_server_id = syncable::GetNullId().GetServerId();
  CreateUnappliedNewItemWithParent("first_unknown_item",
                                   DefaultBookmarkSpecifics(),
                                   "unknown_parent");
  CreateUnappliedNewItemWithParent("first_known_item",
                                   DefaultBookmarkSpecifics(),
                                   root_server_id);
  CreateUnappliedNewItemWithParent("second_unknown_item",
                                   DefaultBookmarkSpecifics(),
                                   "unknown_parent");
  CreateUnappliedNewItemWithParent("second_known_item",
                                   DefaultBookmarkSpecifics(),
                                   "first_known_item");
  CreateUnappliedNewItemWithParent("third_known_item",
                                   DefaultBookmarkSpecifics(),
                                   "fourth_known_item");
  CreateUnappliedNewItemWithParent("fourth_known_item",
                                   DefaultBookmarkSpecifics(),
                                   root_server_id);

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  sessions::ScopedModelSafeGroupRestriction r(status, GROUP_UI);
  ASSERT_TRUE(status->update_progress());
  EXPECT_EQ(6, status->update_progress()->AppliedUpdatesSize())
      << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
  EXPECT_EQ(2, status->conflict_progress()->ConflictingItemsSize())
      << "The updates with unknown ancestors should be in conflict";
  EXPECT_EQ(4, status->update_progress()->SuccessfullyAppliedUpdateCount())
      << "The updates with known ancestors should be successfully applied";
}

TEST_F(ApplyUpdatesCommandTest, DecryptablePassword) {
  // Decryptable password updates should be applied.
  Cryptographer* cryptographer;
  {
      // Storing the cryptographer separately is bad, but for this test we
      // know it's safe.
      ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
      ASSERT_TRUE(dir.good());
      ReadTransaction trans(FROM_HERE, dir);
      cryptographer =
          session()->context()->directory_manager()->GetCryptographer(&trans);
  }

  browser_sync::KeyParams params = {"localhost", "dummy", "foobar"};
  cryptographer->AddKey(params);

  sync_pb::EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData data;
  data.set_origin("http://example.com");

  cryptographer->Encrypt(data,
      specifics.MutableExtension(sync_pb::password)->mutable_encrypted());
  CreateUnappliedNewItem("item", specifics, false);

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  sessions::ScopedModelSafeGroupRestriction r(status, GROUP_PASSWORD);
  ASSERT_TRUE(status->update_progress());
  EXPECT_EQ(1, status->update_progress()->AppliedUpdatesSize())
      << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
  EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
      << "No update should be in conflict because they're all decryptable";
  EXPECT_EQ(1, status->update_progress()->SuccessfullyAppliedUpdateCount())
      << "The updates that can be decrypted should be applied";
}

TEST_F(ApplyUpdatesCommandTest, UndecryptableData) {
  // Undecryptable updates should not be applied.
  sync_pb::EntitySpecifics encrypted_bookmark;
  encrypted_bookmark.mutable_encrypted();
  AddDefaultExtensionValue(syncable::BOOKMARKS, &encrypted_bookmark);
  string root_server_id = syncable::GetNullId().GetServerId();
  CreateUnappliedNewItemWithParent("folder",
                                   encrypted_bookmark,
                                   root_server_id);
  CreateUnappliedNewItem("item2", encrypted_bookmark, false);
  sync_pb::EntitySpecifics encrypted_password;
  encrypted_password.MutableExtension(sync_pb::password);
  CreateUnappliedNewItem("item3", encrypted_password, false);

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  EXPECT_TRUE(status->HasConflictingUpdates())
    << "Updates that can't be decrypted should trigger the syncer to have "
    << "conflicting updates.";
  {
    sessions::ScopedModelSafeGroupRestriction r(status, GROUP_UI);
    ASSERT_TRUE(status->update_progress());
    EXPECT_EQ(2, status->update_progress()->AppliedUpdatesSize())
        << "All updates should have been attempted";
    ASSERT_TRUE(status->conflict_progress());
    EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
        << "The updates that can't be decrypted should not be in regular "
        << "conflict";
    EXPECT_EQ(2, status->conflict_progress()->NonblockingConflictingItemsSize())
        << "The updates that can't be decrypted should be in nonblocking "
        << "conflict";
    EXPECT_EQ(0, status->update_progress()->SuccessfullyAppliedUpdateCount())
        << "No update that can't be decrypted should be applied";
  }
  {
    sessions::ScopedModelSafeGroupRestriction r(status, GROUP_PASSWORD);
    ASSERT_TRUE(status->update_progress());
    EXPECT_EQ(1, status->update_progress()->AppliedUpdatesSize())
        << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
    EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
        << "The updates that can't be decrypted should not be in regular "
        << "conflict";
    EXPECT_EQ(1, status->conflict_progress()->NonblockingConflictingItemsSize())
        << "The updates that can't be decrypted should be in nonblocking "
        << "conflict";
    EXPECT_EQ(0, status->update_progress()->SuccessfullyAppliedUpdateCount())
        << "No update that can't be decrypted should be applied";
  }
}

TEST_F(ApplyUpdatesCommandTest, SomeUndecryptablePassword) {
  // Only decryptable password updates should be applied.
  {
    sync_pb::EntitySpecifics specifics;
    sync_pb::PasswordSpecificsData data;
    data.set_origin("http://example.com/1");
    {
      ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
      ASSERT_TRUE(dir.good());
      ReadTransaction trans(FROM_HERE, dir);
      Cryptographer* cryptographer =
          session()->context()->directory_manager()->GetCryptographer(&trans);

      KeyParams params = {"localhost", "dummy", "foobar"};
      cryptographer->AddKey(params);

      cryptographer->Encrypt(data,
          specifics.MutableExtension(sync_pb::password)->mutable_encrypted());
    }
    CreateUnappliedNewItem("item1", specifics, false);
  }
  {
    // Create a new cryptographer, independent of the one in the session.
    Cryptographer cryptographer;
    KeyParams params = {"localhost", "dummy", "bazqux"};
    cryptographer.AddKey(params);

    sync_pb::EntitySpecifics specifics;
    sync_pb::PasswordSpecificsData data;
    data.set_origin("http://example.com/2");

    cryptographer.Encrypt(data,
        specifics.MutableExtension(sync_pb::password)->mutable_encrypted());
    CreateUnappliedNewItem("item2", specifics, false);
  }

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  EXPECT_TRUE(status->HasConflictingUpdates())
    << "Updates that can't be decrypted should trigger the syncer to have "
    << "conflicting updates.";
  {
    sessions::ScopedModelSafeGroupRestriction r(status, GROUP_PASSWORD);
    ASSERT_TRUE(status->update_progress());
    EXPECT_EQ(2, status->update_progress()->AppliedUpdatesSize())
        << "All updates should have been attempted";
    ASSERT_TRUE(status->conflict_progress());
    EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
        << "The updates that can't be decrypted should not be in regular "
        << "conflict";
    EXPECT_EQ(1, status->conflict_progress()->NonblockingConflictingItemsSize())
        << "The updates that can't be decrypted should be in nonblocking "
        << "conflict";
    EXPECT_EQ(1, status->update_progress()->SuccessfullyAppliedUpdateCount())
        << "The undecryptable password update shouldn't be applied";
  }
}

TEST_F(ApplyUpdatesCommandTest, NigoriUpdate) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  syncable::ModelTypeSet encrypted_types;
  encrypted_types.insert(syncable::PASSWORDS);
  encrypted_types.insert(syncable::NIGORI);
  {
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    ReadTransaction trans(FROM_HERE, dir);
    cryptographer =
        session()->context()->directory_manager()->GetCryptographer(&trans);
    EXPECT_EQ(encrypted_types, cryptographer->GetEncryptedTypes());
  }

  // Nigori node updates should update the Cryptographer.
  Cryptographer other_cryptographer;
  KeyParams params = {"localhost", "dummy", "foobar"};
  other_cryptographer.AddKey(params);

  sync_pb::EntitySpecifics specifics;
  sync_pb::NigoriSpecifics* nigori =
      specifics.MutableExtension(sync_pb::nigori);
  other_cryptographer.GetKeys(nigori->mutable_encrypted());
  nigori->set_encrypt_bookmarks(true);
  encrypted_types.insert(syncable::BOOKMARKS);
  CreateUnappliedNewItem(syncable::ModelTypeToRootTag(syncable::NIGORI),
                         specifics, true);
  EXPECT_FALSE(cryptographer->has_pending_keys());

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  sessions::ScopedModelSafeGroupRestriction r(status, GROUP_PASSIVE);
  ASSERT_TRUE(status->update_progress());
  EXPECT_EQ(1, status->update_progress()->AppliedUpdatesSize())
      << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
  EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
      << "The nigori update shouldn't be in conflict";
  EXPECT_EQ(1, status->update_progress()->SuccessfullyAppliedUpdateCount())
      << "The nigori update should be applied";

  EXPECT_FALSE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->has_pending_keys());
  EXPECT_EQ(GetAllRealModelTypes(), cryptographer->GetEncryptedTypes());
}

TEST_F(ApplyUpdatesCommandTest, NigoriUpdateForDisabledTypes) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  syncable::ModelTypeSet encrypted_types;
  encrypted_types.insert(syncable::PASSWORDS);
  encrypted_types.insert(syncable::NIGORI);
  {
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    ReadTransaction trans(FROM_HERE, dir);
    cryptographer =
        session()->context()->directory_manager()->GetCryptographer(&trans);
    EXPECT_EQ(encrypted_types, cryptographer->GetEncryptedTypes());
  }

  // Nigori node updates should update the Cryptographer.
  Cryptographer other_cryptographer;
  KeyParams params = {"localhost", "dummy", "foobar"};
  other_cryptographer.AddKey(params);

  sync_pb::EntitySpecifics specifics;
  sync_pb::NigoriSpecifics* nigori =
      specifics.MutableExtension(sync_pb::nigori);
  other_cryptographer.GetKeys(nigori->mutable_encrypted());
  nigori->set_encrypt_sessions(true);
  nigori->set_encrypt_themes(true);
  encrypted_types.insert(syncable::SESSIONS);
  encrypted_types.insert(syncable::THEMES);
  CreateUnappliedNewItem(syncable::ModelTypeToRootTag(syncable::NIGORI),
                         specifics, true);
  EXPECT_FALSE(cryptographer->has_pending_keys());

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  sessions::ScopedModelSafeGroupRestriction r(status, GROUP_PASSIVE);
  ASSERT_TRUE(status->update_progress());
  EXPECT_EQ(1, status->update_progress()->AppliedUpdatesSize())
      << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
  EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
      << "The nigori update shouldn't be in conflict";
  EXPECT_EQ(1, status->update_progress()->SuccessfullyAppliedUpdateCount())
      << "The nigori update should be applied";

  EXPECT_FALSE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->has_pending_keys());
  EXPECT_EQ(GetAllRealModelTypes(), cryptographer->GetEncryptedTypes());
}

TEST_F(ApplyUpdatesCommandTest, EncryptUnsyncedChanges) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  syncable::ModelTypeSet encrypted_types;
  encrypted_types.insert(syncable::PASSWORDS);
  encrypted_types.insert(syncable::NIGORI);
  {
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    ReadTransaction trans(FROM_HERE, dir);
    cryptographer =
        session()->context()->directory_manager()->GetCryptographer(&trans);
    EXPECT_EQ(encrypted_types, cryptographer->GetEncryptedTypes());


    // With default encrypted_types, this should be true.
    EXPECT_TRUE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));

    Syncer::UnsyncedMetaHandles handles;
    SyncerUtil::GetUnsyncedEntries(&trans, &handles);
    EXPECT_TRUE(handles.empty());
  }

  // Create unsynced bookmarks without encryption.
  // First item is a folder
  Id folder_id = id_factory_.NewLocalId();
  CreateUnsyncedItem(folder_id, id_factory_.root(), "folder",
                     true, syncable::BOOKMARKS, NULL);
  // Next five items are children of the folder
  size_t i;
  size_t batch_s = 5;
  for (i = 0; i < batch_s; ++i) {
    CreateUnsyncedItem(id_factory_.NewLocalId(), folder_id,
                       base::StringPrintf("Item %"PRIuS"", i), false,
                       syncable::BOOKMARKS, NULL);
  }
  // Next five items are children of the root.
  for (; i < 2*batch_s; ++i) {
    CreateUnsyncedItem(id_factory_.NewLocalId(), id_factory_.root(),
                       base::StringPrintf("Item %"PRIuS"", i), false,
                       syncable::BOOKMARKS, NULL);
  }

  KeyParams params = {"localhost", "dummy", "foobar"};
  cryptographer->AddKey(params);
  sync_pb::EntitySpecifics specifics;
  sync_pb::NigoriSpecifics* nigori =
      specifics.MutableExtension(sync_pb::nigori);
  cryptographer->GetKeys(nigori->mutable_encrypted());
  nigori->set_encrypt_bookmarks(true);
  encrypted_types.insert(syncable::BOOKMARKS);
  CreateUnappliedNewItem(syncable::ModelTypeToRootTag(syncable::NIGORI),
                         specifics, true);
  EXPECT_FALSE(cryptographer->has_pending_keys());
  EXPECT_TRUE(cryptographer->is_ready());

  {
    // Ensure we have unsynced nodes that aren't properly encrypted.
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    ReadTransaction trans(FROM_HERE, dir);
    EXPECT_FALSE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));

    Syncer::UnsyncedMetaHandles handles;
    SyncerUtil::GetUnsyncedEntries(&trans, &handles);
    EXPECT_EQ(2*batch_s+1, handles.size());
  }

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  sessions::ScopedModelSafeGroupRestriction r(status, GROUP_PASSIVE);
  ASSERT_TRUE(status->update_progress());
  EXPECT_EQ(1, status->update_progress()->AppliedUpdatesSize())
      << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
  EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
      << "No updates should be in conflict";
  EXPECT_EQ(0, status->conflict_progress()->NonblockingConflictingItemsSize())
      << "No updates should be in conflict";
  EXPECT_EQ(1, status->update_progress()->SuccessfullyAppliedUpdateCount())
      << "The nigori update should be applied";
  EXPECT_FALSE(cryptographer->has_pending_keys());
  EXPECT_TRUE(cryptographer->is_ready());
  {
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    ReadTransaction trans(FROM_HERE, dir);

    // If ProcessUnsyncedChangesForEncryption worked, all our unsynced changes
    // should be encrypted now.
    EXPECT_EQ(GetAllRealModelTypes(), cryptographer->GetEncryptedTypes());
    EXPECT_TRUE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));

    Syncer::UnsyncedMetaHandles handles;
    SyncerUtil::GetUnsyncedEntries(&trans, &handles);
    EXPECT_EQ(2*batch_s+1, handles.size());
  }
}

TEST_F(ApplyUpdatesCommandTest, CannotEncryptUnsyncedChanges) {
  // Storing the cryptographer separately is bad, but for this test we
  // know it's safe.
  Cryptographer* cryptographer;
  syncable::ModelTypeSet encrypted_types;
  encrypted_types.insert(syncable::PASSWORDS);
  encrypted_types.insert(syncable::NIGORI);
  {
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    ReadTransaction trans(FROM_HERE, dir);
    cryptographer =
        session()->context()->directory_manager()->GetCryptographer(&trans);
    EXPECT_EQ(encrypted_types, cryptographer->GetEncryptedTypes());


    // With default encrypted_types, this should be true.
    EXPECT_TRUE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));

    Syncer::UnsyncedMetaHandles handles;
    SyncerUtil::GetUnsyncedEntries(&trans, &handles);
    EXPECT_TRUE(handles.empty());
  }

  // Create unsynced bookmarks without encryption.
  // First item is a folder
  Id folder_id = id_factory_.NewLocalId();
  CreateUnsyncedItem(folder_id, id_factory_.root(), "folder", true,
                     syncable::BOOKMARKS, NULL);
  // Next five items are children of the folder
  size_t i;
  size_t batch_s = 5;
  for (i = 0; i < batch_s; ++i) {
    CreateUnsyncedItem(id_factory_.NewLocalId(), folder_id,
                       base::StringPrintf("Item %"PRIuS"", i), false,
                       syncable::BOOKMARKS, NULL);
  }
  // Next five items are children of the root.
  for (; i < 2*batch_s; ++i) {
    CreateUnsyncedItem(id_factory_.NewLocalId(), id_factory_.root(),
                       base::StringPrintf("Item %"PRIuS"", i), false,
                       syncable::BOOKMARKS, NULL);
  }

  // We encrypt with new keys, triggering the local cryptographer to be unready
  // and unable to decrypt data (once updated).
  Cryptographer other_cryptographer;
  KeyParams params = {"localhost", "dummy", "foobar"};
  other_cryptographer.AddKey(params);
  sync_pb::EntitySpecifics specifics;
  sync_pb::NigoriSpecifics* nigori =
      specifics.MutableExtension(sync_pb::nigori);
  other_cryptographer.GetKeys(nigori->mutable_encrypted());
  nigori->set_encrypt_bookmarks(true);
  encrypted_types.insert(syncable::BOOKMARKS);
  CreateUnappliedNewItem(syncable::ModelTypeToRootTag(syncable::NIGORI),
                         specifics, true);
  EXPECT_FALSE(cryptographer->has_pending_keys());

  {
    // Ensure we have unsynced nodes that aren't properly encrypted.
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    ReadTransaction trans(FROM_HERE, dir);
    EXPECT_FALSE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));
    Syncer::UnsyncedMetaHandles handles;
    SyncerUtil::GetUnsyncedEntries(&trans, &handles);
    EXPECT_EQ(2*batch_s+1, handles.size());
  }

  apply_updates_command_.ExecuteImpl(session());

  sessions::StatusController* status = session()->mutable_status_controller();
  sessions::ScopedModelSafeGroupRestriction r(status, GROUP_PASSIVE);
  ASSERT_TRUE(status->update_progress());
  EXPECT_EQ(1, status->update_progress()->AppliedUpdatesSize())
      << "All updates should have been attempted";
  ASSERT_TRUE(status->conflict_progress());
  EXPECT_EQ(0, status->conflict_progress()->ConflictingItemsSize())
      << "The unsynced changes don't trigger a blocking conflict with the "
      << "nigori update.";
  EXPECT_EQ(1, status->conflict_progress()->NonblockingConflictingItemsSize())
      << "The unsynced changes trigger a non-blocking conflict with the "
      << "nigori update.";
  EXPECT_EQ(0, status->update_progress()->SuccessfullyAppliedUpdateCount())
      << "The nigori update should not be applied";
  EXPECT_FALSE(cryptographer->is_ready());
  EXPECT_TRUE(cryptographer->has_pending_keys());
  {
    // Ensure the unsynced nodes are still not encrypted.
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    ReadTransaction trans(FROM_HERE, dir);

    // Since we're in conflict, the specifics don't reflect the unapplied
    // changes.
    EXPECT_FALSE(VerifyUnsyncedChangesAreEncrypted(&trans, encrypted_types));
    encrypted_types.clear();
    encrypted_types.insert(syncable::PASSWORDS);
    encrypted_types.insert(syncable::BOOKMARKS);
    EXPECT_EQ(GetAllRealModelTypes(), cryptographer->GetEncryptedTypes());

    Syncer::UnsyncedMetaHandles handles;
    SyncerUtil::GetUnsyncedEntries(&trans, &handles);
    EXPECT_EQ(2*batch_s+1, handles.size());
  }
}

}  // namespace browser_sync
