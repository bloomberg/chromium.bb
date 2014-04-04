// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/invalidation/fake_invalidation_service.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/ui_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/api/attachments/fake_attachment_service.h"
#include "sync/api/sync_data.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/data_type_debug_info_listener.h"
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
using testing::Invoke;
using testing::Return;
using testing::_;

namespace {
const char kTestProfileName[] = "test-profile";
}

typedef std::map<const std::string, const base::Value*> PreferenceValues;

ACTION_P(CreateAndSaveChangeProcessor, change_processor) {
  syncer::UserShare* user_share = arg0->GetUserShare();
  *change_processor = new GenericChangeProcessor(
      arg1,
      arg2,
      arg3,
      user_share,
      syncer::FakeAttachmentService::CreateForTest());
  return *change_processor;
}

ACTION_P(ReturnNewDataTypeManagerWithDebugListener, debug_listener) {
  return new browser_sync::DataTypeManagerImpl(
      debug_listener,
      arg1,
      arg2,
      arg3,
      arg4,
      arg5);
}

// TODO(zea): Refactor to remove the ProfileSyncService usage.
class ProfileSyncServicePreferenceTest
    : public AbstractProfileSyncServiceTest,
      public syncer::DataTypeDebugInfoListener {
 public:
  int64 SetSyncedValue(const std::string& name, const base::Value& value) {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
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

  // DataTypeDebugInfoListener implementation.
  virtual void OnDataTypeConfigureComplete(
      const std::vector<syncer::DataTypeConfigurationStats>&
          configuration_stats) OVERRIDE {
    ASSERT_EQ(1u, configuration_stats.size());
    association_stats_ = configuration_stats[0].association_stats;
  }

 protected:
  ProfileSyncServicePreferenceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        debug_ptr_factory_(this),
        example_url0_("http://example.com/0"),
        example_url1_("http://example.com/1"),
        example_url2_("http://example.com/2"),
        not_synced_preference_name_("nonsense_pref_name"),
        not_synced_preference_default_value_("default"),
        non_default_charset_value_("foo") {}

  virtual void SetUp() {
    AbstractProfileSyncServiceTest::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile::TestingFactories testing_factories;
    testing_factories.push_back(std::make_pair(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        BuildAutoIssuingFakeProfileOAuth2TokenService));
    profile_ = profile_manager_.CreateTestingProfile(
        kTestProfileName,
        scoped_ptr<PrefServiceSyncable>(),
        base::UTF8ToUTF16(kTestProfileName),
        0,
        std::string(),
        testing_factories);
    invalidation::InvalidationServiceFactory::GetInstance()->SetTestingFactory(
        profile_, invalidation::FakeInvalidationService::Build);
    prefs_ = profile_->GetTestingPrefService();

    prefs_->registry()->RegisterStringPref(
        not_synced_preference_name_.c_str(),
        not_synced_preference_default_value_,
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }

  virtual void TearDown() {
    profile_ = NULL;
    profile_manager_.DeleteTestingProfile(kTestProfileName);
    AbstractProfileSyncServiceTest::TearDown();
  }

  int GetSyncPreferenceCount() {
    syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode node(&trans);
    if (node.InitByTagLookup(syncer::ModelTypeToRootTag(syncer::PREFERENCES)) !=
        syncer::BaseNode::INIT_OK)
      return 0;
    return node.GetTotalNodeCount() - 1;
  }

  bool StartSyncService(const base::Closure& callback,
                        bool will_fail_association) {
    if (sync_service_)
      return false;

    SigninManagerBase* signin =
         SigninManagerFactory::GetForProfile(profile_);
    signin->SetAuthenticatedUsername("test");
    sync_service_ = TestProfileSyncService::BuildAutoStartAsyncInit(profile_,
                                                                    callback);
    pref_sync_service_ = reinterpret_cast<PrefModelAssociator*>(
        prefs_->GetSyncableService(syncer::PREFERENCES));
    if (!pref_sync_service_)
      return false;
    ProfileSyncComponentsFactoryMock* components =
        sync_service_->components_factory_mock();
    EXPECT_CALL(*components, GetSyncableServiceForType(syncer::PREFERENCES)).
        WillOnce(Return(pref_sync_service_->AsWeakPtr()));

    EXPECT_CALL(*components, CreateDataTypeManager(_, _, _, _, _, _)).
        WillOnce(ReturnNewDataTypeManagerWithDebugListener(
                     syncer::MakeWeakHandle(debug_ptr_factory_.GetWeakPtr())));
    dtc_ = new UIDataTypeController(base::MessageLoopProxy::current(),
                                    base::Closure(),
                                    syncer::PREFERENCES,
                                    components,
                                    profile_,
                                    sync_service_);
    EXPECT_CALL(*components, CreateSharedChangeProcessor()).
        WillOnce(Return(new SharedChangeProcessor()));
    EXPECT_CALL(*components, CreateGenericChangeProcessor(_, _, _, _)).
        WillOnce(CreateAndSaveChangeProcessor(
                     &change_processor_));
    sync_service_->RegisterDataTypeController(dtc_);
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
        ->UpdateCredentials("test", "oauth2_login_token");

    sync_service_->Initialize();
    base::MessageLoop::current()->Run();

    // It's possible this test triggered an unrecoverable error, in which case
    // we can't get the preference count.
    if (sync_service_->ShouldPushChanges()) {
        EXPECT_EQ(GetSyncPreferenceCount(),
                  association_stats_.num_sync_items_after_association);
    }
    EXPECT_EQ(association_stats_.num_sync_items_after_association,
              association_stats_.num_sync_items_before_association +
              association_stats_.num_sync_items_added -
              association_stats_.num_sync_items_deleted);

    return true;
  }

  const base::Value& GetPreferenceValue(const std::string& name) {
    const PrefService::Preference* preference =
        prefs_->FindPreference(name.c_str());
    return *preference->GetValue();
  }

  // Caller gets ownership of the returned value.
  const base::Value* GetSyncedValue(const std::string& name) {
    syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
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
                         const base::Value& value,
                         syncer::WriteNode* node) {
    syncer::SyncData sync_data;
    if (!pref_sync_service_->CreatePrefSyncData(name,
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

  bool HasSyncData(const std::string& pref_name) {
    return pref_sync_service_->IsPrefSynced(pref_name);
  }

  std::string ValueString(const base::Value& value) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    json.Serialize(value);
    return serialized;
  }

  // Returns whether a given preference name is a new name of a migrated
  // preference. Exposed here for testing.
  static bool IsMigratedPreference(const char* preference_name) {
    return PrefModelAssociator::IsMigratedPreference(preference_name);
  }
  static bool IsOldMigratedPreference(const char* old_preference_name) {
    return PrefModelAssociator::IsOldMigratedPreference(old_preference_name);
  }

  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  TestingPrefServiceSyncable* prefs_;

  UIDataTypeController* dtc_;
  PrefModelAssociator* pref_sync_service_;
  GenericChangeProcessor* change_processor_;
  syncer::DataTypeAssociationStats association_stats_;
  base::WeakPtrFactory<DataTypeDebugInfoListener> debug_ptr_factory_;

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
      : callback_(base::Bind(
            &AddPreferenceEntriesHelper::AddPreferenceEntriesCallback,
            base::Unretained(this), test, entries)),
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
  EXPECT_TRUE(pref_sync_service_->CreatePrefSyncData(pref->name(),
      *pref->GetValue(), &sync_data));
  EXPECT_EQ(std::string(prefs::kHomePage),
            syncer::SyncDataLocal(sync_data).GetTag());
  const sync_pb::PreferenceSpecifics& specifics(sync_data.GetSpecifics().
      preference());
  EXPECT_EQ(std::string(prefs::kHomePage), specifics.name());

  scoped_ptr<base::Value> value(base::JSONReader::Read(specifics.value()));
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
    base::ListValue* url_list = update.Get();
    url_list->Append(new base::StringValue(example_url0_));
    url_list->Append(new base::StringValue(example_url1_));
  }
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  scoped_ptr<const base::Value> value(GetSyncedValue(prefs::kHomePage));
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
    base::ListValue* url_list = update.Get();
    url_list->Append(new base::StringValue(example_url0_));
    url_list->Append(new base::StringValue(example_url1_));
  }

  PreferenceValues cloud_data;
  STLValueDeleter<PreferenceValues> cloud_data_deleter(&cloud_data);
  cloud_data[prefs::kHomePage] = new base::StringValue(example_url1_);
  base::ListValue* urls_to_restore = new base::ListValue;
  urls_to_restore->Append(new base::StringValue(example_url1_));
  urls_to_restore->Append(new base::StringValue(example_url2_));
  cloud_data[prefs::kURLsToRestoreOnStartup] = urls_to_restore;
  cloud_data[prefs::kDefaultCharset] =
      new base::StringValue(non_default_charset_value_);

  AddPreferenceEntriesHelper helper(this, cloud_data);
  ASSERT_TRUE(StartSyncService(helper.callback(), false));
  ASSERT_TRUE(helper.success());

  scoped_ptr<const base::Value> value(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(value.get());
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ(example_url1_, string_value);
  EXPECT_EQ(example_url1_, prefs_->GetString(prefs::kHomePage));

  scoped_ptr<base::ListValue> expected_urls(new base::ListValue);
  expected_urls->Append(new base::StringValue(example_url1_));
  expected_urls->Append(new base::StringValue(example_url2_));
  expected_urls->Append(new base::StringValue(example_url0_));
  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_urls.get()));
  EXPECT_TRUE(GetPreferenceValue(prefs::kURLsToRestoreOnStartup).
              Equals(expected_urls.get()));

  value.reset(GetSyncedValue(prefs::kDefaultCharset));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ(non_default_charset_value_, string_value);
  EXPECT_EQ(non_default_charset_value_,
            prefs_->GetString(prefs::kDefaultCharset));
}

TEST_F(ProfileSyncServicePreferenceTest, ModelAssociationMigrateOldData) {
  ASSERT_TRUE(IsMigratedPreference(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(IsOldMigratedPreference(prefs::kURLsToRestoreOnStartupOld));

  PreferenceValues cloud_data;
  STLValueDeleter<PreferenceValues> cloud_data_deleter(&cloud_data);
  base::ListValue* urls_to_restore = new base::ListValue;
  urls_to_restore->Append(new base::StringValue(example_url1_));
  urls_to_restore->Append(new base::StringValue(example_url2_));
  cloud_data[prefs::kURLsToRestoreOnStartupOld] = urls_to_restore;

  AddPreferenceEntriesHelper helper(this, cloud_data);
  ASSERT_TRUE(StartSyncService(helper.callback(), false));
  ASSERT_TRUE(helper.success());

  // Expect that the new preference data contains the old pref's values.
  scoped_ptr<base::ListValue> expected_urls(new base::ListValue);
  expected_urls->Append(new base::StringValue(example_url1_));
  expected_urls->Append(new base::StringValue(example_url2_));

  ASSERT_TRUE(HasSyncData(prefs::kURLsToRestoreOnStartup));
  scoped_ptr<const base::Value> value(
      GetSyncedValue(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_urls.get()));
  EXPECT_TRUE(GetPreferenceValue(prefs::kURLsToRestoreOnStartup).
              Equals(expected_urls.get()));

  // The old preference value should be the same.
  expected_urls.reset(new base::ListValue);
  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartupOld));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(GetPreferenceValue(prefs::kURLsToRestoreOnStartupOld).
              Equals(expected_urls.get()));
}

TEST_F(ProfileSyncServicePreferenceTest,
       ModelAssociationCloudHasOldMigratedData) {
  ASSERT_TRUE(IsMigratedPreference(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(IsOldMigratedPreference(prefs::kURLsToRestoreOnStartupOld));
  prefs_->SetString(prefs::kHomePage, example_url0_);
  {
    ListPrefUpdate update(prefs_, prefs::kURLsToRestoreOnStartup);
    base::ListValue* url_list = update.Get();
    url_list->Append(new base::StringValue(example_url0_));
    url_list->Append(new base::StringValue(example_url1_));
  }

  PreferenceValues cloud_data;
  STLValueDeleter<PreferenceValues> cloud_data_deleter(&cloud_data);
  cloud_data[prefs::kHomePage] = new base::StringValue(example_url1_);
  base::ListValue* urls_to_restore = new base::ListValue;
  urls_to_restore->Append(new base::StringValue(example_url1_));
  urls_to_restore->Append(new base::StringValue(example_url2_));
  cloud_data[prefs::kURLsToRestoreOnStartupOld] = urls_to_restore;

  AddPreferenceEntriesHelper helper(this, cloud_data);
  ASSERT_TRUE(StartSyncService(helper.callback(), false));
  ASSERT_TRUE(helper.success());

  scoped_ptr<const base::Value> value(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(value.get());
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ(example_url1_, string_value);
  EXPECT_EQ(example_url1_, prefs_->GetString(prefs::kHomePage));

  // Expect that the new preference data contains the merged old prefs values.
  scoped_ptr<base::ListValue> expected_urls(new base::ListValue);
  expected_urls->Append(new base::StringValue(example_url1_));
  expected_urls->Append(new base::StringValue(example_url2_));
  expected_urls->Append(new base::StringValue(example_url0_));

  ASSERT_TRUE(HasSyncData(prefs::kURLsToRestoreOnStartup));
  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_urls.get()));
  EXPECT_TRUE(GetPreferenceValue(prefs::kURLsToRestoreOnStartup).
              Equals(expected_urls.get()));

  // The old preference name should also contain the merged sync data.
  expected_urls.reset(new base::ListValue);
  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartupOld));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(GetPreferenceValue(prefs::kURLsToRestoreOnStartupOld).
              Equals(expected_urls.get()));
}

