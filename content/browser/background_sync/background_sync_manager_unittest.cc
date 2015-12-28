// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_manager.h"

#include <stdint.h>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/simple_test_clock.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/background_sync/background_sync_registration_handle.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kPattern1[] = "https://example.com/a";
const char kPattern2[] = "https://example.com/b";
const char kScript1[] = "https://example.com/a/script.js";
const char kScript2[] = "https://example.com/b/script.js";

void RegisterServiceWorkerCallback(bool* called,
                                   int64_t* store_registration_id,
                                   ServiceWorkerStatusCode status,
                                   const std::string& status_message,
                                   int64_t registration_id) {
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

void NotifyWhenFinishedCallback(bool* was_called,
                                BackgroundSyncStatus* out_status,
                                BackgroundSyncState* out_state,
                                BackgroundSyncStatus status,
                                BackgroundSyncState state) {
  *was_called = true;
  *out_status = status;
  *out_state = state;
}

class TestPowerSource : public base::PowerMonitorSource {
 public:
  void GeneratePowerStateEvent(bool on_battery_power) {
    test_on_battery_power_ = on_battery_power;
    ProcessPowerEvent(POWER_STATE_EVENT);
  }

 private:
  bool IsOnBatteryPowerImpl() final { return test_on_battery_power_; }
  bool test_on_battery_power_ = false;
};

class TestBackgroundSyncController : public BackgroundSyncController {
 public:
  TestBackgroundSyncController() = default;

  // BackgroundSyncController Overrides
  void NotifyBackgroundSyncRegistered(const GURL& origin) override {
    registration_count_ += 1;
    registration_origin_ = origin;
  }
  void RunInBackground(bool enabled, int64_t min_ms) override {
    run_in_background_count_ += 1;
    run_in_background_enabled_ = enabled;
    run_in_background_min_ms_ = min_ms;
  }
  void GetParameterOverrides(
      BackgroundSyncParameters* parameters) const override {
    *parameters = background_sync_parameters_;
  }

  int registration_count() const { return registration_count_; }
  GURL registration_origin() const { return registration_origin_; }
  int run_in_background_count() const { return run_in_background_count_; }
  bool run_in_background_enabled() const { return run_in_background_enabled_; }
  int64_t run_in_background_min_ms() const { return run_in_background_min_ms_; }
  BackgroundSyncParameters* background_sync_parameters() {
    return &background_sync_parameters_;
  }

 private:
  int registration_count_ = 0;
  GURL registration_origin_;

  int run_in_background_count_ = 0;
  bool run_in_background_enabled_ = true;
  int64_t run_in_background_min_ms_ = 0;
  BackgroundSyncParameters background_sync_parameters_;

  DISALLOW_COPY_AND_ASSIGN(TestBackgroundSyncController);
};

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
      : BackgroundSyncManager(service_worker_context) {
  }

  void DoInit() { Init(); }

  void StoreDataInBackendContinue(
      int64_t sw_registration_id,
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
    ASSERT_FALSE(continuation_.is_null());
    continuation_.Run();
    continuation_.Reset();
  }

  void ClearDelayedTask() { delayed_task_.Reset(); }

  void set_corrupt_backend(bool corrupt_backend) {
    corrupt_backend_ = corrupt_backend;
  }
  void set_delay_backend(bool delay_backend) { delay_backend_ = delay_backend; }
  void set_one_shot_callback(const OneShotCallback& callback) {
    one_shot_callback_ = callback;
  }

  base::Closure delayed_task() const { return delayed_task_; }
  base::TimeDelta delayed_task_delta() const { return delayed_task_delta_; }

  BackgroundSyncEventLastChance last_chance() const { return last_chance_; }

  void set_has_main_frame_provider_host(bool value) {
    has_main_frame_provider_host_ = value;
  }

  const BackgroundSyncParameters* background_sync_parameters() const {
    return parameters_.get();
  }

 protected:
  void StoreDataInBackend(
      int64_t sw_registration_id,
      const GURL& origin,
      const std::string& key,
      const std::string& data,
      const ServiceWorkerStorage::StatusCallback& callback) override {
    EXPECT_TRUE(continuation_.is_null());
    if (corrupt_backend_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
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
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(callback, std::vector<std::pair<int64_t, std::string>>(),
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
      BackgroundSyncRegistrationHandle::HandleId handle_id,
      const scoped_refptr<ServiceWorkerVersion>& active_version,
      BackgroundSyncEventLastChance last_chance,
      const ServiceWorkerVersion::StatusCallback& callback) override {
    ASSERT_FALSE(one_shot_callback_.is_null());
    last_chance_ = last_chance;
    one_shot_callback_.Run(active_version, callback);
  }

  void ScheduleDelayedTask(const base::Closure& callback,
                           base::TimeDelta delay) override {
    delayed_task_ = callback;
    delayed_task_delta_ = delay;
  }

  void HasMainFrameProviderHost(const GURL& origin,
                                const BoolCallback& callback) override {
    callback.Run(has_main_frame_provider_host_);
  }

 private:
  bool corrupt_backend_ = false;
  bool delay_backend_ = false;
  bool has_main_frame_provider_host_ = true;
  BackgroundSyncEventLastChance last_chance_ =
      BACKGROUND_SYNC_EVENT_LAST_CHANCE_IS_NOT_LAST_CHANCE;
  base::Closure continuation_;
  OneShotCallback one_shot_callback_;
  base::Closure delayed_task_;
  base::TimeDelta delayed_task_delta_;
};

class BackgroundSyncManagerTest : public testing::Test {
 public:
  BackgroundSyncManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        network_change_notifier_(net::NetworkChangeNotifier::CreateMock()) {
    sync_options_1_.tag = "foo";
    sync_options_1_.periodicity = SYNC_ONE_SHOT;
    sync_options_1_.network_state = NETWORK_STATE_ONLINE;
    sync_options_1_.power_state = POWER_STATE_AUTO;

    sync_options_2_.tag = "bar";
    sync_options_2_.periodicity = SYNC_ONE_SHOT;
    sync_options_2_.network_state = NETWORK_STATE_ONLINE;
    sync_options_2_.power_state = POWER_STATE_AUTO;
  }

  void SetUp() override {
    // Don't let the tests be confused by the real-world device connectivity
    background_sync_test_util::SetIgnoreNetworkChangeNotifier(true);

    // TODO(jkarlin): Create a new object with all of the necessary SW calls
    // so that we can inject test versions instead of bringing up all of this
    // extra SW stuff.
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));

    // Create a StoragePartition with the correct BrowserContext so that the
    // BackgroundSyncManager can find the BrowserContext through it.
    storage_partition_impl_.reset(new StoragePartitionImpl(
        helper_->browser_context(), base::FilePath(), nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr));
    helper_->context_wrapper()->set_storage_partition(
        storage_partition_impl_.get());

    power_monitor_source_ = new TestPowerSource();
    // power_monitor_ takes ownership of power_monitor_source.
    power_monitor_.reset(new base::PowerMonitor(
        scoped_ptr<base::PowerMonitorSource>(power_monitor_source_)));

    SetOnBatteryPower(false);

    scoped_ptr<TestBackgroundSyncController> background_sync_controller(
        new TestBackgroundSyncController());
    test_controller_ = background_sync_controller.get();
    helper_->browser_context()->SetBackgroundSyncController(
        std::move(background_sync_controller));

    SetMaxSyncAttemptsAndRestartManager(1);

    // Wait for storage to finish initializing before registering service
    // workers.
    base::RunLoop().RunUntilIdle();
    RegisterServiceWorkers();
  }

  void TearDown() override {
    // Restore the network observer functionality for subsequent tests
    background_sync_test_util::SetIgnoreNetworkChangeNotifier(false);
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
    // calling BackgroundSyncManager::Register.
    helper_->context_wrapper()->FindReadyRegistrationForId(
        sw_registration_id_1_, GURL(kPattern1).GetOrigin(),
        base::Bind(FindServiceWorkerRegistrationCallback, &sw_registration_1_));

    helper_->context_wrapper()->FindReadyRegistrationForId(
        sw_registration_id_2_, GURL(kPattern1).GetOrigin(),
        base::Bind(FindServiceWorkerRegistrationCallback, &sw_registration_2_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(sw_registration_1_);
    EXPECT_TRUE(sw_registration_2_);
  }

  void SetNetwork(net::NetworkChangeNotifier::ConnectionType connection_type) {
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        connection_type);
    if (test_background_sync_manager_) {
      BackgroundSyncNetworkObserver* network_observer =
          test_background_sync_manager_->GetNetworkObserverForTesting();
      network_observer->NotifyManagerIfNetworkChangedForTesting(
          connection_type);
      base::RunLoop().RunUntilIdle();
    }
  }

  void SetOnBatteryPower(bool on_battery_power) {
    power_monitor_source_->GeneratePowerStateEvent(on_battery_power);
    base::RunLoop().RunUntilIdle();
  }

  void StatusAndRegistrationCallback(
      bool* was_called,
      BackgroundSyncStatus status,
      scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle) {
    *was_called = true;
    callback_status_ = status;
    callback_registration_handle_ = std::move(registration_handle);
  }

  void StatusAndRegistrationsCallback(
      bool* was_called,
      BackgroundSyncStatus status,
      scoped_ptr<ScopedVector<BackgroundSyncRegistrationHandle>>
          registration_handles) {
    *was_called = true;
    callback_status_ = status;
    callback_registration_handles_ = std::move(registration_handles);
  }

  void StatusCallback(bool* was_called, BackgroundSyncStatus status) {
    *was_called = true;
    callback_status_ = status;
  }

 protected:
  void CreateBackgroundSyncManager() {
    ClearRegistrationHandles();

    test_background_sync_manager_ =
        new TestBackgroundSyncManager(helper_->context_wrapper());
    background_sync_manager_.reset(test_background_sync_manager_);

    test_clock_ = new base::SimpleTestClock();
    background_sync_manager_->set_clock(make_scoped_ptr(test_clock_));

    // Many tests do not expect the sync event to fire immediately after
    // register (and cleanup up the sync registrations).  Tests can control when
    // the sync event fires by manipulating the network state as needed.
    // NOTE: The setup of the network connection must happen after the
    //       BackgroundSyncManager has been created.
    SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  }

  void InitBackgroundSyncManager() {
    test_background_sync_manager_->DoInit();
    base::RunLoop().RunUntilIdle();
  }

  // Clear the registrations so that the BackgroundSyncManager can release them.
  void ClearRegistrationHandles() {
    callback_registration_handle_.reset();
    callback_registration_handles_.reset();
  }

  void SetupBackgroundSyncManager() {
    CreateBackgroundSyncManager();
    InitBackgroundSyncManager();
  }

  void SetupCorruptBackgroundSyncManager() {
    CreateBackgroundSyncManager();
    test_background_sync_manager_->set_corrupt_backend(true);
    InitBackgroundSyncManager();
  }

  void SetupDelayedBackgroundSyncManager() {
    CreateBackgroundSyncManager();
    test_background_sync_manager_->set_delay_backend(true);
    InitBackgroundSyncManager();
  }

  void DeleteBackgroundSyncManager() {
    ClearRegistrationHandles();
    background_sync_manager_.reset();
    test_background_sync_manager_ = nullptr;
    test_clock_ = nullptr;
  }

  bool Register(const BackgroundSyncRegistrationOptions& sync_options) {
    return RegisterWithServiceWorkerId(sw_registration_id_1_, sync_options);
  }

  bool RegisterWithServiceWorkerId(
      int64_t sw_registration_id,
      const BackgroundSyncRegistrationOptions& options) {
    bool was_called = false;
    background_sync_manager_->Register(
        sw_registration_id, options, true /* requested_from_service_worker */,
        base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return callback_status_ == BACKGROUND_SYNC_STATUS_OK;
  }

  bool RegisterFromDocumentWithServiceWorkerId(
      int64_t sw_registration_id,
      const BackgroundSyncRegistrationOptions& options) {
    bool was_called = false;
    background_sync_manager_->Register(
        sw_registration_id, options, false /* requested_from_service_worker */,
        base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return callback_status_ == BACKGROUND_SYNC_STATUS_OK;
  }

  bool Unregister(BackgroundSyncRegistrationHandle* registration_handle) {
    return UnregisterWithServiceWorkerId(sw_registration_id_1_,
                                         registration_handle);
  }

  bool UnregisterWithServiceWorkerId(
      int64_t sw_registration_id,
      BackgroundSyncRegistrationHandle* registration_handle) {
    bool was_called = false;
    registration_handle->Unregister(
        sw_registration_id,
        base::Bind(&BackgroundSyncManagerTest::StatusCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return callback_status_ == BACKGROUND_SYNC_STATUS_OK;
  }

  bool NotifyWhenFinished(
      BackgroundSyncRegistrationHandle* registration_handle) {
    callback_finished_called_ = false;
    callback_finished_status_ = BACKGROUND_SYNC_STATUS_NOT_FOUND;
    callback_finished_state_ = BACKGROUND_SYNC_STATE_FAILED;

    registration_handle->NotifyWhenFinished(
        base::Bind(&NotifyWhenFinishedCallback, &callback_finished_called_,
                   &callback_finished_status_, &callback_finished_state_));
    base::RunLoop().RunUntilIdle();

    if (callback_finished_called_)
      EXPECT_EQ(BACKGROUND_SYNC_STATUS_OK, callback_finished_status_);

    return callback_finished_called_;
  }

  BackgroundSyncState FinishedState() {
    EXPECT_TRUE(callback_finished_called_);
    EXPECT_EQ(BACKGROUND_SYNC_STATUS_OK, callback_finished_status_);
    return callback_finished_state_;
  }

  bool GetRegistration(
      const BackgroundSyncRegistrationOptions& registration_options) {
    return GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                              registration_options);
  }

  bool GetRegistrationWithServiceWorkerId(
      int64_t sw_registration_id,
      const BackgroundSyncRegistrationOptions& registration_options) {
    bool was_called = false;
    background_sync_manager_->GetRegistration(
        sw_registration_id, registration_options.tag,
        registration_options.periodicity,
        base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    if (callback_status_ == BACKGROUND_SYNC_STATUS_OK) {
      EXPECT_STREQ(registration_options.tag.c_str(),
                   callback_registration_handle_->options()->tag.c_str());
    }

    return callback_status_ == BACKGROUND_SYNC_STATUS_OK;
  }

  bool GetRegistrations(SyncPeriodicity periodicity) {
    return GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                              periodicity);
  }

  bool GetRegistrationWithServiceWorkerId(int64_t sw_registration_id,
                                          SyncPeriodicity periodicity) {
    bool was_called = false;
    background_sync_manager_->GetRegistrations(
        sw_registration_id, periodicity,
        base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationsCallback,
                   base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    return callback_status_ == BACKGROUND_SYNC_STATUS_OK;
  }

  void StorageRegistrationCallback(ServiceWorkerStatusCode result) {
    callback_sw_status_code_ = result;
  }

  void UnregisterServiceWorker(uint64_t sw_registration_id) {
    bool called = false;
    helper_->context()->UnregisterServiceWorker(
        PatternForSWId(sw_registration_id),
        base::Bind(&UnregisterServiceWorkerCallback, &called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
  }

  GURL PatternForSWId(int64_t sw_id) {
    EXPECT_TRUE(sw_id == sw_registration_id_1_ ||
                sw_id == sw_registration_id_2_);
    return sw_id == sw_registration_id_1_ ? GURL(kPattern1) : GURL(kPattern2);
  }

  void SetupForSyncEvent(
      const TestBackgroundSyncManager::OneShotCallback& callback) {
    test_background_sync_manager_->set_one_shot_callback(callback);
    SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);
  }

  void InitSyncEventTest() {
    SetupForSyncEvent(
        base::Bind(OneShotSuccessfulCallback, &sync_events_called_));
  }

  void InitFailedSyncEventTest() {
    SetupForSyncEvent(base::Bind(OneShotFailedCallback, &sync_events_called_));
  }

  void InitDelayedSyncEventTest() {
    SetupForSyncEvent(base::Bind(OneShotDelayedCallback, &sync_events_called_,
                                 &sync_fired_callback_));
  }

  void RegisterAndVerifySyncEventDelayed(
      const BackgroundSyncRegistrationOptions& sync_options) {
    int sync_events_called = sync_events_called_;
    EXPECT_TRUE(sync_fired_callback_.is_null());

    EXPECT_TRUE(Register(sync_options));

    EXPECT_EQ(sync_events_called + 1, sync_events_called_);
    EXPECT_TRUE(GetRegistration(sync_options_1_));
    EXPECT_FALSE(sync_fired_callback_.is_null());
  }

  void DeleteServiceWorkerAndStartOver() {
    helper_->context()->ScheduleDeleteAndStartOver();
    base::RunLoop().RunUntilIdle();
  }

  int MaxTagLength() const { return BackgroundSyncManager::kMaxTagLength; }

  void SetMaxSyncAttemptsAndRestartManager(int max_sync_attempts) {
    BackgroundSyncParameters* parameters =
        test_controller_->background_sync_parameters();
    parameters->max_sync_attempts = max_sync_attempts;

    // Restart the BackgroundSyncManager so that it updates its parameters.
    SetupBackgroundSyncManager();
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  TestPowerSource* power_monitor_source_ = nullptr;  // owned by power_monitor_
  scoped_ptr<base::PowerMonitor> power_monitor_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_ptr<BackgroundSyncManager> background_sync_manager_;
  scoped_ptr<StoragePartitionImpl> storage_partition_impl_;
  TestBackgroundSyncManager* test_background_sync_manager_ = nullptr;
  TestBackgroundSyncController* test_controller_;
  base::SimpleTestClock* test_clock_ = nullptr;

  int64_t sw_registration_id_1_;
  int64_t sw_registration_id_2_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_1_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_2_;

  BackgroundSyncRegistrationOptions sync_options_1_;
  BackgroundSyncRegistrationOptions sync_options_2_;

  // Callback values.
  BackgroundSyncStatus callback_status_ = BACKGROUND_SYNC_STATUS_OK;
  scoped_ptr<BackgroundSyncRegistrationHandle> callback_registration_handle_;
  scoped_ptr<ScopedVector<BackgroundSyncRegistrationHandle>>
      callback_registration_handles_;
  ServiceWorkerStatusCode callback_sw_status_code_ = SERVICE_WORKER_OK;
  bool callback_finished_called_ = false;
  BackgroundSyncStatus callback_finished_status_ =
      BACKGROUND_SYNC_STATUS_NOT_FOUND;
  BackgroundSyncState callback_finished_state_ = BACKGROUND_SYNC_STATE_FAILED;
  int sync_events_called_ = 0;
  ServiceWorkerVersion::StatusCallback sync_fired_callback_;
};

TEST_F(BackgroundSyncManagerTest, Register) {
  EXPECT_TRUE(Register(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, RegistractionIntact) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_STREQ(sync_options_1_.tag.c_str(),
               callback_registration_handle_->options()->tag.c_str());
  EXPECT_TRUE(callback_registration_handle_->IsValid());
}

TEST_F(BackgroundSyncManagerTest, RegisterWithoutLiveSWRegistration) {
  sw_registration_1_ = nullptr;
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER, callback_status_);
}

TEST_F(BackgroundSyncManagerTest, RegisterWithoutActiveSWRegistration) {
  sw_registration_1_->UnsetVersion(sw_registration_1_->active_version());
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER, callback_status_);
}

TEST_F(BackgroundSyncManagerTest, RegisterOverwrites) {
  EXPECT_TRUE(Register(sync_options_1_));
  scoped_ptr<BackgroundSyncRegistrationHandle> first_registration_handle =
      std::move(callback_registration_handle_);

  sync_options_1_.min_period = 100;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_LT(first_registration_handle->handle_id(),
            callback_registration_handle_->handle_id());
  EXPECT_FALSE(first_registration_handle->options()->Equals(
      *callback_registration_handle_->options()));
}

TEST_F(BackgroundSyncManagerTest, RegisterOverlappingPeriodicAndOneShotTags) {
  // Registrations with the same tags but different periodicities should not
  // collide.
  sync_options_1_.tag = "";
  sync_options_2_.tag = "";
  sync_options_1_.periodicity = SYNC_PERIODIC;
  sync_options_2_.periodicity = SYNC_ONE_SHOT;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_EQ(SYNC_PERIODIC,
            callback_registration_handle_->options()->periodicity);
  EXPECT_TRUE(GetRegistration(sync_options_2_));
  EXPECT_EQ(SYNC_ONE_SHOT,
            callback_registration_handle_->options()->periodicity);
}

TEST_F(BackgroundSyncManagerTest, RegisterBadBackend) {
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DuplicateRegistrationHandle) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(
      sync_options_1_.Equals(*callback_registration_handle_->options()));

  scoped_ptr<BackgroundSyncRegistrationHandle> dup_handle =
      background_sync_manager_->DuplicateRegistrationHandle(
          callback_registration_handle_->handle_id());

  EXPECT_TRUE(sync_options_1_.Equals(*dup_handle->options()));
  EXPECT_NE(callback_registration_handle_->handle_id(),
            dup_handle->handle_id());
}

TEST_F(BackgroundSyncManagerTest, TwoRegistrations) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationNonExisting) {
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationExisting) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationBadBackend) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(Register(sync_options_2_));
  // Registration should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsZero) {
  EXPECT_TRUE(GetRegistrations(SYNC_ONE_SHOT));
  EXPECT_EQ(0u, callback_registration_handles_->size());
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsOne) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistrations(sync_options_1_.periodicity));

  EXPECT_EQ(1u, callback_registration_handles_->size());
  sync_options_1_.Equals(*(*callback_registration_handles_)[0]->options());
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsTwo) {
  EXPECT_EQ(sync_options_1_.periodicity, sync_options_2_.periodicity);

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(GetRegistrations(sync_options_1_.periodicity));

  EXPECT_EQ(2u, callback_registration_handles_->size());
  sync_options_1_.Equals(*(*callback_registration_handles_)[0]->options());
  sync_options_2_.Equals(*(*callback_registration_handles_)[1]->options());
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsPeriodicity) {
  sync_options_1_.periodicity = SYNC_ONE_SHOT;
  sync_options_2_.periodicity = SYNC_PERIODIC;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));

  EXPECT_TRUE(GetRegistrations(SYNC_ONE_SHOT));
  EXPECT_EQ(1u, callback_registration_handles_->size());
  sync_options_1_.Equals(*(*callback_registration_handles_)[0]->options());

  EXPECT_TRUE(GetRegistrations(SYNC_PERIODIC));
  EXPECT_EQ(1u, callback_registration_handles_->size());
  sync_options_2_.Equals(*(*callback_registration_handles_)[0]->options());
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsBadBackend) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_TRUE(GetRegistrations(sync_options_1_.periodicity));
  EXPECT_FALSE(Register(sync_options_2_));
  // Registration should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  EXPECT_FALSE(GetRegistrations(sync_options_1_.periodicity));
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistrations(sync_options_1_.periodicity));
}

