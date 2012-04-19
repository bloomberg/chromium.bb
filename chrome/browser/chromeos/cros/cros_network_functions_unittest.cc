// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_network_functions.h"
#include "chrome/browser/chromeos/cros/gvalue_util.h"
#include "chrome/browser/chromeos/cros/mock_chromeos_network.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_flimflam_device_client.h"
#include "chromeos/dbus/mock_flimflam_manager_client.h"
#include "chromeos/dbus/mock_flimflam_profile_client.h"
#include "chromeos/dbus/mock_flimflam_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::SaveArg;
using ::testing::StrEq;

namespace chromeos {

namespace {

const char kExamplePath[] = "/foo/bar/baz";

// A mock to check arguments of NetworkPropertiesCallback and ensure that the
// callback is called exactly once.
class MockNetworkPropertiesCallback {
 public:
  // Creates a NetworkPropertiesCallback with expectations.
  static NetworkPropertiesCallback CreateCallback(
      const char* expected_path,
      const base::DictionaryValue& expected_result) {
    MockNetworkPropertiesCallback* mock_callback =
        new MockNetworkPropertiesCallback(expected_result);

    EXPECT_CALL(*mock_callback, Run(StrEq(expected_path), _)).WillOnce(
        Invoke(mock_callback, &MockNetworkPropertiesCallback::CheckResult));

    return base::Bind(&MockNetworkPropertiesCallback::Run,
                      base::Owned(mock_callback));
  }

 private:
  explicit MockNetworkPropertiesCallback(
      const base::DictionaryValue& expected_result)
      : expected_result_(expected_result) {}

  MOCK_METHOD2(Run, void(const char* path,
                         const base::DictionaryValue* result));

  void CheckResult(const char* path, const base::DictionaryValue* result) {
    EXPECT_TRUE(expected_result_.Equals(result));
  }

  const base::DictionaryValue& expected_result_;
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
        new MockNetworkPropertiesWatcherCallback(expected_value);

    EXPECT_CALL(*mock_callback, Run(expected_path, expected_key, _)).WillOnce(
        Invoke(mock_callback,
               &MockNetworkPropertiesWatcherCallback::CheckValue));

    return base::Bind(&MockNetworkPropertiesWatcherCallback::Run,
                      base::Owned(mock_callback));
  }

 private:
  explicit MockNetworkPropertiesWatcherCallback(
      const base::Value& expected_value) : expected_value_(expected_value) {}

  MOCK_METHOD3(Run, void(const std::string& expected_path,
                         const std::string& expected_key,
                         const base::Value& value));

  void CheckValue(const std::string& expected_path,
                  const std::string& expected_key,
                  const base::Value& value) {
    EXPECT_TRUE(expected_value_.Equals(&value));
  }

  const base::Value& expected_value_;
};

}  // namespace

// Test for cros_network_functions.cc with Libcros.
class CrosNetworkFunctionsLibcrosTest : public testing::Test {
 public:
  CrosNetworkFunctionsLibcrosTest() : argument_ghash_table_(NULL) {}

  virtual void SetUp() {
    ::g_type_init();
    MockChromeOSNetwork::Initialize();
    SetLibcrosNetworkFunctionsEnabled(true);
  }

  virtual void TearDown() {
    MockChromeOSNetwork::Shutdown();
  }

  // Handles call for SetNetwork*PropertyGValue functions and copies the value.
  void OnSetNetworkPropertyGValue(const char* path,
                                  const char* property,
                                  const GValue* gvalue) {
    argument_gvalue_.reset(new GValue());
    g_value_init(argument_gvalue_.get(), G_VALUE_TYPE(gvalue));
    g_value_copy(gvalue, argument_gvalue_.get());
  }

  // Handles call for SetNetworkManagerPropertyGValue.
  void OnSetNetworkManagerPropertyGValue(const char* property,
                                         const GValue* gvalue) {
    OnSetNetworkPropertyGValue("", property, gvalue);
  }

  // Handles call for MonitorNetworkManagerProperties
  NetworkPropertiesMonitor OnMonitorNetworkManagerProperties(
      MonitorPropertyGValueCallback callback, void* object) {
    property_changed_callback_ = base::Bind(callback, object);
    return NULL;
  }

  // Handles call for MonitorNetwork*Properties
  NetworkPropertiesMonitor OnMonitorNetworkProperties(
      MonitorPropertyGValueCallback callback, const char* path, void* object) {
    property_changed_callback_ = base::Bind(callback, object);
    return NULL;
  }

