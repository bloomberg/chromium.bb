// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kDefaultCellularDevicePath[] = "stub_cellular_device";
const char kUnknownCellularDevicePath[] = "unknown_cellular_device";
const char kDefaultWifiDevicePath[] = "stub_wifi_device";
const char kResultSuccess[] = "success";

}  // namespace

class NetworkDeviceHandlerTest : public testing::Test {
 public:
  NetworkDeviceHandlerTest() {}
  virtual ~NetworkDeviceHandlerTest() {}

  virtual void SetUp() OVERRIDE {
    DBusThreadManager::InitializeWithStub();

    ShillDeviceClient::TestInterface* device_test =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice(
        kDefaultCellularDevicePath, shill::kTypeCellular, "cellular1");
    device_test->AddDevice(kDefaultWifiDevicePath, shill::kTypeWifi, "wifi1");

    base::FundamentalValue allow_roaming(false);
    device_test->SetDeviceProperty(kDefaultCellularDevicePath,
                                   shill::kCellularAllowRoamingProperty,
                                   allow_roaming);

    base::ListValue test_ip_configs;
    test_ip_configs.AppendString("ip_config1");
    device_test->SetDeviceProperty(
        kDefaultWifiDevicePath, shill::kIPConfigsProperty, test_ip_configs);

    success_callback_ = base::Bind(&NetworkDeviceHandlerTest::SuccessCallback,
                                   base::Unretained(this));
    properties_success_callback_ =
        base::Bind(&NetworkDeviceHandlerTest::PropertiesSuccessCallback,
                   base::Unretained(this));
    error_callback_ = base::Bind(&NetworkDeviceHandlerTest::ErrorCallback,
                                 base::Unretained(this));
    network_device_handler_.reset(new NetworkDeviceHandler);
  }

  virtual void TearDown() OVERRIDE {
    network_device_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  void ErrorCallback(const std::string& error_name,
                     scoped_ptr<base::DictionaryValue> error_data) {
    result_ = error_name;
  }

  void SuccessCallback() {
    result_ = kResultSuccess;
  }

  void PropertiesSuccessCallback(const std::string& device_path,
                                 const base::DictionaryValue& properties) {
    result_ = kResultSuccess;
    properties_.reset(properties.DeepCopy());
  }

 protected:
  std::string result_;

  scoped_ptr<NetworkDeviceHandler> network_device_handler_;
  base::MessageLoopForUI message_loop_;
  base::Closure success_callback_;
  network_handler::DictionaryResultCallback properties_success_callback_;
  network_handler::ErrorCallback error_callback_;
  scoped_ptr<base::DictionaryValue> properties_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkDeviceHandlerTest);
};

TEST_F(NetworkDeviceHandlerTest, GetDeviceProperties) {
  network_device_handler_->GetDeviceProperties(
      kDefaultWifiDevicePath, properties_success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  std::string type;
  properties_->GetString(shill::kTypeProperty, &type);
  EXPECT_EQ(shill::kTypeWifi, type);
}

TEST_F(NetworkDeviceHandlerTest, SetDeviceProperty) {
  // Check that GetDeviceProperties returns the expected initial values.
  network_device_handler_->GetDeviceProperties(kDefaultCellularDevicePath,
                                               properties_success_callback_,
                                               error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  bool allow_roaming;
  EXPECT_TRUE(properties_->GetBooleanWithoutPathExpansion(
      shill::kCellularAllowRoamingProperty, &allow_roaming));
  EXPECT_FALSE(allow_roaming);

  // Set the shill::kCellularAllowRoamingProperty to true. The call
  // should succeed and the value should be set.
  base::FundamentalValue allow_roaming_value(true);
  network_device_handler_->SetDeviceProperty(
      kDefaultCellularDevicePath,
      shill::kCellularAllowRoamingProperty,
      allow_roaming_value,
      success_callback_,
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // GetDeviceProperties should return the value set by SetDeviceProperty.
  network_device_handler_->GetDeviceProperties(kDefaultCellularDevicePath,
                                               properties_success_callback_,
                                               error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  EXPECT_TRUE(properties_->GetBooleanWithoutPathExpansion(
      shill::kCellularAllowRoamingProperty, &allow_roaming));
  EXPECT_TRUE(allow_roaming);

  // Set property on an invalid path.
  network_device_handler_->SetDeviceProperty(
      kUnknownCellularDevicePath,
      shill::kCellularAllowRoamingProperty,
      allow_roaming_value,
      success_callback_,
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorFailure, result_);
}

TEST_F(NetworkDeviceHandlerTest, RequestRefreshIPConfigs) {
  network_device_handler_->RequestRefreshIPConfigs(
      kDefaultWifiDevicePath, success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  // TODO(stevenjb): Add test interface to ShillIPConfigClient and test
  // refresh calls.
}

TEST_F(NetworkDeviceHandlerTest, SetCarrier) {
  const char kCarrier[] = "carrier";

  // Test that the success callback gets called.
  network_device_handler_->SetCarrier(
      kDefaultCellularDevicePath, kCarrier, success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->SetCarrier(
      kUnknownCellularDevicePath, kCarrier, success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorFailure, result_);
}

TEST_F(NetworkDeviceHandlerTest, RequirePin) {
  const char kPin[] = "1234";

  // Test that the success callback gets called.
  network_device_handler_->RequirePin(kDefaultCellularDevicePath,
                                      true,
                                      kPin,
                                      success_callback_,
                                      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->RequirePin(kUnknownCellularDevicePath,
                                      true,
                                      kPin,
                                      success_callback_,
                                      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorFailure, result_);
}

TEST_F(NetworkDeviceHandlerTest, EnterPin) {
  const char kPin[] = "1234";

  // Test that the success callback gets called.
  network_device_handler_->EnterPin(
      kDefaultCellularDevicePath, kPin, success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->EnterPin(
      kUnknownCellularDevicePath, kPin, success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorFailure, result_);
}

TEST_F(NetworkDeviceHandlerTest, UnblockPin) {
  const char kPuk[] = "12345678";
  const char kPin[] = "1234";

  // Test that the success callback gets called.
  network_device_handler_->UnblockPin(kDefaultCellularDevicePath,
                                      kPin,
                                      kPuk,
                                      success_callback_,
                                      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->UnblockPin(kUnknownCellularDevicePath,
                                      kPin,
                                      kPuk,
                                      success_callback_,
                                      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorFailure, result_);
}

TEST_F(NetworkDeviceHandlerTest, ChangePin) {
  const char kOldPin[] = "4321";
  const char kNewPin[] = "1234";

  // Test that the success callback gets called.
  network_device_handler_->ChangePin(kDefaultCellularDevicePath,
                                     kOldPin,
                                     kNewPin,
                                     success_callback_,
                                     error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->ChangePin(kUnknownCellularDevicePath,
                                     kOldPin,
                                     kNewPin,
                                     success_callback_,
                                     error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorFailure, result_);
}

}  // namespace chromeos
