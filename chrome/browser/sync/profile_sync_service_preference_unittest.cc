// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_value_serializer.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "base/task.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/api/sync_data.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/glue/preference_data_type_controller.h"
#include "chrome/browser/sync/glue/syncable_service_adapter.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/internal_api/change_record.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::JSONReader;
using browser_sync::GenericChangeProcessor;
using browser_sync::PreferenceDataTypeController;
using browser_sync::SyncBackendHost;
using browser_sync::SyncableServiceAdapter;
using sync_api::ChangeRecord;
using testing::_;
using testing::Invoke;
using testing::Return;

typedef std::map<const std::string, const Value*> PreferenceValues;

ACTION_P4(BuildPrefSyncComponents, profile_sync_service, pref_sync_service,
    model_associator_ptr, change_processor_ptr) {
  sync_api::UserShare* user_share = profile_sync_service->GetUserShare();
  *change_processor_ptr = new GenericChangeProcessor(
      profile_sync_service,
      pref_sync_service->AsWeakPtr(),
      user_share);
  *model_associator_ptr = new browser_sync::SyncableServiceAdapter(
      syncable::PREFERENCES,
      pref_sync_service,
      *change_processor_ptr);
  return ProfileSyncComponentsFactory::SyncComponents(*model_associator_ptr,
                                                      *change_processor_ptr);
}

// TODO(zea): Refactor to remove the ProfileSyncService usage.
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
    AbstractProfileSyncServiceTest::SetUp();
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();
    prefs_ = profile_->GetTestingPrefService();

    prefs_->RegisterStringPref(not_synced_preference_name_.c_str(),
                               not_synced_preference_default_value_,
                               PrefService::UNSYNCABLE_PREF);
  }

  virtual void TearDown() {
    service_.reset();
    profile_.reset();
    AbstractProfileSyncServiceTest::TearDown();
  }

  bool StartSyncService(Task* task, bool will_fail_association) {
    if (service_.get())
      return false;

    service_.reset(new TestProfileSyncService(
        &factory_, profile_.get(), "test", false, task));
    pref_sync_service_ = reinterpret_cast<PrefModelAssociator*>(
        prefs_->GetSyncableService());
    if (!pref_sync_service_)
      return false;
    EXPECT_CALL(factory_, CreatePreferenceSyncComponents(_, _)).
        WillOnce(BuildPrefSyncComponents(service_.get(),
                                         pref_sync_service_,
                                         &model_associator_,
                                         &change_processor_));

    EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
        WillOnce(ReturnNewDataTypeManager());

    dtc_ = new PreferenceDataTypeController(&factory_,
                                            profile_.get(),
                                            service_.get());
    service_->RegisterDataTypeController(dtc_);
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
    sync_api::ReadTransaction trans(FROM_HERE, service_->GetUserShare());
    sync_api::ReadNode node(&trans);

    if (!node.InitByClientTagLookup(syncable::PREFERENCES, name))
      return NULL;

    const sync_pb::PreferenceSpecifics& specifics(
        node.GetEntitySpecifics().GetExtension(sync_pb::preference));

    JSONReader reader;
    return reader.JsonToValue(specifics.value(), false, false);
  }

  int64 WriteSyncedValue(const std::string& name,
                         const Value& value,
                         sync_api::WriteNode* node) {
    SyncData sync_data;
    if (!PrefModelAssociator::CreatePrefSyncData(name,
                                                 value,
                                                 &sync_data)) {
      return sync_api::kInvalidId;
    }
    node->SetEntitySpecifics(sync_data.GetSpecifics());
    return node->GetId();
  }

  int64 SetSyncedValue(const std::string& name, const Value& value) {
    sync_api::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
    sync_api::ReadNode root(&trans);
    if (!root.InitByTagLookup(
        syncable::ModelTypeToRootTag(syncable::PREFERENCES))) {
      return sync_api::kInvalidId;
    }

    sync_api::WriteNode tag_node(&trans);
    sync_api::WriteNode node(&trans);

    if (tag_node.InitByClientTagLookup(syncable::PREFERENCES, name))
      return WriteSyncedValue(name, value, &tag_node);
    if (node.InitUniqueByCreation(syncable::PREFERENCES, root, name))
      return WriteSyncedValue(name, value, &node);

    return sync_api::kInvalidId;
  }

  bool IsSynced(const std::string& pref_name) {
    return pref_sync_service_->registered_preferences().count(pref_name) > 0;
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

  PreferenceDataTypeController* dtc_;
  PrefModelAssociator* pref_sync_service_;
  SyncableServiceAdapter* model_associator_;
  GenericChangeProcessor* change_processor_;

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

TEST_F(ProfileSyncServicePreferenceTest, CreatePrefSyncData) {
  prefs_->SetString(prefs::kHomePage, example_url0_);
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  const PrefService::Preference* pref =
      prefs_->FindPreference(prefs::kHomePage);
  SyncData sync_data;
  EXPECT_TRUE(PrefModelAssociator::CreatePrefSyncData(pref->name(),
      *pref->GetValue(), &sync_data));
  EXPECT_EQ(std::string(prefs::kHomePage), sync_data.GetTag());
  const sync_pb::PreferenceSpecifics& specifics(sync_data.GetSpecifics().
      GetExtension(sync_pb::preference));
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
  int64 node_id = SetSyncedValue(prefs::kHomePage, *expected);
  ASSERT_NE(node_id, sync_api::kInvalidId);
  {
    sync_api::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_UPDATE));
  }
  change_processor_->CommitChangesFromSyncModel();

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
  {
    sync_api::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_ADD));
  }
  change_processor_->CommitChangesFromSyncModel();

  const Value& actual = GetPreferenceValue(prefs::kHomePage);
  EXPECT_TRUE(expected->Equals(&actual));
  EXPECT_EQ(1U,
      pref_sync_service_->registered_preferences().count(prefs::kHomePage));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedSyncNodeUnknownPreference) {
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url0_));
  int64 node_id = SetSyncedValue("unknown preference", *expected);
  ASSERT_NE(node_id, sync_api::kInvalidId);
  {
    sync_api::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_UPDATE));
  }
  change_processor_->CommitChangesFromSyncModel();

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

  // An incoming sync transaction should change the user value, not the managed
  // value.
  scoped_ptr<Value> sync_value(
      Value::CreateStringValue("http://crbug.com"));
  int64 node_id = SetSyncedValue(prefs::kHomePage, *sync_value);
  ASSERT_NE(node_id, sync_api::kInvalidId);
  {
    sync_api::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_UPDATE));
  }
  change_processor_->CommitChangesFromSyncModel();

  EXPECT_TRUE(managed_value->Equals(prefs_->GetManagedPref(prefs::kHomePage)));
  EXPECT_TRUE(sync_value->Equals(prefs_->GetUserPref(prefs::kHomePage)));
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

  // The pref value should be the one dictated by policy.
  EXPECT_TRUE(managed_value->Equals(&GetPreferenceValue(prefs::kHomePage)));

  // Switch kHomePage back to unmanaged.
  profile_->GetTestingPrefService()->RemoveManagedPref(prefs::kHomePage);

  // The original value should be picked up.
  EXPECT_TRUE(initial_value->Equals(&GetPreferenceValue(prefs::kHomePage)));
}