TEST_F(BackgroundSyncManagerTest, Unregister) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, Reregister) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_TRUE(Register(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterSecond) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterBadBackend) {
  sync_options_1_.min_period += 1;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Unregister(callback_registration_handle_.get()));
  // Unregister should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, RegisterMaxTagLength) {
  sync_options_1_.tag = std::string(MaxTagLength(), 'a');
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));

  sync_options_1_.tag = std::string(MaxTagLength() + 1, 'a');
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_NOT_ALLOWED, callback_status_);
}

TEST_F(BackgroundSyncManagerTest, RegistrationIncreasesId) {
  EXPECT_TRUE(Register(sync_options_1_));
  scoped_ptr<BackgroundSyncRegistrationHandle> registered_handle =
      std::move(callback_registration_handle_);
  BackgroundSyncRegistration::RegistrationId cur_id =
      registered_handle->handle_id();

  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_LT(cur_id, callback_registration_handle_->handle_id());
  cur_id = callback_registration_handle_->handle_id();

  EXPECT_TRUE(Unregister(registered_handle.get()));
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_LT(cur_id, callback_registration_handle_->handle_id());
}

TEST_F(BackgroundSyncManagerTest, RebootRecovery) {
  EXPECT_TRUE(Register(sync_options_1_));

  SetupBackgroundSyncManager();

  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, RebootRecoveryTwoServiceWorkers) {
  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_1_, sync_options_1_));
  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_2_, sync_options_2_));

  SetupBackgroundSyncManager();

  EXPECT_TRUE(GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                                 sync_options_1_));
  EXPECT_FALSE(GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                                  sync_options_2_));
  EXPECT_FALSE(GetRegistrationWithServiceWorkerId(sw_registration_id_2_,
                                                  sync_options_1_));
  EXPECT_TRUE(GetRegistrationWithServiceWorkerId(sw_registration_id_2_,
                                                 sync_options_2_));

  EXPECT_TRUE(GetRegistrationWithServiceWorkerId(sw_registration_id_1_,
                                                 sync_options_1_));
  EXPECT_TRUE(GetRegistrationWithServiceWorkerId(sw_registration_id_2_,
                                                 sync_options_2_));

  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_1_, sync_options_2_));
  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_2_, sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, InitWithBadBackend) {
  SetupCorruptBackgroundSyncManager();

  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, SequentialOperations) {
  // Schedule Init and all of the operations on a delayed backend. Verify that
  // the operations complete sequentially.
  SetupDelayedBackgroundSyncManager();

  bool register_called = false;
  bool get_registration_called = false;
  test_background_sync_manager_->Register(
      sw_registration_id_1_, sync_options_1_,
      true /* requested_from_service_worker */,
      base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                 base::Unretained(this), &register_called));
  test_background_sync_manager_->GetRegistration(
      sw_registration_id_1_, sync_options_1_.tag, sync_options_1_.periodicity,
      base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                 base::Unretained(this), &get_registration_called));

  base::RunLoop().RunUntilIdle();
  // Init should be blocked while loading from the backend.
  EXPECT_FALSE(register_called);
  EXPECT_FALSE(get_registration_called);

  test_background_sync_manager_->Continue();
  base::RunLoop().RunUntilIdle();
  // Register should be blocked while storing to the backend.
  EXPECT_FALSE(register_called);
  EXPECT_FALSE(get_registration_called);

  test_background_sync_manager_->Continue();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(register_called);
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_OK, callback_status_);
  // GetRegistration should run immediately as it doesn't write to disk.
  EXPECT_TRUE(get_registration_called);
}