TEST_F(ProfileSyncServicePreferenceTest,
       ModelAssociationCloudHasNewMigratedData) {
  ASSERT_TRUE(IsMigratedPreference(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(IsOldMigratedPreference(prefs::kURLsToRestoreOnStartupOld));
  prefs_->SetString(prefs::kHomePage, example_url0_);
  {
    ListPrefUpdate update(prefs_, prefs::kURLsToRestoreOnStartupOld);
    base::ListValue* url_list = update.Get();
    url_list->Append(new base::StringValue(example_url0_));
    url_list->Append(new base::StringValue(example_url1_));
  }

  PreferenceValues cloud_data;
  STLValueDeleter<PreferenceValues> cloud_data_deleter(&cloud_data);
  cloud_data[prefs::kHomePage] = new base::StringValue(example_url1_);
  base::ListValue* urls_to_restore = new base::ListValue;
  urls_to_restore->Append(new base::StringValue(example_url1_));
  urls_to_restore->Append(new base::StringValue(example_url2_));
  cloud_data[prefs::kURLsToRestoreOnStartup] = urls_to_restore;

  AddPreferenceEntriesHelper helper(this, cloud_data);
  ASSERT_TRUE(StartSyncService(helper.callback(), false));
  ASSERT_TRUE(helper.success());

  scoped_ptr<const base::Value> value(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(value.get());
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ(example_url1_, string_value);
  EXPECT_EQ(example_url1_, prefs_->GetString(prefs::kHomePage));

  // Expect that the cloud data under the new migrated preference name sticks.
  scoped_ptr<base::ListValue> expected_urls(new base::ListValue);
  expected_urls->Append(new base::StringValue(example_url1_));
  expected_urls->Append(new base::StringValue(example_url2_));

  ASSERT_TRUE(HasSyncData(prefs::kURLsToRestoreOnStartup));
  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_urls.get()));
  EXPECT_TRUE(GetPreferenceValue(prefs::kURLsToRestoreOnStartup).
              Equals(expected_urls.get()));

  // The old preference data should still be here, though not synced.
  expected_urls.reset(new base::ListValue);
  expected_urls->Append(new base::StringValue(example_url0_));
  expected_urls->Append(new base::StringValue(example_url1_));

  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartupOld));
  ASSERT_FALSE(value.get());
  EXPECT_TRUE(GetPreferenceValue(prefs::kURLsToRestoreOnStartupOld).
              Equals(expected_urls.get()));
}

