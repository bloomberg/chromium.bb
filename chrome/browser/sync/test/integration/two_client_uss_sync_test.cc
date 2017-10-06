// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/profile_sync_components_factory_impl.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/model/fake_model_type_sync_bridge.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"

using browser_sync::ChromeSyncClient;
using browser_sync::ProfileSyncComponentsFactoryImpl;
using syncer::ConflictResolution;
using syncer::FakeModelTypeSyncBridge;
using syncer::ModelTypeChangeProcessor;
using syncer::ModelTypeSyncBridge;

namespace {

const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kKey3[] = "key3";
const char kKey4[] = "key4";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";
const char* kPassphrase = "12345";

// A ChromeSyncClient that provides a ModelTypeSyncBridge for PREFERENCES.
class TestSyncClient : public ChromeSyncClient {
 public:
  TestSyncClient(Profile* profile, ModelTypeSyncBridge* bridge)
      : ChromeSyncClient(profile), bridge_(bridge) {}

  base::WeakPtr<ModelTypeSyncBridge> GetSyncBridgeForModelType(
      syncer::ModelType type) override {
    return type == syncer::PREFERENCES
               ? bridge_->AsWeakPtr()
               : ChromeSyncClient::GetSyncBridgeForModelType(type);
  }

 private:
  ModelTypeSyncBridge* const bridge_;
};

// A FakeModelTypeSyncBridge that supports observing ApplySyncChanges.
class TestModelTypeSyncBridge : public FakeModelTypeSyncBridge {
 public:
  class Observer {
   public:
    virtual void OnApplySyncChanges() = 0;
  };

  TestModelTypeSyncBridge()
      : FakeModelTypeSyncBridge(base::Bind(&ModelTypeChangeProcessor::Create,
                                           base::RepeatingClosure())) {
    change_processor()->ModelReadyToSync(db().CreateMetadataBatch());
  }

  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_changes,
      syncer::EntityChangeList entity_changes) override {
    auto error = FakeModelTypeSyncBridge::ApplySyncChanges(
        std::move(metadata_changes), entity_changes);
    NotifyObservers();
    return error;
  }

  void AddObserver(Observer* observer) { observers_.insert(observer); }
  void RemoveObserver(Observer* observer) { observers_.erase(observer); }

 private:
  void NotifyObservers() {
    for (Observer* observer : observers_) {
      observer->OnApplySyncChanges();
    }
  }

  std::set<Observer*> observers_;
};

// A StatusChangeChecker for checking the status of keys in a
// TestModelTypeSyncBridge::Store.
class KeyChecker : public StatusChangeChecker,
                   public TestModelTypeSyncBridge::Observer {
 public:
  KeyChecker(TestModelTypeSyncBridge* bridge, const std::string& key)
      : bridge_(bridge), key_(key) {
    bridge_->AddObserver(this);
  }

  ~KeyChecker() override { bridge_->RemoveObserver(this); }

  void OnApplySyncChanges() override { CheckExitCondition(); }

 protected:
  TestModelTypeSyncBridge* const bridge_;
  const std::string key_;
};

// Wait for data for a key to have a certain value.
class DataChecker : public KeyChecker {
 public:
  DataChecker(TestModelTypeSyncBridge* bridge,
              const std::string& key,
              const std::string& value)
      : KeyChecker(bridge, key), value_(value) {}

  bool IsExitConditionSatisfied() override {
    const auto& db = bridge_->db();
    return db.HasData(key_) && db.GetValue(key_) == value_;
  }

  std::string GetDebugMessage() const override {
    return "Waiting for data for key '" + key_ + "' to be '" + value_ + "'.";
  }

 private:
  const std::string value_;
};

// Wait for data for a key to be absent.
class DataAbsentChecker : public KeyChecker {
 public:
  DataAbsentChecker(TestModelTypeSyncBridge* bridge, const std::string& key)
      : KeyChecker(bridge, key) {}

  bool IsExitConditionSatisfied() override {
    return !bridge_->db().HasData(key_);
  }

  std::string GetDebugMessage() const override {
    return "Waiting for data for key '" + key_ + "' to be absent.";
  }
};

// Wait for metadata for a key to be present.
class MetadataPresentChecker : public KeyChecker {
 public:
  MetadataPresentChecker(TestModelTypeSyncBridge* bridge,
                         const std::string& key)
      : KeyChecker(bridge, key) {}

  bool IsExitConditionSatisfied() override {
    return bridge_->db().HasMetadata(key_);
  }

  std::string GetDebugMessage() const override {
    return "Waiting for metadata for key '" + key_ + "' to be present.";
  }
};