TEST_F(BackgroundSyncManagerTest, UnregisterServiceWorker) {
  EXPECT_TRUE(Register(sync_options_1_));
  UnregisterServiceWorker(sw_registration_id_1_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest,
       UnregisterServiceWorkerDuringSyncRegistration) {
  EXPECT_TRUE(Register(sync_options_1_));

  test_background_sync_manager_->set_delay_backend(true);
  bool callback_called = false;
  test_background_sync_manager_->Register(
      sw_registration_id_1_, sync_options_2_,
      true /* requested_from_service_worker */,
      base::Bind(&BackgroundSyncManagerTest::StatusAndRegistrationCallback,
                 base::Unretained(this), &callback_called));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_called);
  UnregisterServiceWorker(sw_registration_id_1_);

  test_background_sync_manager_->Continue();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_STORAGE_ERROR, callback_status_);

  test_background_sync_manager_->set_delay_backend(false);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DeleteAndStartOverServiceWorkerContext) {
  EXPECT_TRUE(Register(sync_options_1_));
  DeleteServiceWorkerAndStartOver();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DisabledManagerWorksAfterBrowserRestart) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_2_));

  // The manager is now disabled and not accepting new requests until browser
  // restart or notification that the storage has been wiped.
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(Register(sync_options_2_));

  // Simulate restarting the browser by creating a new BackgroundSyncManager.
  SetupBackgroundSyncManager();
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, DisabledManagerWorksAfterDeleteAndStartOver) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_2_));

  // The manager is now disabled and not accepting new requests until browser
  // restart or notification that the storage has been wiped.
  test_background_sync_manager_->set_corrupt_backend(false);
  DeleteServiceWorkerAndStartOver();

  RegisterServiceWorkers();

  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsId) {
  BackgroundSyncRegistration reg_1;
  BackgroundSyncRegistration reg_2;

  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_2.set_id(reg_1.id() + 1);
  EXPECT_TRUE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsTag) {
  BackgroundSyncRegistration reg_1;
  BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_2.options()->tag = "bar";
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsPeriodicity) {
  BackgroundSyncRegistration reg_1;
  BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_1.options()->periodicity = SYNC_PERIODIC;
  reg_2.options()->periodicity = SYNC_ONE_SHOT;
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsMinPeriod) {
  BackgroundSyncRegistration reg_1;
  BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_2.options()->min_period = reg_1.options()->min_period + 1;
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsNetworkState) {
  BackgroundSyncRegistration reg_1;
  BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_1.options()->network_state = NETWORK_STATE_ANY;
  reg_2.options()->network_state = NETWORK_STATE_ONLINE;
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsPowerState) {
  BackgroundSyncRegistration reg_1;
  BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_1.options()->power_state = POWER_STATE_AUTO;
  reg_2.options()->power_state = POWER_STATE_AVOID_DRAINING;
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, StoreAndRetrievePreservesValues) {
  BackgroundSyncRegistrationOptions options;
  // Set non-default values for each field.
  options.tag = "foo";
  EXPECT_NE(SYNC_PERIODIC, options.periodicity);
  options.periodicity = SYNC_PERIODIC;
  options.min_period += 1;
  EXPECT_NE(NETWORK_STATE_ANY, options.network_state);
  options.network_state = NETWORK_STATE_ANY;
  EXPECT_NE(POWER_STATE_AUTO, options.power_state);
  options.power_state = POWER_STATE_AUTO;

  // Store the registration.
  EXPECT_TRUE(Register(options));

  // Simulate restarting the sync manager, forcing the next read to come from
  // disk.
  SetupBackgroundSyncManager();

  EXPECT_TRUE(GetRegistration(options));
  EXPECT_TRUE(options.Equals(*callback_registration_handle_->options()));
}

