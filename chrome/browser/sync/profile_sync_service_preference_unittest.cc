// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/json/json_reader.h"
#include "base/stl_util-inl.h"
#include "base/string_piece.h"
#include "base/task.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/preference_change_processor.h"
#include "chrome/browser/sync/glue/preference_data_type_controller.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "content/common/json_value_serializer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::JSONReader;
using browser_sync::PreferenceChangeProcessor;
using browser_sync::PreferenceDataTypeController;
using browser_sync::PreferenceModelAssociator;
using browser_sync::SyncBackendHost;
using sync_api::SyncManager;
using testing::_;
using testing::Return;

typedef std::map<const std::string, const Value*> PreferenceValues;

class ProfileSyncServicePreferenceTest
    : public AbstractProfileSyncServiceTest {
 protected:
  ProfileSyncServicePreferenceTest()
      : example_url0_("http://example.com/0"),
        example_url1_("http://example.com/1"),
        example_url2_("http://example.com/2"),
        not_synced_preference_name_("nonsense_pref_name"),
        not_synced_preference_default_value_("default"),
        non_default_charset_value_("foo") {}

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();
    prefs_ = profile_->GetTestingPrefService();

    prefs_->RegisterStringPref(not_synced_preference_name_.c_str(),
                               not_synced_preference_default_value_);
  }

  virtual void TearDown() {
    service_.reset();
    {
      // The request context gets deleted on the I/O thread. To prevent a leak
      // supply one here.
      BrowserThread io_thread(BrowserThread::IO, MessageLoop::current());
      profile_.reset();
    }
    MessageLoop::current()->RunAllPending();
  }

  bool StartSyncService(Task* task, bool will_fail_association) {
    if (service_.get())
      return false;

    service_.reset(new TestProfileSyncService(
        &factory_, profile_.get(), "test", false, task));

    // Register the preference data type.
    model_associator_ =
        new PreferenceModelAssociator(service_.get());
    change_processor_ = new PreferenceChangeProcessor(model_associator_,
                                                      service_.get());
    EXPECT_CALL(factory_, CreatePreferenceSyncComponents(_, _)).
        WillOnce(Return(ProfileSyncFactory::SyncComponents(
            model_associator_, change_processor_)));

    EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
        WillOnce(ReturnNewDataTypeManager());

    service_->RegisterDataTypeController(
        new PreferenceDataTypeController(&factory_,
                                         profile_.get(),
                                         service_.get()));
    profile_->GetTokenService()->IssueAuthTokenForTest(
        GaiaConstants::kSyncService, "token");
    service_->Initialize();
    MessageLoop::current()->Run();
    return true;
  }

  const Value& GetPreferenceValue(const std::string& name) {
    const PrefService::Preference* preference =
        prefs_->FindPreference(name.c_str());
    return *preference->GetValue();
  }

  // Caller gets ownership of the returned value.
  const Value* GetSyncedValue(const std::string& name) {
    sync_api::ReadTransaction trans(service_->GetUserShare());
    sync_api::ReadNode node(&trans);

    int64 node_id = model_associator_->GetSyncIdFromChromeId(name);
    if (node_id == sync_api::kInvalidId)
      return NULL;
    if (!node.InitByIdLookup(node_id))
      return NULL;

    const sync_pb::PreferenceSpecifics& specifics(
        node.GetPreferenceSpecifics());

    JSONReader reader;
    return reader.JsonToValue(specifics.value(), false, false);
  }

  int64 WriteSyncedValue(const std::string& name,
                         const Value& value,
                         sync_api::WriteNode* node) {
    if (!PreferenceModelAssociator::WritePreferenceToNode(name, value, node))
      return sync_api::kInvalidId;
    return node->GetId();
  }

  int64 SetSyncedValue(const std::string& name, const Value& value) {
    sync_api::WriteTransaction trans(service_->GetUserShare());
    sync_api::ReadNode root(&trans);
    if (!root.InitByTagLookup(browser_sync::kPreferencesTag))
      return sync_api::kInvalidId;

    sync_api::WriteNode tag_node(&trans);
    sync_api::WriteNode node(&trans);

    int64 node_id = model_associator_->GetSyncIdFromChromeId(name);
    if (node_id == sync_api::kInvalidId) {
      if (tag_node.InitByClientTagLookup(syncable::PREFERENCES, name))
        return WriteSyncedValue(name, value, &tag_node);
      if (node.InitUniqueByCreation(syncable::PREFERENCES, root, name))
        return WriteSyncedValue(name, value, &node);
    } else if (node.InitByIdLookup(node_id)) {
      return WriteSyncedValue(name, value, &node);
    }
    return sync_api::kInvalidId;
  }

  SyncManager::ChangeRecord* MakeChangeRecord(const std::string& name,
                                              SyncManager::ChangeRecord) {
    int64 node_id = model_associator_->GetSyncIdFromChromeId(name);
    SyncManager::ChangeRecord* record = new SyncManager::ChangeRecord();
    record->action = SyncManager::ChangeRecord::ACTION_UPDATE;
    record->id = node_id;
    return record;
  }

  bool IsSynced(const std::string& pref_name) {
    return model_associator_->synced_preferences().count(pref_name) > 0;
  }

  std::string ValueString(const Value& value) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    json.Serialize(value);
    return serialized;
  }

  friend class AddPreferenceEntriesTask;

  scoped_ptr<TestingProfile> profile_;
  TestingPrefService* prefs_;

  PreferenceModelAssociator* model_associator_;
  PreferenceChangeProcessor* change_processor_;
  std::string example_url0_;
  std::string example_url1_;
  std::string example_url2_;
  std::string not_synced_preference_name_;
  std::string not_synced_preference_default_value_;
  std::string non_default_charset_value_;
};