TEST_F(ProfileSyncServicePreferenceTest,
       ModelAssociationCloudAddsOldAndNewMigratedData) {
  ASSERT_TRUE(IsMigratedPreference(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(IsOldMigratedPreference(prefs::kURLsToRestoreOnStartupOld));
  prefs_->SetString(prefs::kHomePage, example_url0_);
  {
    ListPrefUpdate update_old(prefs_, prefs::kURLsToRestoreOnStartupOld);
    base::ListValue* url_list_old = update_old.Get();
    url_list_old->Append(new base::StringValue(example_url0_));
    url_list_old->Append(new base::StringValue(example_url1_));
    ListPrefUpdate update(prefs_, prefs::kURLsToRestoreOnStartup);
    base::ListValue* url_list = update.Get();
    url_list->Append(new base::StringValue(example_url1_));
    url_list->Append(new base::StringValue(example_url2_));
  }

  PreferenceValues cloud_data;
  STLValueDeleter<PreferenceValues> cloud_data_deleter(&cloud_data);
  cloud_data[prefs::kHomePage] = new base::StringValue(example_url1_);

  AddPreferenceEntriesHelper helper(this, cloud_data);
  ASSERT_TRUE(StartSyncService(helper.callback(), false));
  ASSERT_TRUE(helper.success());

  scoped_ptr<const base::Value> value(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(value.get());
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ(example_url1_, string_value);
  EXPECT_EQ(example_url1_, prefs_->GetString(prefs::kHomePage));

  // Expect that the cloud data under the new migrated preference name sticks.
  scoped_ptr<base::ListValue> expected_urls(new base::ListValue);
  expected_urls->Append(new base::StringValue(example_url1_));
  expected_urls->Append(new base::StringValue(example_url2_));

  ASSERT_TRUE(HasSyncData(prefs::kURLsToRestoreOnStartup));
  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartup));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_urls.get()));
  EXPECT_TRUE(GetPreferenceValue(prefs::kURLsToRestoreOnStartup).
              Equals(expected_urls.get()));

  // Should not have synced in the old startup url values.
  value.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartupOld));
  ASSERT_FALSE(value.get());
  EXPECT_FALSE(GetPreferenceValue(prefs::kURLsToRestoreOnStartupOld).
               Equals(expected_urls.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, FailModelAssociation) {
  ASSERT_TRUE(StartSyncService(base::Closure(), true));
  EXPECT_TRUE(sync_service_->HasUnrecoverableError());
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedPreferenceWithDefaultValue) {
  const PrefService::Preference* pref =
      prefs_->FindPreference(prefs::kHomePage);
  EXPECT_TRUE(pref->IsDefaultValue());

  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  base::StringValue expected(example_url0_);
  profile_->GetPrefs()->Set(prefs::kHomePage, expected);

  scoped_ptr<const base::Value> actual(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(expected.Equals(actual.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedPreferenceWithValue) {
  profile_->GetPrefs()->SetString(prefs::kHomePage, example_url0_);
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  base::StringValue expected(example_url1_);
  profile_->GetPrefs()->Set(prefs::kHomePage, expected);

  scoped_ptr<const base::Value> actual(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(expected.Equals(actual.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedSyncNodeActionUpdate) {
  profile_->GetPrefs()->SetString(prefs::kHomePage, example_url0_);
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  base::StringValue expected(example_url1_);
  int64 node_id = SetSyncedValue(prefs::kHomePage, expected);
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans, 0,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_UPDATE));
  }
  change_processor_->CommitChangesFromSyncModel();

  const base::Value& actual = GetPreferenceValue(prefs::kHomePage);
  EXPECT_TRUE(expected.Equals(&actual));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedSyncNodeActionAdd) {
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  base::StringValue expected(example_url0_);
  int64 node_id = SetSyncedValue(prefs::kHomePage, expected);
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans, 0,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_ADD));
  }
  change_processor_->CommitChangesFromSyncModel();

  const base::Value& actual = GetPreferenceValue(prefs::kHomePage);
  EXPECT_TRUE(expected.Equals(&actual));
  EXPECT_EQ(1U,
      pref_sync_service_->registered_preferences().count(prefs::kHomePage));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedSyncNodeUnknownPreference) {
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  base::StringValue expected(example_url0_);
  int64 node_id = SetSyncedValue("unknown preference", expected);
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans, 0,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_UPDATE));
  }
  change_processor_->CommitChangesFromSyncModel();

  // Nothing interesting happens on the client when it gets an update
  // of an unknown preference.  We just should not crash.
}

