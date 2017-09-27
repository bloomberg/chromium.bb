// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_synchronizer.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/data_with_timestamp.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::_;

namespace chromeos {

namespace tether {

namespace {

const char kId1[] = "id1";
const char kId2[] = "id2";
const char kId3[] = "id3";

int64_t kTimeBetweenEachCommandMs = 200;

struct RegisterAdvertisementArgs {
  RegisterAdvertisementArgs(
      const device::BluetoothAdvertisement::UUIDList& service_uuids,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback&
          error_callback)
      : service_uuids(service_uuids),
        callback(callback),
        error_callback(error_callback) {}

  device::BluetoothAdvertisement::UUIDList service_uuids;
  const device::BluetoothAdapter::CreateAdvertisementCallback callback;
  const device::BluetoothAdapter::AdvertisementErrorCallback error_callback;
};

struct UnregisterAdvertisementArgs {
  UnregisterAdvertisementArgs(
      const device::BluetoothAdvertisement::SuccessCallback& callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback)
      : callback(callback), error_callback(error_callback) {}

  const device::BluetoothAdvertisement::SuccessCallback callback;
  const device::BluetoothAdvertisement::ErrorCallback error_callback;
};

struct StartDiscoverySessionArgs {
  StartDiscoverySessionArgs(
      const device::BluetoothAdapter::DiscoverySessionCallback& callback,
      const device::BluetoothAdapter::ErrorCallback& error_callback)
      : callback(callback), error_callback(error_callback) {}

  const device::BluetoothAdapter::DiscoverySessionCallback callback;
  const device::BluetoothAdapter::ErrorCallback error_callback;
};

struct StopDiscoverySessionArgs {
  StopDiscoverySessionArgs(
      const base::Closure& callback,
      const device::BluetoothDiscoverySession::ErrorCallback& error_callback)
      : callback(callback), error_callback(error_callback) {}

  const base::Closure callback;
  const device::BluetoothDiscoverySession::ErrorCallback error_callback;
};

class MockBluetoothAdapterWithAdvertisements
    : public device::MockBluetoothAdapter {
 public:
  MockBluetoothAdapterWithAdvertisements() : MockBluetoothAdapter() {}

  MOCK_METHOD1(RegisterAdvertisementWithArgsStruct,
               void(RegisterAdvertisementArgs*));

  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback&
          error_callback) override {
    RegisterAdvertisementWithArgsStruct(new RegisterAdvertisementArgs(
        *advertisement_data->service_uuids(), callback, error_callback));
  }

 protected:
  ~MockBluetoothAdapterWithAdvertisements() override {}
};

class FakeBluetoothAdvertisement : public device::BluetoothAdvertisement {
 public:
  // |unregister_callback| should be called with the callbacks passed to
  // Unregister() whenever an Unregister() call occurs.
  FakeBluetoothAdvertisement(
      const base::Callback<
          void(const device::BluetoothAdvertisement::SuccessCallback&,
               const device::BluetoothAdvertisement::ErrorCallback&)>&
          unregister_callback)
      : unregister_callback_(unregister_callback) {}

  // BluetoothAdvertisement:
  void Unregister(
      const device::BluetoothAdvertisement::SuccessCallback& success_callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback)
      override {
    unregister_callback_.Run(success_callback, error_callback);
  }

 private:
  ~FakeBluetoothAdvertisement() override {}

  base::Callback<void(const device::BluetoothAdvertisement::SuccessCallback&,
                      const device::BluetoothAdvertisement::ErrorCallback&)>
      unregister_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothAdvertisement);
};

class FakeDiscoverySession : public device::MockBluetoothDiscoverySession {
 public:
  // |stop_callback| should be called with the callbacks passed to
  // Stop() whenever a Stop() call occurs.
  FakeDiscoverySession(
      const base::Callback<
          void(const base::Closure&,
               const device::BluetoothDiscoverySession::ErrorCallback&)>&
          stop_callback)
      : stop_callback_(stop_callback) {}
  ~FakeDiscoverySession() override {}

  // BluetoothDiscoverySession:
  void Stop(const base::Closure& callback,
            const device::BluetoothDiscoverySession::ErrorCallback&
                error_callback) override {
    stop_callback_.Run(callback, error_callback);
  }

 private:
  base::Callback<void(const base::Closure&,
                      const device::BluetoothDiscoverySession::ErrorCallback&)>
      stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeDiscoverySession);
};

// Creates a UUIDList with one element of value |id|.
std::unique_ptr<device::BluetoothAdvertisement::UUIDList> CreateUUIDList(
    const std::string& id) {
  return base::MakeUnique<device::BluetoothAdvertisement::UUIDList>(1u, id);
}

