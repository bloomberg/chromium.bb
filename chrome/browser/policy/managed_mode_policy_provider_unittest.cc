// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/prefs/testing_pref_store.h"
#include "base/strings/string_util.h"
#include "chrome/browser/policy/configuration_policy_provider_test.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

class MockChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockChangeProcessor() {}
  virtual ~MockChangeProcessor() {}

  // SyncChangeProcessor implementation:
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  const syncer::SyncChangeList& changes() const { return change_list_; }

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE {
    return syncer::SyncDataList();
  }

 private:
  syncer::SyncChangeList change_list_;

  DISALLOW_COPY_AND_ASSIGN(MockChangeProcessor);
};

syncer::SyncError MockChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  change_list_ = change_list;
  return syncer::SyncError();
}

class MockSyncErrorFactory : public syncer::SyncErrorFactory {
 public:
  explicit MockSyncErrorFactory(syncer::ModelType type);
  virtual ~MockSyncErrorFactory();

  // SyncErrorFactory implementation:
  virtual syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message) OVERRIDE;

 private:
  syncer::ModelType type_;

  DISALLOW_COPY_AND_ASSIGN(MockSyncErrorFactory);
};

MockSyncErrorFactory::MockSyncErrorFactory(syncer::ModelType type)
    : type_(type) {}

MockSyncErrorFactory::~MockSyncErrorFactory() {}

syncer::SyncError MockSyncErrorFactory::CreateAndUploadError(
    const tracked_objects::Location& location,
    const std::string& message) {
  return syncer::SyncError(location,
                           syncer::SyncError::DATATYPE_ERROR,
                           message,
                           type_);
}

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  virtual ~TestHarness();

  static PolicyProviderTestHarness* Create();

  // PolicyProviderTestHarness implementation:
  virtual void SetUp() OVERRIDE;

  virtual ConfigurationPolicyProvider* CreateProvider(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const PolicyDefinitionList* policy_definition_list) OVERRIDE;

  virtual void InstallEmptyPolicy() OVERRIDE;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) OVERRIDE;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) OVERRIDE;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) OVERRIDE;
  virtual void InstallStringListPolicy(
      const std::string& policy_name,
      const base::ListValue* policy_value) OVERRIDE;
  virtual void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) OVERRIDE;

 private:
  void InstallSimplePolicy(const std::string& policy_name,
                           scoped_ptr<base::Value> policy_value);

  scoped_refptr<TestingPrefStore> pref_store_;

  // Owned by the test fixture.
  ManagedModePolicyProvider* provider_;

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness()
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER),
      pref_store_(new TestingPrefStore) {
  pref_store_->SetInitializationCompleted();
}

TestHarness::~TestHarness() {}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness();
}

void TestHarness::SetUp() {}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const PolicyDefinitionList* policy_definition_list) {
  provider_ = new ManagedModePolicyProvider(pref_store_.get());
  syncer::SyncDataList initial_sync_data;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor(
      new MockChangeProcessor());
  scoped_ptr<syncer::SyncErrorFactory> error_handler(
      new MockSyncErrorFactory(syncer::MANAGED_USER_SETTINGS));
  syncer::SyncMergeResult result =
      provider_->MergeDataAndStartSyncing(syncer::MANAGED_USER_SETTINGS,
                                          initial_sync_data,
                                          sync_processor.Pass(),
                                          error_handler.Pass());
  EXPECT_FALSE(result.error().IsSet());
  return provider_;
}

void TestHarness::InstallSimplePolicy(const std::string& policy_name,
                                      scoped_ptr<base::Value> policy_value) {
  syncer::SyncData data = ManagedModePolicyProvider::CreateSyncDataForPolicy(
      policy_name, policy_value.get());
  syncer::SyncChangeList change_list;
  change_list.push_back(
      syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD, data));
  syncer::SyncError error =
      provider_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
}

