// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/ui_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/api/sync_data.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/preference_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::JSONReader;
using browser_sync::GenericChangeProcessor;
using browser_sync::SharedChangeProcessor;
using browser_sync::UIDataTypeController;
using syncer::ChangeRecord;
using testing::_;
using testing::Invoke;
using testing::Return;

typedef std::map<const std::string, const Value*> PreferenceValues;

ACTION_P(CreateAndSaveChangeProcessor, change_processor) {
  syncer::UserShare* user_share = arg0->GetUserShare();
  *change_processor = new GenericChangeProcessor(arg1, arg2, user_share);
  return *change_processor;
}

// TODO(zea): Refactor to remove the ProfileSyncService usage.
class ProfileSyncServicePreferenceTest
    : public AbstractProfileSyncServiceTest {
 public:
  int64 SetSyncedValue(const std::string& name, const Value& value) {
    syncer::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
    syncer::ReadNode root(&trans);
    if (root.InitByTagLookup(syncer::ModelTypeToRootTag(
            syncer::PREFERENCES)) != syncer::BaseNode::INIT_OK) {
      return syncer::kInvalidId;
    }

    syncer::WriteNode tag_node(&trans);
    syncer::WriteNode node(&trans);

    if (tag_node.InitByClientTagLookup(syncer::PREFERENCES, name) ==
            syncer::BaseNode::INIT_OK) {
      return WriteSyncedValue(name, value, &tag_node);
    }

    syncer::WriteNode::InitUniqueByCreationResult result =
        node.InitUniqueByCreation(syncer::PREFERENCES, root, name);
    if (result == syncer::WriteNode::INIT_SUCCESS)
      return WriteSyncedValue(name, value, &node);

    return syncer::kInvalidId;
  }

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

  bool StartSyncService(const base::Closure& callback,
                        bool will_fail_association) {
    if (service_.get())
      return false;

    SigninManager* signin = SigninManagerFactory::GetForProfile(profile_.get());
    signin->SetAuthenticatedUsername("test");
    ProfileSyncComponentsFactoryMock* factory =
        new ProfileSyncComponentsFactoryMock();
    service_.reset(new TestProfileSyncService(
        factory,
        profile_.get(),
        signin,
        ProfileSyncService::AUTO_START,
        false,
        callback));
    pref_sync_service_ = reinterpret_cast<PrefModelAssociator*>(
        prefs_->GetSyncableService());
    if (!pref_sync_service_)
      return false;
    EXPECT_CALL(*factory, GetSyncableServiceForType(syncer::PREFERENCES)).
        WillOnce(Return(pref_sync_service_->AsWeakPtr()));

    EXPECT_CALL(*factory, CreateDataTypeManager(_, _, _)).
        WillOnce(ReturnNewDataTypeManager());
    dtc_ = new UIDataTypeController(syncer::PREFERENCES,
                                    factory,
                                    profile_.get(),
                                    service_.get());
    EXPECT_CALL(*factory, CreateSharedChangeProcessor()).
        WillOnce(Return(new SharedChangeProcessor()));
    EXPECT_CALL(*factory, CreateGenericChangeProcessor(_, _, _)).
        WillOnce(CreateAndSaveChangeProcessor(&change_processor_));
    service_->RegisterDataTypeController(dtc_);
    TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
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
    syncer::ReadTransaction trans(FROM_HERE, service_->GetUserShare());
    syncer::ReadNode node(&trans);

    if (node.InitByClientTagLookup(syncer::PREFERENCES, name) !=
            syncer::BaseNode::INIT_OK) {
      return NULL;
    }

    const sync_pb::PreferenceSpecifics& specifics(
        node.GetEntitySpecifics().preference());

    return base::JSONReader::Read(specifics.value());
  }

  int64 WriteSyncedValue(const std::string& name,
                         const Value& value,
                         syncer::WriteNode* node) {
    syncer::SyncData sync_data;
    if (!PrefModelAssociator::CreatePrefSyncData(name,
                                                 value,
                                                 &sync_data)) {
      return syncer::kInvalidId;
    }
    node->SetEntitySpecifics(sync_data.GetSpecifics());
    return node->GetId();
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

  scoped_ptr<TestingProfile> profile_;
  TestingPrefService* prefs_;

  UIDataTypeController* dtc_;
  PrefModelAssociator* pref_sync_service_;
  GenericChangeProcessor* change_processor_;

  std::string example_url0_;
  std::string example_url1_;
  std::string example_url2_;
  std::string not_synced_preference_name_;
  std::string not_synced_preference_default_value_;
  std::string non_default_charset_value_;
};

class AddPreferenceEntriesHelper {
 public:
  AddPreferenceEntriesHelper(ProfileSyncServicePreferenceTest* test,
                             const PreferenceValues& entries)
      : ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
            base::Bind(
                &AddPreferenceEntriesHelper::AddPreferenceEntriesCallback,
                base::Unretained(this), test, entries))),
        success_(false) {
  }

  const base::Closure& callback() const { return callback_; }
  bool success() { return success_; }

 private:
  void AddPreferenceEntriesCallback(ProfileSyncServicePreferenceTest* test,
                                    const PreferenceValues& entries) {
    if (!test->CreateRoot(syncer::PREFERENCES))
      return;

    for (PreferenceValues::const_iterator i = entries.begin();
         i != entries.end(); ++i) {
      if (test->SetSyncedValue(i->first, *i->second) == syncer::kInvalidId)
        return;
    }
    success_ = true;
  }

  base::Closure callback_;
  bool success_;
};