  // Handles call for ConfigureService.
  void OnConfigureService(const char* identifier,
                          const GHashTable* properties,
                          NetworkActionCallback callback,
                          void* object) {
    // const_cast here because nothing can be done with const GHashTable*.
    argument_ghash_table_.reset(const_cast<GHashTable*>(properties));
    g_hash_table_ref(argument_ghash_table_.get());
  }

  // Handles call for RequestNetwork*Properties.
  void OnRequestNetworkProperties2(NetworkPropertiesGValueCallback callback,
                                   void* object) {
    callback(object, kExamplePath, result_ghash_table_.get());
  }

  // Handles call for RequestNetwork*Properties, ignores the first argument.
  void OnRequestNetworkProperties3(const char* arg1,
                                   NetworkPropertiesGValueCallback callback,
                                   void* object) {
    OnRequestNetworkProperties2(callback, object);
  }

  // Handles call for RequestNetwork*Properties, ignores the first two
  // arguments.
  void OnRequestNetworkProperties4(const char* arg1,
                                   const char* arg2,
                                   NetworkPropertiesGValueCallback callback,
                                   void* object) {
    OnRequestNetworkProperties2(callback, object);
  }

  // Handles call for RequestNetwork*Properties, ignores the first three
  // arguments.
  void OnRequestNetworkProperties5(const char* arg1,
                                   const char* arg2,
                                   const char* arg3,
                                   NetworkPropertiesGValueCallback callback,
                                   void* object) {
    OnRequestNetworkProperties2(callback, object);
  }

  // Does nothing.  Used as an argument.
  static void OnNetworkAction(void* object,
                              const char* path,
                              NetworkMethodErrorType error,
                              const char* error_message) {}