TEST_F(BackgroundSyncManagerTest, EmptyTagSupported) {
  sync_options_1_.tag = "a";
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(
      sync_options_1_.Equals(*callback_registration_handle_->options()));
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, OverlappingPeriodicAndOneShotTags) {
  // Registrations with the same tags but different periodicities should not
  // collide.
  sync_options_1_.tag = "";
  sync_options_2_.tag = "";
  sync_options_1_.periodicity = SYNC_PERIODIC;
  sync_options_2_.periodicity = SYNC_ONE_SHOT;

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));

  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_EQ(SYNC_PERIODIC,
            callback_registration_handle_->options()->periodicity);
  EXPECT_TRUE(GetRegistration(sync_options_2_));
  EXPECT_EQ(SYNC_ONE_SHOT,
            callback_registration_handle_->options()->periodicity);

  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));
  EXPECT_EQ(SYNC_ONE_SHOT,
            callback_registration_handle_->options()->periodicity);

  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, OneShotFiresOnRegistration) {
  InitSyncEventTest();

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, NotifyWhenFinishedAfterEventSuccess) {
  InitSyncEventTest();

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, sync_events_called_);

  EXPECT_TRUE(NotifyWhenFinished(callback_registration_handle_.get()));
  EXPECT_EQ(BACKGROUND_SYNC_STATE_SUCCESS, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, NotifyWhenFinishedBeforeEventSuccess) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Finish firing the event.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_EQ(BACKGROUND_SYNC_STATE_SUCCESS, FinishedState());
}

