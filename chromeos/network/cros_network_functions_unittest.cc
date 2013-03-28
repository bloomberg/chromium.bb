// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_gsm_sms_client.h"
#include "chromeos/dbus/mock_shill_device_client.h"
#include "chromeos/dbus/mock_shill_ipconfig_client.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/mock_shill_service_client.h"
#include "chromeos/network/cros_network_functions.h"
#include "chromeos/network/sms_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;

// Matcher to match base::Value.
MATCHER_P(IsEqualTo, value, "") { return arg.Equals(value); }

// Matcher to match SMS.
MATCHER_P(IsSMSEqualTo, sms, "") {
  return sms.timestamp == arg.timestamp &&
      std::string(sms.number) == arg.number &&
      std::string(sms.text) == arg.text &&
      sms.validity == arg.validity &&
      sms.msgclass == arg.msgclass;
}

// Matcher to match IPConfig::path
MATCHER_P(IsIPConfigPathEqualTo, str, "") { return str == arg.path; }

namespace chromeos {

namespace {

const char kExamplePath[] = "/foo/bar/baz";

// A mock to check arguments of NetworkPropertiesCallback and ensure that the
// callback is called exactly once.
class MockNetworkPropertiesCallback {
 public:
  // Creates a NetworkPropertiesCallback with expectations.
  static NetworkPropertiesCallback CreateCallback(
      const std::string& expected_path,
      const base::DictionaryValue& expected_result) {
    MockNetworkPropertiesCallback* mock_callback =
        new MockNetworkPropertiesCallback;

    EXPECT_CALL(*mock_callback,
                Run(expected_path, Pointee(IsEqualTo(&expected_result))))
        .Times(1);

    return base::Bind(&MockNetworkPropertiesCallback::Run,
                      base::Owned(mock_callback));
  }

  MOCK_METHOD2(Run, void(const std::string& path,
                         const base::DictionaryValue* result));
};

// A mock to check arguments of NetworkPropertiesWatcherCallback and ensure that
// the callback is called exactly once.
class MockNetworkPropertiesWatcherCallback {
 public:
  // Creates a NetworkPropertiesWatcherCallback with expectations.
  static NetworkPropertiesWatcherCallback CreateCallback(
      const std::string& expected_path,
      const std::string& expected_key,
      const base::Value& expected_value) {
    MockNetworkPropertiesWatcherCallback* mock_callback =
        new MockNetworkPropertiesWatcherCallback;

    EXPECT_CALL(*mock_callback,
                Run(expected_path, expected_key, IsEqualTo(&expected_value)))
        .Times(1);

    return base::Bind(&MockNetworkPropertiesWatcherCallback::Run,
                      base::Owned(mock_callback));
  }

  MOCK_METHOD3(Run, void(const std::string& expected_path,
                         const std::string& expected_key,
                         const base::Value& value));
};

}  // namespace

// Test for cros_network_functions.cc without Libcros.
class CrosNetworkFunctionsTest : public testing::Test {
 public:
  CrosNetworkFunctionsTest() : mock_profile_client_(NULL),
                               dictionary_value_result_(NULL) {}

  virtual void SetUp() {
    MockDBusThreadManager* mock_dbus_thread_manager = new MockDBusThreadManager;
    EXPECT_CALL(*mock_dbus_thread_manager, GetSystemBus())
        .WillRepeatedly(Return(reinterpret_cast<dbus::Bus*>(NULL)));
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    mock_device_client_ =
        mock_dbus_thread_manager->mock_shill_device_client();
    mock_ipconfig_client_ =
        mock_dbus_thread_manager->mock_shill_ipconfig_client();
    mock_manager_client_ =
        mock_dbus_thread_manager->mock_shill_manager_client();
    mock_profile_client_ =
        mock_dbus_thread_manager->mock_shill_profile_client();
    mock_service_client_ =
        mock_dbus_thread_manager->mock_shill_service_client();
    mock_gsm_sms_client_ = mock_dbus_thread_manager->mock_gsm_sms_client();
  }

