// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_shill_device_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/network/network_device_handler_impl.h"
#include "chromeos/network/network_state_handler.h"
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
  NetworkDeviceHandlerTest() : fake_device_client_(NULL) {}
  virtual ~NetworkDeviceHandlerTest() {}

  virtual void SetUp() OVERRIDE {
    FakeDBusThreadManager* dbus_manager = new FakeDBusThreadManager;
    dbus_manager->SetFakeShillClients();

    fake_device_client_ = new FakeShillDeviceClient;
    dbus_manager->SetShillDeviceClient(
        scoped_ptr<ShillDeviceClient>(fake_device_client_));
    DBusThreadManager::InitializeForTesting(dbus_manager);

    success_callback_ = base::Bind(&NetworkDeviceHandlerTest::SuccessCallback,
                                   base::Unretained(this));
    properties_success_callback_ =
        base::Bind(&NetworkDeviceHandlerTest::PropertiesSuccessCallback,
                   base::Unretained(this));
    string_success_callback_ =
        base::Bind(&NetworkDeviceHandlerTest::StringSuccessCallback,
                   base::Unretained(this));
    error_callback_ = base::Bind(&NetworkDeviceHandlerTest::ErrorCallback,
                                 base::Unretained(this));

    network_state_handler_.reset(NetworkStateHandler::InitializeForTest());
    NetworkDeviceHandlerImpl* device_handler = new NetworkDeviceHandlerImpl;
    device_handler->Init(network_state_handler_.get());
    network_device_handler_.reset(device_handler);

    // Add devices after handlers have been initialized.
    ShillDeviceClient::TestInterface* device_test =
        fake_device_client_->GetTestInterface();
    device_test->AddDevice(
        kDefaultCellularDevicePath, shill::kTypeCellular, "cellular1");
    device_test->AddDevice(kDefaultWifiDevicePath, shill::kTypeWifi, "wifi1");

    base::ListValue test_ip_configs;
    test_ip_configs.AppendString("ip_config1");
    device_test->SetDeviceProperty(
        kDefaultWifiDevicePath, shill::kIPConfigsProperty, test_ip_configs);

    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    network_device_handler_.reset();
    network_state_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  void ErrorCallback(const std::string& error_name,
                     scoped_ptr<base::DictionaryValue> error_data) {
    LOG(ERROR) << "ErrorCallback: " << error_name;
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

  void StringSuccessCallback(const std::string& result) {
    LOG(ERROR) << "StringSuccessCallback: " << result;
    result_ = kResultSuccess;
  }

 protected:
  std::string result_;

  FakeShillDeviceClient* fake_device_client_;
  scoped_ptr<NetworkDeviceHandler> network_device_handler_;
  scoped_ptr<NetworkStateHandler> network_state_handler_;
  base::MessageLoopForUI message_loop_;
  base::Closure success_callback_;
  network_handler::DictionaryResultCallback properties_success_callback_;
  network_handler::StringResultCallback string_success_callback_;
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
  // Set the shill::kScanIntervalProperty to true. The call
  // should succeed and the value should be set.
  network_device_handler_->SetDeviceProperty(kDefaultCellularDevicePath,
                                             shill::kScanIntervalProperty,
                                             base::FundamentalValue(1),
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
  int interval = 0;
  EXPECT_TRUE(properties_->GetIntegerWithoutPathExpansion(
      shill::kScanIntervalProperty, &interval));
  EXPECT_EQ(1, interval);

  // Repeat the same with value false.
  network_device_handler_->SetDeviceProperty(kDefaultCellularDevicePath,
                                             shill::kScanIntervalProperty,
                                             base::FundamentalValue(2),
                                             success_callback_,
                                             error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  network_device_handler_->GetDeviceProperties(kDefaultCellularDevicePath,
                                               properties_success_callback_,
                                               error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  EXPECT_TRUE(properties_->GetIntegerWithoutPathExpansion(
      shill::kScanIntervalProperty, &interval));
  EXPECT_EQ(2, interval);

  // Set property on an invalid path.
  network_device_handler_->SetDeviceProperty(kUnknownCellularDevicePath,
                                             shill::kScanIntervalProperty,
                                             base::FundamentalValue(1),
                                             success_callback_,
                                             error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);

  // Setting a owner-protected device property through SetDeviceProperty must
  // fail.
  network_device_handler_->SetDeviceProperty(
      kDefaultCellularDevicePath,
      shill::kCellularAllowRoamingProperty,
      base::FundamentalValue(true),
      success_callback_,
      error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_NE(kResultSuccess, result_);

}

TEST_F(NetworkDeviceHandlerTest, CellularAllowRoaming) {
  // Start with disabled data roaming.
  ShillDeviceClient::TestInterface* device_test =
      fake_device_client_->GetTestInterface();
  device_test->SetDeviceProperty(kDefaultCellularDevicePath,
                                 shill::kCellularAllowRoamingProperty,
                                 base::FundamentalValue(false));

  network_device_handler_->SetCellularAllowRoaming(true);
  message_loop_.RunUntilIdle();

  // Roaming should be enabled now.
  network_device_handler_->GetDeviceProperties(kDefaultCellularDevicePath,
                                               properties_success_callback_,
                                               error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  bool allow_roaming;
  EXPECT_TRUE(properties_->GetBooleanWithoutPathExpansion(
      shill::kCellularAllowRoamingProperty, &allow_roaming));
  EXPECT_TRUE(allow_roaming);

  network_device_handler_->SetCellularAllowRoaming(false);
  message_loop_.RunUntilIdle();

  // Roaming should be disable again.
  network_device_handler_->GetDeviceProperties(kDefaultCellularDevicePath,
                                               properties_success_callback_,
                                               error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  EXPECT_TRUE(properties_->GetBooleanWithoutPathExpansion(
      shill::kCellularAllowRoamingProperty, &allow_roaming));
  EXPECT_FALSE(allow_roaming);
}

TEST_F(NetworkDeviceHandlerTest, SetWifiTDLSEnabled) {
  // We add a wifi device by default, initial call should succeed.
  network_device_handler_->SetWifiTDLSEnabled(
      "fake_ip_address", true, string_success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
}

TEST_F(NetworkDeviceHandlerTest, SetWifiTDLSEnabledMissing) {
  // Remove the wifi device. Call should fail with "device missing" error.
  fake_device_client_->GetTestInterface()->RemoveDevice(kDefaultWifiDevicePath);
  message_loop_.RunUntilIdle();
  network_device_handler_->SetWifiTDLSEnabled(
      "fake_ip_address", true, string_success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
}

TEST_F(NetworkDeviceHandlerTest, SetWifiTDLSEnabledBusy) {
  // Set the busy count, call should succeed after repeat attempt.
  fake_device_client_->set_tdls_busy_count(1);
  network_device_handler_->SetWifiTDLSEnabled(
      "fake_ip_address", true, string_success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Set the busy count to a large number, call should fail after max number
  // of repeat attempt.
  fake_device_client_->set_tdls_busy_count(100000);
  network_device_handler_->SetWifiTDLSEnabled(
      "fake_ip_address", true, string_success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorTimeout, result_);
}

TEST_F(NetworkDeviceHandlerTest, GetWifiTDLSStatus) {
  // We add a wifi device by default, initial call should succeed.
  network_device_handler_->GetWifiTDLSStatus(
      "fake_ip_address", string_success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // Remove the wifi device. Call should fail with "device missing" error.
  fake_device_client_->GetTestInterface()->RemoveDevice(kDefaultWifiDevicePath);
  message_loop_.RunUntilIdle();
  network_device_handler_->GetWifiTDLSStatus(
      "fake_ip_address", string_success_callback_, error_callback_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
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
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
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
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
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
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
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
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
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
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
}

}  // namespace chromeos