void TestHarness::InstallEmptyPolicy() {}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  InstallSimplePolicy(
      policy_name,
      scoped_ptr<base::Value>(base::Value::CreateStringValue(policy_value)));
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  InstallSimplePolicy(
      policy_name,
      scoped_ptr<base::Value>(new base::FundamentalValue(policy_value)));
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  InstallSimplePolicy(
      policy_name,
      scoped_ptr<base::Value>(new base::FundamentalValue(policy_value)));
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const base::ListValue* policy_value) {
  InstallSimplePolicy(policy_name,
                      scoped_ptr<base::Value>(policy_value->DeepCopy()));
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  syncer::SyncChangeList change_list;
  for (base::DictionaryValue::Iterator it(*policy_value); !it.IsAtEnd();
       it.Advance()) {
    syncer::SyncData data = ManagedModePolicyProvider::CreateSyncDataForPolicy(
        ManagedModePolicyProvider::MakeSplitSettingKey(policy_name, it.key()),
        &it.value());
    change_list.push_back(
        syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD, data));
  }
  syncer::SyncError error =
      provider_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_CASE_P(
    ManagedModePolicyProviderTest,
    ConfigurationPolicyProviderTest,
    testing::Values(TestHarness::Create));

const char kAtomicItemName[] = "X-Wombat";
const char kSplitItemName[] = "X-SuperMoosePowers";

class ManagedModePolicyProviderAPITest : public PolicyTestBase {
 protected:
  ManagedModePolicyProviderAPITest()
      : pref_store_(new TestingPrefStore), provider_(pref_store_.get()) {
    pref_store_->SetInitializationCompleted();
  }
  virtual ~ManagedModePolicyProviderAPITest() {}

  scoped_ptr<syncer::SyncChangeProcessor> CreateSyncProcessor() {
    sync_processor_ = new MockChangeProcessor();
    return scoped_ptr<syncer::SyncChangeProcessor>(sync_processor_);
  }

  void StartSyncing(const syncer::SyncDataList& initial_sync_data) {
    scoped_ptr<syncer::SyncErrorFactory> error_handler(
        new MockSyncErrorFactory(syncer::MANAGED_USER_SETTINGS));
    syncer::SyncMergeResult result =
        provider_.MergeDataAndStartSyncing(syncer::MANAGED_USER_SETTINGS,
                                           initial_sync_data,
                                           CreateSyncProcessor(),
                                           error_handler.Pass());
    EXPECT_FALSE(result.error().IsSet());
  }

  void UploadSplitItem(const std::string& key, const std::string& value) {
    dict_.SetStringWithoutPathExpansion(key, value);
    provider_.UploadItem(
        ManagedModePolicyProvider::MakeSplitSettingKey(kSplitItemName, key),
        scoped_ptr<base::Value>(base::Value::CreateStringValue(value)));
  }

  void UploadAtomicItem(const std::string& value) {
    atomic_setting_value_.reset(new base::StringValue(value));
    provider_.UploadItem(
        kAtomicItemName,
        scoped_ptr<base::Value>(base::Value::CreateStringValue(value)));
  }

  void VerifySyncDataItem(syncer::SyncData sync_data) {
    const sync_pb::ManagedUserSettingSpecifics& managed_user_setting =
        sync_data.GetSpecifics().managed_user_setting();
    base::Value* expected_value = NULL;
    if (managed_user_setting.name() == kAtomicItemName) {
      expected_value = atomic_setting_value_.get();
    } else {
      EXPECT_TRUE(StartsWithASCII(managed_user_setting.name(),
                                  std::string(kSplitItemName) + ':',
                                  true));
      std::string key =
          managed_user_setting.name().substr(strlen(kSplitItemName) + 1);
      EXPECT_TRUE(dict_.GetWithoutPathExpansion(key, &expected_value));
    }

    scoped_ptr<Value> value(
        base::JSONReader::Read(managed_user_setting.value()));
    EXPECT_TRUE(expected_value->Equals(value.get()));
  }

  // PolicyTestBase overrides:
  virtual void TearDown() OVERRIDE {
    provider_.Shutdown();
    PolicyTestBase::TearDown();
  }