// Wait for metadata for a key to be absent.
class MetadataAbsentChecker : public KeyChecker {
 public:
  MetadataAbsentChecker(TestModelTypeSyncBridge* bridge, const std::string& key)
      : KeyChecker(bridge, key) {}

  bool IsExitConditionSatisfied() override {
    return !bridge_->db().HasMetadata(key_);
  }

  std::string GetDebugMessage() const override {
    return "Waiting for metadata for key '" + key_ + "' to be absent.";
  }
};

// Wait for PREFERENCES to no longer be running.
class PrefsNotRunningChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PrefsNotRunningChecker(browser_sync::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    return !service()->IsDataTypeControllerRunning(syncer::PREFERENCES);
  }

  std::string GetDebugMessage() const override {
    return "Waiting for prefs to be not running.";
  }
};

class TwoClientUssSyncTest : public SyncTest {
 public:
  TwoClientUssSyncTest() : SyncTest(TWO_CLIENT) {
    DisableVerifier();
    sync_client_factory_ = base::Bind(&TwoClientUssSyncTest::CreateSyncClient,
                                      base::Unretained(this));
    ProfileSyncServiceFactory::SetSyncClientFactoryForTest(
        &sync_client_factory_);
    ProfileSyncComponentsFactoryImpl::OverridePrefsForUssTest(true);
    // The test infra creates a profile before the two made for sync tests.
    number_of_clients_ignored_ = 1;
#if defined(OS_CHROMEOS)
    // ChromeOS will force loading a signin profile and a lock screen apps
    // profile, so we need to ingore two more clients.
    number_of_clients_ignored_ += 2;
#endif
  }

  ~TwoClientUssSyncTest() override {
    ProfileSyncServiceFactory::SetSyncClientFactoryForTest(nullptr);
    ProfileSyncComponentsFactoryImpl::OverridePrefsForUssTest(false);
  }

  bool TestUsesSelfNotifications() override { return false; }

  TestModelTypeSyncBridge* GetModelTypeSyncBridge(int i) {
    return bridges_.at(i).get();
  }

 protected:
  std::unique_ptr<syncer::SyncClient> CreateSyncClient(Profile* profile) {
    if (number_of_clients_ignored_ > 0) {
      --number_of_clients_ignored_;
      return std::make_unique<ChromeSyncClient>(profile);
    }
    auto bridge = std::make_unique<TestModelTypeSyncBridge>();
    auto client = std::make_unique<TestSyncClient>(profile, bridge.get());
    clients_.push_back(client.get());
    bridges_.push_back(std::move(bridge));
    return std::move(client);
  }

  ProfileSyncServiceFactory::SyncClientFactory sync_client_factory_;
  std::vector<std::unique_ptr<TestModelTypeSyncBridge>> bridges_;
  std::vector<TestSyncClient*> clients_;
  size_t number_of_clients_ignored_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientUssSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(2U, clients_.size());
  ASSERT_EQ(2U, bridges_.size());
  TestModelTypeSyncBridge* model0 = GetModelTypeSyncBridge(0);
  TestModelTypeSyncBridge* model1 = GetModelTypeSyncBridge(1);

  // Add an entity.
  model0->WriteItem(kKey1, kValue1);
  ASSERT_TRUE(DataChecker(model1, kKey1, kValue1).Wait());

  // Update an entity.
  model0->WriteItem(kKey1, kValue2);
  ASSERT_TRUE(DataChecker(model1, kKey1, kValue2).Wait());

