// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kResultSuccess[] = "success";

}  // namespace

class NetworkDeviceHandlerTest : public testing::Test {
 public:
  NetworkDeviceHandlerTest() {}
  virtual ~NetworkDeviceHandlerTest() {}

  virtual void SetUp() OVERRIDE {
    DBusThreadManager::InitializeWithStub();
    message_loop_.RunUntilIdle();
    success_callback_ = base::Bind(&NetworkDeviceHandlerTest::SuccessCallback,
                                   base::Unretained(this));
    error_callback_ = base::Bind(&NetworkDeviceHandlerTest::ErrorCallback,
                                 base::Unretained(this));
    network_device_handler_.reset(new NetworkDeviceHandler());
  }

  virtual void TearDown() OVERRIDE {
    network_device_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  base::Closure GetErrorInvokingCallback(
      const std::string& device_path,
      const std::string& error_name) {
    return base::Bind(&NetworkDeviceHandlerTest::InvokeDBusErrorCallback,
                      base::Unretained(this),
                      device_path,
                      base::Bind(&NetworkDeviceHandlerTest::ErrorCallback,
                                 base::Unretained(this)),
                      error_name);
  }

  void ErrorCallback(const std::string& error_name,
                     scoped_ptr<base::DictionaryValue> error_data) {
    result_ = error_name;
  }

  void SuccessCallback() {
    result_ = kResultSuccess;
  }

  void InvokeDBusErrorCallback(
      const std::string& device_path,
      const network_handler::ErrorCallback& callback,
      const std::string& error_name) {
    network_device_handler_->HandleShillCallFailure(
        device_path, callback, error_name, "Error message.");
  }

 protected:

  static const char kDefaultCellularDevicePath[];
  static const char kDefaultWifiDevicePath[];

  std::string result_;

  scoped_ptr<NetworkDeviceHandler> network_device_handler_;
  base::MessageLoopForUI message_loop_;
  base::Closure success_callback_;
  network_handler::ErrorCallback error_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkDeviceHandlerTest);
};

const char NetworkDeviceHandlerTest::kDefaultCellularDevicePath[] =
    "/device/stub_cellular_device";
const char NetworkDeviceHandlerTest::kDefaultWifiDevicePath[] =
    "/device/stub_wifi_device";

TEST_F(NetworkDeviceHandlerTest, ErrorTranslation) {
  EXPECT_TRUE(result_.empty());
  network_handler::ErrorCallback callback =
      base::Bind(&NetworkDeviceHandlerTest::ErrorCallback,
                 base::Unretained(this));

  network_device_handler_->HandleShillCallFailure(
      kDefaultCellularDevicePath,
      callback,
      "org.chromium.flimflam.Error.Failure",
      "Error happened.");
  EXPECT_EQ(NetworkDeviceHandler::kErrorFailure, result_);

  network_device_handler_->HandleShillCallFailure(
      kDefaultCellularDevicePath,
      callback,
      "org.chromium.flimflam.Error.IncorrectPin",
      "Incorrect pin.");
  EXPECT_EQ(NetworkDeviceHandler::kErrorIncorrectPin, result_);

  network_device_handler_->HandleShillCallFailure(
      kDefaultCellularDevicePath,
      callback,
      "org.chromium.flimflam.Error.NotSupported",
      "Operation not supported.");
  EXPECT_EQ(NetworkDeviceHandler::kErrorNotSupported, result_);

  network_device_handler_->HandleShillCallFailure(
      kDefaultCellularDevicePath,
      callback,
      "org.chromium.flimflam.Error.PinBlocked",
      "PIN is blocked.");
  EXPECT_EQ(NetworkDeviceHandler::kErrorPinBlocked, result_);

  network_device_handler_->HandleShillCallFailure(
      kDefaultCellularDevicePath,
      callback,
      "org.chromium.flimflam.Error.PinRequired",
      "A PIN error has occurred.");
  EXPECT_EQ(NetworkDeviceHandler::kErrorPinRequired, result_);

  network_device_handler_->HandleShillCallFailure(
      kDefaultCellularDevicePath,
      callback,
      "org.chromium.flimflam.Error.WorldExploded",
      "The earth is no more.");
  EXPECT_EQ(NetworkDeviceHandler::kErrorUnknown, result_);
}

TEST_F(NetworkDeviceHandlerTest, SetCarrier) {
  const char kCarrier[] = "carrier";

  // Test that the success callback gets called.
  network_device_handler_->SetCarrier(
      kDefaultCellularDevicePath,
      kCarrier,
      success_callback_,
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error gets properly translated and propagates to the
  // error callback.
  network_device_handler_->SetCarrier(
      kDefaultCellularDevicePath,
      kCarrier,
      GetErrorInvokingCallback(kDefaultCellularDevicePath,
                               "org.chromium.flimflam.Error.NotSupported"),
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorNotSupported, result_);
}

TEST_F(NetworkDeviceHandlerTest, RequirePin) {
  const char kPin[] = "1234";

  // Test that the success callback gets called.
  network_device_handler_->RequirePin(
      kDefaultCellularDevicePath,
      true,
      kPin,
      success_callback_,
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error gets properly translated and propagates to the
  // error callback.
  network_device_handler_->RequirePin(
      kDefaultCellularDevicePath,
      true,
      kPin,
      GetErrorInvokingCallback(kDefaultCellularDevicePath,
                               "org.chromium.flimflam.Error.IncorrectPin"),
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorIncorrectPin, result_);
}

TEST_F(NetworkDeviceHandlerTest, EnterPin) {
  const char kPin[] = "1234";

  // Test that the success callback gets called.
  network_device_handler_->EnterPin(
      kDefaultCellularDevicePath,
      kPin,
      success_callback_,
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error gets properly translated and propagates to the
  // error callback.
  network_device_handler_->EnterPin(
      kDefaultCellularDevicePath,
      kPin,
      GetErrorInvokingCallback(kDefaultCellularDevicePath,
                               "org.chromium.flimflam.Error.IncorrectPin"),
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorIncorrectPin, result_);
}

TEST_F(NetworkDeviceHandlerTest, UnblockPin) {
  const char kPuk[] = "12345678";
  const char kPin[] = "1234";

  // Test that the success callback gets called.
  network_device_handler_->UnblockPin(
      kDefaultCellularDevicePath,
      kPin,
      kPuk,
      success_callback_,
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error gets properly translated and propagates to the
  // error callback.
  network_device_handler_->UnblockPin(
      kDefaultCellularDevicePath,
      kPin,
      kPuk,
      GetErrorInvokingCallback(kDefaultCellularDevicePath,
                               "org.chromium.flimflam.Error.PinRequired"),
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorPinRequired, result_);
}

TEST_F(NetworkDeviceHandlerTest, ChangePin) {
  const char kOldPin[] = "4321";
  const char kNewPin[] = "1234";

  // Test that the success callback gets called.
  network_device_handler_->ChangePin(
      kDefaultCellularDevicePath,
      kOldPin,
      kNewPin,
      success_callback_,
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error gets properly translated and propagates to the
  // error callback.
  network_device_handler_->ChangePin(
      kDefaultCellularDevicePath,
      kOldPin,
      kNewPin,
      GetErrorInvokingCallback(kDefaultCellularDevicePath,
                               "org.chromium.flimflam.Error.PinBlocked"),
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorPinBlocked, result_);
}

}  // namespace chromeos