TEST_F(ProfileSyncServicePreferenceTest, CreatePrefSyncData) {
  prefs_->SetString(prefs::kHomePage, example_url0_);
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  const PrefService::Preference* pref =
      prefs_->FindPreference(prefs::kHomePage);
  syncer::SyncData sync_data;
  EXPECT_TRUE(PrefModelAssociator::CreatePrefSyncData(pref->name(),
      *pref->GetValue(), &sync_data));
  EXPECT_EQ(std::string(prefs::kHomePage), sync_data.GetTag());
  const sync_pb::PreferenceSpecifics& specifics(sync_data.GetSpecifics().
      preference());
  EXPECT_EQ(std::string(prefs::kHomePage), specifics.name());

  scoped_ptr<Value> value(base::JSONReader::Read(specifics.value()));
  EXPECT_TRUE(pref->GetValue()->Equals(value.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, ModelAssociationDoNotSyncDefaults) {
  const PrefService::Preference* pref =
      prefs_->FindPreference(prefs::kHomePage);
  EXPECT_TRUE(pref->IsDefaultValue());
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());
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
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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

  AddPreferenceEntriesHelper helper(this, cloud_data);
  ASSERT_TRUE(StartSyncService(helper.callback(), false));
  ASSERT_TRUE(helper.success());

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
  ASSERT_TRUE(StartSyncService(base::Closure(), true));
  EXPECT_TRUE(service_->HasUnrecoverableError());
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedPreferenceWithDefaultValue) {
  const PrefService::Preference* pref =
      prefs_->FindPreference(prefs::kHomePage);
  EXPECT_TRUE(pref->IsDefaultValue());

  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url0_));
  profile_->GetPrefs()->Set(prefs::kHomePage, *expected);

  scoped_ptr<const Value> actual(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(expected->Equals(actual.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedPreferenceWithValue) {
  profile_->GetPrefs()->SetString(prefs::kHomePage, example_url0_);
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url1_));
  profile_->GetPrefs()->Set(prefs::kHomePage, *expected);

  scoped_ptr<const Value> actual(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(expected->Equals(actual.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedSyncNodeActionUpdate) {
  profile_->GetPrefs()->SetString(prefs::kHomePage, example_url0_);
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url1_));
  int64 node_id = SetSyncedValue(prefs::kHomePage, *expected);
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
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
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url0_));
  int64 node_id = SetSyncedValue(prefs::kHomePage, *expected);
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
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
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  scoped_ptr<Value> expected(Value::CreateStringValue(example_url0_));
  int64 node_id = SetSyncedValue("unknown preference", *expected);
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
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

  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
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
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
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
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());
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
