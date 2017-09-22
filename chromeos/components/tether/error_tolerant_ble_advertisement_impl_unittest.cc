// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/error_tolerant_ble_advertisement_impl.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/tether/ble_constants.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::_;

namespace chromeos {

namespace tether {

namespace {

const uint8_t kInvertedConnectionFlag = 0x01;
const char kDeviceId[] = "deviceId";

struct RegisterAdvertisementArgs {
  RegisterAdvertisementArgs(
      const device::BluetoothAdvertisement::ServiceData& service_data,
      const device::BluetoothAdvertisement::UUIDList& service_uuids,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback&
          error_callback)
      : service_data(service_data),
        service_uuids(service_uuids),
        callback(callback),
        error_callback(error_callback) {}

  RegisterAdvertisementArgs(const RegisterAdvertisementArgs& other)
      : service_data(other.service_data),
        service_uuids(other.service_uuids),
        callback(other.callback),
        error_callback(other.error_callback) {}

  device::BluetoothAdvertisement::ServiceData service_data;
  device::BluetoothAdvertisement::UUIDList service_uuids;
  const device::BluetoothAdapter::CreateAdvertisementCallback callback;
  const device::BluetoothAdapter::AdvertisementErrorCallback error_callback;
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
        *advertisement_data->service_data(),
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

std::unique_ptr<cryptauth::DataWithTimestamp> GenerateAdvertisementData() {
  return base::MakeUnique<cryptauth::DataWithTimestamp>("advertisement1", 1000L,
                                                        2000L);
}

}  // namespace

class ErrorTolerantBleAdvertisementImplTest : public testing::Test {
 protected:
  ErrorTolerantBleAdvertisementImplTest()
      : fake_advertisement_data_(GenerateAdvertisementData()) {}

  void SetUp() override {
    fake_advertisement_ = nullptr;
    register_advertisement_args_.reset();
    unregister_callback_.Reset();
    unregister_error_callback_.Reset();
    stopped_callback_called_ = false;

    mock_adapter_ = make_scoped_refptr(
        new NiceMock<MockBluetoothAdapterWithAdvertisements>());
    ON_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_))
        .WillByDefault(Invoke(this, &ErrorTolerantBleAdvertisementImplTest::
                                        OnAdapterRegisterAdvertisement));

    mock_timer_ = new base::MockTimer(true /* retain_user_task */,
                                      false /* is_repeating */);

    advertisement_ = base::MakeUnique<ErrorTolerantBleAdvertisementImpl>(
        kDeviceId, mock_adapter_,
        base::MakeUnique<cryptauth::DataWithTimestamp>(
            *fake_advertisement_data_));
    advertisement_->SetFakeTimerForTest(base::WrapUnique(mock_timer_));

    VerifyServiceDataMatches();
  }

  void OnAdapterRegisterAdvertisement(RegisterAdvertisementArgs* args) {
    register_advertisement_args_ = base::WrapUnique(args);
  }

  void VerifyServiceDataMatches() {
    EXPECT_TRUE(register_advertisement_args_);

    // First, verify that the service UUID list is correct.
    EXPECT_EQ(1u, register_advertisement_args_->service_uuids.size());
    EXPECT_EQ(kAdvertisingServiceUuid,
              register_advertisement_args_->service_uuids[0]);

    // Then, verify that the service data is correct.
    EXPECT_EQ(1u, register_advertisement_args_->service_data.size());
    std::vector<uint8_t> service_data_from_args =
        register_advertisement_args_->service_data[kAdvertisingServiceUuid];
    EXPECT_EQ(service_data_from_args.size(),
              advertisement_->advertisement_data().data.size() + 1);
    EXPECT_FALSE(memcmp(advertisement_->advertisement_data().data.data(),
                        service_data_from_args.data(),
                        register_advertisement_args_->service_data.size()));
    EXPECT_EQ(kInvertedConnectionFlag, service_data_from_args.back());
  }

  void InvokeRegisterCallback(bool success) {
    EXPECT_TRUE(register_advertisement_args_);

    // Make a copy and reset the original before invoking callback.
    device::BluetoothAdapter::CreateAdvertisementCallback callback_copy =
        register_advertisement_args_->callback;
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback_copy =
        register_advertisement_args_->error_callback;
    register_advertisement_args_.reset();

    if (success) {
      fake_advertisement_ = new FakeBluetoothAdvertisement(
          base::Bind(&ErrorTolerantBleAdvertisementImplTest::OnUnregisterCalled,
                     base::Unretained(this)));
      callback_copy.Run(make_scoped_refptr(fake_advertisement_));
      return;
    }

    error_callback_copy.Run(device::BluetoothAdvertisement::ErrorCode::
                                INVALID_ADVERTISEMENT_ERROR_CODE);
  }

  void FireTimer() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    mock_timer_->Fire();
  }