  base::DictionaryValue dict_;
  scoped_ptr<base::Value> atomic_setting_value_;
  scoped_refptr<TestingPrefStore> pref_store_;
  ManagedModePolicyProvider provider_;

  // Owned by the ManagedModePolicyProvider.
  MockChangeProcessor* sync_processor_;
};

TEST_F(ManagedModePolicyProviderAPITest, UploadItem) {
  UploadSplitItem("foo", "bar");
  UploadSplitItem("blurp", "baz");
  UploadAtomicItem("hurdle");

  // Uploading should produce changes when we start syncing.
  StartSyncing(syncer::SyncDataList());
  const syncer::SyncChangeList& changes = sync_processor_->changes();
  ASSERT_EQ(3u, changes.size());
  for (syncer::SyncChangeList::const_iterator it = changes.begin();
       it != changes.end(); ++it) {
    ASSERT_TRUE(it->IsValid());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, it->change_type());
    VerifySyncDataItem(it->sync_data());
  }

  // It should also show up in local Sync data.
  syncer::SyncDataList sync_data =
      provider_.GetAllSyncData(syncer::MANAGED_USER_SETTINGS);
  EXPECT_EQ(3u, sync_data.size());
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end(); ++it) {
    VerifySyncDataItem(*it);
  }

  // Uploading after we have started syncing should work too.
  UploadSplitItem("froodle", "narf");
  ASSERT_EQ(1u, sync_processor_->changes().size());
  syncer::SyncChange change = sync_processor_->changes()[0];
  ASSERT_TRUE(change.IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change.change_type());
  VerifySyncDataItem(change.sync_data());

  sync_data = provider_.GetAllSyncData(syncer::MANAGED_USER_SETTINGS);
  EXPECT_EQ(4u, sync_data.size());
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end(); ++it) {
    VerifySyncDataItem(*it);
  }

  // Uploading an item with a previously seen key should create an UPDATE
  // action.
  UploadSplitItem("blurp", "snarl");
  ASSERT_EQ(1u, sync_processor_->changes().size());
  change = sync_processor_->changes()[0];
  ASSERT_TRUE(change.IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change.change_type());
  VerifySyncDataItem(change.sync_data());

  sync_data = provider_.GetAllSyncData(syncer::MANAGED_USER_SETTINGS);
  EXPECT_EQ(4u, sync_data.size());
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end(); ++it) {
    VerifySyncDataItem(*it);
  }

  UploadAtomicItem("fjord");
  ASSERT_EQ(1u, sync_processor_->changes().size());
  change = sync_processor_->changes()[0];
  ASSERT_TRUE(change.IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change.change_type());
  VerifySyncDataItem(change.sync_data());

  sync_data = provider_.GetAllSyncData(syncer::MANAGED_USER_SETTINGS);
  EXPECT_EQ(4u, sync_data.size());
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end(); ++it) {
    VerifySyncDataItem(*it);
  }

  // The uploaded items should not show up as policies.
  PolicyBundle empty_bundle;
  EXPECT_TRUE(provider_.policies().Equals(empty_bundle));

  // Restarting sync should not create any new changes.
  provider_.StopSyncing(syncer::MANAGED_USER_SETTINGS);
  StartSyncing(sync_data);
  ASSERT_EQ(0u, sync_processor_->changes().size());
}

const char kPolicyKey[] = "TestingPolicy";

TEST_F(ManagedModePolicyProviderAPITest, SetLocalPolicy) {
  PolicyBundle empty_bundle;
  EXPECT_TRUE(provider_.policies().Equals(empty_bundle));

  base::StringValue policy_value("PolicyValue");
  provider_.SetLocalPolicyForTesting(
      kPolicyKey, scoped_ptr<base::Value>(policy_value.DeepCopy()));

  PolicyBundle expected_bundle;
  PolicyMap* policy_map = &expected_bundle.Get(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_map->Set(kPolicyKey,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  policy_value.DeepCopy(),
                  NULL);
  EXPECT_TRUE(provider_.policies().Equals(expected_bundle));
}

}  // namespace policy
