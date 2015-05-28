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
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

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

void FindServiceWorkerRegistrationCallback(
    scoped_refptr<ServiceWorkerRegistration>* out_registration,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *out_registration = registration;
}

void UnregisterServiceWorkerCallback(bool* called,
                                     ServiceWorkerStatusCode code) {
  EXPECT_EQ(SERVICE_WORKER_OK, code);
  *called = true;
}

void OneShotSuccessfulCallback(
    int* count,
    const scoped_refptr<ServiceWorkerVersion>& active_version,
    const ServiceWorkerVersion::StatusCallback& callback) {
  *count += 1;
  callback.Run(SERVICE_WORKER_OK);
}

void OneShotFailedCallback(
    int* count,
    const scoped_refptr<ServiceWorkerVersion>& active_version,
    const ServiceWorkerVersion::StatusCallback& callback) {
  *count += 1;
  callback.Run(SERVICE_WORKER_ERROR_FAILED);
}

void OneShotDelayedCallback(
    int* count,
    ServiceWorkerVersion::StatusCallback* out_callback,
    const scoped_refptr<ServiceWorkerVersion>& active_version,
    const ServiceWorkerVersion::StatusCallback& callback) {
  *count += 1;
  *out_callback = callback;
}

}  // namespace

// A BackgroundSyncManager that can simulate delaying and corrupting the backend
// storage and service worker onsync events.
class TestBackgroundSyncManager : public BackgroundSyncManager {
 public:
  using OneShotCallback =
      base::Callback<void(const scoped_refptr<ServiceWorkerVersion>&,
                          const ServiceWorkerVersion::StatusCallback&)>;

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
  void set_one_shot_callback(const OneShotCallback& callback) {
    one_shot_callback_ = callback;
  }

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

  void FireOneShotSync(
      const scoped_refptr<ServiceWorkerVersion>& active_version,
      const ServiceWorkerVersion::StatusCallback& callback) override {
    if (one_shot_callback_.is_null()) {
      BackgroundSyncManager::FireOneShotSync(active_version, callback);
    } else {
      one_shot_callback_.Run(active_version, callback);
    }
  }

 private:
  bool corrupt_backend_ = false;
  bool delay_backend_ = false;
  base::Closure continuation_;
  OneShotCallback one_shot_callback_;
};

class BackgroundSyncManagerTest : public testing::Test {
 public:
  BackgroundSyncManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        network_change_notifier_(net::NetworkChangeNotifier::CreateMock()),
        test_background_sync_manager_(nullptr),
        sync_reg_1_(BackgroundSyncManager::BackgroundSyncRegistration()),
        sync_reg_2_(BackgroundSyncManager::BackgroundSyncRegistration()),
        callback_error_(BackgroundSyncManager::ERROR_TYPE_OK),
        callback_sw_status_code_(SERVICE_WORKER_OK),
        sync_events_called_(0) {
    sync_reg_1_.tag = "foo";
    sync_reg_1_.periodicity = SYNC_ONE_SHOT;
    sync_reg_1_.network_state = NETWORK_STATE_ONLINE;
    sync_reg_1_.power_state = POWER_STATE_AUTO;

    sync_reg_2_.tag = "bar";
    sync_reg_2_.periodicity = SYNC_ONE_SHOT;
    sync_reg_2_.network_state = NETWORK_STATE_ONLINE;
    sync_reg_2_.power_state = POWER_STATE_AUTO;
  }

  void SetUp() override {
    SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);

    helper_.reset(
        new EmbeddedWorkerTestHelper(base::FilePath(), kRenderProcessId));

    background_sync_manager_ =
        BackgroundSyncManager::Create(helper_->context_wrapper());

