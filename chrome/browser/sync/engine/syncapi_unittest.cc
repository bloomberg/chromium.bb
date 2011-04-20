// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the SyncApi. Note that a lot of the underlying
// functionality is provided by the Syncable layer, which has its own
// unit tests. We'll test SyncApi specific things in this harness.

#include <map>

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/engine/http_post_provider_factory.h"
#include "chrome/browser/sync/engine/http_post_provider_interface.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_backend.h"
#include "chrome/browser/sync/js_event_handler.h"
#include "chrome/browser/sync/js_event_router.h"
#include "chrome/browser/sync/js_test_util.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/protocol/proto_value_conversions.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/nigori_util.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/test/sync/engine/test_user_share.h"
#include "chrome/test/values_test_util.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::Cryptographer;
using browser_sync::HasArgsAsList;
using browser_sync::KeyParams;
using browser_sync::JsArgList;
using browser_sync::MockJsEventHandler;
using browser_sync::MockJsEventRouter;
using browser_sync::ModelSafeRoutingInfo;
using browser_sync::ModelSafeWorker;
using browser_sync::ModelSafeWorkerRegistrar;
using browser_sync::sessions::SyncSessionSnapshot;
using syncable::ModelType;
using syncable::ModelTypeSet;
using test::ExpectDictionaryValue;
using test::ExpectStringValue;
using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::SaveArg;
using testing::StrictMock;

namespace sync_api {

namespace {

void ExpectInt64Value(int64 expected_value,
                      const DictionaryValue& value, const std::string& key) {
  std::string int64_str;
  EXPECT_TRUE(value.GetString(key, &int64_str));
  int64 val = 0;
  EXPECT_TRUE(base::StringToInt64(int64_str, &val));
  EXPECT_EQ(expected_value, val);
}

// Makes a non-folder child of the root node.  Returns the id of the
// newly-created node.
int64 MakeNode(UserShare* share,
               ModelType model_type,
               const std::string& client_tag) {
  WriteTransaction trans(share);
  ReadNode root_node(&trans);
  root_node.InitByRootLookup();
  WriteNode node(&trans);
  EXPECT_TRUE(node.InitUniqueByCreation(model_type, root_node, client_tag));
  node.SetIsFolder(false);
  return node.GetId();
}

// Make a folder as a child of the root node. Returns the id of the
// newly-created node.
int64 MakeFolder(UserShare* share,
                 syncable::ModelType model_type,
                 const std::string& client_tag) {
  WriteTransaction trans(share);
  ReadNode root_node(&trans);
  root_node.InitByRootLookup();
  WriteNode node(&trans);
  EXPECT_TRUE(node.InitUniqueByCreation(model_type, root_node, client_tag));
  node.SetIsFolder(true);
  return node.GetId();
}

// Makes a non-folder child of a non-root node. Returns the id of the
// newly-created node.
int64 MakeNodeWithParent(UserShare* share,
                         ModelType model_type,
                         const std::string& client_tag,
                         int64 parent_id) {
  WriteTransaction trans(share);
  ReadNode parent_node(&trans);
  parent_node.InitByIdLookup(parent_id);
  WriteNode node(&trans);
  EXPECT_TRUE(node.InitUniqueByCreation(model_type, parent_node, client_tag));
  node.SetIsFolder(false);
  return node.GetId();
}

// Makes a folder child of a non-root node. Returns the id of the
// newly-created node.
int64 MakeFolderWithParent(UserShare* share,
                           ModelType model_type,
                           int64 parent_id,
                           BaseNode* predecessor) {
  WriteTransaction trans(share);
  ReadNode parent_node(&trans);
  parent_node.InitByIdLookup(parent_id);
  WriteNode node(&trans);
  EXPECT_TRUE(node.InitByCreation(model_type, parent_node, predecessor));
  node.SetIsFolder(true);
  return node.GetId();
}

// Creates the "synced" root node for a particular datatype. We use the syncable
// methods here so that the syncer treats these nodes as if they were already
// received from the server.
int64 MakeServerNodeForType(UserShare* share,
                            ModelType model_type) {
  sync_pb::EntitySpecifics specifics;
  syncable::AddDefaultExtensionValue(model_type, &specifics);
  syncable::ScopedDirLookup dir(share->dir_manager.get(), share->name);
  EXPECT_TRUE(dir.good());
  syncable::WriteTransaction trans(dir, syncable::UNITTEST, __FILE__, __LINE__);
  // Attempt to lookup by nigori tag.
  std::string type_tag = syncable::ModelTypeToRootTag(model_type);
  syncable::Id node_id = syncable::Id::CreateFromServerId(type_tag);
  syncable::MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                               node_id);
  EXPECT_TRUE(entry.good());
  entry.Put(syncable::BASE_VERSION, 1);
  entry.Put(syncable::SERVER_VERSION, 1);
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, false);
  entry.Put(syncable::SERVER_PARENT_ID, syncable::kNullId);
  entry.Put(syncable::SERVER_IS_DIR, true);
  entry.Put(syncable::IS_DIR, true);
  entry.Put(syncable::SERVER_SPECIFICS, specifics);
  entry.Put(syncable::UNIQUE_SERVER_TAG, type_tag);
  entry.Put(syncable::NON_UNIQUE_NAME, type_tag);
  entry.Put(syncable::IS_DEL, false);
  entry.Put(syncable::SPECIFICS, specifics);
  return entry.Get(syncable::META_HANDLE);
}

}  // namespace