TEST_F(BackgroundSyncManagerTest,
       NotifyWhenFinishedBeforeUnregisteredEventSuccess) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Unregistering should set the state to UNREGISTERED but finished shouldn't
  // be called until the event finishes firing, at which point its state should
  // be SUCCESS.
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_FALSE(GetRegistration(sync_options_1_));

  // Finish firing the event.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BACKGROUND_SYNC_STATE_SUCCESS, FinishedState());
}

TEST_F(BackgroundSyncManagerTest,
       NotifyWhenFinishedBeforeUnregisteredEventFailure) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Unregistering should set the state to UNREGISTERED but finished shouldn't
  // be called until the event finishes firing, at which point its state should
  // be FAILED.
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_FALSE(GetRegistration(sync_options_1_));

  // Finish firing the event.
  sync_fired_callback_.Run(SERVICE_WORKER_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_EQ(BACKGROUND_SYNC_STATE_FAILED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest,
       NotifyWhenFinishedBeforeUnregisteredEventFires) {
  InitSyncEventTest();

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_TRUE(NotifyWhenFinished(callback_registration_handle_.get()));
  EXPECT_EQ(BACKGROUND_SYNC_STATE_UNREGISTERED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest,
       NotifyWhenFinishedBeforeEventSuccessDroppedHandle) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Drop the client's handle to the registration before the event fires, ensure
  // that the finished callback is still run.
  callback_registration_handle_ = nullptr;

  // Finish firing the event.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_EQ(BACKGROUND_SYNC_STATE_SUCCESS, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, NotifyWhenFinishedAfterEventFailure) {
  InitFailedSyncEventTest();

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, sync_events_called_);

  EXPECT_TRUE(NotifyWhenFinished(callback_registration_handle_.get()));
  EXPECT_EQ(BACKGROUND_SYNC_STATE_FAILED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, NotifyWhenFinishedBeforeEventFailure) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Finish firing the event.
  sync_fired_callback_.Run(SERVICE_WORKER_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BACKGROUND_SYNC_STATE_FAILED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, NotifyWhenFinishedAfterUnregistered) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));

  EXPECT_TRUE(NotifyWhenFinished(callback_registration_handle_.get()));
  EXPECT_EQ(BACKGROUND_SYNC_STATE_UNREGISTERED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, NotifyWhenFinishedBeforeUnregistered) {
  Register(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_EQ(BACKGROUND_SYNC_STATE_UNREGISTERED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, ReregisterMidSyncFirstAttemptFails) {
  InitDelayedSyncEventTest();
  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Reregister the event mid-sync
  EXPECT_TRUE(Register(sync_options_1_));

  // The first sync attempt fails.
  sync_fired_callback_.Run(SERVICE_WORKER_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_finished_called_);

  // It should fire again since it was reregistered mid-sync.
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATE_SUCCESS, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, ReregisterMidSyncFirstAttemptSucceeds) {
  InitDelayedSyncEventTest();
  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Reregister the event mid-sync
  EXPECT_TRUE(Register(sync_options_1_));

  // The first sync event succeeds.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_finished_called_);

  // It should fire again since it was reregistered mid-sync.
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATE_SUCCESS, FinishedState());
}

TEST_F(BackgroundSyncManagerTest,
       NotifyUnregisteredMidSyncNoRetryAttemptsLeft) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Unregister the event mid-sync.
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));

  // Finish firing the event.
  sync_fired_callback_.Run(SERVICE_WORKER_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();

  // Since there were no retry attempts left, the sync ultimately failed.
  EXPECT_EQ(BACKGROUND_SYNC_STATE_FAILED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest,
       NotifyUnregisteredMidSyncWithRetryAttemptsLeft) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Unregister the event mid-sync.
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));

  // Finish firing the event.
  sync_fired_callback_.Run(SERVICE_WORKER_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  // Since there was one retry attempt left, the sync didn't completely fail
  // before it was unregistered.
  EXPECT_EQ(BACKGROUND_SYNC_STATE_UNREGISTERED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, OverwritePendingRegistration) {
  // An overwritten pending registration should complete with
  // BACKGROUND_SYNC_STATE_UNREGISTERED.
  sync_options_1_.power_state = POWER_STATE_AVOID_DRAINING;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_EQ(POWER_STATE_AVOID_DRAINING,
            callback_registration_handle_->options()->power_state);
  scoped_ptr<BackgroundSyncRegistrationHandle> original_handle =
      std::move(callback_registration_handle_);

  // Overwrite the pending registration.
  sync_options_1_.power_state = POWER_STATE_AUTO;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_EQ(POWER_STATE_AUTO,
            callback_registration_handle_->options()->power_state);

  EXPECT_TRUE(NotifyWhenFinished(original_handle.get()));
  EXPECT_EQ(BACKGROUND_SYNC_STATE_UNREGISTERED, FinishedState());
  EXPECT_EQ(0, sync_events_called_);
}

TEST_F(BackgroundSyncManagerTest, OverwriteFiringRegistrationWhichSucceeds) {
  // An overwritten pending registration should complete with
  // BACKGROUND_SYNC_STATE_SUCCESS if firing completes successfully.
  InitDelayedSyncEventTest();

  sync_options_1_.power_state = POWER_STATE_AVOID_DRAINING;
  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  scoped_ptr<BackgroundSyncRegistrationHandle> original_handle =
      std::move(callback_registration_handle_);

  // The next registration won't block.
  InitSyncEventTest();

  // Overwrite the firing registration.
  sync_options_1_.power_state = POWER_STATE_AUTO;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_FALSE(NotifyWhenFinished(original_handle.get()));

  // Successfully finish the first event.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BACKGROUND_SYNC_STATE_SUCCESS, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, OverwriteFiringRegistrationWhichFails) {
  // An overwritten pending registration should complete with
  // BACKGROUND_SYNC_STATE_FAILED if firing fails.
  InitDelayedSyncEventTest();

  sync_options_1_.power_state = POWER_STATE_AVOID_DRAINING;
  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  scoped_ptr<BackgroundSyncRegistrationHandle> original_handle =
      std::move(callback_registration_handle_);

  // The next registration won't block.
  InitSyncEventTest();

  // Overwrite the firing registration.
  sync_options_1_.power_state = POWER_STATE_AUTO;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_FALSE(NotifyWhenFinished(original_handle.get()));

  // Fail the first event.
  sync_fired_callback_.Run(SERVICE_WORKER_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BACKGROUND_SYNC_STATE_FAILED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, DisableWhilePendingNotifiesFinished) {
  InitSyncEventTest();

  // Register a one-shot that must wait for network connectivity before it
  // can fire.
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Corrupting the backend should result in the manager disabling itself on the
  // next operation. While disabling, it should finalize any pending
  // registrations.
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_2_));
  EXPECT_EQ(BACKGROUND_SYNC_STATE_UNREGISTERED, FinishedState());
}

