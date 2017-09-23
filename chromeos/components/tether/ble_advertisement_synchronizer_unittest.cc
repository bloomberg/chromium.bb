// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertisement_synchronizer.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/data_with_timestamp.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
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
  // |unregister_callback| should be called with the success callback passed to
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

class BleAdvertisementSynchronizerTest : public testing::Test {
 protected:
  BleAdvertisementSynchronizerTest()
      : fake_advertisement_(make_scoped_refptr(new FakeBluetoothAdvertisement(
            base::Bind(&BleAdvertisementSynchronizerTest::OnUnregisterCalled,
                       base::Unretained(this))))) {}

  void SetUp() override {
    num_register_success_ = 0;
    num_register_error_ = 0;
    num_unregister_success_ = 0;
    num_unregister_error_ = 0;
    register_args_list_.clear();

    mock_adapter_ = make_scoped_refptr(
        new NiceMock<MockBluetoothAdapterWithAdvertisements>());
    ON_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_))
        .WillByDefault(Invoke(
            this,
            &BleAdvertisementSynchronizerTest::OnAdapterRegisterAdvertisement));

    mock_timer_ = new base::MockTimer(true /* retain_user_task */,
                                      false /* is_repeating */);

    test_clock_ = new base::SimpleTestClock();
    test_clock_->Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));

    synchronizer_ =
        base::MakeUnique<BleAdvertisementSynchronizer>(mock_adapter_);
    synchronizer_->SetTestDoubles(base::WrapUnique(mock_timer_),
                                  base::WrapUnique(test_clock_));
  }

  base::TimeDelta TimeDeltaMillis(int64_t num_millis) {
    return base::TimeDelta::FromMilliseconds(num_millis);
  }

  void OnAdapterRegisterAdvertisement(RegisterAdvertisementArgs* args) {
    register_args_list_.emplace_back(base::WrapUnique(args));
  }

  void RegisterAdvertisement(const std::string& id) {
    synchronizer_->RegisterAdvertisement(
        GenerateAdvertisementData(id),
        base::Bind(&BleAdvertisementSynchronizerTest::OnAdvertisementRegistered,
                   base::Unretained(this)),
        base::Bind(
            &BleAdvertisementSynchronizerTest::OnErrorRegisteringAdvertisement,
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
        base::Bind(
            &BleAdvertisementSynchronizerTest::OnAdvertisementUnregistered,
            base::Unretained(this)),
        base::Bind(&BleAdvertisementSynchronizerTest::
                       OnErrorUnregisteringAdvertisement,
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

  void FireTimer() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    mock_timer_->Fire();
  }

  void OnUnregisterCalled(
      const device::BluetoothAdvertisement::SuccessCallback& success_callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback) {
    unregister_args_list_.emplace_back(base::WrapUnique(
        new UnregisterAdvertisementArgs(success_callback, error_callback)));
  }

  const scoped_refptr<FakeBluetoothAdvertisement> fake_advertisement_;

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>> mock_adapter_;

  base::MockTimer* mock_timer_;
  base::SimpleTestClock* test_clock_;

  std::vector<std::unique_ptr<RegisterAdvertisementArgs>> register_args_list_;
  std::vector<std::unique_ptr<UnregisterAdvertisementArgs>>
      unregister_args_list_;

  int num_register_success_;
  int num_register_error_;
  int num_unregister_success_;
  int num_unregister_error_;

  bool stopped_callback_called_;

  std::unique_ptr<BleAdvertisementSynchronizer> synchronizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BleAdvertisementSynchronizerTest);
};

TEST_F(BleAdvertisementSynchronizerTest, TestRegisterSuccess) {
  RegisterAdvertisement(kId1);
  InvokeRegisterCallback(true /* success */, kId1, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_register_success_);
}

TEST_F(BleAdvertisementSynchronizerTest, TestRegisterError) {
  RegisterAdvertisement(kId1);
  InvokeRegisterCallback(false /* success */, kId1, 0u /* reg_arg_index */);
  EXPECT_EQ(1, num_register_error_);
}

TEST_F(BleAdvertisementSynchronizerTest, TestUnregisterSuccess) {
  UnregisterAdvertisement();
  InvokeUnregisterCallback(true /* success */, 0u /* unreg_arg_index */);
  EXPECT_EQ(1, num_unregister_success_);
}

TEST_F(BleAdvertisementSynchronizerTest, TestUnregisterError) {
  UnregisterAdvertisement();
  InvokeUnregisterCallback(false /* success */, 0u /* unreg_arg_index */);
  EXPECT_EQ(1, num_unregister_error_);
}

TEST_F(BleAdvertisementSynchronizerTest, TestThrottling) {
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
