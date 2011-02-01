// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the SyncApi. Note that a lot of the underlying
// functionality is provided by the Syncable layer, which has its own
// unit tests. We'll test SyncApi specific things in this harness.

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_backend.h"
#include "chrome/browser/sync/js_event_handler.h"
#include "chrome/browser/sync/js_event_router.h"
#include "chrome/browser/sync/js_test_util.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::HasArgsAsList;
using browser_sync::KeyParams;
using browser_sync::JsArgList;
using browser_sync::MockJsEventHandler;
using browser_sync::MockJsEventRouter;
using testing::_;
using testing::StrictMock;

namespace sync_api {

class SyncApiTest : public testing::Test {
 public:
  virtual void SetUp() {
    setter_upper_.SetUp();
    share_.dir_manager.reset(setter_upper_.manager());
    share_.name = setter_upper_.name();
  }

  virtual void TearDown() {
    // |share_.dir_manager| does not actually own its value.
    ignore_result(share_.dir_manager.release());
    setter_upper_.TearDown();
  }

 protected:
  UserShare share_;
  browser_sync::TestDirectorySetterUpper setter_upper_;
};

TEST_F(SyncApiTest, SanityCheckTest) {
  {
    ReadTransaction trans(&share_);
    EXPECT_TRUE(trans.GetWrappedTrans() != NULL);
  }
  {
    WriteTransaction trans(&share_);
    EXPECT_TRUE(trans.GetWrappedTrans() != NULL);
  }
  {
    // No entries but root should exist
    ReadTransaction trans(&share_);
    ReadNode node(&trans);
    // Metahandle 1 can be root, sanity check 2
    EXPECT_FALSE(node.InitByIdLookup(2));
  }
}

TEST_F(SyncApiTest, BasicTagWrite) {
  {
    WriteTransaction trans(&share_);
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_EQ(root_node.GetFirstChildId(), 0);

    WriteNode wnode(&trans);
    EXPECT_TRUE(wnode.InitUniqueByCreation(syncable::BOOKMARKS,
        root_node, "testtag"));
    wnode.SetIsFolder(false);
  }
  {
    ReadTransaction trans(&share_);
    ReadNode node(&trans);
    EXPECT_TRUE(node.InitByClientTagLookup(syncable::BOOKMARKS,
        "testtag"));

    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_NE(node.GetId(), 0);
    EXPECT_EQ(node.GetId(), root_node.GetFirstChildId());
  }
}

TEST_F(SyncApiTest, GenerateSyncableHash) {
  EXPECT_EQ("OyaXV5mEzrPS4wbogmtKvRfekAI=",
      BaseNode::GenerateSyncableHash(syncable::BOOKMARKS, "tag1"));
  EXPECT_EQ("iNFQtRFQb+IZcn1kKUJEZDDkLs4=",
      BaseNode::GenerateSyncableHash(syncable::PREFERENCES, "tag1"));
  EXPECT_EQ("gO1cPZQXaM73sHOvSA+tKCKFs58=",
      BaseNode::GenerateSyncableHash(syncable::AUTOFILL, "tag1"));

  EXPECT_EQ("A0eYIHXM1/jVwKDDp12Up20IkKY=",
      BaseNode::GenerateSyncableHash(syncable::BOOKMARKS, "tag2"));
  EXPECT_EQ("XYxkF7bhS4eItStFgiOIAU23swI=",
      BaseNode::GenerateSyncableHash(syncable::PREFERENCES, "tag2"));
  EXPECT_EQ("GFiWzo5NGhjLlN+OyCfhy28DJTQ=",
      BaseNode::GenerateSyncableHash(syncable::AUTOFILL, "tag2"));
}

TEST_F(SyncApiTest, ModelTypesSiloed) {
  {
    WriteTransaction trans(&share_);
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_EQ(root_node.GetFirstChildId(), 0);

    WriteNode bookmarknode(&trans);
    EXPECT_TRUE(bookmarknode.InitUniqueByCreation(syncable::BOOKMARKS,
        root_node, "collideme"));
    bookmarknode.SetIsFolder(false);

    WriteNode prefnode(&trans);
    EXPECT_TRUE(prefnode.InitUniqueByCreation(syncable::PREFERENCES,
        root_node, "collideme"));
    prefnode.SetIsFolder(false);

    WriteNode autofillnode(&trans);
    EXPECT_TRUE(autofillnode.InitUniqueByCreation(syncable::AUTOFILL,
        root_node, "collideme"));
    autofillnode.SetIsFolder(false);
  }
  {
    ReadTransaction trans(&share_);

    ReadNode bookmarknode(&trans);
    EXPECT_TRUE(bookmarknode.InitByClientTagLookup(syncable::BOOKMARKS,
        "collideme"));

    ReadNode prefnode(&trans);
    EXPECT_TRUE(prefnode.InitByClientTagLookup(syncable::PREFERENCES,
        "collideme"));

    ReadNode autofillnode(&trans);
    EXPECT_TRUE(autofillnode.InitByClientTagLookup(syncable::AUTOFILL,
        "collideme"));

    EXPECT_NE(bookmarknode.GetId(), prefnode.GetId());
    EXPECT_NE(autofillnode.GetId(), prefnode.GetId());
    EXPECT_NE(bookmarknode.GetId(), autofillnode.GetId());
  }
}

TEST_F(SyncApiTest, ReadMissingTagsFails) {
  {
    ReadTransaction trans(&share_);
    ReadNode node(&trans);
    EXPECT_FALSE(node.InitByClientTagLookup(syncable::BOOKMARKS,
        "testtag"));
  }
  {
    WriteTransaction trans(&share_);
    WriteNode node(&trans);
    EXPECT_FALSE(node.InitByClientTagLookup(syncable::BOOKMARKS,
        "testtag"));
  }
}

// TODO(chron): Hook this all up to the server and write full integration tests
//              for update->undelete behavior.
TEST_F(SyncApiTest, TestDeleteBehavior) {

  int64 node_id;
  int64 folder_id;
  std::wstring test_title(L"test1");

  {
    WriteTransaction trans(&share_);
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    // we'll use this spare folder later
    WriteNode folder_node(&trans);
    EXPECT_TRUE(folder_node.InitByCreation(syncable::BOOKMARKS,
        root_node, NULL));
    folder_id = folder_node.GetId();

    WriteNode wnode(&trans);
    EXPECT_TRUE(wnode.InitUniqueByCreation(syncable::BOOKMARKS,
        root_node, "testtag"));
    wnode.SetIsFolder(false);
    wnode.SetTitle(test_title);

    node_id = wnode.GetId();
  }

  // Ensure we can delete something with a tag.
  {
    WriteTransaction trans(&share_);
    WriteNode wnode(&trans);
    EXPECT_TRUE(wnode.InitByClientTagLookup(syncable::BOOKMARKS,
        "testtag"));
    EXPECT_FALSE(wnode.GetIsFolder());
    EXPECT_EQ(wnode.GetTitle(), test_title);

    wnode.Remove();
  }

  // Lookup of a node which was deleted should return failure,
  // but have found some data about the node.
  {
    ReadTransaction trans(&share_);
    ReadNode node(&trans);
    EXPECT_FALSE(node.InitByClientTagLookup(syncable::BOOKMARKS,
        "testtag"));
    // Note that for proper function of this API this doesn't need to be
    // filled, we're checking just to make sure the DB worked in this test.
    EXPECT_EQ(node.GetTitle(), test_title);
  }

  {
    WriteTransaction trans(&share_);
    ReadNode folder_node(&trans);
    EXPECT_TRUE(folder_node.InitByIdLookup(folder_id));

    WriteNode wnode(&trans);
    // This will undelete the tag.
    EXPECT_TRUE(wnode.InitUniqueByCreation(syncable::BOOKMARKS,
        folder_node, "testtag"));
    EXPECT_EQ(wnode.GetIsFolder(), false);
    EXPECT_EQ(wnode.GetParentId(), folder_node.GetId());
    EXPECT_EQ(wnode.GetId(), node_id);
    EXPECT_NE(wnode.GetTitle(), test_title);  // Title should be cleared
    wnode.SetTitle(test_title);
  }

  // Now look up should work.
  {
    ReadTransaction trans(&share_);
    ReadNode node(&trans);
    EXPECT_TRUE(node.InitByClientTagLookup(syncable::BOOKMARKS,
        "testtag"));
    EXPECT_EQ(node.GetTitle(), test_title);
    EXPECT_EQ(node.GetModelType(), syncable::BOOKMARKS);
  }
}

TEST_F(SyncApiTest, WriteAndReadPassword) {
  KeyParams params = {"localhost", "username", "passphrase"};
  share_.dir_manager->cryptographer()->AddKey(params);
  {
    WriteTransaction trans(&share_);
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode password_node(&trans);
    EXPECT_TRUE(password_node.InitUniqueByCreation(syncable::PASSWORDS,
                                                   root_node, "foo"));
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    password_node.SetPasswordSpecifics(data);
  }
  {
    ReadTransaction trans(&share_);
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    ReadNode password_node(&trans);
    EXPECT_TRUE(password_node.InitByClientTagLookup(syncable::PASSWORDS,
                                                    "foo"));
    const sync_pb::PasswordSpecificsData& data =
        password_node.GetPasswordSpecifics();
    EXPECT_EQ("secret", data.password_value());
  }
}

namespace {

class SyncManagerTest : public testing::Test {
 protected:
  SyncManagerTest() : ui_thread_(BrowserThread::UI, &ui_loop_) {}