// Creates advertisement data with a UUID list with one element of value |id|.
std::unique_ptr<device::BluetoothAdvertisement::Data> GenerateAdvertisementData(
    const std::string& id) {
  auto data = base::MakeUnique<device::BluetoothAdvertisement::Data>(
      device::BluetoothAdvertisement::AdvertisementType::
          ADVERTISEMENT_TYPE_PERIPHERAL);
  data->set_service_uuids(CreateUUIDList(id));
  return data;
}

}  // namespace

class BleSynchronizerTest : public testing::Test {
 protected:
  BleSynchronizerTest()
      : fake_advertisement_(base::MakeRefCounted<FakeBluetoothAdvertisement>(
            base::Bind(&BleSynchronizerTest::OnUnregisterCalled,
                       base::Unretained(this)))),
        fake_discovery_session_(base::WrapUnique(new FakeDiscoverySession(
            base::Bind(&BleSynchronizerTest::OnStopCalled,
                       base::Unretained(this))))),
        fake_discovery_session_weak_ptr_factory_(
            fake_discovery_session_.get()) {}

  void SetUp() override {
    num_register_success_ = 0;
    num_register_error_ = 0;
    num_unregister_success_ = 0;
    num_unregister_error_ = 0;
    num_start_success_ = 0;
    num_start_error_ = 0;
    num_stop_success_ = 0;
    num_stop_error_ = 0;
    register_args_list_.clear();

    mock_adapter_ = make_scoped_refptr(
        new NiceMock<MockBluetoothAdapterWithAdvertisements>());
    ON_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_))
        .WillByDefault(
            Invoke(this, &BleSynchronizerTest::OnAdapterRegisterAdvertisement));
    ON_CALL(*mock_adapter_, StartDiscoverySession(_, _))
        .WillByDefault(
            Invoke(this, &BleSynchronizerTest::OnAdapterStartDiscoverySession));

    mock_timer_ = new base::MockTimer(true /* retain_user_task */,
                                      false /* is_repeating */);

    test_clock_ = new base::SimpleTestClock();
    test_clock_->Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));

    synchronizer_ = base::MakeUnique<BleSynchronizer>(mock_adapter_);
    synchronizer_->SetTestDoubles(base::WrapUnique(mock_timer_),
                                  base::WrapUnique(test_clock_));
  }

  base::TimeDelta TimeDeltaMillis(int64_t num_millis) {
    return base::TimeDelta::FromMilliseconds(num_millis);
  }

  void OnAdapterRegisterAdvertisement(RegisterAdvertisementArgs* args) {
    register_args_list_.emplace_back(base::WrapUnique(args));
  }

  void OnAdapterStartDiscoverySession(
      const device::BluetoothAdapter::DiscoverySessionCallback& callback,
      const device::BluetoothAdapter::ErrorCallback& error_callback) {
    start_discovery_args_list_.emplace_back(base::WrapUnique(
        new StartDiscoverySessionArgs(callback, error_callback)));
  }

  void RegisterAdvertisement(const std::string& id) {
    synchronizer_->RegisterAdvertisement(
        GenerateAdvertisementData(id),
        base::Bind(&BleSynchronizerTest::OnAdvertisementRegistered,
                   base::Unretained(this)),
        base::Bind(&BleSynchronizerTest::OnErrorRegisteringAdvertisement,
                   base::Unretained(this)));
  }

  void InvokeRegisterCallback(bool success,
                              const std::string& expected_id,
                              size_t reg_arg_index) {
    EXPECT_TRUE(register_args_list_.size() >= reg_arg_index);
    EXPECT_EQ(*CreateUUIDList(expected_id),
              register_args_list_[reg_arg_index]->service_uuids);

    if (success) {
      register_args_list_[reg_arg_index]->callback.Run(
          make_scoped_refptr(new device::MockBluetoothAdvertisement()));
    } else {
      register_args_list_[reg_arg_index]->error_callback.Run(
          device::BluetoothAdvertisement::ErrorCode::
              INVALID_ADVERTISEMENT_ERROR_CODE);
    }

    // Reset to make sure that this callback is never double-invoked.
    register_args_list_[reg_arg_index].reset();
  }

  void OnAdvertisementRegistered(
      scoped_refptr<device::BluetoothAdvertisement> advertisement) {
    ++num_register_success_;
  }

  void OnErrorRegisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code) {
    ++num_register_error_;
  }

  void UnregisterAdvertisement() {
    synchronizer_->UnregisterAdvertisement(
        fake_advertisement_,
        base::Bind(&BleSynchronizerTest::OnAdvertisementUnregistered,
                   base::Unretained(this)),
        base::Bind(&BleSynchronizerTest::OnErrorUnregisteringAdvertisement,
                   base::Unretained(this)));
  }

  void InvokeUnregisterCallback(bool success, size_t unreg_arg_index) {
    EXPECT_TRUE(unregister_args_list_.size() >= unreg_arg_index);

    if (success) {
      unregister_args_list_[unreg_arg_index]->callback.Run();
    } else {
      unregister_args_list_[unreg_arg_index]->error_callback.Run(
          device::BluetoothAdvertisement::ErrorCode::
              INVALID_ADVERTISEMENT_ERROR_CODE);
    }

    // Reset to make sure that this callback is never double-invoked.
    unregister_args_list_[unreg_arg_index].reset();
  }

  void OnAdvertisementUnregistered() { ++num_unregister_success_; }

  void OnErrorUnregisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code) {
    ++num_unregister_error_;
  }

  void StartDiscoverySession() {
    synchronizer_->StartDiscoverySession(
        base::Bind(&BleSynchronizerTest::OnDiscoverySessionStarted,
                   base::Unretained(this)),
        base::Bind(&BleSynchronizerTest::OnErrorStartingDiscoverySession,
                   base::Unretained(this)));
  }

  void InvokeStartDiscoveryCallback(bool success, size_t start_arg_index) {
    EXPECT_TRUE(start_discovery_args_list_.size() >= start_arg_index);

    if (success) {
      start_discovery_args_list_[start_arg_index]->callback.Run(
          base::MakeUnique<device::MockBluetoothDiscoverySession>());
    } else {
      start_discovery_args_list_[start_arg_index]->error_callback.Run();
    }

    // Reset to make sure that this callback is never double-invoked.
    start_discovery_args_list_[start_arg_index].reset();
  }

  void OnDiscoverySessionStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
    ++num_start_success_;
  }

  void OnErrorStartingDiscoverySession() { ++num_start_error_; }

  void StopDiscoverySession(
      base::WeakPtr<device::BluetoothDiscoverySession> discovery_session) {
    synchronizer_->StopDiscoverySession(
        discovery_session,
        base::Bind(&BleSynchronizerTest::OnDiscoverySessionStopped,
                   base::Unretained(this)),
        base::Bind(&BleSynchronizerTest::OnErrorStoppingDiscoverySession,
                   base::Unretained(this)));
  }

  void InvokeStopDiscoveryCallback(bool success, size_t stop_arg_index) {
    EXPECT_TRUE(stop_discovery_args_list_.size() >= stop_arg_index);

    if (success)
      stop_discovery_args_list_[stop_arg_index]->callback.Run();
    else
      stop_discovery_args_list_[stop_arg_index]->error_callback.Run();

    // Reset to make sure that this callback is never double-invoked.
    stop_discovery_args_list_[stop_arg_index].reset();
  }

  void OnDiscoverySessionStopped() { ++num_stop_success_; }

  void OnErrorStoppingDiscoverySession() { ++num_stop_error_; }

  void FireTimer() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    mock_timer_->Fire();
  }

  void OnUnregisterCalled(
      const device::BluetoothAdvertisement::SuccessCallback& callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback) {
    unregister_args_list_.emplace_back(base::WrapUnique(
        new UnregisterAdvertisementArgs(callback, error_callback)));
  }

  void OnStopCalled(
      const base::Closure& callback,
      const device::BluetoothDiscoverySession::ErrorCallback& error_callback) {
    stop_discovery_args_list_.emplace_back(base::WrapUnique(
        new StopDiscoverySessionArgs(callback, error_callback)));
  }

  const scoped_refptr<FakeBluetoothAdvertisement> fake_advertisement_;
  const std::unique_ptr<device::MockBluetoothDiscoverySession>
      fake_discovery_session_;
  base::WeakPtrFactory<device::MockBluetoothDiscoverySession>
      fake_discovery_session_weak_ptr_factory_;

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>> mock_adapter_;

  base::MockTimer* mock_timer_;
  base::SimpleTestClock* test_clock_;

  std::vector<std::unique_ptr<RegisterAdvertisementArgs>> register_args_list_;
  std::vector<std::unique_ptr<UnregisterAdvertisementArgs>>
      unregister_args_list_;
  std::vector<std::unique_ptr<StartDiscoverySessionArgs>>
      start_discovery_args_list_;
  std::vector<std::unique_ptr<StopDiscoverySessionArgs>>
      stop_discovery_args_list_;

  int num_register_success_;
  int num_register_error_;
  int num_unregister_success_;
  int num_unregister_error_;
  int num_start_success_;
  int num_start_error_;
  int num_stop_success_;
  int num_stop_error_;

  bool stopped_callback_called_;

  std::unique_ptr<BleSynchronizer> synchronizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BleSynchronizerTest);
};