  virtual void TearDown() {
    DBusThreadManager::Shutdown();
    mock_profile_client_ = NULL;
  }

  // Handles responses for GetProperties method calls for ShillManagerClient.
  void OnGetManagerProperties(
      const ShillClientHelper::DictionaryValueCallback& callback) {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *dictionary_value_result_);
  }

  // Handles responses for GetProperties method calls.
  void OnGetProperties(
      const dbus::ObjectPath& path,
      const ShillClientHelper::DictionaryValueCallback& callback) {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *dictionary_value_result_);
  }

  // Handles responses for GetProperties method calls that return
  // errors in an error callback.
  void OnGetPropertiesWithoutStatus(
      const dbus::ObjectPath& path,
      const ShillClientHelper::DictionaryValueCallbackWithoutStatus& callback,
      const ShillClientHelper::ErrorCallback& error_callback) {
    callback.Run(*dictionary_value_result_);
  }

  // Handles responses for GetEntry method calls.
  void OnGetEntry(
      const dbus::ObjectPath& profile_path,
      const std::string& entry_path,
      const ShillClientHelper::DictionaryValueCallbackWithoutStatus& callback,
      const ShillClientHelper::ErrorCallback& error_callback) {
    callback.Run(*dictionary_value_result_);
  }

  // Mock NetworkOperationCallback.
  MOCK_METHOD3(MockNetworkOperationCallback,
               void(const std::string& path,
                    NetworkMethodErrorType error,
                    const std::string& error_message));

  // Mock MonitorSMSCallback.
  MOCK_METHOD2(MockMonitorSMSCallback,
               void(const std::string& modem_device_path, const SMS& message));

 protected:
  MockShillDeviceClient* mock_device_client_;
  MockShillIPConfigClient* mock_ipconfig_client_;
  MockShillManagerClient* mock_manager_client_;
  MockShillProfileClient* mock_profile_client_;
  MockShillServiceClient* mock_service_client_;
  MockGsmSMSClient* mock_gsm_sms_client_;
  const base::DictionaryValue* dictionary_value_result_;
};

TEST_F(CrosNetworkFunctionsTest, CrosActivateCellularModem) {
  const std::string service_path = "/";
  const std::string carrier = "carrier";
  EXPECT_CALL(*mock_service_client_,
              CallActivateCellularModemAndBlock(dbus::ObjectPath(service_path),
                                                carrier))
      .WillOnce(Return(true));
  EXPECT_TRUE(CrosActivateCellularModem(service_path, carrier));
}

