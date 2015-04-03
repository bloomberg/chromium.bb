// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_manager.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kOrigin[] = "https://example.com";
const char kPattern1[] = "https://example.com/a";
const char kPattern2[] = "https://example.com/b";
const char kScript1[] = "https://example.com/a/script.js";
const char kScript2[] = "https://example.com/b/script.js";
const int kRenderProcessId = 99;

void RegisterServiceWorkerCallback(bool* called,
                                   int64* store_registration_id,
                                   ServiceWorkerStatusCode status,
                                   const std::string& status_message,
                                   int64 registration_id) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
}

void UnregisterServiceWorkerCallback(bool* called,
                                     ServiceWorkerStatusCode code) {
  EXPECT_EQ(SERVICE_WORKER_OK, code);
  *called = true;
}

}  // namespace

// A BackgroundSyncManager that can simulate delaying and corrupting the
// backend. This class assumes (and verifies) that only one operation runs at a
// time.
class TestBackgroundSyncManager : public BackgroundSyncManager {
 public:
  explicit TestBackgroundSyncManager(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
      : BackgroundSyncManager(service_worker_context) {}

  void DoInit() { Init(); }

  void StoreDataInBackendContinue(
      int64 sw_registration_id,
      const GURL& origin,
      const std::string& key,
      const std::string& data,
      const ServiceWorkerStorage::StatusCallback& callback) {
    BackgroundSyncManager::StoreDataInBackend(sw_registration_id, origin, key,
                                              data, callback);
  }

  void GetDataFromBackendContinue(
      const std::string& key,
      const ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback&
          callback) {
    BackgroundSyncManager::GetDataFromBackend(key, callback);
  }

  void Continue() {
    continuation_.Run();
    continuation_.Reset();
  }

  void set_corrupt_backend(bool corrupt_backend) {
    corrupt_backend_ = corrupt_backend;
  }
  void set_delay_backend(bool delay_backend) { delay_backend_ = delay_backend; }

 protected:
  void StoreDataInBackend(
      int64 sw_registration_id,
      const GURL& origin,
      const std::string& key,
      const std::string& data,
      const ServiceWorkerStorage::StatusCallback& callback) override {
    EXPECT_TRUE(continuation_.is_null());
    if (corrupt_backend_) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
      return;
    }
    continuation_ =
        base::Bind(&TestBackgroundSyncManager::StoreDataInBackendContinue,
                   base::Unretained(this), sw_registration_id, origin, key,
                   data, callback);
    if (delay_backend_)
      return;

    Continue();
  }

  void GetDataFromBackend(
      const std::string& key,
      const ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback&
          callback) override {
    EXPECT_TRUE(continuation_.is_null());
    if (corrupt_backend_) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, std::vector<std::pair<int64, std::string>>(),
                     SERVICE_WORKER_ERROR_FAILED));
      return;
    }
    continuation_ =
        base::Bind(&TestBackgroundSyncManager::GetDataFromBackendContinue,
                   base::Unretained(this), key, callback);
    if (delay_backend_)
      return;

    Continue();
  }

 private:
  bool corrupt_backend_ = false;
  bool delay_backend_ = false;
  base::Closure continuation_;
};

class BackgroundSyncManagerTest : public testing::Test {
 public:
  BackgroundSyncManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        sync_reg_1_(BackgroundSyncManager::BackgroundSyncRegistration("foo")),
        sync_reg_2_(BackgroundSyncManager::BackgroundSyncRegistration("bar")),
        callback_error_(BackgroundSyncManager::ERROR_TYPE_OK),
        callback_sw_status_code_(SERVICE_WORKER_OK) {}