TEST_F(BackgroundSyncManagerTest, DisableWhileFiringNotifiesFinished) {
  InitDelayedSyncEventTest();

  // Register a one-shot that pauses mid-fire.
  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  EXPECT_FALSE(NotifyWhenFinished(callback_registration_handle_.get()));

  // Corrupting the backend should result in the manager disabling itself on the
  // next operation. Even though the manager is disabled, the firing sync event
  // should still be able to complete successfully and notify as much.
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_2_));
  EXPECT_FALSE(callback_finished_called_);
  test_background_sync_manager_->set_corrupt_backend(false);

  // Successfully complete the firing event.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BACKGROUND_SYNC_STATE_SUCCESS, FinishedState());
}

// TODO(jkarlin): Change this to a periodic test as one-shots can't be power
// dependent according to spec.
TEST_F(BackgroundSyncManagerTest, OneShotFiresOnPowerChange) {
  InitSyncEventTest();
  sync_options_1_.power_state = POWER_STATE_AVOID_DRAINING;

  SetOnBatteryPower(true);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  SetOnBatteryPower(false);
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

// TODO(jkarlin): Change this to a periodic test as one-shots can't be power
// dependent according to spec.
TEST_F(BackgroundSyncManagerTest, MultipleOneShotsFireOnPowerChange) {
  InitSyncEventTest();
  sync_options_1_.power_state = POWER_STATE_AVOID_DRAINING;
  sync_options_2_.power_state = POWER_STATE_AVOID_DRAINING;

  SetOnBatteryPower(true);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));

  SetOnBatteryPower(false);
  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, OneShotFiresOnNetworkChange) {
  InitSyncEventTest();

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, MultipleOneShotsFireOnNetworkChange) {
  InitSyncEventTest();

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);

  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, OneShotFiresOnManagerRestart) {
  InitSyncEventTest();

  // Initially the event won't run because there is no network.
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Simulate closing the browser.
  DeleteBackgroundSyncManager();

  // The next time the manager is started, the network is good.
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);
  SetupBackgroundSyncManager();
  InitSyncEventTest();

  // The event should have fired.
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, FailedOneShotShouldBeRemoved) {
  InitFailedSyncEventTest();

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, FailedOneShotReregisteredAndFires) {
  InitFailedSyncEventTest();

  // The initial sync event fails.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));

  InitSyncEventTest();

  // Reregistering should cause the sync event to fire again, this time
  // succeeding.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DelayOneShotMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  // Finish firing the event and verify that the registration is removed.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, OverwriteRegistrationMidSync) {
  InitDelayedSyncEventTest();

  sync_options_1_.network_state = NETWORK_STATE_ANY;
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);

  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  // Don't delay the next sync.
  test_background_sync_manager_->set_one_shot_callback(
      base::Bind(OneShotSuccessfulCallback, &sync_events_called_));

  // Register a different sync event with the same tag, overwriting the first.
  sync_options_1_.network_state = NETWORK_STATE_ONLINE;
  EXPECT_TRUE(Register(sync_options_1_));

  // The new sync event won't run as the network requirements aren't met.
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Finish the first event, note that the second is still registered.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Change the network and the second should run.
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterOneShotMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_FALSE(GetRegistration(sync_options_1_));

  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, BadBackendMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  test_background_sync_manager_->set_corrupt_backend(true);
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();

  // The backend should now be disabled because it couldn't unregister the
  // one-shot.
  EXPECT_FALSE(Register(sync_options_2_));
  EXPECT_FALSE(
      RegisterWithServiceWorkerId(sw_registration_id_2_, sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterServiceWorkerMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  UnregisterServiceWorker(sw_registration_id_1_);

  sync_fired_callback_.Run(SERVICE_WORKER_OK);

  // The backend isn't disabled, but the first service worker registration is
  // gone.
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_2_, sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, KillManagerMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  // Create a new manager which should fire the sync again on init.
  SetupBackgroundSyncManager();
  InitSyncEventTest();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_EQ(2, sync_events_called_);
}

TEST_F(BackgroundSyncManagerTest, RegisterFromServiceWorkerWithoutMainFrame) {
  test_background_sync_manager_->set_has_main_frame_provider_host(false);
  EXPECT_FALSE(Register(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest,
       RegisterFromDocumentWithoutMainFrameProviderHost) {
  test_background_sync_manager_->set_has_main_frame_provider_host(false);
  EXPECT_TRUE(RegisterFromDocumentWithServiceWorkerId(sw_registration_id_1_,
                                                      sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest,
       RegisterExistingFromServiceWorkerWithoutMainFrame) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_has_main_frame_provider_host(false);
  EXPECT_FALSE(Register(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterSucceedsWithoutMainFrame) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_has_main_frame_provider_host(false);
  EXPECT_TRUE(Unregister(callback_registration_handle_.get()));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DefaultParameters) {
  *test_controller_->background_sync_parameters() = BackgroundSyncParameters();
  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();

  EXPECT_EQ(BackgroundSyncParameters(),
            *test_background_sync_manager_->background_sync_parameters());
}

TEST_F(BackgroundSyncManagerTest, OverrideParameters) {
  BackgroundSyncParameters* parameters =
      test_controller_->background_sync_parameters();
  parameters->disable = true;
  parameters->max_sync_attempts = 100;
  parameters->initial_retry_delay = base::TimeDelta::FromMinutes(200);
  parameters->retry_delay_factor = 300;
  parameters->min_sync_recovery_time = base::TimeDelta::FromMinutes(400);
  parameters->max_sync_event_duration = base::TimeDelta::FromMinutes(500);

  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();

  // Check that the manager is disabled
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_STORAGE_ERROR, callback_status_);

  const BackgroundSyncParameters* manager_parameters =
      test_background_sync_manager_->background_sync_parameters();
  EXPECT_EQ(*parameters, *manager_parameters);
}

TEST_F(BackgroundSyncManagerTest, DisablingFromControllerKeepsRegistrations) {
  EXPECT_TRUE(Register(sync_options_1_));

  BackgroundSyncParameters* parameters =
      test_controller_->background_sync_parameters();
  parameters->disable = true;

  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();
  EXPECT_FALSE(GetRegistration(sync_options_1_));  // fails because disabled

  // Reenable the BackgroundSyncManager on next launch
  parameters->disable = false;

  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();
  EXPECT_TRUE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DisabledPermanently) {
  BackgroundSyncParameters* parameters =
      test_controller_->background_sync_parameters();
  parameters->disable = true;

  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();

  // Check that the manager is disabled
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_STORAGE_ERROR, callback_status_);

  // If the service worker is wiped and the manager is restarted, the manager
  // should stay disabled.
  DeleteServiceWorkerAndStartOver();
  RegisterServiceWorkers();
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_STORAGE_ERROR, callback_status_);
}

TEST_F(BackgroundSyncManagerTest, NotifyBackgroundSyncRegistered) {
  // Verify that the BackgroundSyncController is informed of registrations.
  EXPECT_EQ(0, test_controller_->registration_count());
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, test_controller_->registration_count());
  EXPECT_EQ(GURL(kPattern1).GetOrigin().spec(),
            test_controller_->registration_origin().spec());
}

TEST_F(BackgroundSyncManagerTest, WakeBrowserCalled) {
  InitDelayedSyncEventTest();

  // The BackgroundSyncManager should declare in initialization
  // that it doesn't need to be woken up since it has no registrations.
  EXPECT_LT(0, test_controller_->run_in_background_count());
  EXPECT_FALSE(test_controller_->run_in_background_enabled());

  SetNetwork(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_FALSE(test_controller_->run_in_background_enabled());

  // Register a one-shot but it can't fire due to lack of network, wake up is
  // required.
  Register(sync_options_1_);
  EXPECT_TRUE(test_controller_->run_in_background_enabled());

  // Start the event but it will pause mid-sync due to
  // InitDelayedSyncEventTest() above.
  SetNetwork(net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_TRUE(test_controller_->run_in_background_enabled());
  EXPECT_EQ(test_background_sync_manager_->background_sync_parameters()
                ->min_sync_recovery_time,
            base::TimeDelta::FromMilliseconds(
                test_controller_->run_in_background_min_ms()));

  // Finish the sync.
  sync_fired_callback_.Run(SERVICE_WORKER_OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_controller_->run_in_background_enabled());
}

TEST_F(BackgroundSyncManagerTest, OneAttempt) {
  SetMaxSyncAttemptsAndRestartManager(1);
  InitFailedSyncEventTest();

  // It should permanently fail after failing once.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, TwoAttempts) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(test_background_sync_manager_->delayed_task().is_null());

  // Make sure the delay is reasonable.
  EXPECT_LT(base::TimeDelta::FromMinutes(1),
            test_background_sync_manager_->delayed_task_delta());
  EXPECT_GT(base::TimeDelta::FromHours(1),
            test_background_sync_manager_->delayed_task_delta());

  // Fire again and this time it should permanently fail.
  test_clock_->Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->delayed_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, ThreeAttempts) {
  SetMaxSyncAttemptsAndRestartManager(3);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(test_background_sync_manager_->delayed_task().is_null());

  // The second run will fail but it will setup a timer to try again.
  base::TimeDelta first_delta =
      test_background_sync_manager_->delayed_task_delta();
  test_clock_->Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->delayed_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Verify that the delta grows for each attempt.
  EXPECT_LT(first_delta, test_background_sync_manager_->delayed_task_delta());

  // The third run will permanently fail.
  test_clock_->Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->delayed_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, WaitsFullDelayTime) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(test_background_sync_manager_->delayed_task().is_null());

  // Fire again one second before it's ready to retry. Expect it to reschedule
  // the delay timer for one more second.
  test_clock_->Advance(test_background_sync_manager_->delayed_task_delta() -
                       base::TimeDelta::FromSeconds(1));
  test_background_sync_manager_->delayed_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            test_background_sync_manager_->delayed_task_delta());

  // Fire one second later and it should fail permanently.
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  test_background_sync_manager_->delayed_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, RetryOnBrowserRestart) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Simulate restarting the browser after sufficient time has passed.
  base::TimeDelta delta = test_background_sync_manager_->delayed_task_delta();
  CreateBackgroundSyncManager();
  InitFailedSyncEventTest();
  test_clock_->Advance(delta);
  InitBackgroundSyncManager();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, RescheduleOnBrowserRestart) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Simulate restarting the browser before the retry timer has expired.
  base::TimeDelta delta = test_background_sync_manager_->delayed_task_delta();
  CreateBackgroundSyncManager();
  InitFailedSyncEventTest();
  test_clock_->Advance(delta - base::TimeDelta::FromSeconds(1));
  InitBackgroundSyncManager();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            test_background_sync_manager_->delayed_task_delta());
}

