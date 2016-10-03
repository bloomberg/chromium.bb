// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
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
#include "components/sync/api/fake_model_type_service.h"

using browser_sync::ChromeSyncClient;
using browser_sync::ProfileSyncComponentsFactoryImpl;
using syncer::ConflictResolution;
using syncer::FakeModelTypeService;
using syncer::ModelTypeService;
using syncer::SharedModelTypeProcessor;

const char kKey1[] = "key1";
const char kKey2[] = "key2";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";

// A ChromeSyncClient that provides a ModelTypeService for PREFERENCES.
class TestSyncClient : public ChromeSyncClient {
 public:
  TestSyncClient(Profile* profile, ModelTypeService* service)
      : ChromeSyncClient(profile), service_(service) {}

  base::WeakPtr<ModelTypeService> GetModelTypeServiceForType(
      syncer::ModelType type) override {
    return type == syncer::PREFERENCES
               ? service_->AsWeakPtr()
               : ChromeSyncClient::GetModelTypeServiceForType(type);
  }

 private:
  ModelTypeService* const service_;
};

// A FakeModelTypeService that supports observing ApplySyncChanges.
class TestModelTypeService : public FakeModelTypeService {
 public:
  class Observer {
   public:
    virtual void OnApplySyncChanges() = 0;
  };

  TestModelTypeService()
      : FakeModelTypeService(
            base::Bind(&SharedModelTypeProcessor::CreateAsChangeProcessor)) {}