  void SetUp() override {
    helper_.reset(
        new EmbeddedWorkerTestHelper(base::FilePath(), kRenderProcessId));

    background_sync_manager_ =
        BackgroundSyncManager::Create(helper_->context_wrapper());

    // Wait for storage to finish initializing before registering service
    // workers.
    base::RunLoop().RunUntilIdle();

    bool called_1 = false;
    bool called_2 = false;
    helper_->context()->RegisterServiceWorker(
        GURL(kPattern1), GURL(kScript1), NULL,
        base::Bind(&RegisterServiceWorkerCallback, &called_1,
                   &sw_registration_id_1_));

    helper_->context()->RegisterServiceWorker(
        GURL(kPattern2), GURL(kScript2), NULL,
        base::Bind(&RegisterServiceWorkerCallback, &called_2,
                   &sw_registration_id_2_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called_1);
    EXPECT_TRUE(called_2);
  }

  void StatusAndRegistrationCallback(
      bool* was_called,
      BackgroundSyncManager::ErrorType error,
      const BackgroundSyncManager::BackgroundSyncRegistration& registration) {
    *was_called = true;
    callback_error_ = error;
    callback_registration_ = registration;
  }

  void StatusCallback(bool* was_called,
                      BackgroundSyncManager::ErrorType error) {
    *was_called = true;
    callback_error_ = error;
  }

 protected:
  TestBackgroundSyncManager* UseTestBackgroundSyncManager() {
    TestBackgroundSyncManager* manager =
        new TestBackgroundSyncManager(helper_->context_wrapper());
    background_sync_manager_.reset(manager);
    manager->DoInit();
    return manager;
  }

  bool Register(const BackgroundSyncManager::BackgroundSyncRegistration&
                    sync_registration) {
    return RegisterWithServiceWorkerId(sw_registration_id_1_,
                                       sync_registration);
  }

  bool RegisterWithServiceWorkerId(
      int64 sw_registration_id,
      const BackgroundSyncManager::BackgroundSyncRegistration&
          sync_registration) {
    bool was_called = false;
    background_sync_manager_->Register(
        GURL(kOrigin), sw_registration_id, sync_registration,
        base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return callback_error_ == BackgroundSyncManager::ERROR_TYPE_OK;
  }

  bool Unregister(const BackgroundSyncManager::BackgroundSyncRegistration&
                      sync_registration) {
    return UnregisterWithServiceWorkerId(sw_registration_id_1_,
                                         sync_registration);
  }

  bool UnregisterWithServiceWorkerId(
      int64 sw_registration_id,
      const BackgroundSyncManager::BackgroundSyncRegistration&
          sync_registration) {
    bool was_called = false;
    background_sync_manager_->Unregister(
        GURL(kOrigin), sw_registration_id, sync_registration.name,
        sync_registration.id,
        base::Bind(&BackgroundSyncManagerTest::StatusCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return callback_error_ == BackgroundSyncManager::ERROR_TYPE_OK;
  }

  bool GetRegistration(const std::string& sync_registration_name) {
    return GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                              sync_registration_name);
  }

  bool GetRegistrationWithServiceWorkerId(
      int64 sw_registration_id,
      const std::string& sync_registration_name) {
    bool was_called = false;
    background_sync_manager_->GetRegistration(
        GURL(kOrigin), sw_registration_id, sync_registration_name,
        base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    if (callback_error_ == BackgroundSyncManager::ERROR_TYPE_OK)
      EXPECT_TRUE(sync_registration_name == callback_registration_.name);

    return callback_error_ == BackgroundSyncManager::ERROR_TYPE_OK;
  }

  void StorageRegistrationCallback(ServiceWorkerStatusCode result) {
    callback_sw_status_code_ = result;
  }

  void UnregisterServiceWorker(uint64 sw_registration_id) {
    bool called = false;
    helper_->context()->UnregisterServiceWorker(
        PatternForSWId(sw_registration_id),
        base::Bind(&UnregisterServiceWorkerCallback, &called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
  }

  GURL PatternForSWId(int64 sw_id) {
    EXPECT_TRUE(sw_id == sw_registration_id_1_ ||
                sw_id == sw_registration_id_2_);
    return sw_id == sw_registration_id_1_ ? GURL(kPattern1) : GURL(kPattern2);
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_ptr<BackgroundSyncManager> background_sync_manager_;

  int64 sw_registration_id_1_;
  int64 sw_registration_id_2_;

  BackgroundSyncManager::BackgroundSyncRegistration sync_reg_1_;
  BackgroundSyncManager::BackgroundSyncRegistration sync_reg_2_;

  // Callback values.
  BackgroundSyncManager::ErrorType callback_error_;
  BackgroundSyncManager::BackgroundSyncRegistration callback_registration_;
  ServiceWorkerStatusCode callback_sw_status_code_;
};

TEST_F(BackgroundSyncManagerTest, Register) {
  EXPECT_TRUE(Register(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, RegistractionIntact) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_STREQ(sync_reg_1_.name.c_str(), callback_registration_.name.c_str());
  EXPECT_NE(
      BackgroundSyncManager::BackgroundSyncRegistration::kInvalidRegistrationId,
      callback_registration_.id);
}

TEST_F(BackgroundSyncManagerTest, RegisterExistingKeepsId) {
  EXPECT_TRUE(Register(sync_reg_1_));
  BackgroundSyncManager::BackgroundSyncRegistration first_registration =
      callback_registration_;
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(callback_registration_.Equals(first_registration));
  EXPECT_EQ(first_registration.id, callback_registration_.id);
}

TEST_F(BackgroundSyncManagerTest, RegisterOverwrites) {
  EXPECT_TRUE(Register(sync_reg_1_));
  BackgroundSyncManager::BackgroundSyncRegistration first_registration =
      callback_registration_;

  sync_reg_1_.min_period = 100;
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_LT(first_registration.id, callback_registration_.id);
  EXPECT_FALSE(callback_registration_.Equals(first_registration));
}

TEST_F(BackgroundSyncManagerTest, RegisterBadBackend) {
  TestBackgroundSyncManager* manager = UseTestBackgroundSyncManager();
  manager->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_reg_1_));
  manager->set_corrupt_backend(false);
  EXPECT_FALSE(Register(sync_reg_1_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
}

TEST_F(BackgroundSyncManagerTest, TwoRegistrations) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationNonExisting) {
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationExisting) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(GetRegistration(sync_reg_1_.name));
  EXPECT_FALSE(GetRegistration(sync_reg_2_.name));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationBadBackend) {
  TestBackgroundSyncManager* manager = UseTestBackgroundSyncManager();
  EXPECT_TRUE(Register(sync_reg_1_));
  manager->set_corrupt_backend(true);
  EXPECT_TRUE(GetRegistration(sync_reg_1_.name));
  EXPECT_FALSE(Register(sync_reg_2_));
  // Registration should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
  manager->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
}

TEST_F(BackgroundSyncManagerTest, Unregister) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Unregister(callback_registration_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
}

TEST_F(BackgroundSyncManagerTest, UnregisterWrongId) {
  EXPECT_TRUE(Register(sync_reg_1_));
  callback_registration_.id += 1;
  EXPECT_FALSE(Unregister(callback_registration_));
}

TEST_F(BackgroundSyncManagerTest, Reregister) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Unregister(callback_registration_));
  EXPECT_TRUE(Register(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterNonExisting) {
  EXPECT_FALSE(Unregister(sync_reg_1_));
  EXPECT_EQ(BackgroundSyncManager::ERROR_TYPE_NOT_FOUND, callback_error_);
}

TEST_F(BackgroundSyncManagerTest, UnregisterSecond) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));
  EXPECT_TRUE(Unregister(callback_registration_));
  EXPECT_TRUE(GetRegistration(sync_reg_1_.name));
  EXPECT_TRUE(Register(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterBadBackend) {
  TestBackgroundSyncManager* manager = UseTestBackgroundSyncManager();
  sync_reg_1_.min_period += 1;
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));
  manager->set_corrupt_backend(true);
  EXPECT_FALSE(Unregister(callback_registration_));
  // Unregister should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  manager->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
  EXPECT_FALSE(GetRegistration(sync_reg_2_.name));
}

TEST_F(BackgroundSyncManagerTest, RegistrationIncreasesId) {
  EXPECT_TRUE(Register(sync_reg_1_));
  BackgroundSyncManager::BackgroundSyncRegistration registered_sync =
      callback_registration_;
  BackgroundSyncManager::BackgroundSyncRegistration::RegistrationId cur_id =
      callback_registration_.id;

  EXPECT_TRUE(GetRegistration(sync_reg_1_.name));
  EXPECT_TRUE(Register(sync_reg_2_));
  EXPECT_LT(cur_id, callback_registration_.id);
  cur_id = callback_registration_.id;

  EXPECT_TRUE(Unregister(registered_sync));
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_LT(cur_id, callback_registration_.id);
}

TEST_F(BackgroundSyncManagerTest, RebootRecovery) {
  EXPECT_TRUE(Register(sync_reg_1_));

  background_sync_manager_ =
      BackgroundSyncManager::Create(helper_->context_wrapper());

  EXPECT_TRUE(GetRegistration(sync_reg_1_.name));
  EXPECT_FALSE(GetRegistration(sync_reg_2_.name));
}

TEST_F(BackgroundSyncManagerTest, RebootRecoveryTwoServiceWorkers) {
  EXPECT_TRUE(RegisterWithServiceWorkerId(sw_registration_id_1_, sync_reg_1_));
  EXPECT_TRUE(RegisterWithServiceWorkerId(sw_registration_id_2_, sync_reg_2_));

  background_sync_manager_ =
      BackgroundSyncManager::Create(helper_->context_wrapper());

  EXPECT_TRUE(GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                                 sync_reg_1_.name));
  EXPECT_FALSE(GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                                  sync_reg_2_.name));
  EXPECT_FALSE(GetRegistrationWithServiceWorkerId(sw_registration_id_2_,
                                                  sync_reg_1_.name));
  EXPECT_TRUE(GetRegistrationWithServiceWorkerId(sw_registration_id_2_,
                                                 sync_reg_2_.name));

  EXPECT_TRUE(GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                                 sync_reg_1_.name));
  EXPECT_TRUE(GetRegistrationWithServiceWorkerId(sw_registration_id_2_,
                                                 sync_reg_2_.name));