TEST_F(ProfileSyncServicePreferenceTest, ManagedPreferences) {
  // Make the homepage preference managed.
  base::StringValue managed_value("http://example.com");
  prefs_->SetManagedPref(prefs::kHomePage, managed_value.DeepCopy());

  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  // Changing the homepage preference should not sync anything.
  base::StringValue user_value("http://chromium..com");
  prefs_->SetUserPref(prefs::kHomePage, user_value.DeepCopy());
  EXPECT_EQ(NULL, GetSyncedValue(prefs::kHomePage));

  // An incoming sync transaction should change the user value, not the managed
  // value.
  base::StringValue sync_value("http://crbug.com");
  int64 node_id = SetSyncedValue(prefs::kHomePage, sync_value);
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans, 0,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_UPDATE));
  }
  change_processor_->CommitChangesFromSyncModel();

  EXPECT_TRUE(managed_value.Equals(prefs_->GetManagedPref(prefs::kHomePage)));
  EXPECT_TRUE(sync_value.Equals(prefs_->GetUserPref(prefs::kHomePage)));
}

// List preferences have special handling at association time due to our ability
// to merge the local and sync value. Make sure the merge logic doesn't merge
// managed preferences.
TEST_F(ProfileSyncServicePreferenceTest, ManagedListPreferences) {
  // Make the list of urls to restore on startup managed.
  base::ListValue managed_value;
  managed_value.Append(new base::StringValue(example_url0_));
  managed_value.Append(new base::StringValue(example_url1_));
  prefs_->SetManagedPref(prefs::kURLsToRestoreOnStartup,
                         managed_value.DeepCopy());

  // Set a cloud version.
  PreferenceValues cloud_data;
  STLValueDeleter<PreferenceValues> cloud_data_deleter(&cloud_data);
  base::ListValue* urls_to_restore = new base::ListValue;
  urls_to_restore->Append(new base::StringValue(example_url1_));
  urls_to_restore->Append(new base::StringValue(example_url2_));
  cloud_data[prefs::kURLsToRestoreOnStartup] = urls_to_restore;

  // Start sync and verify the synced value didn't get merged.
  AddPreferenceEntriesHelper helper(this, cloud_data);
  ASSERT_TRUE(StartSyncService(helper.callback(), false));
  ASSERT_TRUE(helper.success());
  scoped_ptr<const base::Value> actual(
      GetSyncedValue(prefs::kURLsToRestoreOnStartup));
  EXPECT_TRUE(cloud_data[prefs::kURLsToRestoreOnStartup]->Equals(actual.get()));

  // Changing the user's urls to restore on startup pref should not sync
  // anything.
  base::ListValue user_value;
  user_value.Append(new base::StringValue("http://chromium.org"));
  prefs_->SetUserPref(prefs::kURLsToRestoreOnStartup, user_value.DeepCopy());
  actual.reset(GetSyncedValue(prefs::kURLsToRestoreOnStartup));
  EXPECT_TRUE(cloud_data[prefs::kURLsToRestoreOnStartup]->Equals(actual.get()));

  // An incoming sync transaction should change the user value, not the managed
  // value.
  base::ListValue sync_value;
  sync_value.Append(new base::StringValue("http://crbug.com"));
  int64 node_id = SetSyncedValue(prefs::kURLsToRestoreOnStartup, sync_value);
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans, 0,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_UPDATE));
  }
  change_processor_->CommitChangesFromSyncModel();

  EXPECT_TRUE(managed_value.Equals(
          prefs_->GetManagedPref(prefs::kURLsToRestoreOnStartup)));
  EXPECT_TRUE(sync_value.Equals(
          prefs_->GetUserPref(prefs::kURLsToRestoreOnStartup)));
}