  // Delete an entity.
  model0->DeleteItem(kKey1);
  ASSERT_TRUE(DataAbsentChecker(model1, kKey1).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, DisableEnable) {
  ASSERT_TRUE(SetupSync());
  TestModelTypeSyncBridge* model0 = GetModelTypeSyncBridge(0);
  TestModelTypeSyncBridge* model1 = GetModelTypeSyncBridge(1);

  // Add an entity to test with.
  model0->WriteItem(kKey1, kValue1);
  ASSERT_TRUE(DataChecker(model1, kKey1, kValue1).Wait());
  ASSERT_EQ(1U, model0->db().data_count());
  ASSERT_EQ(1U, model0->db().metadata_count());
  ASSERT_EQ(1U, model1->db().data_count());
  ASSERT_EQ(1U, model1->db().metadata_count());

  // Disable PREFERENCES.
  syncer::ModelTypeSet types = syncer::UserSelectableTypes();
  types.Remove(syncer::PREFERENCES);
  GetSyncService(0)->OnUserChoseDatatypes(false, types);

  // Wait for it to take effect and remove the metadata.
  ASSERT_TRUE(MetadataAbsentChecker(model0, kKey1).Wait());
  ASSERT_EQ(1U, model0->db().data_count());
  ASSERT_EQ(0U, model0->db().metadata_count());
  // Model 1 should not be affected.
  ASSERT_EQ(1U, model1->db().data_count());
  ASSERT_EQ(1U, model1->db().metadata_count());

  // Re-enable PREFERENCES.
  GetSyncService(0)->OnUserChoseDatatypes(true, syncer::UserSelectableTypes());

  // Wait for metadata to be re-added.
  ASSERT_TRUE(MetadataPresentChecker(model0, kKey1).Wait());
  ASSERT_EQ(1U, model0->db().data_count());
  ASSERT_EQ(1U, model0->db().metadata_count());
  ASSERT_EQ(1U, model1->db().data_count());
  ASSERT_EQ(1U, model1->db().metadata_count());
}

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, ConflictResolution) {
  ASSERT_TRUE(SetupSync());
  TestModelTypeSyncBridge* model0 = GetModelTypeSyncBridge(0);
  TestModelTypeSyncBridge* model1 = GetModelTypeSyncBridge(1);
  model0->SetConflictResolution(ConflictResolution::UseNew(
      FakeModelTypeSyncBridge::GenerateEntityData(kKey1, kValue3)));
  model1->SetConflictResolution(ConflictResolution::UseNew(
      FakeModelTypeSyncBridge::GenerateEntityData(kKey1, kValue3)));

  // Write conflicting entities.
  model0->WriteItem(kKey1, kValue1);
  // Wait for the server to see the first commit to avoid a race condition where
  // both clients commit without seeing each other's update.
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::PREFERENCES, 1).Wait());
  model1->WriteItem(kKey1, kValue2);

  // Wait for them to be resolved to kResolutionValue by the custom conflict
  // resolution logic in TestModelTypeSyncBridge.
  ASSERT_TRUE(DataChecker(model0, kKey1, kValue3).Wait());
  ASSERT_TRUE(DataChecker(model1, kKey1, kValue3).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, Error) {
  ASSERT_TRUE(SetupSync());
  TestModelTypeSyncBridge* model0 = GetModelTypeSyncBridge(0);
  TestModelTypeSyncBridge* model1 = GetModelTypeSyncBridge(1);

  // Add an entity.
  model0->WriteItem(kKey1, kValue1);
  ASSERT_TRUE(DataChecker(model1, kKey1, kValue1).Wait());

  // Set an error in model 1 to trigger in the next GetUpdates.
  model1->ErrorOnNextCall();
  // Write an item on model 0 to trigger a GetUpdates in model 1.
  model0->WriteItem(kKey1, kValue2);

  // The type should stop syncing but keep tracking metadata.
  ASSERT_TRUE(PrefsNotRunningChecker(GetSyncService(1)).Wait());
  ASSERT_EQ(1U, model1->db().metadata_count());
  model1->WriteItem(kKey2, kValue2);
  ASSERT_EQ(2U, model1->db().metadata_count());
}

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, Encryption) {
  ASSERT_TRUE(SetupSync());
  TestModelTypeSyncBridge* model0 = GetModelTypeSyncBridge(0);
  TestModelTypeSyncBridge* model1 = GetModelTypeSyncBridge(1);

  model0->WriteItem(kKey1, kValue1);
  ASSERT_TRUE(DataChecker(model1, kKey1, kValue1).Wait());

  GetSyncService(0)->SetEncryptionPassphrase(kPassphrase,
                                             syncer::SyncService::EXPLICIT);
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  // Wait for client 1 to know that a passphrase is happening to avoid potential
  // race conditions and make the functionality this case tests more consistent.
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(1)).Wait());

  model0->WriteItem(kKey1, kValue2);
  model0->WriteItem(kKey2, kValue1);
  model1->WriteItem(kKey3, kValue1);

  ASSERT_TRUE(GetSyncService(1)->SetDecryptionPassphrase(kPassphrase));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(1)).Wait());

  model0->WriteItem(kKey4, kValue1);

  ASSERT_TRUE(DataChecker(model1, kKey1, kValue2).Wait());
  ASSERT_TRUE(DataChecker(model1, kKey2, kValue1).Wait());
  ASSERT_TRUE(DataChecker(model1, kKey4, kValue1).Wait());

  ASSERT_TRUE(DataChecker(model0, kKey1, kValue2).Wait());
  ASSERT_TRUE(DataChecker(model0, kKey3, kValue1).Wait());
}

}  // namespace