  EXPECT_TRUE(RegisterWithServiceWorkerId(sw_registration_id_1_, sync_reg_2_));
  EXPECT_TRUE(RegisterWithServiceWorkerId(sw_registration_id_2_, sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, InitWithBadBackend) {
  TestBackgroundSyncManager* manager =
      new TestBackgroundSyncManager(helper_->context_wrapper());
  background_sync_manager_.reset(manager);
  manager->set_corrupt_backend(true);
  manager->DoInit();

  EXPECT_FALSE(Register(sync_reg_1_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
}

TEST_F(BackgroundSyncManagerTest, SequentialOperations) {
  // Schedule Init and all of the operations on a delayed backend. Verify that
  // the operations complete sequentially.
  TestBackgroundSyncManager* manager =
      new TestBackgroundSyncManager(helper_->context_wrapper());
  background_sync_manager_.reset(manager);
  manager->set_delay_backend(true);
  manager->DoInit();

  const int64 kExpectedInitialId =
      BackgroundSyncManager::BackgroundSyncRegistrations::kInitialId;

  bool register_called = false;
  bool unregister_called = false;
  bool get_registration_called = false;
  manager->Register(
      GURL(kOrigin), sw_registration_id_1_, sync_reg_1_,
      base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                 base::Unretained(this), &register_called));
  manager->Unregister(GURL(kOrigin), sw_registration_id_1_, sync_reg_1_.name,
                      kExpectedInitialId,
                      base::Bind(&BackgroundSyncManagerTest::StatusCallback,
                                 base::Unretained(this), &unregister_called));
  manager->GetRegistration(
      GURL(kOrigin), sw_registration_id_1_, sync_reg_1_.name,
      base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                 base::Unretained(this), &get_registration_called));

  base::RunLoop().RunUntilIdle();
  // Init should be blocked while loading from the backend.
  EXPECT_FALSE(register_called);
  EXPECT_FALSE(unregister_called);
  EXPECT_FALSE(get_registration_called);

  manager->Continue();
  base::RunLoop().RunUntilIdle();
  // Register should be blocked while storing to the backend.
  EXPECT_FALSE(register_called);
  EXPECT_FALSE(unregister_called);
  EXPECT_FALSE(get_registration_called);

  manager->Continue();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(register_called);
  EXPECT_EQ(kExpectedInitialId, callback_registration_.id);
  EXPECT_EQ(BackgroundSyncManager::ERROR_TYPE_OK, callback_error_);
  // Unregister should be blocked while storing to the backend.
  EXPECT_FALSE(unregister_called);
  EXPECT_FALSE(get_registration_called);

  manager->Continue();
  base::RunLoop().RunUntilIdle();
  // Unregister should be done and since GetRegistration doesn't require the
  // backend it should be done too.
  EXPECT_EQ(BackgroundSyncManager::ERROR_TYPE_NOT_FOUND, callback_error_);
  EXPECT_TRUE(unregister_called);
  EXPECT_TRUE(get_registration_called);
}

TEST_F(BackgroundSyncManagerTest, UnregisterServiceWorker) {
  EXPECT_TRUE(Register(sync_reg_1_));
  UnregisterServiceWorker(sw_registration_id_1_);
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
}

TEST_F(BackgroundSyncManagerTest,
       UnregisterServiceWorkerDuringSyncRegistration) {
  TestBackgroundSyncManager* manager =
      new TestBackgroundSyncManager(helper_->context_wrapper());
  background_sync_manager_.reset(manager);
  manager->DoInit();

  EXPECT_TRUE(Register(sync_reg_1_));

  manager->set_delay_backend(true);
  bool callback_called = false;
  manager->Register(
      GURL(kOrigin), sw_registration_id_1_, sync_reg_2_,
      base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                 base::Unretained(this), &callback_called));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_called);
  UnregisterServiceWorker(sw_registration_id_1_);