TEST_F(ProfileSyncServicePreferenceTest, DynamicManagedPreferences) {
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  base::StringValue initial_value("http://example.com/initial");
  profile_->GetPrefs()->Set(prefs::kHomePage, initial_value);
  scoped_ptr<const base::Value> actual(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(initial_value.Equals(actual.get()));

  // Switch kHomePage to managed and set a different value.
  base::StringValue managed_value("http://example.com/managed");
  profile_->GetTestingPrefService()->SetManagedPref(prefs::kHomePage,
                                                    managed_value.DeepCopy());

  // The pref value should be the one dictated by policy.
  EXPECT_TRUE(managed_value.Equals(&GetPreferenceValue(prefs::kHomePage)));

  // Switch kHomePage back to unmanaged.
  profile_->GetTestingPrefService()->RemoveManagedPref(prefs::kHomePage);

  // The original value should be picked up.
  EXPECT_TRUE(initial_value.Equals(&GetPreferenceValue(prefs::kHomePage)));
}

TEST_F(ProfileSyncServicePreferenceTest,
       DynamicManagedPreferencesWithSyncChange) {
  CreateRootHelper create_root(this, syncer::PREFERENCES);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  base::StringValue initial_value("http://example.com/initial");
  profile_->GetPrefs()->Set(prefs::kHomePage, initial_value);
  scoped_ptr<const base::Value> actual(GetSyncedValue(prefs::kHomePage));
  EXPECT_TRUE(initial_value.Equals(actual.get()));

  // Switch kHomePage to managed and set a different value.
  base::StringValue managed_value("http://example.com/managed");
  profile_->GetTestingPrefService()->SetManagedPref(prefs::kHomePage,
                                                    managed_value.DeepCopy());

  // Change the sync value.
  base::StringValue sync_value("http://example.com/sync");
  int64 node_id = SetSyncedValue(prefs::kHomePage, sync_value);
  ASSERT_NE(node_id, syncer::kInvalidId);
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans, 0,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_ADD));
  }
  change_processor_->CommitChangesFromSyncModel();

  // The pref value should still be the one dictated by policy.
  EXPECT_TRUE(managed_value.Equals(&GetPreferenceValue(prefs::kHomePage)));

  // Switch kHomePage back to unmanaged.
  profile_->GetTestingPrefService()->RemoveManagedPref(prefs::kHomePage);

  // Sync value should be picked up.
  EXPECT_TRUE(sync_value.Equals(&GetPreferenceValue(prefs::kHomePage)));
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
  base::StringValue managed_value("http://example.com/managed");
  profile_->GetTestingPrefService()->SetManagedPref(prefs::kHomePage,
                                                    managed_value.DeepCopy());
  // The pref value should be the one dictated by policy.
  EXPECT_TRUE(managed_value.Equals(&GetPreferenceValue(prefs::kHomePage)));
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