TEST_F(BleSynchronizerTest, TestRegisterSuccess) {
  RegisterAdvertisement(kId1);
  InvokeRegisterCallback(true /* success */, kId1, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_register_success_);
}

TEST_F(BleSynchronizerTest, TestRegisterError) {
  RegisterAdvertisement(kId1);
  InvokeRegisterCallback(false /* success */, kId1, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_register_error_);
}

TEST_F(BleSynchronizerTest, TestUnregisterSuccess) {
  UnregisterAdvertisement();
  InvokeUnregisterCallback(true /* success */, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_unregister_success_);
}

TEST_F(BleSynchronizerTest, TestUnregisterError) {
  UnregisterAdvertisement();
  InvokeUnregisterCallback(false /* success */, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_unregister_error_);
}

TEST_F(BleSynchronizerTest, TestStartSuccess) {
  StartDiscoverySession();
  InvokeStartDiscoveryCallback(true /* success */, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_start_success_);
}

TEST_F(BleSynchronizerTest, TestStartError) {
  StartDiscoverySession();
  InvokeStartDiscoveryCallback(false /* success */, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_start_error_);
}

TEST_F(BleSynchronizerTest, TestStopSuccess) {
  StopDiscoverySession(fake_discovery_session_weak_ptr_factory_.GetWeakPtr());
  InvokeStopDiscoveryCallback(true /* success */, 0u /* unreg_arg_index */);
  EXPECT_EQ(1, num_stop_success_);
}