TEST_F(CrosNetworkFunctionsTest, CrosCompleteCellularActivation) {
  const std::string service_path = "/";
  EXPECT_CALL(*mock_service_client_,
              CompleteCellularActivation(dbus::ObjectPath(service_path), _, _))
      .Times(1);

  CrosCompleteCellularActivation(service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosSetNetworkServiceProperty) {
  const std::string service_path = "/";
  const std::string property = "property";
  const std::string key1 = "key1";
  const std::string string1 = "string1";
  const std::string key2 = "key2";
  const std::string string2 = "string2";
  base::DictionaryValue value;
  value.SetString(key1, string1);
  value.SetString(key2, string2);
  EXPECT_CALL(*mock_service_client_,
              SetProperty(dbus::ObjectPath(service_path), property,
                          IsEqualTo(&value), _, _)).Times(1);

  CrosSetNetworkServiceProperty(service_path, property, value);
}

TEST_F(CrosNetworkFunctionsTest, CrosClearNetworkServiceProperty) {
  const std::string service_path = "/";
  const std::string property = "property";
  EXPECT_CALL(*mock_service_client_,
              ClearProperty(dbus::ObjectPath(service_path), property, _, _))
      .Times(1);

  CrosClearNetworkServiceProperty(service_path, property);
}

TEST_F(CrosNetworkFunctionsTest, CrosSetNetworkDeviceProperty) {
  const std::string device_path = "/";
  const std::string property = "property";
  const bool kBool = true;
  const base::FundamentalValue value(kBool);
  EXPECT_CALL(*mock_device_client_,
              SetProperty(dbus::ObjectPath(device_path), StrEq(property),
                          IsEqualTo(&value), _, _)).Times(1);

  CrosSetNetworkDeviceProperty(device_path, property, value);
}

TEST_F(CrosNetworkFunctionsTest, CrosSetNetworkIPConfigProperty) {
  const std::string ipconfig_path = "/";
  const std::string property = "property";
  const int kInt = 1234;
  const base::FundamentalValue value(kInt);
  EXPECT_CALL(*mock_ipconfig_client_,
              SetProperty(dbus::ObjectPath(ipconfig_path), property,
                          IsEqualTo(&value), _)).Times(1);
  CrosSetNetworkIPConfigProperty(ipconfig_path, property, value);
}

TEST_F(CrosNetworkFunctionsTest, CrosSetNetworkManagerProperty) {
  const std::string property = "property";
  const base::StringValue value("string");
  EXPECT_CALL(*mock_manager_client_,
              SetProperty(property, IsEqualTo(&value), _, _)).Times(1);

  CrosSetNetworkManagerProperty(property, value);
}

TEST_F(CrosNetworkFunctionsTest, CrosDeleteServiceFromProfile) {
  const std::string profile_path("/profile/path");
  const std::string service_path("/service/path");
  EXPECT_CALL(*mock_profile_client_,
              DeleteEntry(dbus::ObjectPath(profile_path), service_path, _, _))
      .Times(1);
  CrosDeleteServiceFromProfile(profile_path, service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosMonitorNetworkManagerProperties) {
  const std::string key = "key";
  const int kValue = 42;
  const base::FundamentalValue value(kValue);

  // Start monitoring.
  ShillPropertyChangedObserver* observer = NULL;
  EXPECT_CALL(*mock_manager_client_, AddPropertyChangedObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  CrosNetworkWatcher* watcher = CrosMonitorNetworkManagerProperties(
      MockNetworkPropertiesWatcherCallback::CreateCallback(
          flimflam::kFlimflamServicePath, key, value));
  // Call callback.
  observer->OnPropertyChanged(key, value);
  // Stop monitoring.
  EXPECT_CALL(*mock_manager_client_,
              RemovePropertyChangedObserver(_)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsTest, CrosMonitorNetworkServiceProperties) {
  const dbus::ObjectPath path("/path");
  const std::string key = "key";
  const int kValue = 42;
  const base::FundamentalValue value(kValue);
  // Start monitoring.
  ShillPropertyChangedObserver* observer = NULL;
  EXPECT_CALL(*mock_service_client_, AddPropertyChangedObserver(path, _))
      .WillOnce(SaveArg<1>(&observer));
  NetworkPropertiesWatcherCallback callback =
      MockNetworkPropertiesWatcherCallback::CreateCallback(path.value(),
                                                           key, value);
  CrosNetworkWatcher* watcher = CrosMonitorNetworkServiceProperties(
      callback, path.value());
  // Call callback.
  observer->OnPropertyChanged(key, value);
  // Stop monitoring.
  EXPECT_CALL(*mock_service_client_,
              RemovePropertyChangedObserver(path, _)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsTest, CrosMonitorNetworkDeviceProperties) {
  const dbus::ObjectPath path("/path");
  const std::string key = "key";
  const int kValue = 42;
  const base::FundamentalValue value(kValue);
  // Start monitoring.
  ShillPropertyChangedObserver* observer = NULL;
  EXPECT_CALL(*mock_device_client_, AddPropertyChangedObserver(path, _))
      .WillOnce(SaveArg<1>(&observer));
  NetworkPropertiesWatcherCallback callback =
      MockNetworkPropertiesWatcherCallback::CreateCallback(path.value(),
                                                           key, value);
  CrosNetworkWatcher* watcher = CrosMonitorNetworkDeviceProperties(
      callback, path.value());
  // Call callback.
  observer->OnPropertyChanged(key, value);
  // Stop monitoring.
  EXPECT_CALL(*mock_device_client_,
              RemovePropertyChangedObserver(path, _)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsTest, CrosMonitorSMS) {
  const std::string dbus_connection = ":1.1";
  const dbus::ObjectPath object_path("/object/path");
  base::DictionaryValue device_properties;
  device_properties.SetWithoutPathExpansion(
      flimflam::kDBusConnectionProperty,
      new base::StringValue(dbus_connection));
  device_properties.SetWithoutPathExpansion(
      flimflam::kDBusObjectProperty,
      new base::StringValue(object_path.value()));

  const std::string number = "0123456789";
  const std::string text = "Hello.";
  const std::string timestamp_string =
      "120424123456+00";  // 2012-04-24 12:34:56
  base::Time::Exploded timestamp_exploded = {};
  timestamp_exploded.year = 2012;
  timestamp_exploded.month = 4;
  timestamp_exploded.day_of_month = 24;
  timestamp_exploded.hour = 12;
  timestamp_exploded.minute = 34;
  timestamp_exploded.second = 56;
  const base::Time timestamp = base::Time::FromUTCExploded(timestamp_exploded);
  const std::string smsc = "9876543210";
  const uint32 kValidity = 1;
  const uint32 kMsgclass = 2;
  const uint32 kIndex = 0;
  const bool kComplete = true;
  base::DictionaryValue* sms_dictionary = new base::DictionaryValue;
  sms_dictionary->SetWithoutPathExpansion(
      SMSWatcher::kNumberKey, new base::StringValue(number));
  sms_dictionary->SetWithoutPathExpansion(
      SMSWatcher::kTextKey, new base::StringValue(text));
  sms_dictionary->SetWithoutPathExpansion(
      SMSWatcher::kTimestampKey,
      new base::StringValue(timestamp_string));
  sms_dictionary->SetWithoutPathExpansion(SMSWatcher::kSmscKey,
                                         new base::StringValue(smsc));
  sms_dictionary->SetWithoutPathExpansion(
      SMSWatcher::kValidityKey, base::Value::CreateDoubleValue(kValidity));
  sms_dictionary->SetWithoutPathExpansion(
      SMSWatcher::kClassKey, base::Value::CreateDoubleValue(kMsgclass));
  sms_dictionary->SetWithoutPathExpansion(
      SMSWatcher::kIndexKey, base::Value::CreateDoubleValue(kIndex));

  base::ListValue sms_list;
  sms_list.Append(sms_dictionary);

  SMS sms;
  sms.timestamp = timestamp;
  sms.number = number.c_str();
  sms.text = text.c_str();
  sms.smsc = smsc.c_str();
  sms.validity = kValidity;
  sms.msgclass = kMsgclass;

  const std::string modem_device_path = "/modem/device/path";

  // Set expectations.
  ShillDeviceClient::DictionaryValueCallback get_properties_callback;
  EXPECT_CALL(*mock_device_client_,
              GetProperties(dbus::ObjectPath(modem_device_path), _))
      .WillOnce(SaveArg<1>(&get_properties_callback));
  GsmSMSClient::SmsReceivedHandler sms_received_handler;
  EXPECT_CALL(*mock_gsm_sms_client_,
              SetSmsReceivedHandler(dbus_connection, object_path, _))
      .WillOnce(SaveArg<2>(&sms_received_handler));

  GsmSMSClient::ListCallback list_callback;
  EXPECT_CALL(*mock_gsm_sms_client_, List(dbus_connection, object_path, _))
      .WillOnce(SaveArg<2>(&list_callback));

  EXPECT_CALL(*this, MockMonitorSMSCallback(
      modem_device_path, IsSMSEqualTo(sms))).Times(2);

  GsmSMSClient::DeleteCallback delete_callback;
  EXPECT_CALL(*mock_gsm_sms_client_,
              Delete(dbus_connection, object_path, kIndex, _))
      .WillRepeatedly(SaveArg<3>(&delete_callback));

  GsmSMSClient::GetCallback get_callback;
  EXPECT_CALL(*mock_gsm_sms_client_,
              Get(dbus_connection, object_path, kIndex, _))
      .WillOnce(SaveArg<3>(&get_callback));

  // Start monitoring.
  CrosNetworkWatcher* watcher = CrosMonitorSMS(
      modem_device_path,
      base::Bind(&CrosNetworkFunctionsTest::MockMonitorSMSCallback,
                 base::Unretained(this)));
  // Return GetProperties() result.
  get_properties_callback.Run(DBUS_METHOD_CALL_SUCCESS, device_properties);
  // Return List() result.
  ASSERT_FALSE(list_callback.is_null());
  list_callback.Run(sms_list);
  // Return Delete() result.
  ASSERT_FALSE(delete_callback.is_null());
  delete_callback.Run();
  // Send fake signal.
  ASSERT_FALSE(sms_received_handler.is_null());
  sms_received_handler.Run(kIndex, kComplete);
  // Return Get() result.
  ASSERT_FALSE(get_callback.is_null());
  get_callback.Run(*sms_dictionary);
  // Return Delete() result.
  ASSERT_FALSE(delete_callback.is_null());
  delete_callback.Run();
  // Stop monitoring.
  EXPECT_CALL(*mock_gsm_sms_client_,
              ResetSmsReceivedHandler(dbus_connection, object_path)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkManagerProperties) {
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, new base::StringValue(value1));
  result.SetWithoutPathExpansion(key2, new base::StringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_manager_client_,
              GetProperties(_)).WillOnce(
                  Invoke(this,
                         &CrosNetworkFunctionsTest::OnGetManagerProperties));

  CrosRequestNetworkManagerProperties(
      MockNetworkPropertiesCallback::CreateCallback(
          flimflam::kFlimflamServicePath, result));
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkServiceProperties) {
  const std::string service_path = "/service/path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, new base::StringValue(value1));
  result.SetWithoutPathExpansion(key2, new base::StringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_service_client_,
              GetProperties(dbus::ObjectPath(service_path), _)).WillOnce(
                  Invoke(this, &CrosNetworkFunctionsTest::OnGetProperties));

  CrosRequestNetworkServiceProperties(
      service_path,
      MockNetworkPropertiesCallback::CreateCallback(service_path, result));
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkDeviceProperties) {
  const std::string device_path = "/device/path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, new base::StringValue(value1));
  result.SetWithoutPathExpansion(key2, new base::StringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_device_client_,
              GetProperties(dbus::ObjectPath(device_path), _)).WillOnce(
                  Invoke(this, &CrosNetworkFunctionsTest::OnGetProperties));

  CrosRequestNetworkDeviceProperties(
      device_path,
      MockNetworkPropertiesCallback::CreateCallback(device_path, result));
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkProfileProperties) {
  const std::string profile_path = "/profile/path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, new base::StringValue(value1));
  result.SetWithoutPathExpansion(key2, new base::StringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(
      *mock_profile_client_,
      GetProperties(dbus::ObjectPath(profile_path), _, _)).WillOnce(
          Invoke(this,
                 &CrosNetworkFunctionsTest::OnGetPropertiesWithoutStatus));

  CrosRequestNetworkProfileProperties(
      profile_path,
      MockNetworkPropertiesCallback::CreateCallback(profile_path, result));
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkProfileEntryProperties) {
  const std::string profile_path = "profile path";
  const std::string profile_entry_path = "profile entry path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, new base::StringValue(value1));
  result.SetWithoutPathExpansion(key2, new base::StringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_profile_client_,
              GetEntry(dbus::ObjectPath(profile_path),
                       profile_entry_path, _, _))
      .WillOnce(Invoke(this, &CrosNetworkFunctionsTest::OnGetEntry));

  CrosRequestNetworkProfileEntryProperties(
      profile_path, profile_entry_path,
      MockNetworkPropertiesCallback::CreateCallback(profile_entry_path,
                                                    result));
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestHiddenWifiNetworkProperties) {
  const std::string ssid = "ssid";
  const std::string security = "security";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, new base::StringValue(value1));
  result.SetWithoutPathExpansion(key2, new base::StringValue(value2));
  dictionary_value_result_ = &result;
  // Create expected argument to ShillManagerClient::GetService.
  base::DictionaryValue properties;
  properties.SetWithoutPathExpansion(
      flimflam::kModeProperty,
      new base::StringValue(flimflam::kModeManaged));
  properties.SetWithoutPathExpansion(
      flimflam::kTypeProperty,
      new base::StringValue(flimflam::kTypeWifi));
  properties.SetWithoutPathExpansion(
      flimflam::kSSIDProperty,
      new base::StringValue(ssid));
  properties.SetWithoutPathExpansion(
      flimflam::kSecurityProperty,
      new base::StringValue(security));
  // Set expectations.
  const dbus::ObjectPath service_path("/service/path");
  ObjectPathCallback callback;
  EXPECT_CALL(*mock_manager_client_, GetService(IsEqualTo(&properties), _, _))
      .WillOnce(SaveArg<1>(&callback));
  EXPECT_CALL(*mock_service_client_,
              GetProperties(service_path, _)).WillOnce(
                  Invoke(this, &CrosNetworkFunctionsTest::OnGetProperties));

  // Call function.
  CrosRequestHiddenWifiNetworkProperties(
      ssid, security,
      MockNetworkPropertiesCallback::CreateCallback(service_path.value(),
                                                    result));
  // Run callback to invoke GetProperties.
  callback.Run(service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestVirtualNetworkProperties) {
  const std::string service_name = "service name";
  const std::string server_hostname = "server hostname";
  const std::string provider_type = "provider type";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, new base::StringValue(value1));
  result.SetWithoutPathExpansion(key2, new base::StringValue(value2));
  dictionary_value_result_ = &result;
  // Create expected argument to ShillManagerClient::ConfigureService.
  base::DictionaryValue properties;
  properties.SetWithoutPathExpansion(
      flimflam::kTypeProperty, new base::StringValue("vpn"));
  properties.SetWithoutPathExpansion(
      flimflam::kNameProperty,
      new base::StringValue(service_name));
  properties.SetWithoutPathExpansion(
      flimflam::kProviderHostProperty,
      new base::StringValue(server_hostname));
  properties.SetWithoutPathExpansion(
      flimflam::kProviderTypeProperty,
      new base::StringValue(provider_type));

  // Set expectations.
  const dbus::ObjectPath service_path("/service/path");
  ObjectPathCallback callback;
  EXPECT_CALL(*mock_manager_client_,
              ConfigureService(IsEqualTo(&properties), _, _))
      .WillOnce(SaveArg<1>(&callback));
  EXPECT_CALL(*mock_service_client_,
              GetProperties(service_path, _)).WillOnce(
                  Invoke(this, &CrosNetworkFunctionsTest::OnGetProperties));

  // Call function.
  CrosRequestVirtualNetworkProperties(
      service_name, server_hostname, provider_type,
      MockNetworkPropertiesCallback::CreateCallback(service_path.value(),
                                                    result));
  // Run callback to invoke GetProperties.
  callback.Run(service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkServiceDisconnect) {
  const std::string service_path = "/service/path";
  EXPECT_CALL(*mock_service_client_,
              Disconnect(dbus::ObjectPath(service_path), _, _)).Times(1);
  CrosRequestNetworkServiceDisconnect(service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestRemoveNetworkService) {
  const std::string service_path = "/service/path";
  EXPECT_CALL(*mock_service_client_,
              Remove(dbus::ObjectPath(service_path), _, _)).Times(1);
  CrosRequestRemoveNetworkService(service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkScan) {
  EXPECT_CALL(*mock_manager_client_,
              RequestScan(flimflam::kTypeWifi, _, _)).Times(1);
  CrosRequestNetworkScan(flimflam::kTypeWifi);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkDeviceEnable) {
  const bool kEnable = true;
  EXPECT_CALL(*mock_manager_client_,
              EnableTechnology(flimflam::kTypeWifi, _, _)).Times(1);
  CrosRequestNetworkDeviceEnable(flimflam::kTypeWifi, kEnable);

  const bool kDisable = false;
  EXPECT_CALL(*mock_manager_client_,
              DisableTechnology(flimflam::kTypeWifi, _, _)).Times(1);
  CrosRequestNetworkDeviceEnable(flimflam::kTypeWifi, kDisable);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestRequirePin) {
  const std::string device_path = "/device/path";
  const std::string pin = "123456";
  const bool kRequire = true;

  // Set expectations.
  base::Closure callback;
  EXPECT_CALL(*mock_device_client_,
              RequirePin(dbus::ObjectPath(device_path), pin, kRequire, _, _))
      .WillOnce(SaveArg<3>(&callback));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);
  CrosRequestRequirePin(
      device_path, pin, kRequire,
      base::Bind(&CrosNetworkFunctionsTest::MockNetworkOperationCallback,
                 base::Unretained(this)));
  // Run saved callback.
  callback.Run();
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestEnterPin) {
  const std::string device_path = "/device/path";
  const std::string pin = "123456";

  // Set expectations.
  base::Closure callback;
  EXPECT_CALL(*mock_device_client_,
              EnterPin(dbus::ObjectPath(device_path), pin, _, _))
      .WillOnce(SaveArg<2>(&callback));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);
  CrosRequestEnterPin(
      device_path, pin,
      base::Bind(&CrosNetworkFunctionsTest::MockNetworkOperationCallback,
                 base::Unretained(this)));
  // Run saved callback.
  callback.Run();
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestUnblockPin) {
  const std::string device_path = "/device/path";
  const std::string unblock_code = "987654";
  const std::string pin = "123456";

  // Set expectations.
  base::Closure callback;
  EXPECT_CALL(
      *mock_device_client_,
      UnblockPin(dbus::ObjectPath(device_path), unblock_code, pin, _, _))
      .WillOnce(SaveArg<3>(&callback));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);
  CrosRequestUnblockPin(device_path, unblock_code, pin,
      base::Bind(&CrosNetworkFunctionsTest::MockNetworkOperationCallback,
                 base::Unretained(this)));
  // Run saved callback.
  callback.Run();
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestChangePin) {
  const std::string device_path = "/device/path";
  const std::string old_pin = "123456";
  const std::string new_pin = "234567";

  // Set expectations.
  base::Closure callback;
  EXPECT_CALL(*mock_device_client_,
              ChangePin(dbus::ObjectPath(device_path), old_pin, new_pin,  _, _))
      .WillOnce(SaveArg<3>(&callback));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);
  CrosRequestChangePin(device_path, old_pin, new_pin,
      base::Bind(&CrosNetworkFunctionsTest::MockNetworkOperationCallback,
                 base::Unretained(this)));
  // Run saved callback.
  callback.Run();
}

TEST_F(CrosNetworkFunctionsTest, CrosProposeScan) {
  const std::string device_path = "/device/path";
  EXPECT_CALL(*mock_device_client_,
              ProposeScan(dbus::ObjectPath(device_path), _)).Times(1);
  CrosProposeScan(device_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestCellularRegister) {
  const std::string device_path = "/device/path";
  const std::string network_id = "networkid";

  // Set expectations.
  base::Closure callback;
  EXPECT_CALL(*mock_device_client_,
              Register(dbus::ObjectPath(device_path), network_id, _, _))
      .WillOnce(SaveArg<2>(&callback));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);
  CrosRequestCellularRegister(device_path, network_id,
      base::Bind(&CrosNetworkFunctionsTest::MockNetworkOperationCallback,
                 base::Unretained(this)));
  // Run saved callback.
  callback.Run();
}

TEST_F(CrosNetworkFunctionsTest, CrosSetOfflineMode) {
  const bool kOffline = true;
  const base::FundamentalValue value(kOffline);
  EXPECT_CALL(*mock_manager_client_, SetProperty(
      flimflam::kOfflineModeProperty, IsEqualTo(&value), _, _)).Times(1);
  CrosSetOfflineMode(kOffline);
}

TEST_F(CrosNetworkFunctionsTest, CrosListIPConfigsAndBlock) {
  const std::string device_path = "/device/path";
  std::string ipconfig_path = "/ipconfig/path";

  const IPConfigType kType = IPCONFIG_TYPE_DHCP;
  const std::string address = "address";
  const int kMtu = 123;
  const int kPrefixlen = 24;
  const std::string netmask = "255.255.255.0";
  const std::string broadcast = "broadcast";
  const std::string peer_address = "peer address";
  const std::string gateway = "gateway";
  const std::string domainname = "domainname";
  const std::string name_server1 = "nameserver1";
  const std::string name_server2 = "nameserver2";
  const std::string name_servers = name_server1 + "," + name_server2;
  const std::string hardware_address = "hardware address";

  base::ListValue* ipconfigs = new base::ListValue;
  ipconfigs->Append(new base::StringValue(ipconfig_path));
  base::DictionaryValue* device_properties = new base::DictionaryValue;
  device_properties->SetWithoutPathExpansion(
      flimflam::kIPConfigsProperty, ipconfigs);
  device_properties->SetWithoutPathExpansion(
      flimflam::kAddressProperty,
      new base::StringValue(hardware_address));

  base::ListValue* name_servers_list = new base::ListValue;
  name_servers_list->Append(new base::StringValue(name_server1));
  name_servers_list->Append(new base::StringValue(name_server2));
  base::DictionaryValue* ipconfig_properties = new base::DictionaryValue;
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kMethodProperty,
      new base::StringValue(flimflam::kTypeDHCP));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kAddressProperty,
      new base::StringValue(address));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kMtuProperty,
      base::Value::CreateIntegerValue(kMtu));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kPrefixlenProperty,
      base::Value::CreateIntegerValue(kPrefixlen));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kBroadcastProperty,
      new base::StringValue(broadcast));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kPeerAddressProperty,
      new base::StringValue(peer_address));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kGatewayProperty,
      new base::StringValue(gateway));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kDomainNameProperty,
      new base::StringValue(domainname));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kNameServersProperty, name_servers_list);

  EXPECT_CALL(*mock_device_client_,
              CallGetPropertiesAndBlock(dbus::ObjectPath(device_path)))
      .WillOnce(Return(device_properties));
  EXPECT_CALL(*mock_ipconfig_client_,
              CallGetPropertiesAndBlock(dbus::ObjectPath(ipconfig_path)))
      .WillOnce(Return(ipconfig_properties));

  NetworkIPConfigVector result_ipconfigs;
  std::vector<std::string> result_ipconfig_paths;
  std::string result_hardware_address;
  EXPECT_TRUE(
      CrosListIPConfigsAndBlock(device_path,
                                &result_ipconfigs,
                                &result_ipconfig_paths,
                                &result_hardware_address));

  EXPECT_EQ(hardware_address, result_hardware_address);
  ASSERT_EQ(1U, result_ipconfigs.size());
  EXPECT_EQ(device_path, result_ipconfigs[0].device_path);
  EXPECT_EQ(kType, result_ipconfigs[0].type);
  EXPECT_EQ(address, result_ipconfigs[0].address);
  EXPECT_EQ(netmask, result_ipconfigs[0].netmask);
  EXPECT_EQ(gateway, result_ipconfigs[0].gateway);
  EXPECT_EQ(name_servers, result_ipconfigs[0].name_servers);
  ASSERT_EQ(1U, result_ipconfig_paths.size());
  EXPECT_EQ(ipconfig_path, result_ipconfig_paths[0]);
}

TEST_F(CrosNetworkFunctionsTest, CrosConfigureService) {
  const std::string key1 = "key1";
  const std::string string1 = "string1";
  const std::string key2 = "key2";
  const std::string string2 = "string2";
  base::DictionaryValue value;
  value.SetString(key1, string1);
  value.SetString(key2, string2);
  EXPECT_CALL(*mock_manager_client_, ConfigureService(IsEqualTo(&value), _, _))
      .Times(1);
  CrosConfigureService(value);
}

}  // namespace chromeos