    // Wait for storage to finish initializing before registering service
    // workers.
    base::RunLoop().RunUntilIdle();
    RegisterServiceWorkers();
  }

  void RegisterServiceWorkers() {
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

    // Hang onto the registrations as they need to be "live" when
    // calling BackgroundSyncMasnager::Register.
    helper_->context_wrapper()->FindRegistrationForId(
        sw_registration_id_1_, GURL(kPattern1).GetOrigin(),
        base::Bind(FindServiceWorkerRegistrationCallback, &sw_registration_1_));

    helper_->context_wrapper()->FindRegistrationForId(
        sw_registration_id_2_, GURL(kPattern1).GetOrigin(),
        base::Bind(FindServiceWorkerRegistrationCallback, &sw_registration_2_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(sw_registration_1_);
    EXPECT_TRUE(sw_registration_2_);
  }

  void SetNetwork(net::NetworkChangeNotifier::ConnectionType connection_type) {
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        connection_type);
    base::RunLoop().RunUntilIdle();
  }

  void StatusAndRegistrationCallback(
      bool* was_called,
      BackgroundSyncManager::ErrorType error,
      const BackgroundSyncManager::BackgroundSyncRegistration& registration) {
    *was_called = true;
    callback_error_ = error;
    callback_registration_ = registration;
  }

  void StatusAndRegistrationsCallback(
      bool* was_called,
      BackgroundSyncManager::ErrorType error,
      const std::vector<BackgroundSyncManager::BackgroundSyncRegistration>&
          registrations) {
    *was_called = true;
    callback_error_ = error;
    callback_registrations_ = registrations;
  }

  void StatusCallback(bool* was_called,
                      BackgroundSyncManager::ErrorType error) {
    *was_called = true;
    callback_error_ = error;
  }

 protected:
  void UseTestBackgroundSyncManager() {
    test_background_sync_manager_ =
        new TestBackgroundSyncManager(helper_->context_wrapper());
    test_background_sync_manager_->DoInit();
    background_sync_manager_.reset(test_background_sync_manager_);
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
        sw_registration_id, sync_registration,
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
        sw_registration_id, sync_registration.tag,
        sync_registration.periodicity, sync_registration.id,
        base::Bind(&BackgroundSyncManagerTest::StatusCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return callback_error_ == BackgroundSyncManager::ERROR_TYPE_OK;
  }

  bool GetRegistration(const BackgroundSyncManager::BackgroundSyncRegistration&
                           sync_registration) {
    return GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                              sync_registration);
  }

  bool GetRegistrationWithServiceWorkerId(
      int64 sw_registration_id,
      const BackgroundSyncManager::BackgroundSyncRegistration&
          sync_registration) {
    bool was_called = false;
    background_sync_manager_->GetRegistration(
        sw_registration_id, sync_registration.tag,
        sync_registration.periodicity,
        base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    if (callback_error_ == BackgroundSyncManager::ERROR_TYPE_OK) {
      EXPECT_STREQ(sync_registration.tag.c_str(),
                   callback_registration_.tag.c_str());
    }

    return callback_error_ == BackgroundSyncManager::ERROR_TYPE_OK;
  }

  bool GetRegistrations(SyncPeriodicity periodicity) {
    return GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                              periodicity);
  }

  bool GetRegistrationWithServiceWorkerId(int64 sw_registration_id,
                                          SyncPeriodicity periodicity) {
    bool was_called = false;
    background_sync_manager_->GetRegistrations(
        sw_registration_id, periodicity,
        base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationsCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

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

  void InitSyncEventTest() {
    UseTestBackgroundSyncManager();
    test_background_sync_manager_->set_one_shot_callback(
        base::Bind(OneShotSuccessfulCallback, &sync_events_called_));
    base::RunLoop().RunUntilIdle();
  }

  void InitFailedSyncEventTest() {
    UseTestBackgroundSyncManager();
    test_background_sync_manager_->set_one_shot_callback(
        base::Bind(OneShotFailedCallback, &sync_events_called_));
    base::RunLoop().RunUntilIdle();
  }

  void InitDelayedSyncEventTest() {
    UseTestBackgroundSyncManager();
    test_background_sync_manager_->set_one_shot_callback(base::Bind(
        OneShotDelayedCallback, &sync_events_called_, &sync_fired_callback_));
    base::RunLoop().RunUntilIdle();
  }

  void RegisterAndVerifySyncEventDelayed(
      const BackgroundSyncManager::BackgroundSyncRegistration&
          sync_registration) {
    int sync_events_called = sync_events_called_;
    EXPECT_TRUE(sync_fired_callback_.is_null());

    EXPECT_TRUE(Register(sync_registration));

    EXPECT_EQ(sync_events_called + 1, sync_events_called_);
    EXPECT_TRUE(GetRegistration(sync_reg_1_));
    EXPECT_FALSE(sync_fired_callback_.is_null());
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_ptr<BackgroundSyncManager> background_sync_manager_;
  TestBackgroundSyncManager* test_background_sync_manager_;

  int64 sw_registration_id_1_;
  int64 sw_registration_id_2_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_1_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_2_;

  BackgroundSyncManager::BackgroundSyncRegistration sync_reg_1_;
  BackgroundSyncManager::BackgroundSyncRegistration sync_reg_2_;

  // Callback values.
  BackgroundSyncManager::ErrorType callback_error_;
  BackgroundSyncManager::BackgroundSyncRegistration callback_registration_;
  std::vector<BackgroundSyncManager::BackgroundSyncRegistration>
      callback_registrations_;
  ServiceWorkerStatusCode callback_sw_status_code_;
  int sync_events_called_;
  ServiceWorkerVersion::StatusCallback sync_fired_callback_;
};

TEST_F(BackgroundSyncManagerTest, Register) {
  EXPECT_TRUE(Register(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, RegistractionIntact) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_STREQ(sync_reg_1_.tag.c_str(), callback_registration_.tag.c_str());
  EXPECT_NE(
      BackgroundSyncManager::BackgroundSyncRegistration::kInvalidRegistrationId,
      callback_registration_.id);
}

TEST_F(BackgroundSyncManagerTest, RegisterWithoutLiveSWRegistration) {
  sw_registration_1_ = nullptr;
  EXPECT_FALSE(Register(sync_reg_1_));
  EXPECT_EQ(BackgroundSyncManager::ERROR_TYPE_NO_SERVICE_WORKER,
            callback_error_);
}

TEST_F(BackgroundSyncManagerTest, RegisterWithoutActiveSWRegistration) {
  sw_registration_1_->UnsetVersion(sw_registration_1_->active_version());
  EXPECT_FALSE(Register(sync_reg_1_));
  EXPECT_EQ(BackgroundSyncManager::ERROR_TYPE_NO_SERVICE_WORKER,
            callback_error_);
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

TEST_F(BackgroundSyncManagerTest, RegisterOverlappingPeriodicAndOneShotTags) {
  // Registrations with the same tags but different periodicities should not
  // collide.
  sync_reg_1_.tag = "";
  sync_reg_2_.tag = "";
  sync_reg_1_.periodicity = SYNC_PERIODIC;
  sync_reg_2_.periodicity = SYNC_ONE_SHOT;
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));
  EXPECT_TRUE(GetRegistration(sync_reg_1_));
  EXPECT_EQ(SYNC_PERIODIC, callback_registration_.periodicity);
  EXPECT_TRUE(GetRegistration(sync_reg_2_));
  EXPECT_EQ(SYNC_ONE_SHOT, callback_registration_.periodicity);
}

TEST_F(BackgroundSyncManagerTest, RegisterBadBackend) {
  UseTestBackgroundSyncManager();
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_reg_1_));
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(Register(sync_reg_1_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, TwoRegistrations) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationNonExisting) {
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationExisting) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(GetRegistration(sync_reg_1_));
  EXPECT_FALSE(GetRegistration(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationBadBackend) {
  UseTestBackgroundSyncManager();
  EXPECT_TRUE(Register(sync_reg_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_TRUE(GetRegistration(sync_reg_1_));
  EXPECT_FALSE(Register(sync_reg_2_));
  // Registration should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsZero) {
  EXPECT_TRUE(GetRegistrations(SYNC_ONE_SHOT));
  EXPECT_EQ(0u, callback_registrations_.size());
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsOne) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(GetRegistrations(sync_reg_1_.periodicity));

  EXPECT_EQ(1u, callback_registrations_.size());
  sync_reg_1_.Equals(callback_registrations_[0]);
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsTwo) {
  EXPECT_EQ(sync_reg_1_.periodicity, sync_reg_2_.periodicity);

  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));
  EXPECT_TRUE(GetRegistrations(sync_reg_1_.periodicity));

  EXPECT_EQ(2u, callback_registrations_.size());
  sync_reg_1_.Equals(callback_registrations_[0]);
  sync_reg_2_.Equals(callback_registrations_[1]);
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsPeriodicity) {
  sync_reg_1_.periodicity = SYNC_ONE_SHOT;
  sync_reg_2_.periodicity = SYNC_PERIODIC;
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));

  EXPECT_TRUE(GetRegistrations(SYNC_ONE_SHOT));
  EXPECT_EQ(1u, callback_registrations_.size());
  sync_reg_1_.Equals(callback_registrations_[0]);

  EXPECT_TRUE(GetRegistrations(SYNC_PERIODIC));
  EXPECT_EQ(1u, callback_registrations_.size());
  sync_reg_2_.Equals(callback_registrations_[0]);
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsBadBackend) {
  UseTestBackgroundSyncManager();
  EXPECT_TRUE(Register(sync_reg_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_TRUE(GetRegistrations(sync_reg_1_.periodicity));
  EXPECT_FALSE(Register(sync_reg_2_));
  // Registration should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  EXPECT_FALSE(GetRegistrations(sync_reg_1_.periodicity));
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistrations(sync_reg_1_.periodicity));
}

TEST_F(BackgroundSyncManagerTest, Unregister) {
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Unregister(callback_registration_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
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
  EXPECT_TRUE(GetRegistration(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterBadBackend) {
  UseTestBackgroundSyncManager();
  sync_reg_1_.min_period += 1;
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Unregister(callback_registration_));
  // Unregister should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
  EXPECT_FALSE(GetRegistration(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, RegistrationIncreasesId) {
  EXPECT_TRUE(Register(sync_reg_1_));
  BackgroundSyncManager::BackgroundSyncRegistration registered_sync =
      callback_registration_;
  BackgroundSyncManager::BackgroundSyncRegistration::RegistrationId cur_id =
      callback_registration_.id;

  EXPECT_TRUE(GetRegistration(sync_reg_1_));
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

  EXPECT_TRUE(GetRegistration(sync_reg_1_));
  EXPECT_FALSE(GetRegistration(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, RebootRecoveryTwoServiceWorkers) {
  EXPECT_TRUE(RegisterWithServiceWorkerId(sw_registration_id_1_, sync_reg_1_));
  EXPECT_TRUE(RegisterWithServiceWorkerId(sw_registration_id_2_, sync_reg_2_));

  background_sync_manager_ =
      BackgroundSyncManager::Create(helper_->context_wrapper());

  EXPECT_TRUE(
      GetRegistrationWithServiceWorkerId(sw_registration_id_1_, sync_reg_1_));
  EXPECT_FALSE(
      GetRegistrationWithServiceWorkerId(sw_registration_id_1_, sync_reg_2_));
  EXPECT_FALSE(
      GetRegistrationWithServiceWorkerId(sw_registration_id_2_, sync_reg_1_));
  EXPECT_TRUE(
      GetRegistrationWithServiceWorkerId(sw_registration_id_2_, sync_reg_2_));

  EXPECT_TRUE(
      GetRegistrationWithServiceWorkerId(sw_registration_id_1_, sync_reg_1_));
  EXPECT_TRUE(
      GetRegistrationWithServiceWorkerId(sw_registration_id_2_, sync_reg_2_));

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
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
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
      BackgroundSyncManager::BackgroundSyncRegistration::kInitialId;

  bool register_called = false;
  bool unregister_called = false;
  bool get_registration_called = false;
  manager->Register(
      sw_registration_id_1_, sync_reg_1_,
      base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                 base::Unretained(this), &register_called));
  manager->Unregister(sw_registration_id_1_, sync_reg_1_.tag,
                      sync_reg_1_.periodicity, kExpectedInitialId,
                      base::Bind(&BackgroundSyncManagerTest::StatusCallback,
                                 base::Unretained(this), &unregister_called));
  manager->GetRegistration(
      sw_registration_id_1_, sync_reg_1_.tag, sync_reg_1_.periodicity,
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
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
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
      sw_registration_id_1_, sync_reg_2_,
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
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, DeleteAndStartOverServiceWorkerContext) {
  EXPECT_TRUE(Register(sync_reg_1_));
  helper_->context()->ScheduleDeleteAndStartOver();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
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
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
  EXPECT_FALSE(Register(sync_reg_2_));

  // Simulate restarting the browser by creating a new BackgroundSyncManager.
  background_sync_manager_.reset(
      new TestBackgroundSyncManager(helper_->context_wrapper()));
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
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

  RegisterServiceWorkers();

  EXPECT_TRUE(Register(sync_reg_2_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
  EXPECT_TRUE(GetRegistration(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsId) {
  BackgroundSyncManager::BackgroundSyncRegistration reg_1;
  BackgroundSyncManager::BackgroundSyncRegistration reg_2;

  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_2.id = reg_1.id + 1;
  EXPECT_TRUE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsTag) {
  BackgroundSyncManager::BackgroundSyncRegistration reg_1;
  BackgroundSyncManager::BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_2.tag = "bar";
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsPeriodicity) {
  BackgroundSyncManager::BackgroundSyncRegistration reg_1;
  BackgroundSyncManager::BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_1.periodicity = SYNC_PERIODIC;
  reg_2.periodicity = SYNC_ONE_SHOT;
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsMinPeriod) {
  BackgroundSyncManager::BackgroundSyncRegistration reg_1;
  BackgroundSyncManager::BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_2.min_period = reg_1.min_period + 1;
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsNetworkState) {
  BackgroundSyncManager::BackgroundSyncRegistration reg_1;
  BackgroundSyncManager::BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_1.network_state = NETWORK_STATE_ANY;
  reg_2.network_state = NETWORK_STATE_ONLINE;
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsPowerState) {
  BackgroundSyncManager::BackgroundSyncRegistration reg_1;
  BackgroundSyncManager::BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_1.power_state = POWER_STATE_AUTO;
  reg_2.power_state = POWER_STATE_AVOID_DRAINING;
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, StoreAndRetrievePreservesValues) {
  BackgroundSyncManager::BackgroundSyncRegistration reg_1;
  // Set non-default values for each field.
  reg_1.tag = "foo";
  EXPECT_NE(SYNC_PERIODIC, reg_1.periodicity);
  reg_1.periodicity = SYNC_PERIODIC;
  reg_1.min_period += 1;
  EXPECT_NE(NETWORK_STATE_ANY, reg_1.network_state);
  reg_1.network_state = NETWORK_STATE_ANY;
  EXPECT_NE(POWER_STATE_AUTO, reg_1.power_state);
  reg_1.power_state = POWER_STATE_AUTO;

  // Store the registration.
  EXPECT_TRUE(Register(reg_1));

  // Simulate restarting the sync manager, forcing the next read to come from
  // disk.
  UseTestBackgroundSyncManager();

  EXPECT_TRUE(GetRegistration(reg_1));
  EXPECT_TRUE(reg_1.Equals(callback_registration_));
}

TEST_F(BackgroundSyncManagerTest, EmptyTagSupported) {
  sync_reg_1_.tag = "a";
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(GetRegistration(sync_reg_1_));
  EXPECT_TRUE(sync_reg_1_.Equals(callback_registration_));
  EXPECT_TRUE(Unregister(callback_registration_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, OverlappingPeriodicAndOneShotTags) {
  // Registrations with the same tags but different periodicities should not
  // collide.
  sync_reg_1_.tag = "";
  sync_reg_2_.tag = "";
  sync_reg_1_.periodicity = SYNC_PERIODIC;
  sync_reg_2_.periodicity = SYNC_ONE_SHOT;

  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));

  EXPECT_TRUE(GetRegistration(sync_reg_1_));
  EXPECT_EQ(SYNC_PERIODIC, callback_registration_.periodicity);
  EXPECT_TRUE(GetRegistration(sync_reg_2_));
  EXPECT_EQ(SYNC_ONE_SHOT, callback_registration_.periodicity);

  EXPECT_TRUE(GetRegistration(sync_reg_1_));
  EXPECT_TRUE(Unregister(callback_registration_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
  EXPECT_TRUE(GetRegistration(sync_reg_2_));
  EXPECT_EQ(SYNC_ONE_SHOT, callback_registration_.periodicity);

  EXPECT_TRUE(Unregister(callback_registration_));
  EXPECT_FALSE(GetRegistration(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, OneShotFiresOnRegistration) {
  InitSyncEventTest();

  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, OneShotFiresOnNetworkChange) {
  InitSyncEventTest();

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_reg_1_));

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, MultipleOneShotsFireOnNetworkChange) {
  InitSyncEventTest();

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_TRUE(Register(sync_reg_2_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_reg_1_));
  EXPECT_TRUE(GetRegistration(sync_reg_2_));

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
  EXPECT_FALSE(GetRegistration(sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, OneShotFiresOnManagerRestart) {
  InitSyncEventTest();

  // Initially the event won't run because there is no network.
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_reg_1_));

  // Simulate closing the browser.
  background_sync_manager_.reset();

  // The next time the manager is started, the network is good.
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);
  InitSyncEventTest();

  // The event should have fired.
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, FailedOneShotStillExists) {
  InitFailedSyncEventTest();

  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_reg_1_));

  // The failed one-shot should stay registered but not fire until the
  // ServiceWorker is reloaded with an active client. Therefore, changing the
  // network should not cause the event to run again.
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_2G);
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, DelayOneShotMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_reg_1_);

  // Finish firing the event and verify that the registration is removed.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, OverwriteRegistrationMidSync) {
  InitDelayedSyncEventTest();

  sync_reg_1_.network_state = NETWORK_STATE_ANY;
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);

  RegisterAndVerifySyncEventDelayed(sync_reg_1_);

  // Don't delay the next sync.
  test_background_sync_manager_->set_one_shot_callback(
      base::Bind(OneShotSuccessfulCallback, &sync_events_called_));

  // Register a different sync event with the same tag, overwriting the first.
  sync_reg_1_.network_state = NETWORK_STATE_ONLINE;
  EXPECT_TRUE(Register(sync_reg_1_));

  // The new sync event won't run as the network requirements aren't met.
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_reg_1_));

  // Finish the first event, note that the second is still registered.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_reg_1_));

  // Change the network and the second should run.
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, ReregisterOneShotMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_reg_1_);

  // Register the same sync, but don't delay it. It shouldn't run as it's
  // already firing.
  test_background_sync_manager_->set_one_shot_callback(
      base::Bind(OneShotSuccessfulCallback, &sync_events_called_));
  EXPECT_TRUE(Register(sync_reg_1_));
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_reg_1_));

  // Finish the original event, note that the second never runs.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterOneShotMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_reg_1_);

  EXPECT_TRUE(Unregister(callback_registration_));
  EXPECT_FALSE(GetRegistration(sync_reg_1_));

  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, BadBackendMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_reg_1_);

  test_background_sync_manager_->set_corrupt_backend(true);
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();

  // The backend should now be disabled because it couldn't unregister the
  // one-shot.
  EXPECT_FALSE(Register(sync_reg_2_));
  EXPECT_FALSE(RegisterWithServiceWorkerId(sw_registration_id_2_, sync_reg_2_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterServiceWorkerMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_reg_1_);
  UnregisterServiceWorker(sw_registration_id_1_);

  sync_fired_callback_.Run(SERVICE_WORKER_OK);

  // The backend isn't disabled, but the first service worker registration is
  // gone.
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
  EXPECT_FALSE(Register(sync_reg_1_));
  EXPECT_TRUE(RegisterWithServiceWorkerId(sw_registration_id_2_, sync_reg_1_));
}

TEST_F(BackgroundSyncManagerTest, KillManagerMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_reg_1_);

  // Create a new manager which should fire the sync again on init.
  InitSyncEventTest();
  EXPECT_FALSE(GetRegistration(sync_reg_1_));
  EXPECT_EQ(2, sync_events_called_);
}

}  // namespace content