  void ReleaseAdvertisement() {
    advertisement_->AdvertisementReleased(fake_advertisement_);
  }

  void CallStop() {
    advertisement_->Stop(
        base::Bind(&ErrorTolerantBleAdvertisementImplTest::OnStopped,
                   base::Unretained(this)));
  }

  void OnStopped() { stopped_callback_called_ = true; }

  void OnUnregisterCalled(
      const device::BluetoothAdvertisement::SuccessCallback& success_callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback) {
    EXPECT_TRUE(unregister_callback_.is_null());
    EXPECT_TRUE(unregister_error_callback_.is_null());

    unregister_callback_ = success_callback;
    unregister_error_callback_ = error_callback;
  }

  void InvokeUnregisterCallback(bool success) {
    EXPECT_FALSE(unregister_callback_.is_null());
    EXPECT_FALSE(unregister_error_callback_.is_null());

    // Make a copy and reset the original before invoking callback.
    device::BluetoothAdvertisement::SuccessCallback callback_copy =
        unregister_callback_;
    device::BluetoothAdvertisement::ErrorCallback error_callback_copy =
        unregister_error_callback_;
    unregister_callback_.Reset();
    unregister_error_callback_.Reset();

    if (success) {
      callback_copy.Run();
      return;
    }

    error_callback_copy.Run(device::BluetoothAdvertisement::ErrorCode::
                                INVALID_ADVERTISEMENT_ERROR_CODE);
  }

  const std::unique_ptr<cryptauth::DataWithTimestamp> fake_advertisement_data_;

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>> mock_adapter_;

  base::MockTimer* mock_timer_;
  FakeBluetoothAdvertisement* fake_advertisement_;

  std::unique_ptr<RegisterAdvertisementArgs> register_advertisement_args_;
  device::BluetoothAdvertisement::SuccessCallback unregister_callback_;
  device::BluetoothAdvertisement::ErrorCallback unregister_error_callback_;

  bool stopped_callback_called_;

  std::unique_ptr<ErrorTolerantBleAdvertisementImpl> advertisement_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ErrorTolerantBleAdvertisementImplTest);
};

TEST_F(ErrorTolerantBleAdvertisementImplTest, TestRegisterAndStop_Success) {
  InvokeRegisterCallback(true /* success */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  CallStop();
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  InvokeUnregisterCallback(true /* success */);
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_TRUE(stopped_callback_called_);
}

TEST_F(ErrorTolerantBleAdvertisementImplTest, TestStoppedBeforeStarted) {
  // Before the advertisement has been started successfully.
  CallStop();
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Now, finish registering the advertisement.
  InvokeRegisterCallback(true /* success */);
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Successfully unregister.
  InvokeUnregisterCallback(true /* success */);
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_TRUE(stopped_callback_called_);
}

TEST_F(ErrorTolerantBleAdvertisementImplTest, TestRegisterAndStop_Released) {
  InvokeRegisterCallback(true /* success */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Simulate that advertisement being released. A new one should be created.
  ReleaseAdvertisement();
  VerifyServiceDataMatches();

  InvokeRegisterCallback(true /* success */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  CallStop();
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  InvokeUnregisterCallback(true /* success */);
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_TRUE(stopped_callback_called_);
}

TEST_F(ErrorTolerantBleAdvertisementImplTest, TestRegisterAndStop_Errors) {
  // Fail to register.
  InvokeRegisterCallback(false /* success */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Should have no pending request; fire the timer to try again.
  EXPECT_FALSE(register_advertisement_args_);
  FireTimer();
  VerifyServiceDataMatches();

  // Fail to register again.
  InvokeRegisterCallback(false /* success */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Should have no pending request; fire the timer to try again.
  EXPECT_FALSE(register_advertisement_args_);
  FireTimer();
  VerifyServiceDataMatches();

  // This time, succeed.
  InvokeRegisterCallback(true /* success */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Timer should not be running.
  EXPECT_FALSE(mock_timer_->IsRunning());

  CallStop();
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Fail to unregister.
  InvokeUnregisterCallback(false /* success */);
  EXPECT_FALSE(stopped_callback_called_);

  // Should have no pending request; fire the timer to try again.
  EXPECT_FALSE(register_advertisement_args_);
  FireTimer();

  // Fail to unregister again.
  InvokeUnregisterCallback(false /* success */);
  EXPECT_FALSE(stopped_callback_called_);

  // Should have no pending request; fire the timer to try again.
  EXPECT_FALSE(register_advertisement_args_);
  FireTimer();

  // This time, succeed.
  InvokeUnregisterCallback(true /* success */);
  EXPECT_TRUE(stopped_callback_called_);

  // Timer should not be running.
  EXPECT_FALSE(mock_timer_->IsRunning());
}

}  // namespace tether

}  // namespace chromeos