class SyncApiTest : public testing::Test {
 public:
  virtual void SetUp() {
    test_user_share_.SetUp();
  }

  virtual void TearDown() {
    test_user_share_.TearDown();
  }

 protected:
  browser_sync::TestUserShare test_user_share_;
};

TEST_F(SyncApiTest, SanityCheckTest) {
  {
    ReadTransaction trans(test_user_share_.user_share());
    EXPECT_TRUE(trans.GetWrappedTrans() != NULL);
  }
  {
    WriteTransaction trans(test_user_share_.user_share());
    EXPECT_TRUE(trans.GetWrappedTrans() != NULL);
  }
  {
    // No entries but root should exist
    ReadTransaction trans(test_user_share_.user_share());
    ReadNode node(&trans);
    // Metahandle 1 can be root, sanity check 2
    EXPECT_FALSE(node.InitByIdLookup(2));
  }
}

TEST_F(SyncApiTest, BasicTagWrite) {
  {
    ReadTransaction trans(test_user_share_.user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_EQ(root_node.GetFirstChildId(), 0);
  }

  ignore_result(MakeNode(test_user_share_.user_share(),
                         syncable::BOOKMARKS, "testtag"));

  {
    ReadTransaction trans(test_user_share_.user_share());
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
    WriteTransaction trans(test_user_share_.user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_EQ(root_node.GetFirstChildId(), 0);
  }

  ignore_result(MakeNode(test_user_share_.user_share(),
                         syncable::BOOKMARKS, "collideme"));
  ignore_result(MakeNode(test_user_share_.user_share(),
                         syncable::PREFERENCES, "collideme"));
  ignore_result(MakeNode(test_user_share_.user_share(),
                         syncable::AUTOFILL, "collideme"));

  {
    ReadTransaction trans(test_user_share_.user_share());

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
    ReadTransaction trans(test_user_share_.user_share());
    ReadNode node(&trans);
    EXPECT_FALSE(node.InitByClientTagLookup(syncable::BOOKMARKS,
        "testtag"));
  }
  {
    WriteTransaction trans(test_user_share_.user_share());
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
    WriteTransaction trans(test_user_share_.user_share());
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
    WriteTransaction trans(test_user_share_.user_share());
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
    ReadTransaction trans(test_user_share_.user_share());
    ReadNode node(&trans);
    EXPECT_FALSE(node.InitByClientTagLookup(syncable::BOOKMARKS,
        "testtag"));
    // Note that for proper function of this API this doesn't need to be
    // filled, we're checking just to make sure the DB worked in this test.
    EXPECT_EQ(node.GetTitle(), test_title);
  }

  {
    WriteTransaction trans(test_user_share_.user_share());
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
    ReadTransaction trans(test_user_share_.user_share());
    ReadNode node(&trans);
    EXPECT_TRUE(node.InitByClientTagLookup(syncable::BOOKMARKS,
        "testtag"));
    EXPECT_EQ(node.GetTitle(), test_title);
    EXPECT_EQ(node.GetModelType(), syncable::BOOKMARKS);
  }
}

TEST_F(SyncApiTest, WriteAndReadPassword) {
  KeyParams params = {"localhost", "username", "passphrase"};
  {
    ReadTransaction trans(test_user_share_.user_share());
    trans.GetCryptographer()->AddKey(params);
  }
  {
    WriteTransaction trans(test_user_share_.user_share());
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
    ReadTransaction trans(test_user_share_.user_share());
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

void CheckNodeValue(const BaseNode& node, const DictionaryValue& value) {
  ExpectInt64Value(node.GetId(), value, "id");
  ExpectInt64Value(node.GetModificationTime(), value, "modificationTime");
  ExpectInt64Value(node.GetParentId(), value, "parentId");
  {
    bool is_folder = false;
    EXPECT_TRUE(value.GetBoolean("isFolder", &is_folder));
    EXPECT_EQ(node.GetIsFolder(), is_folder);
  }
  ExpectStringValue(WideToUTF8(node.GetTitle()), value, "title");
  {
    ModelType expected_model_type = node.GetModelType();
    std::string type_str;
    EXPECT_TRUE(value.GetString("type", &type_str));
    if (expected_model_type >= syncable::FIRST_REAL_MODEL_TYPE) {
      ModelType model_type =
          syncable::ModelTypeFromString(type_str);
      EXPECT_EQ(expected_model_type, model_type);
    } else if (expected_model_type == syncable::TOP_LEVEL_FOLDER) {
      EXPECT_EQ("Top-level folder", type_str);
    } else if (expected_model_type == syncable::UNSPECIFIED) {
      EXPECT_EQ("Unspecified", type_str);
    } else {
      ADD_FAILURE();
    }
  }
  ExpectInt64Value(node.GetExternalId(), value, "externalId");
  ExpectInt64Value(node.GetPredecessorId(), value, "predecessorId");
  ExpectInt64Value(node.GetSuccessorId(), value, "successorId");
  ExpectInt64Value(node.GetFirstChildId(), value, "firstChildId");
  {
    scoped_ptr<DictionaryValue> expected_entry(node.GetEntry()->ToValue());
    Value* entry = NULL;
    EXPECT_TRUE(value.Get("entry", &entry));
    EXPECT_TRUE(Value::Equals(entry, expected_entry.get()));
  }
  EXPECT_EQ(11u, value.size());
}

}  // namespace

TEST_F(SyncApiTest, BaseNodeToValue) {
  ReadTransaction trans(test_user_share_.user_share());
  ReadNode node(&trans);
  node.InitByRootLookup();
  scoped_ptr<DictionaryValue> value(node.ToValue());
  if (value.get()) {
    CheckNodeValue(node, *value);
  } else {
    ADD_FAILURE();
  }
}

namespace {

void ExpectChangeRecordActionValue(SyncManager::ChangeRecord::Action
                                       expected_value,
                                   const DictionaryValue& value,
                                   const std::string& key) {
  std::string str_value;
  EXPECT_TRUE(value.GetString(key, &str_value));
  switch (expected_value) {
    case SyncManager::ChangeRecord::ACTION_ADD:
      EXPECT_EQ("Add", str_value);
      break;
    case SyncManager::ChangeRecord::ACTION_UPDATE:
      EXPECT_EQ("Update", str_value);
      break;
    case SyncManager::ChangeRecord::ACTION_DELETE:
      EXPECT_EQ("Delete", str_value);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void CheckNonDeleteChangeRecordValue(const SyncManager::ChangeRecord& record,
                                     const DictionaryValue& value,
                                     BaseTransaction* trans) {
  EXPECT_NE(SyncManager::ChangeRecord::ACTION_DELETE, record.action);
  ExpectChangeRecordActionValue(record.action, value, "action");
  {
    ReadNode node(trans);
    EXPECT_TRUE(node.InitByIdLookup(record.id));
    scoped_ptr<DictionaryValue> expected_node_value(node.ToValue());
    ExpectDictionaryValue(*expected_node_value, value, "node");
  }
}

void CheckDeleteChangeRecordValue(const SyncManager::ChangeRecord& record,
                                  const DictionaryValue& value) {
  EXPECT_EQ(SyncManager::ChangeRecord::ACTION_DELETE, record.action);
  ExpectChangeRecordActionValue(record.action, value, "action");
  DictionaryValue* node_value = NULL;
  EXPECT_TRUE(value.GetDictionary("node", &node_value));
  if (node_value) {
    ExpectInt64Value(record.id, *node_value, "id");
    scoped_ptr<DictionaryValue> expected_specifics_value(
        browser_sync::EntitySpecificsToValue(record.specifics));
    ExpectDictionaryValue(*expected_specifics_value,
                          *node_value, "specifics");
    scoped_ptr<DictionaryValue> expected_extra_value;
    if (record.extra.get()) {
      expected_extra_value.reset(record.extra->ToValue());
    }
    Value* extra_value = NULL;
    EXPECT_EQ(record.extra.get() != NULL,
              node_value->Get("extra", &extra_value));
    EXPECT_TRUE(Value::Equals(extra_value, expected_extra_value.get()));
  }
}

class MockExtraChangeRecordData
    : public SyncManager::ExtraPasswordChangeRecordData {
 public:
  MOCK_CONST_METHOD0(ToValue, DictionaryValue*());
};

}  // namespace

TEST_F(SyncApiTest, ChangeRecordToValue) {
  int64 child_id = MakeNode(test_user_share_.user_share(),
                            syncable::BOOKMARKS, "testtag");
  sync_pb::EntitySpecifics child_specifics;
  {
    ReadTransaction trans(test_user_share_.user_share());
    ReadNode node(&trans);
    EXPECT_TRUE(node.InitByIdLookup(child_id));
    child_specifics = node.GetEntry()->Get(syncable::SPECIFICS);
  }

  // Add
  {
    ReadTransaction trans(test_user_share_.user_share());
    SyncManager::ChangeRecord record;
    record.action = SyncManager::ChangeRecord::ACTION_ADD;
    record.id = 1;
    record.specifics = child_specifics;
    record.extra.reset(new StrictMock<MockExtraChangeRecordData>());
    scoped_ptr<DictionaryValue> value(record.ToValue(&trans));
    CheckNonDeleteChangeRecordValue(record, *value, &trans);
  }

  // Update
  {
    ReadTransaction trans(test_user_share_.user_share());
    SyncManager::ChangeRecord record;
    record.action = SyncManager::ChangeRecord::ACTION_UPDATE;
    record.id = child_id;
    record.specifics = child_specifics;
    record.extra.reset(new StrictMock<MockExtraChangeRecordData>());
    scoped_ptr<DictionaryValue> value(record.ToValue(&trans));
    CheckNonDeleteChangeRecordValue(record, *value, &trans);
  }

  // Delete (no extra)
  {
    ReadTransaction trans(test_user_share_.user_share());
    SyncManager::ChangeRecord record;
    record.action = SyncManager::ChangeRecord::ACTION_DELETE;
    record.id = child_id + 1;
    record.specifics = child_specifics;
    scoped_ptr<DictionaryValue> value(record.ToValue(&trans));
    CheckDeleteChangeRecordValue(record, *value);
  }

  // Delete (with extra)
  {
    ReadTransaction trans(test_user_share_.user_share());
    SyncManager::ChangeRecord record;
    record.action = SyncManager::ChangeRecord::ACTION_DELETE;
    record.id = child_id + 1;
    record.specifics = child_specifics;

    DictionaryValue extra_value;
    extra_value.SetString("foo", "bar");
    scoped_ptr<StrictMock<MockExtraChangeRecordData> > extra(
        new StrictMock<MockExtraChangeRecordData>());
    EXPECT_CALL(*extra, ToValue()).Times(2).WillRepeatedly(
        Invoke(&extra_value, &DictionaryValue::DeepCopy));

    record.extra.reset(extra.release());
    scoped_ptr<DictionaryValue> value(record.ToValue(&trans));
    CheckDeleteChangeRecordValue(record, *value);
  }
}

namespace {

class TestHttpPostProviderFactory : public HttpPostProviderFactory {
 public:
  virtual ~TestHttpPostProviderFactory() {}
  virtual HttpPostProviderInterface* Create() {
    NOTREACHED();
    return NULL;
  }
  virtual void Destroy(HttpPostProviderInterface* http) {
    NOTREACHED();
  }
};

class SyncManagerObserverMock : public SyncManager::Observer {
 public:
  MOCK_METHOD4(OnChangesApplied,
               void(ModelType,
                    const BaseTransaction*,
                    const SyncManager::ChangeRecord*,
                    int));  // NOLINT
  MOCK_METHOD1(OnChangesComplete, void(ModelType));  // NOLINT
  MOCK_METHOD1(OnSyncCycleCompleted,
               void(const SyncSessionSnapshot*));  // NOLINT
  MOCK_METHOD0(OnInitializationComplete, void());  // NOLINT
  MOCK_METHOD1(OnAuthError, void(const GoogleServiceAuthError&));  // NOLINT
  MOCK_METHOD1(OnPassphraseRequired, void(bool));  // NOLINT
  MOCK_METHOD0(OnPassphraseFailed, void());  // NOLINT
  MOCK_METHOD1(OnPassphraseAccepted, void(const std::string&));  // NOLINT
  MOCK_METHOD0(OnStopSyncingPermanently, void());  // NOLINT
  MOCK_METHOD1(OnUpdatedToken, void(const std::string&));  // NOLINT
  MOCK_METHOD1(OnMigrationNeededForTypes, void(const ModelTypeSet&));
  MOCK_METHOD0(OnClearServerDataFailed, void());  // NOLINT
  MOCK_METHOD0(OnClearServerDataSucceeded, void());  // NOLINT
  MOCK_METHOD1(OnEncryptionComplete, void(const ModelTypeSet&));  // NOLINT
};

class SyncNotifierMock : public sync_notifier::SyncNotifier {
 public:
  MOCK_METHOD1(AddObserver, void(sync_notifier::SyncNotifierObserver*));
  MOCK_METHOD1(RemoveObserver, void(sync_notifier::SyncNotifierObserver*));
  MOCK_METHOD1(SetState, void(const std::string&));
  MOCK_METHOD2(UpdateCredentials,
               void(const std::string&, const std::string&));
  MOCK_METHOD1(UpdateEnabledTypes,
               void(const syncable::ModelTypeSet&));
  MOCK_METHOD0(SendNotification, void());
};

class SyncManagerTest : public testing::Test,
                        public ModelSafeWorkerRegistrar {
 protected:
  SyncManagerTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        sync_notifier_observer_(NULL),
        update_enabled_types_call_count_(0) {}

  // Test implementation.
  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    SyncCredentials credentials;
    credentials.email = "foo@bar.com";
    credentials.sync_token = "sometoken";

    sync_notifier_mock_.reset(new StrictMock<SyncNotifierMock>());
    EXPECT_CALL(*sync_notifier_mock_, AddObserver(_)).
        WillOnce(Invoke(this, &SyncManagerTest::SyncNotifierAddObserver));
    EXPECT_CALL(*sync_notifier_mock_, SetState(""));
    EXPECT_CALL(*sync_notifier_mock_,
                UpdateCredentials(credentials.email, credentials.sync_token));
    EXPECT_CALL(*sync_notifier_mock_, UpdateEnabledTypes(_)).
        Times(AtLeast(1)).
        WillRepeatedly(
            Invoke(this, &SyncManagerTest::SyncNotifierUpdateEnabledTypes));
    EXPECT_CALL(*sync_notifier_mock_, RemoveObserver(_)).
        WillOnce(Invoke(this, &SyncManagerTest::SyncNotifierRemoveObserver));

    EXPECT_FALSE(sync_notifier_observer_);

    sync_manager_.Init(temp_dir_.path(), "bogus", 0, false,
                       new TestHttpPostProviderFactory(), this, "bogus",
                       credentials, sync_notifier_mock_.get(), "",
                       true /* setup_for_test_mode */);

    EXPECT_TRUE(sync_notifier_observer_);
    sync_manager_.AddObserver(&observer_);

    EXPECT_EQ(1, update_enabled_types_call_count_);

    ModelSafeRoutingInfo routes;
    GetModelSafeRoutingInfo(&routes);
    for (ModelSafeRoutingInfo::iterator i = routes.begin(); i != routes.end();
         ++i) {
      EXPECT_CALL(observer_, OnChangesApplied(i->first, _, _, 1))
          .RetiresOnSaturation();
      EXPECT_CALL(observer_, OnChangesComplete(i->first))
          .RetiresOnSaturation();
      type_roots_[i->first] = MakeServerNodeForType(
          sync_manager_.GetUserShare(), i->first);
    }
  }

  void TearDown() {
    sync_manager_.RemoveObserver(&observer_);
    sync_manager_.Shutdown();
    EXPECT_FALSE(sync_notifier_observer_);
  }

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) {
    NOTIMPLEMENTED();
    out->clear();
  }
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
    (*out)[syncable::NIGORI] = browser_sync::GROUP_PASSIVE;
    (*out)[syncable::BOOKMARKS] = browser_sync::GROUP_PASSIVE;
    (*out)[syncable::THEMES] = browser_sync::GROUP_PASSIVE;
    (*out)[syncable::SESSIONS] = browser_sync::GROUP_PASSIVE;
    (*out)[syncable::PASSWORDS] = browser_sync::GROUP_PASSIVE;
  }

  // Helper methods.
  bool SetUpEncryption() {
    // We need to create the nigori node as if it were an applied server update.
    UserShare* share = sync_manager_.GetUserShare();
    int64 nigori_id = GetIdForDataType(syncable::NIGORI);
    if (nigori_id == kInvalidId)
      return false;

    // Set the nigori cryptographer information.
    WriteTransaction trans(share);
    Cryptographer* cryptographer = trans.GetCryptographer();
    if (!cryptographer)
      return false;
    KeyParams params = {"localhost", "dummy", "foobar"};
    cryptographer->AddKey(params);
    sync_pb::NigoriSpecifics nigori;
    cryptographer->GetKeys(nigori.mutable_encrypted());
    WriteNode node(&trans);
    node.InitByIdLookup(nigori_id);
    node.SetNigoriSpecifics(nigori);
    return cryptographer->is_ready();
  }

  int64 GetIdForDataType(ModelType type) {
    if (type_roots_.count(type) == 0)
      return 0;
    return type_roots_[type];
  }

  void SyncNotifierAddObserver(
      sync_notifier::SyncNotifierObserver* sync_notifier_observer) {
    EXPECT_EQ(NULL, sync_notifier_observer_);
    sync_notifier_observer_ = sync_notifier_observer;
  }

  void SyncNotifierRemoveObserver(
      sync_notifier::SyncNotifierObserver* sync_notifier_observer) {
    EXPECT_EQ(sync_notifier_observer_, sync_notifier_observer);
    sync_notifier_observer_ = NULL;
  }

  void SyncNotifierUpdateEnabledTypes(
      const syncable::ModelTypeSet& types) {
    ModelSafeRoutingInfo routes;
    GetModelSafeRoutingInfo(&routes);
    syncable::ModelTypeSet expected_types;
    for (ModelSafeRoutingInfo::const_iterator it = routes.begin();
         it != routes.end(); ++it) {
      expected_types.insert(it->first);
    }
    EXPECT_EQ(expected_types, types);
    ++update_enabled_types_call_count_;
  }

 private:
  // Needed by |ui_thread_|.
  MessageLoopForUI ui_loop_;
  // Needed by |sync_manager_|.
  BrowserThread ui_thread_;
  // Needed by |sync_manager_|.
  ScopedTempDir temp_dir_;
  // Sync Id's for the roots of the enabled datatypes.
  std::map<ModelType, int64> type_roots_;
  scoped_ptr<StrictMock<SyncNotifierMock> > sync_notifier_mock_;

 protected:
  SyncManager sync_manager_;
  StrictMock<SyncManagerObserverMock> observer_;
  sync_notifier::SyncNotifierObserver* sync_notifier_observer_;
  int update_enabled_types_call_count_;
};

TEST_F(SyncManagerTest, UpdateEnabledTypes) {
  EXPECT_EQ(1, update_enabled_types_call_count_);
  // Triggers SyncNotifierUpdateEnabledTypes.
  sync_manager_.UpdateEnabledTypes();
  EXPECT_EQ(2, update_enabled_types_call_count_);
}

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
                             HasArgsAsList(false_args), &event_handler));

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

TEST_F(SyncManagerTest, ProcessMessageGetRootNode) {
  const JsArgList kNoArgs;

  browser_sync::JsBackend* js_backend = sync_manager_.GetJsBackend();

  StrictMock<MockJsEventHandler> event_handler;
  StrictMock<MockJsEventRouter> event_router;

  JsArgList return_args;

  EXPECT_CALL(event_router,
              RouteJsEvent("onGetRootNodeFinished", _, &event_handler)).
      WillOnce(SaveArg<1>(&return_args));

  js_backend->SetParentJsEventRouter(&event_router);

  // Should trigger the reply.
  js_backend->ProcessMessage("getRootNode", kNoArgs, &event_handler);

  EXPECT_EQ(1u, return_args.Get().GetSize());
  DictionaryValue* node_info = NULL;
  EXPECT_TRUE(return_args.Get().GetDictionary(0, &node_info));
  if (node_info) {
    ReadTransaction trans(sync_manager_.GetUserShare());
    ReadNode node(&trans);
    node.InitByRootLookup();
    CheckNodeValue(node, *node_info);
  } else {
    ADD_FAILURE();
  }

  js_backend->RemoveParentJsEventRouter();
}

void CheckGetNodeByIdReturnArgs(const SyncManager& sync_manager,
                                const JsArgList& return_args,
                                int64 id) {
  EXPECT_EQ(1u, return_args.Get().GetSize());
  DictionaryValue* node_info = NULL;
  EXPECT_TRUE(return_args.Get().GetDictionary(0, &node_info));
  if (node_info) {
    ReadTransaction trans(sync_manager.GetUserShare());
    ReadNode node(&trans);
    node.InitByIdLookup(id);
    CheckNodeValue(node, *node_info);
  } else {
    ADD_FAILURE();
  }
}

TEST_F(SyncManagerTest, ProcessMessageGetNodeById) {
  int64 child_id =
      MakeNode(sync_manager_.GetUserShare(), syncable::BOOKMARKS, "testtag");

  browser_sync::JsBackend* js_backend = sync_manager_.GetJsBackend();

  StrictMock<MockJsEventHandler> event_handler;
  StrictMock<MockJsEventRouter> event_router;

  JsArgList return_args;

  EXPECT_CALL(event_router,
              RouteJsEvent("onGetNodeByIdFinished", _, &event_handler))
      .Times(2).WillRepeatedly(SaveArg<1>(&return_args));

  js_backend->SetParentJsEventRouter(&event_router);

  // Should trigger the reply.
  {
    ListValue args;
    args.Append(Value::CreateStringValue("1"));
    js_backend->ProcessMessage("getNodeById", JsArgList(args), &event_handler);
  }

  CheckGetNodeByIdReturnArgs(sync_manager_, return_args, 1);

  // Should trigger another reply.
  {
    ListValue args;
    args.Append(Value::CreateStringValue(base::Int64ToString(child_id)));
    js_backend->ProcessMessage("getNodeById", JsArgList(args), &event_handler);
  }

  CheckGetNodeByIdReturnArgs(sync_manager_, return_args, child_id);

  js_backend->RemoveParentJsEventRouter();
}

TEST_F(SyncManagerTest, ProcessMessageGetNodeByIdFailure) {
  browser_sync::JsBackend* js_backend = sync_manager_.GetJsBackend();

  StrictMock<MockJsEventHandler> event_handler;
  StrictMock<MockJsEventRouter> event_router;

  ListValue null_args;
  null_args.Append(Value::CreateNullValue());

  EXPECT_CALL(event_router,
              RouteJsEvent("onGetNodeByIdFinished",
                           HasArgsAsList(null_args), &event_handler))
      .Times(5);

  js_backend->SetParentJsEventRouter(&event_router);

  {
    ListValue args;
    js_backend->ProcessMessage("getNodeById", JsArgList(args), &event_handler);
  }

  {
    ListValue args;
    args.Append(Value::CreateStringValue(""));
    js_backend->ProcessMessage("getNodeById", JsArgList(args), &event_handler);
  }

  {
    ListValue args;
    args.Append(Value::CreateStringValue("nonsense"));
    js_backend->ProcessMessage("getNodeById", JsArgList(args), &event_handler);
  }

  {
    ListValue args;
    args.Append(Value::CreateStringValue("nonsense"));
    js_backend->ProcessMessage("getNodeById", JsArgList(args), &event_handler);
  }

  {
    ListValue args;
    args.Append(Value::CreateStringValue("0"));
    js_backend->ProcessMessage("getNodeById", JsArgList(args), &event_handler);
  }

  // TODO(akalin): Figure out how to test InitByIdLookup() failure.

  js_backend->RemoveParentJsEventRouter();
}

TEST_F(SyncManagerTest, OnNotificationStateChange) {
  StrictMock<MockJsEventRouter> event_router;

  ListValue true_args;
  true_args.Append(Value::CreateBooleanValue(true));
  ListValue false_args;
  false_args.Append(Value::CreateBooleanValue(false));

  EXPECT_CALL(event_router,
              RouteJsEvent("onSyncNotificationStateChange",
                           HasArgsAsList(true_args), NULL));
  EXPECT_CALL(event_router,
              RouteJsEvent("onSyncNotificationStateChange",
                           HasArgsAsList(false_args), NULL));

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
                           HasArgsAsList(expected_args), NULL));

  browser_sync::JsBackend* js_backend = sync_manager_.GetJsBackend();

  sync_manager_.TriggerOnIncomingNotificationForTest(empty_model_types);
  sync_manager_.TriggerOnIncomingNotificationForTest(model_types);

  js_backend->SetParentJsEventRouter(&event_router);
  sync_manager_.TriggerOnIncomingNotificationForTest(model_types);
  js_backend->RemoveParentJsEventRouter();

  sync_manager_.TriggerOnIncomingNotificationForTest(empty_model_types);
  sync_manager_.TriggerOnIncomingNotificationForTest(model_types);
}