 protected:
  ScopedGValue argument_gvalue_;
  ScopedGHashTable argument_ghash_table_;
  ScopedGHashTable result_ghash_table_;
  base::Callback<void(const char* path,
                      const char* key,
                      const GValue* gvalue)> property_changed_callback_;
};

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosSetNetworkServiceProperty) {
  const char service_path[] = "/";
  const char property[] = "property";
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      SetNetworkServicePropertyGValue(service_path, property, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnSetNetworkPropertyGValue));
  const char key1[] = "key1";
  const std::string string1 = "string1";
  const char key2[] = "key2";
  const std::string string2 = "string2";
  base::DictionaryValue value;
  value.SetString(key1, string1);
  value.SetString(key2, string2);
  CrosSetNetworkServiceProperty(service_path, property, value);

  // Check the argument GHashTable.
  GHashTable* ghash_table =
      static_cast<GHashTable*>(g_value_get_boxed(argument_gvalue_.get()));
  ASSERT_TRUE(ghash_table != NULL);
  EXPECT_EQ(string1, static_cast<const char*>(
      g_hash_table_lookup(ghash_table, key1)));
  EXPECT_EQ(string2, static_cast<const char*>(
      g_hash_table_lookup(ghash_table, key2)));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosSetNetworkDeviceProperty) {
  const char device_path[] = "/";
  const char property[] = "property";
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      SetNetworkDevicePropertyGValue(device_path, property, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnSetNetworkPropertyGValue));
  const bool kBool = true;
  const base::FundamentalValue value(kBool);
  CrosSetNetworkDeviceProperty(device_path, property, value);
  EXPECT_EQ(kBool, g_value_get_boolean(argument_gvalue_.get()));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosSetNetworkIPConfigProperty) {
  const char ipconfig_path[] = "/";
  const char property[] = "property";
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      SetNetworkIPConfigPropertyGValue(ipconfig_path, property, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnSetNetworkPropertyGValue));
  const int kInt = 1234;
  const base::FundamentalValue value(kInt);
  CrosSetNetworkIPConfigProperty(ipconfig_path, property, value);
  EXPECT_EQ(kInt, g_value_get_int(argument_gvalue_.get()));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosSetNetworkManagerProperty) {
  const char property[] = "property";
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      SetNetworkManagerPropertyGValue(property, _))
      .WillOnce(Invoke(
          this,
          &CrosNetworkFunctionsLibcrosTest::OnSetNetworkManagerPropertyGValue));
  const std::string kString = "string";
  const base::StringValue value(kString);
  CrosSetNetworkManagerProperty(property, value);
  EXPECT_EQ(kString, g_value_get_string(argument_gvalue_.get()));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosMonitorNetworkManagerProperties) {
  const std::string path = "path";
  const std::string key = "key";
  const int kValue = 42;
  const base::FundamentalValue value(kValue);

  // Start monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              MonitorNetworkManagerProperties(_, _))
      .WillOnce(Invoke(
          this,
          &CrosNetworkFunctionsLibcrosTest::OnMonitorNetworkManagerProperties));
  CrosNetworkWatcher* watcher = CrosMonitorNetworkManagerProperties(
      MockNetworkPropertiesWatcherCallback::CreateCallback(path, key, value));

  // Call callback.
  ScopedGValue gvalue(new GValue());
  g_value_init(gvalue.get(), G_TYPE_INT);
  g_value_set_int(gvalue.get(), kValue);
  property_changed_callback_.Run(path.c_str(), key.c_str(), gvalue.get());

  // Stop monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              DisconnectNetworkPropertiesMonitor(_)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosMonitorNetworkServiceProperties) {
  const std::string path = "path";
  const std::string key = "key";
  const int kValue = 42;
  const base::FundamentalValue value(kValue);

  // Start monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              MonitorNetworkServiceProperties(_, path.c_str(), _))
      .WillOnce(Invoke(
          this,
          &CrosNetworkFunctionsLibcrosTest::OnMonitorNetworkProperties));
  CrosNetworkWatcher* watcher = CrosMonitorNetworkServiceProperties(
      MockNetworkPropertiesWatcherCallback::CreateCallback(path, key, value),
      path);

  // Call callback.
  ScopedGValue gvalue(new GValue());
  g_value_init(gvalue.get(), G_TYPE_INT);
  g_value_set_int(gvalue.get(), kValue);
  property_changed_callback_.Run(path.c_str(), key.c_str(), gvalue.get());

  // Stop monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              DisconnectNetworkPropertiesMonitor(_)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosMonitorNetworkDeviceProperties) {
  const std::string path = "path";
  const std::string key = "key";
  const int kValue = 42;
  const base::FundamentalValue value(kValue);

  // Start monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              MonitorNetworkDeviceProperties(_, path.c_str(), _))
      .WillOnce(Invoke(
          this,
          &CrosNetworkFunctionsLibcrosTest::OnMonitorNetworkProperties));
  CrosNetworkWatcher* watcher = CrosMonitorNetworkDeviceProperties(
      MockNetworkPropertiesWatcherCallback::CreateCallback(path, key, value),
      path);

  // Call callback.
  ScopedGValue gvalue(new GValue());
  g_value_init(gvalue.get(), G_TYPE_INT);
  g_value_set_int(gvalue.get(), kValue);
  property_changed_callback_.Run(path.c_str(), key.c_str(), gvalue.get());

  // Stop monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              DisconnectNetworkPropertiesMonitor(_)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosMonitorCellularDataPlan) {
  MonitorDataPlanCallback callback =
      reinterpret_cast<MonitorDataPlanCallback>(42);  // Dummy value.
  void* object = reinterpret_cast<void*>(84);  // Dummy value.

  // Start monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              MonitorCellularDataPlan(callback, object)).Times(1);
  CrosNetworkWatcher* watcher = CrosMonitorCellularDataPlan(callback, object);

  // Stop monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              DisconnectDataPlanUpdateMonitor(_)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosMonitorSMS) {
  const std::string modem_device_path = "/modem/device/path";
  MonitorSMSCallback callback =
      reinterpret_cast<MonitorSMSCallback>(42);  // Dummy value.
  void* object = reinterpret_cast<void*>(84);  // Dummy value.

  // Start monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              MonitorSMS(modem_device_path.c_str(), callback, object)).Times(1);
  CrosNetworkWatcher* watcher = CrosMonitorSMS(
      modem_device_path, callback, object);

  // Stop monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              DisconnectSMSMonitor(_)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestNetworkManagerProperties) {
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  result_ghash_table_.reset(
      ConvertDictionaryValueToStringValueGHashTable(result));
  // Set expectations.
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      RequestNetworkManagerProperties(_, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties2));

  CrosRequestNetworkManagerProperties(
      MockNetworkPropertiesCallback::CreateCallback(kExamplePath, result));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestNetworkServiceProperties) {
  const std::string service_path = "service path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  result_ghash_table_.reset(
      ConvertDictionaryValueToStringValueGHashTable(result));
  // Set expectations.
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      RequestNetworkServiceProperties(service_path.c_str(), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties3));

  CrosRequestNetworkServiceProperties(
      service_path.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(kExamplePath, result));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestNetworkDeviceProperties) {
  const std::string device_path = "device path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  result_ghash_table_.reset(
      ConvertDictionaryValueToStringValueGHashTable(result));
  // Set expectations.
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      RequestNetworkDeviceProperties(device_path.c_str(), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties3));

  CrosRequestNetworkDeviceProperties(
      device_path.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(kExamplePath, result));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestNetworkProfileProperties) {
  const std::string profile_path = "profile path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  result_ghash_table_.reset(
      ConvertDictionaryValueToStringValueGHashTable(result));
  // Set expectations.
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      RequestNetworkProfileProperties(profile_path.c_str(), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties3));

  CrosRequestNetworkProfileProperties(
      profile_path.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(kExamplePath, result));
}