class AddPreferenceEntriesTask : public Task {
 public:
  AddPreferenceEntriesTask(ProfileSyncServicePreferenceTest* test,
                           const PreferenceValues& entries)
      : test_(test), entries_(entries), success_(false) {
  }

  virtual void Run() {
    if (!test_->CreateRoot(syncable::PREFERENCES))
      return;
    for (PreferenceValues::const_iterator i = entries_.begin();
         i != entries_.end(); ++i) {
      if (test_->SetSyncedValue(i->first, *i->second) == sync_api::kInvalidId)
        return;
    }
    success_ = true;
  }

  bool success() { return success_; }

 private:
  ProfileSyncServicePreferenceTest* test_;
  const PreferenceValues& entries_;
  bool success_;
};

TEST_F(ProfileSyncServicePreferenceTest, WritePreferenceToNode) {
  prefs_->SetString(prefs::kHomePage, example_url0_);
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  const PrefService::Preference* pref =
      prefs_->FindPreference(prefs::kHomePage);
  sync_api::WriteTransaction trans(service_->GetUserShare());
  sync_api::WriteNode node(&trans);
  EXPECT_TRUE(node.InitByClientTagLookup(syncable::PREFERENCES,
                                         prefs::kHomePage));

  EXPECT_TRUE(PreferenceModelAssociator::WritePreferenceToNode(
      pref->name(), *pref->GetValue(), &node));
  EXPECT_EQ(UTF8ToWide(prefs::kHomePage), node.GetTitle());
  const sync_pb::PreferenceSpecifics& specifics(node.GetPreferenceSpecifics());
  EXPECT_EQ(std::string(prefs::kHomePage), specifics.name());

  base::JSONReader reader;
  scoped_ptr<Value> value(reader.JsonToValue(specifics.value(), false, false));
  EXPECT_TRUE(pref->GetValue()->Equals(value.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, ModelAssociationDoNotSyncDefaults) {
  const PrefService::Preference* pref =
      prefs_->FindPreference(prefs::kHomePage);
  EXPECT_TRUE(pref->IsDefaultValue());
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());
  EXPECT_TRUE(IsSynced(prefs::kHomePage));
  EXPECT_TRUE(pref->IsDefaultValue());
  EXPECT_TRUE(GetSyncedValue(prefs::kHomePage) == NULL);
  EXPECT_TRUE(GetSyncedValue(not_synced_preference_name_) == NULL);
}

TEST_F(ProfileSyncServicePreferenceTest, ModelAssociationEmptyCloud) {
  prefs_->SetString(prefs::kHomePage, example_url0_);
  {
    ListPrefUpdate update(prefs_, prefs::kURLsToRestoreOnStartup);
    ListValue* url_list = update.Get();
    url_list->Append(Value::CreateStringValue(example_url0_));
    url_list->Append(Value::CreateStringValue(example_url1_));
  }
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  scoped_ptr<const Value> value(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(GetPreferenceValue(prefs::kHomePage).Equals(value.get()));
  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(
      GetPreferenceValue(prefs::kURLsToRestoreOnStartup).Equals(value.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, ModelAssociationCloudHasData) {
  prefs_->SetString(prefs::kHomePage, example_url0_);
  {
    ListPrefUpdate update(prefs_, prefs::kURLsToRestoreOnStartup);
    ListValue* url_list = update.Get();
    url_list->Append(Value::CreateStringValue(example_url0_));
    url_list->Append(Value::CreateStringValue(example_url1_));
  }

  PreferenceValues cloud_data;
  cloud_data[prefs::kHomePage] = Value::CreateStringValue(example_url1_);
  ListValue* urls_to_restore = new ListValue;
  urls_to_restore->Append(Value::CreateStringValue(example_url1_));
  urls_to_restore->Append(Value::CreateStringValue(example_url2_));
  cloud_data[prefs::kURLsToRestoreOnStartup] = urls_to_restore;
  cloud_data[prefs::kDefaultCharset] =
      Value::CreateStringValue(non_default_charset_value_);

  AddPreferenceEntriesTask task(this, cloud_data);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  scoped_ptr<const Value> value(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(value.get());
  std::string string_value;
  EXPECT_TRUE(static_cast<const StringValue*>(value.get())->
              GetAsString(&string_value));
  EXPECT_EQ(example_url1_, string_value);
  EXPECT_EQ(example_url1_, prefs_->GetString(prefs::kHomePage));

  scoped_ptr<ListValue> expected_urls(new ListValue);
  expected_urls->Append(Value::CreateStringValue(example_url1_));
  expected_urls->Append(Value::CreateStringValue(example_url2_));
  expected_urls->Append(Value::CreateStringValue(example_url0_));
  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_urls.get()));
  EXPECT_TRUE(GetPreferenceValue(prefs::kURLsToRestoreOnStartup).
              Equals(expected_urls.get()));

  value.reset(GetSyncedValue(prefs::kDefaultCharset));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(static_cast<const StringValue*>(value.get())->
              GetAsString(&string_value));
  EXPECT_EQ(non_default_charset_value_, string_value);
  EXPECT_EQ(non_default_charset_value_,
            prefs_->GetString(prefs::kDefaultCharset));
  STLDeleteValues(&cloud_data);
}

TEST_F(ProfileSyncServicePreferenceTest, FailModelAssociation) {
  ASSERT_TRUE(StartSyncService(NULL, true));
  EXPECT_TRUE(service_->unrecoverable_error_detected());
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedPreferenceWithDefaultValue) {
  const PrefService::Preference* pref =
      prefs_->FindPreference(prefs::kHomePage);
  EXPECT_TRUE(pref->IsDefaultValue());

  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url0_));
  profile_->GetPrefs()->Set(prefs::kHomePage, *expected);

  scoped_ptr<const Value> actual(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(expected->Equals(actual.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedPreferenceWithValue) {
  profile_->GetPrefs()->SetString(prefs::kHomePage, example_url0_);
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url1_));
  profile_->GetPrefs()->Set(prefs::kHomePage, *expected);

  scoped_ptr<const Value> actual(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(expected->Equals(actual.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedSyncNodeActionUpdate) {
  profile_->GetPrefs()->SetString(prefs::kHomePage, example_url0_);
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url1_));
  ASSERT_NE(SetSyncedValue(prefs::kHomePage, *expected), sync_api::kInvalidId);
  int64 node_id = model_associator_->GetSyncIdFromChromeId(prefs::kHomePage);
  scoped_ptr<SyncManager::ChangeRecord> record(new SyncManager::ChangeRecord);
  record->action = SyncManager::ChangeRecord::ACTION_UPDATE;
  record->id = node_id;
  {
    sync_api::WriteTransaction trans(service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(&trans, record.get(), 1);
  }

  const Value& actual = GetPreferenceValue(prefs::kHomePage);
  EXPECT_TRUE(expected->Equals(&actual));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedSyncNodeActionAdd) {
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url0_));
  int64 node_id = SetSyncedValue(prefs::kHomePage, *expected);
  ASSERT_NE(node_id, sync_api::kInvalidId);
  scoped_ptr<SyncManager::ChangeRecord> record(new SyncManager::ChangeRecord);
  record->action = SyncManager::ChangeRecord::ACTION_ADD;
  record->id = node_id;
  {
    sync_api::WriteTransaction trans(service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(&trans, record.get(), 1);
  }

  const Value& actual = GetPreferenceValue(prefs::kHomePage);
  EXPECT_TRUE(expected->Equals(&actual));
  EXPECT_EQ(node_id,
            model_associator_->GetSyncIdFromChromeId(prefs::kHomePage));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedSyncNodeUnknownPreference) {
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url0_));
  int64 node_id = SetSyncedValue("unknown preference", *expected);
  ASSERT_NE(node_id, sync_api::kInvalidId);
  scoped_ptr<SyncManager::ChangeRecord> record(new SyncManager::ChangeRecord);
  record->action = SyncManager::ChangeRecord::ACTION_ADD;
  record->id = node_id;
  {
    sync_api::WriteTransaction trans(service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(&trans, record.get(), 1);
  }

  // Nothing interesting happens on the client when it gets an update
  // of an unknown preference.  We just should not crash.
}

TEST_F(ProfileSyncServicePreferenceTest, ManagedPreferences) {
  // Make the homepage preference managed.
  scoped_ptr<Value> managed_value(
      Value::CreateStringValue("http://example.com"));
  prefs_->SetManagedPref(prefs::kHomePage, managed_value->DeepCopy());

  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  // Changing the homepage preference should not sync anything.
  scoped_ptr<Value> user_value(
      Value::CreateStringValue("http://chromium..com"));
  prefs_->SetUserPref(prefs::kHomePage, user_value->DeepCopy());
  EXPECT_EQ(NULL, GetSyncedValue(prefs::kHomePage));

  // An incoming sync transaction shouldn't change the user value.
  scoped_ptr<Value> sync_value(
      Value::CreateStringValue("http://crbug.com"));
  int64 node_id = SetSyncedValue(prefs::kHomePage, *sync_value);
  ASSERT_NE(node_id, sync_api::kInvalidId);
  scoped_ptr<SyncManager::ChangeRecord> record(new SyncManager::ChangeRecord);
  record->action = SyncManager::ChangeRecord::ACTION_UPDATE;
  record->id = node_id;
  {
    sync_api::WriteTransaction trans(service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(&trans, record.get(), 1);
  }
  EXPECT_TRUE(managed_value->Equals(
      prefs_->GetManagedPref(prefs::kHomePage)));
  EXPECT_TRUE(user_value->Equals(
      prefs_->GetUserPref(prefs::kHomePage)));
}

TEST_F(ProfileSyncServicePreferenceTest, DynamicManagedPreferences) {
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  scoped_ptr<Value> initial_value(
      Value::CreateStringValue("http://example.com/initial"));
  profile_->GetPrefs()->Set(prefs::kHomePage, *initial_value);
  scoped_ptr<const Value> actual(GetSyncedValue(prefs::kHomePage));
  EXPECT_TRUE(initial_value->Equals(actual.get()));

  // Switch kHomePage to managed and set a different value.
  scoped_ptr<Value> managed_value(
      Value::CreateStringValue("http://example.com/managed"));
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kHomePage, managed_value->DeepCopy());

  // Sync node should be gone.
  EXPECT_EQ(sync_api::kInvalidId,
            model_associator_->GetSyncIdFromChromeId(prefs::kHomePage));

  // Change the sync value.
  scoped_ptr<Value> sync_value(
      Value::CreateStringValue("http://example.com/sync"));
  int64 node_id = SetSyncedValue(prefs::kHomePage, *sync_value);
  ASSERT_NE(node_id, sync_api::kInvalidId);
  scoped_ptr<SyncManager::ChangeRecord> record(new SyncManager::ChangeRecord);
  record->action = SyncManager::ChangeRecord::ACTION_ADD;
  record->id = node_id;
  {
    sync_api::WriteTransaction trans(service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(&trans, record.get(), 1);
  }

  // The pref value should still be the one dictated by policy.
  EXPECT_TRUE(managed_value->Equals(&GetPreferenceValue(prefs::kHomePage)));

  // Switch kHomePage back to unmanaged.
  profile_->GetTestingPrefService()->RemoveManagedPref(prefs::kHomePage);

  // Sync value should be picked up.
  EXPECT_TRUE(sync_value->Equals(&GetPreferenceValue(prefs::kHomePage)));
}