TEST_F(SyncManagerTest, EncryptDataTypesWithNoData) {
  EXPECT_TRUE(SetUpEncryption());
  ModelTypeSet encrypted_types;
  encrypted_types.insert(syncable::BOOKMARKS);
  // Even though Passwords isn't marked for encryption, it's enabled, so it
  // should automatically be added to the response of OnEncryptionComplete.
  ModelTypeSet expected_types = encrypted_types;
  expected_types.insert(syncable::PASSWORDS);
  EXPECT_CALL(observer_, OnEncryptionComplete(expected_types));
  sync_manager_.EncryptDataTypes(encrypted_types);
  {
    ReadTransaction trans(sync_manager_.GetUserShare());
    EXPECT_EQ(encrypted_types,
              GetEncryptedDataTypes(trans.GetWrappedTrans()));
  }
}

TEST_F(SyncManagerTest, EncryptDataTypesWithData) {
  size_t batch_size = 5;
  EXPECT_TRUE(SetUpEncryption());

  // Create some unencrypted unsynced data.
  int64 folder = MakeFolderWithParent(sync_manager_.GetUserShare(),
                                      syncable::BOOKMARKS,
                                      GetIdForDataType(syncable::BOOKMARKS),
                                      NULL);
  // First batch_size nodes are children of folder.
  size_t i;
  for (i = 0; i < batch_size; ++i) {
    MakeNodeWithParent(sync_manager_.GetUserShare(), syncable::BOOKMARKS,
                       StringPrintf("%"PRIuS"", i), folder);
  }
  // Next batch_size nodes are a different type and on their own.
  for (; i < 2*batch_size; ++i) {
    MakeNodeWithParent(sync_manager_.GetUserShare(), syncable::SESSIONS,
                       StringPrintf("%"PRIuS"", i),
                       GetIdForDataType(syncable::SESSIONS));
  }
  // Last batch_size nodes are a third type that will not need encryption.
  for (; i < 3*batch_size; ++i) {
    MakeNodeWithParent(sync_manager_.GetUserShare(), syncable::THEMES,
                       StringPrintf("%"PRIuS"", i),
                       GetIdForDataType(syncable::THEMES));
  }

  {
    ReadTransaction trans(sync_manager_.GetUserShare());
    EXPECT_TRUE(syncable::VerifyDataTypeEncryption(trans.GetWrappedTrans(),
                                                   syncable::BOOKMARKS,
                                                   false /* not encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryption(trans.GetWrappedTrans(),
                                                   syncable::SESSIONS,
                                                   false /* not encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryption(trans.GetWrappedTrans(),
                                                   syncable::THEMES,
                                                   false /* not encrypted */));
  }

  ModelTypeSet encrypted_types;
  encrypted_types.insert(syncable::BOOKMARKS);
  encrypted_types.insert(syncable::SESSIONS);
  encrypted_types.insert(syncable::PASSWORDS);
  EXPECT_CALL(observer_, OnEncryptionComplete(encrypted_types));
  sync_manager_.EncryptDataTypes(encrypted_types);

  {
    ReadTransaction trans(sync_manager_.GetUserShare());
    encrypted_types.erase(syncable::PASSWORDS);  // Not stored in nigori node.
    EXPECT_EQ(encrypted_types,
              GetEncryptedDataTypes(trans.GetWrappedTrans()));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryption(trans.GetWrappedTrans(),
                                                   syncable::BOOKMARKS,
                                                   true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryption(trans.GetWrappedTrans(),
                                                   syncable::SESSIONS,
                                                   true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryption(trans.GetWrappedTrans(),
                                                   syncable::THEMES,
                                                   false /* not encrypted */));
  }
}

}  // namespace

}  // namespace browser_sync