TEST_F(ProfileSyncServicePreferenceTest,
       DynamicManagedPreferencesWithSyncChange) {
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

  // Change the sync value.
  scoped_ptr<Value> sync_value(
      Value::CreateStringValue("http://example.com/sync"));
  int64 node_id = SetSyncedValue(prefs::kHomePage, *sync_value);
  ASSERT_NE(node_id, sync_api::kInvalidId);
  {
    sync_api::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_ADD));
  }
  change_processor_->CommitChangesFromSyncModel();

  // The pref value should still be the one dictated by policy.
  EXPECT_TRUE(managed_value->Equals(&GetPreferenceValue(prefs::kHomePage)));

  // Switch kHomePage back to unmanaged.
  profile_->GetTestingPrefService()->RemoveManagedPref(prefs::kHomePage);

  // Sync value should be picked up.
  EXPECT_TRUE(sync_value->Equals(&GetPreferenceValue(prefs::kHomePage)));
}

TEST_F(ProfileSyncServicePreferenceTest, DynamicManagedDefaultPreferences) {
  const PrefService::Preference* pref =
      prefs_->FindPreference(prefs::kHomePage);
  EXPECT_TRUE(pref->IsDefaultValue());
  CreateRootTask task(this, syncable::PREFERENCES);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());
  EXPECT_TRUE(IsSynced(prefs::kHomePage));
  EXPECT_TRUE(pref->IsDefaultValue());
  EXPECT_TRUE(GetSyncedValue(prefs::kHomePage) == NULL);
  // Switch kHomePage to managed and set a different value.
  scoped_ptr<Value> managed_value(
      Value::CreateStringValue("http://example.com/managed"));
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kHomePage, managed_value->DeepCopy());
  // The pref value should be the one dictated by policy.
  EXPECT_TRUE(managed_value->Equals(&GetPreferenceValue(prefs::kHomePage)));
  EXPECT_FALSE(pref->IsDefaultValue());
  // There should be no synced value.
  EXPECT_TRUE(GetSyncedValue(prefs::kHomePage) == NULL);
  // Switch kHomePage back to unmanaged.
  profile_->GetTestingPrefService()->RemoveManagedPref(prefs::kHomePage);
  // The original value should be picked up.
  EXPECT_TRUE(pref->IsDefaultValue());
  // There should still be no synced value.
  EXPECT_TRUE(GetSyncedValue(prefs::kHomePage) == NULL);
}