 private:
  // Needed by |ui_thread_|.
  MessageLoopForUI ui_loop_;
  // Needed by |sync_manager_|.
  BrowserThread ui_thread_;

 protected:
  SyncManager sync_manager_;
};

TEST_F(SyncManagerTest, ParentJsEventRouter) {
  StrictMock<MockJsEventRouter> event_router;
  browser_sync::JsBackend* js_backend = sync_manager_.GetJsBackend();
  EXPECT_EQ(NULL, js_backend->GetParentJsEventRouter());
  js_backend->SetParentJsEventRouter(&event_router);
  EXPECT_EQ(&event_router, js_backend->GetParentJsEventRouter());
  js_backend->RemoveParentJsEventRouter();
  EXPECT_EQ(NULL, js_backend->GetParentJsEventRouter());
}

TEST_F(SyncManagerTest, ProcessMessage) {
  const JsArgList kNoArgs;

  browser_sync::JsBackend* js_backend = sync_manager_.GetJsBackend();

  // Messages sent without any parent router should be dropped.
  {
    StrictMock<MockJsEventHandler> event_handler;
    js_backend->ProcessMessage("unknownMessage",
                               kNoArgs, &event_handler);
    js_backend->ProcessMessage("getNotificationState",
                               kNoArgs, &event_handler);
  }

  {
    StrictMock<MockJsEventHandler> event_handler;
    StrictMock<MockJsEventRouter> event_router;

    ListValue false_args;
    false_args.Append(Value::CreateBooleanValue(false));

    EXPECT_CALL(event_router,
                RouteJsEvent("onGetNotificationStateFinished",
                             HasArgsAsList(false_args),
                             &event_handler)).Times(1);

    js_backend->SetParentJsEventRouter(&event_router);

    // This message should be dropped.
    js_backend->ProcessMessage("unknownMessage",
                                 kNoArgs, &event_handler);

    // This should trigger the reply.
    js_backend->ProcessMessage("getNotificationState",
                                 kNoArgs, &event_handler);

    js_backend->RemoveParentJsEventRouter();
  }

  // Messages sent after a parent router has been removed should be
  // dropped.
  {
    StrictMock<MockJsEventHandler> event_handler;
    js_backend->ProcessMessage("unknownMessage",
                                 kNoArgs, &event_handler);
    js_backend->ProcessMessage("getNotificationState",
                                 kNoArgs, &event_handler);
  }
}

TEST_F(SyncManagerTest, OnNotificationStateChange) {
  StrictMock<MockJsEventRouter> event_router;

  ListValue true_args;
  true_args.Append(Value::CreateBooleanValue(true));
  ListValue false_args;
  false_args.Append(Value::CreateBooleanValue(false));

  EXPECT_CALL(event_router,
              RouteJsEvent("onSyncNotificationStateChange",
                           HasArgsAsList(true_args), NULL)).Times(1);
  EXPECT_CALL(event_router,
              RouteJsEvent("onSyncNotificationStateChange",
                           HasArgsAsList(false_args), NULL)).Times(1);

  browser_sync::JsBackend* js_backend = sync_manager_.GetJsBackend();

  sync_manager_.TriggerOnNotificationStateChangeForTest(true);
  sync_manager_.TriggerOnNotificationStateChangeForTest(false);

  js_backend->SetParentJsEventRouter(&event_router);
  sync_manager_.TriggerOnNotificationStateChangeForTest(true);
  sync_manager_.TriggerOnNotificationStateChangeForTest(false);
  js_backend->RemoveParentJsEventRouter();

  sync_manager_.TriggerOnNotificationStateChangeForTest(true);
  sync_manager_.TriggerOnNotificationStateChangeForTest(false);
}

TEST_F(SyncManagerTest, OnIncomingNotification) {
  StrictMock<MockJsEventRouter> event_router;

  const syncable::ModelTypeBitSet empty_model_types;
  syncable::ModelTypeBitSet model_types;
  model_types.set(syncable::BOOKMARKS);
  model_types.set(syncable::THEMES);

  // Build expected_args to have a single argument with the string
  // equivalents of model_types.
  ListValue expected_args;
  {
    ListValue* model_type_list = new ListValue();
    expected_args.Append(model_type_list);
    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
         i < syncable::MODEL_TYPE_COUNT; ++i) {
      if (model_types[i]) {
        model_type_list->Append(
            Value::CreateStringValue(
                syncable::ModelTypeToString(
                    syncable::ModelTypeFromInt(i))));
      }
    }
  }

  EXPECT_CALL(event_router,
              RouteJsEvent("onSyncIncomingNotification",
                           HasArgsAsList(expected_args), NULL)).Times(1);

  browser_sync::JsBackend* js_backend = sync_manager_.GetJsBackend();

  sync_manager_.TriggerOnIncomingNotificationForTest(empty_model_types);
  sync_manager_.TriggerOnIncomingNotificationForTest(model_types);

  js_backend->SetParentJsEventRouter(&event_router);
  sync_manager_.TriggerOnIncomingNotificationForTest(model_types);
  js_backend->RemoveParentJsEventRouter();

  sync_manager_.TriggerOnIncomingNotificationForTest(empty_model_types);
  sync_manager_.TriggerOnIncomingNotificationForTest(model_types);
}

}  // namespace

}  // namespace browser_sync