TEST_F(BackgroundSyncManagerTest, RetryIfClosedMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  // The time delta is the recovery timer.
  base::TimeDelta delta = test_background_sync_manager_->delayed_task_delta();

  // Simulate restarting the browser after the recovery time, the event should
  // fire once and then fail permanently.
  CreateBackgroundSyncManager();
  InitFailedSyncEventTest();
  test_clock_->Advance(delta);
  InitBackgroundSyncManager();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, AllTestsEventuallyFire) {
  SetMaxSyncAttemptsAndRestartManager(3);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));

  // Run it a second time.
  test_clock_->Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->delayed_task().Run();
  base::RunLoop().RunUntilIdle();

  base::TimeDelta delay_delta =
      test_background_sync_manager_->delayed_task_delta();

  // Create a second registration, which will fail and setup a timer.
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_GT(delay_delta, test_background_sync_manager_->delayed_task_delta());

  while (!test_background_sync_manager_->delayed_task().is_null()) {
    test_clock_->Advance(test_background_sync_manager_->delayed_task_delta());
    test_background_sync_manager_->delayed_task().Run();
    test_background_sync_manager_->ClearDelayedTask();
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, LastChance) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_EVENT_LAST_CHANCE_IS_NOT_LAST_CHANCE,
            test_background_sync_manager_->last_chance());
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Run it again.
  test_clock_->Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->delayed_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_EVENT_LAST_CHANCE_IS_LAST_CHANCE,
            test_background_sync_manager_->last_chance());
}

}  // namespace content