TEST_F(BleSynchronizerTest, TestStopError) {
  StopDiscoverySession(fake_discovery_session_weak_ptr_factory_.GetWeakPtr());
  InvokeStopDiscoveryCallback(false /* success */, 0u /* unreg_arg_index */);
  EXPECT_EQ(1, num_stop_error_);
}

TEST_F(BleSynchronizerTest, TestStop_DeletedDiscoverySession) {
  // Simulate an invalidated WeakPtr being processed.
  StopDiscoverySession(base::WeakPtr<device::MockBluetoothDiscoverySession>());
  RegisterAdvertisement(kId1);

  // Stop() should not have been called.
  EXPECT_TRUE(stop_discovery_args_list_.empty());

  // The RegisterAdvertisement() command should be sent without the need for a
  // delay, since the previous command was not actually sent.
  InvokeRegisterCallback(true /* success */, kId1, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_register_success_);
}

TEST_F(BleSynchronizerTest, TestThrottling) {
  RegisterAdvertisement(kId1);
  InvokeRegisterCallback(true /* success */, kId1, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_register_success_);

  // Advance to one millisecond before the limit.
  test_clock_->Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs - 1));
  UnregisterAdvertisement();

  // Should still be empty since it should have been throttled, and the timer
  // should be running.
  EXPECT_TRUE(unregister_args_list_.empty());
  EXPECT_TRUE(mock_timer_->IsRunning());

  // Advance the clock and fire the timer. This should result in the next
  // command being executed.
  test_clock_->Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));
  mock_timer_->Fire();

  InvokeUnregisterCallback(true /* success */, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_unregister_success_);

  // Now, register 2 advertisements at the same time.
  RegisterAdvertisement(kId2);
  RegisterAdvertisement(kId3);

  // Should still only have the original register command..
  EXPECT_EQ(1u, register_args_list_.size());

  // Advance the clock and fire the timer. This should result in the next
  // command being executed.
  test_clock_->Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));
  mock_timer_->Fire();

  EXPECT_EQ(2u, register_args_list_.size());
  InvokeRegisterCallback(false /* success */, kId2, 1u /* reg_arg_index */);
  EXPECT_EQ(1, num_register_error_);

  // Advance the clock and fire the timer. This should result in the next
  // command being executed.
  test_clock_->Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));
  mock_timer_->Fire();

  EXPECT_EQ(3u, register_args_list_.size());
  InvokeRegisterCallback(true /* success */, kId3, 2u /* reg_arg_index */);
  EXPECT_EQ(2, num_register_success_);

  // Advance the clock before doing anything else. The next request should not
  // be throttled.
  EXPECT_FALSE(mock_timer_->IsRunning());
  test_clock_->Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));

  UnregisterAdvertisement();
  InvokeUnregisterCallback(false /* success */, 1u /* reg_arg_index */);
  EXPECT_EQ(1, num_unregister_error_);
}

}  // namespace tether

}  // namespace chromeos