TEST_F(CrosNetworkFunctionsLibcrosTest,
       CrosRequestNetworkProfileEntryProperties) {
  const std::string profile_path = "profile path";
  const std::string profile_entry_path = "profile entry path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  result_ghash_table_.reset(
      ConvertDictionaryValueToStringValueGHashTable(result));
  // Set expectations.
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      RequestNetworkProfileEntryProperties(profile_path.c_str(),
                                           profile_entry_path.c_str(), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties4));

  CrosRequestNetworkProfileEntryProperties(
      profile_path.c_str(),
      profile_entry_path.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(kExamplePath, result));
}

TEST_F(CrosNetworkFunctionsLibcrosTest,
       CrosRequestHiddenWifiNetworkProperties) {
  const std::string ssid = "ssid";
  const std::string security = "security";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  result_ghash_table_.reset(
      ConvertDictionaryValueToStringValueGHashTable(result));
  // Set expectations.
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      RequestHiddenWifiNetworkProperties(ssid.c_str(), security.c_str(), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties4));

  CrosRequestHiddenWifiNetworkProperties(
      ssid.c_str(), security.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(kExamplePath, result));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestVirtualNetworkProperties) {
  const std::string service_name = "service name";
  const std::string server_hostname = "server hostname";
  const std::string provider_name = "provider name";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  result_ghash_table_.reset(
      ConvertDictionaryValueToStringValueGHashTable(result));
  // Set expectations.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              RequestVirtualNetworkProperties(service_name.c_str(),
                                              server_hostname.c_str(),
                                              provider_name.c_str(), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties5));

  CrosRequestVirtualNetworkProperties(
      service_name.c_str(),
      server_hostname.c_str(),
      provider_name.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(kExamplePath, result));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosConfigureService) {
  const char identifier[] = "identifier";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              ConfigureService(identifier, _, &OnNetworkAction, this))
              .WillOnce(Invoke(
                  this, &CrosNetworkFunctionsLibcrosTest::OnConfigureService));
  const char key1[] = "key1";
  const std::string string1 = "string1";
  const char key2[] = "key2";
  const std::string string2 = "string2";
  base::DictionaryValue value;
  value.SetString(key1, string1);
  value.SetString(key2, string2);
  CrosConfigureService(identifier, value, &OnNetworkAction, this);

  // Check the argument GHashTable.
  const GValue* string1_gvalue = static_cast<const GValue*>(
      g_hash_table_lookup(argument_ghash_table_.get(), key1));
  EXPECT_EQ(string1, g_value_get_string(string1_gvalue));
  const GValue* string2_gvalue = static_cast<const GValue*>(
      g_hash_table_lookup(argument_ghash_table_.get(), key2));
  EXPECT_EQ(string2, g_value_get_string(string2_gvalue));
}

// Test for cros_network_functions.cc without Libcros.
class CrosNetworkFunctionsTest : public testing::Test {
 public:
  CrosNetworkFunctionsTest() : mock_profile_client_(NULL),
                               dictionary_value_result_(NULL) {}