  syncer::SyncError ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_changes,
      syncer::EntityChangeList entity_changes) override {
    syncer::SyncError error = FakeModelTypeService::ApplySyncChanges(
        std::move(metadata_changes), entity_changes);
    NotifyObservers();
    return error;
  }

  void OnChangeProcessorSet() override {
    change_processor()->OnMetadataLoaded(syncer::SyncError(),
                                         db().CreateMetadataBatch());
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
// TestModelTypeService::Store.
class KeyChecker : public StatusChangeChecker,
                   public TestModelTypeService::Observer {
 public:
  KeyChecker(TestModelTypeService* service, const std::string& key)
      : service_(service), key_(key) {
    service_->AddObserver(this);
  }

  ~KeyChecker() override { service_->RemoveObserver(this); }

  void OnApplySyncChanges() override { CheckExitCondition(); }

 protected:
  TestModelTypeService* const service_;
  const std::string key_;
};

// Wait for data for a key to have a certain value.
class DataChecker : public KeyChecker {
 public:
  DataChecker(TestModelTypeService* service,
              const std::string& key,
              const std::string& value)
      : KeyChecker(service, key), value_(value) {}

  bool IsExitConditionSatisfied() override {
    const auto& db = service_->db();
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
  DataAbsentChecker(TestModelTypeService* service, const std::string& key)
      : KeyChecker(service, key) {}

  bool IsExitConditionSatisfied() override {
    return !service_->db().HasData(key_);
  }

  std::string GetDebugMessage() const override {
    return "Waiting for data for key '" + key_ + "' to be absent.";
  }
};

// Wait for metadata for a key to be present.
class MetadataPresentChecker : public KeyChecker {
 public:
  MetadataPresentChecker(TestModelTypeService* service, const std::string& key)
      : KeyChecker(service, key) {}

  bool IsExitConditionSatisfied() override {
    return service_->db().HasMetadata(key_);
  }

  std::string GetDebugMessage() const override {
    return "Waiting for metadata for key '" + key_ + "' to be present.";
  }
};

// Wait for metadata for a key to be absent.
class MetadataAbsentChecker : public KeyChecker {
 public:
  MetadataAbsentChecker(TestModelTypeService* service, const std::string& key)
      : KeyChecker(service, key) {}

  bool IsExitConditionSatisfied() override {
    return !service_->db().HasMetadata(key_);
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
  }

  ~TwoClientUssSyncTest() override {
    ProfileSyncServiceFactory::SetSyncClientFactoryForTest(nullptr);
    ProfileSyncComponentsFactoryImpl::OverridePrefsForUssTest(false);
  }

  bool TestUsesSelfNotifications() override { return false; }

  TestModelTypeService* GetModelTypeService(int i) {
    return services_.at(i).get();
  }

 protected:
  std::unique_ptr<syncer::SyncClient> CreateSyncClient(Profile* profile) {
    if (!first_client_ignored_) {
      // The test infra creates a profile before the two made for sync tests.
      first_client_ignored_ = true;
      return base::MakeUnique<ChromeSyncClient>(profile);
    }
    auto service = base::MakeUnique<TestModelTypeService>();
    auto client = base::MakeUnique<TestSyncClient>(profile, service.get());
    clients_.push_back(client.get());
    services_.push_back(std::move(service));
    return std::move(client);
  }

  ProfileSyncServiceFactory::SyncClientFactory sync_client_factory_;
  std::vector<std::unique_ptr<TestModelTypeService>> services_;
  std::vector<TestSyncClient*> clients_;
  bool first_client_ignored_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientUssSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(2U, clients_.size());
  ASSERT_EQ(2U, services_.size());
  TestModelTypeService* model1 = GetModelTypeService(0);
  TestModelTypeService* model2 = GetModelTypeService(1);

  // Add an entity.
  model1->WriteItem(kKey1, kValue1);
  ASSERT_TRUE(DataChecker(model2, kKey1, kValue1).Wait());

  // Update an entity.
  model1->WriteItem(kKey1, kValue2);
  ASSERT_TRUE(DataChecker(model2, kKey1, kValue2).Wait());

  // Delete an entity.
  model1->DeleteItem(kKey1);
  ASSERT_TRUE(DataAbsentChecker(model2, kKey1).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, DisableEnable) {
  ASSERT_TRUE(SetupSync());
  TestModelTypeService* model1 = GetModelTypeService(0);
  TestModelTypeService* model2 = GetModelTypeService(1);

  // Add an entity to test with.
  model1->WriteItem(kKey1, kValue1);
  ASSERT_TRUE(DataChecker(model2, kKey1, kValue1).Wait());
  ASSERT_EQ(1U, model1->db().data_count());
  ASSERT_EQ(1U, model1->db().metadata_count());
  ASSERT_EQ(1U, model2->db().data_count());
  ASSERT_EQ(1U, model2->db().metadata_count());

  // Disable PREFERENCES.
  syncer::ModelTypeSet types = syncer::UserSelectableTypes();
  types.Remove(syncer::PREFERENCES);
  GetSyncService(0)->OnUserChoseDatatypes(false, types);

  // Wait for it to take effect and remove the metadata.
  ASSERT_TRUE(MetadataAbsentChecker(model1, kKey1).Wait());
  ASSERT_EQ(1U, model1->db().data_count());
  ASSERT_EQ(0U, model1->db().metadata_count());
  // Model 2 should not be affected.
  ASSERT_EQ(1U, model2->db().data_count());
  ASSERT_EQ(1U, model2->db().metadata_count());

  // Re-enable PREFERENCES.
  GetSyncService(0)->OnUserChoseDatatypes(true, syncer::UserSelectableTypes());

  // Wait for metadata to be re-added.
  ASSERT_TRUE(MetadataPresentChecker(model1, kKey1).Wait());
  ASSERT_EQ(1U, model1->db().data_count());
  ASSERT_EQ(1U, model1->db().metadata_count());
  ASSERT_EQ(1U, model2->db().data_count());
  ASSERT_EQ(1U, model2->db().metadata_count());
}

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, ConflictResolution) {
  ASSERT_TRUE(SetupSync());
  TestModelTypeService* model1 = GetModelTypeService(0);
  TestModelTypeService* model2 = GetModelTypeService(1);
  model1->SetConflictResolution(ConflictResolution::UseNew(
      FakeModelTypeService::GenerateEntityData(kKey1, kValue3)));
  model2->SetConflictResolution(ConflictResolution::UseNew(
      FakeModelTypeService::GenerateEntityData(kKey1, kValue3)));

  // Write conflicting entities.
  model1->WriteItem(kKey1, kValue1);
  model2->WriteItem(kKey1, kValue2);

  // Wait for them to be resolved to kResolutionValue by the custom conflict
  // resolution logic in TestModelTypeService.
  ASSERT_TRUE(DataChecker(model1, kKey1, kValue3).Wait());
  ASSERT_TRUE(DataChecker(model2, kKey1, kValue3).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, Error) {
  ASSERT_TRUE(SetupSync());
  TestModelTypeService* model1 = GetModelTypeService(0);
  TestModelTypeService* model2 = GetModelTypeService(1);

  // Add an entity.
  model1->WriteItem(kKey1, kValue1);
  ASSERT_TRUE(DataChecker(model2, kKey1, kValue1).Wait());

  // Set an error in model 2 to trigger in the next GetUpdates.
  model2->SetServiceError(syncer::SyncError::DATATYPE_ERROR);
  // Write an item on model 1 to trigger a GetUpdates in model 2.
  model1->WriteItem(kKey1, kValue2);

  // The type should stop syncing but keep tracking metadata.
  ASSERT_TRUE(PrefsNotRunningChecker(GetSyncService(1)).Wait());
  ASSERT_EQ(1U, model2->db().metadata_count());
  model2->WriteItem(kKey2, kValue2);
  ASSERT_EQ(2U, model2->db().metadata_count());
}
