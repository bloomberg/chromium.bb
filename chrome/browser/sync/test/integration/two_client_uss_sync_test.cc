// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/browser/profile_sync_components_factory_impl.h"
#include "components/sync/api/fake_model_type_service.h"

using browser_sync::ChromeSyncClient;
using syncer_v2::FakeModelTypeService;
using syncer_v2::ModelTypeService;
using syncer_v2::SharedModelTypeProcessor;

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
      std::unique_ptr<syncer_v2::MetadataChangeList> metadata_changes,
      syncer_v2::EntityChangeList entity_changes) override {
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
      : service_(service), key_(key) {}
  ~KeyChecker() override {}

  void OnApplySyncChanges() override { CheckExitCondition(); }

  bool Wait() {
    if (IsExitConditionSatisfied()) {
      DVLOG(1) << "Wait() -> Exit before waiting: " << GetDebugMessage();
      return true;
    }

    service_->AddObserver(this);
    StartBlockingWait();
    service_->RemoveObserver(this);
    return !TimedOut();
  }

 protected:
  TestModelTypeService* const service_;
  const std::string key_;
};

// Wait for a key to be present.
class KeyPresentChecker : public KeyChecker {
 public:
  KeyPresentChecker(TestModelTypeService* service, const std::string& key)
      : KeyChecker(service, key) {}
  ~KeyPresentChecker() override {}

  bool IsExitConditionSatisfied() override {
    return service_->db().HasData(key_);
  }

  std::string GetDebugMessage() const override {
    return "Waiting for key '" + key_ + "' to be present.";
  }
};

// Wait for a key to be absent.
class KeyAbsentChecker : public KeyChecker {
 public:
  KeyAbsentChecker(TestModelTypeService* service, const std::string& key)
      : KeyChecker(service, key) {}
  ~KeyAbsentChecker() override {}

  bool IsExitConditionSatisfied() override {
    return !service_->db().HasData(key_);
  }

  std::string GetDebugMessage() const override {
    return "Waiting for key '" + key_ + "' to be absent.";
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
  std::unique_ptr<sync_driver::SyncClient> CreateSyncClient(Profile* profile) {
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

  DISALLOW_COPY_AND_ASSIGN(TwoClientUssSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientUssSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(2U, clients_.size());
  ASSERT_EQ(2U, services_.size());
  TestModelTypeService* model1 = GetModelTypeService(0);
  TestModelTypeService* model2 = GetModelTypeService(1);

  model1->WriteItem("foo", "bar");
  ASSERT_TRUE(KeyPresentChecker(model2, "foo").Wait());
  EXPECT_EQ("bar", model2->db().GetValue("foo"));

  model1->DeleteItem("foo");
  ASSERT_TRUE(KeyAbsentChecker(model2, "foo").Wait());
}