  manager->Continue();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(BackgroundSyncManager::ERROR_TYPE_STORAGE, callback_error_);

  manager->set_delay_backend(false);
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
}

TEST_F(BackgroundSyncManagerTest, DeleteAndStartOverServiceWorkerContext) {
  EXPECT_TRUE(Register(sync_reg_1_));
  helper_->context()->ScheduleDeleteAndStartOver();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
}

TEST_F(BackgroundSyncManagerTest, DisabledManagerWorksAfterBrowserRestart) {
  TestBackgroundSyncManager* manager =
      new TestBackgroundSyncManager(helper_->context_wrapper());
  background_sync_manager_.reset(manager);
  manager->DoInit();
  EXPECT_TRUE(Register(sync_reg_1_));
  manager->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_reg_2_));

  // The manager is now disabled and not accepting new requests until browser
  // restart or notification that the storage has been wiped.
  manager->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
  EXPECT_FALSE(Register(sync_reg_2_));

  // Simulate restarting the browser by creating a new BackgroundSyncManager.
  background_sync_manager_.reset(
      new TestBackgroundSyncManager(helper_->context_wrapper()));
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
  EXPECT_TRUE(Register(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, DisabledManagerWorksAfterDeleteAndStartOver) {
  TestBackgroundSyncManager* manager =
      new TestBackgroundSyncManager(helper_->context_wrapper());
  background_sync_manager_.reset(manager);
  manager->DoInit();
  EXPECT_TRUE(Register(sync_reg_1_));
  manager->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_reg_2_));

  // The manager is now disabled and not accepting new requests until browser
  // restart or notification that the storage has been wiped.
  manager->set_corrupt_backend(false);
  helper_->context()->ScheduleDeleteAndStartOver();
  base::RunLoop().RunUntilIdle();

  bool called = false;
  helper_->context()->RegisterServiceWorker(
      GURL(kPattern1), GURL(kScript1), NULL,
      base::Bind(&RegisterServiceWorkerCallback, &called,
                 &sw_registration_id_1_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  EXPECT_TRUE(Register(sync_reg_2_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_.name));
  EXPECT_TRUE(GetRegistration(sync_reg_2_.name));
}

}  // namespace content