  virtual void SetUp() {
    MockDBusThreadManager* mock_dbus_thread_manager = new MockDBusThreadManager;
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    mock_device_client_ =
        mock_dbus_thread_manager->mock_flimflam_device_client();
    mock_manager_client_ =
        mock_dbus_thread_manager->mock_flimflam_manager_client();
    mock_profile_client_ =
        mock_dbus_thread_manager->mock_flimflam_profile_client();
    mock_service_client_ =
        mock_dbus_thread_manager->mock_flimflam_service_client();
    SetLibcrosNetworkFunctionsEnabled(false);
  }

  virtual void TearDown() {
    DBusThreadManager::Shutdown();
    mock_profile_client_ = NULL;
  }

  // Handles responses for GetProperties method calls for FlimflamManagerClient.
  void OnGetManagerProperties(
      const FlimflamClientHelper::DictionaryValueCallback& callback) {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *dictionary_value_result_);
  }

  // Handles responses for GetProperties method calls.
  void OnGetProperties(
      const dbus::ObjectPath& path,
      const FlimflamClientHelper::DictionaryValueCallback& callback) {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *dictionary_value_result_);
  }

  // Handles responses for GetEntry method calls.
  void OnGetEntry(
      const dbus::ObjectPath& profile_path,
      const std::string& entry_path,
      const FlimflamClientHelper::DictionaryValueCallback& callback) {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *dictionary_value_result_);
  }

 protected:
  MockFlimflamDeviceClient* mock_device_client_;
  MockFlimflamManagerClient* mock_manager_client_;
  MockFlimflamProfileClient* mock_profile_client_;
  MockFlimflamServiceClient* mock_service_client_;
  const base::DictionaryValue* dictionary_value_result_;
};

TEST_F(CrosNetworkFunctionsTest, CrosDeleteServiceFromProfile) {
  const std::string profile_path("/profile/path");
  const std::string service_path("/service/path");
  EXPECT_CALL(*mock_profile_client_,
              DeleteEntry(dbus::ObjectPath(profile_path), service_path, _))
      .Times(1);
  CrosDeleteServiceFromProfile(profile_path.c_str(), service_path.c_str());
}

TEST_F(CrosNetworkFunctionsTest, CrosMonitorNetworkManagerProperties) {
  const std::string key = "key";
  const int kValue = 42;
  const base::FundamentalValue value(kValue);
  // Start monitoring.
  FlimflamClientHelper::PropertyChangedHandler handler;
  EXPECT_CALL(*mock_manager_client_, SetPropertyChangedHandler(_))
      .WillOnce(SaveArg<0>(&handler));
  CrosNetworkWatcher* watcher = CrosMonitorNetworkManagerProperties(
      MockNetworkPropertiesWatcherCallback::CreateCallback(
          flimflam::kFlimflamServicePath, key, value));
  // Call callback.
  handler.Run(key, value);
  // Stop monitoring.
  EXPECT_CALL(*mock_manager_client_, ResetPropertyChangedHandler()).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkManagerProperties) {
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
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
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_service_client_,
              GetProperties(dbus::ObjectPath(service_path), _)).WillOnce(
                  Invoke(this, &CrosNetworkFunctionsTest::OnGetProperties));

  CrosRequestNetworkServiceProperties(
      service_path.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(service_path.c_str(),
                                                    result));
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkDeviceProperties) {
  const std::string device_path = "/device/path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_device_client_,
              GetProperties(dbus::ObjectPath(device_path), _)).WillOnce(
                  Invoke(this, &CrosNetworkFunctionsTest::OnGetProperties));

  CrosRequestNetworkDeviceProperties(
      device_path.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(device_path.c_str(),
                                                    result));
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkProfileProperties) {
  const std::string profile_path = "/profile/path";
  const std::string key1 = "key1";
  const std::string value1 = "value1";
  const std::string key2 = "key.2.";
  const std::string value2 = "value2";
  // Create result value.
  base::DictionaryValue result;
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(profile_path), _)).WillOnce(
                  Invoke(this, &CrosNetworkFunctionsTest::OnGetProperties));

  CrosRequestNetworkProfileProperties(
      profile_path.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(profile_path.c_str(),
                                                    result));
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
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_profile_client_,
              GetEntry(dbus::ObjectPath(profile_path), profile_entry_path, _))
      .WillOnce(Invoke(this, &CrosNetworkFunctionsTest::OnGetEntry));

  CrosRequestNetworkProfileEntryProperties(
      profile_path.c_str(), profile_entry_path.c_str(),
      MockNetworkPropertiesCallback::CreateCallback(profile_entry_path.c_str(),
                                                    result));
}

}  // namespace chromeos
