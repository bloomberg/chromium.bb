// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_network_functions.h"
#include "chrome/browser/chromeos/cros/gvalue_util.h"
#include "chrome/browser/chromeos/cros/mock_chromeos_network.h"
#include "chrome/browser/chromeos/cros/sms_watcher.h"
#include "chromeos/dbus/mock_cashew_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_flimflam_device_client.h"
#include "chromeos/dbus/mock_flimflam_ipconfig_client.h"
#include "chromeos/dbus/mock_flimflam_manager_client.h"
#include "chromeos/dbus/mock_flimflam_network_client.h"
#include "chromeos/dbus/mock_flimflam_profile_client.h"
#include "chromeos/dbus/mock_flimflam_service_client.h"
#include "chromeos/dbus/mock_gsm_sms_client.h"
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

// A mock to check arguments of CellularDataPlanWatcherCallback and ensure that
// the callback is called exactly once.
class MockDataPlanUpdateWatcherCallback {
 public:
  // Creates a NetworkPropertiesWatcherCallback with expectations.
  static DataPlanUpdateWatcherCallback CreateCallback(
      const std::string& expected_modem_service_path,
      const CellularDataPlanVector& expected_data_plan_vector) {
    MockDataPlanUpdateWatcherCallback* mock_callback =
        new MockDataPlanUpdateWatcherCallback;
    mock_callback->expected_data_plan_vector_ = &expected_data_plan_vector;

    EXPECT_CALL(*mock_callback,
                Run(expected_modem_service_path, _))
        .WillOnce(Invoke(mock_callback,
                         &MockDataPlanUpdateWatcherCallback::CheckDataPlans));

    return base::Bind(&MockDataPlanUpdateWatcherCallback::Run,
                      base::Owned(mock_callback));
  }

  MOCK_METHOD2(Run, void(const std::string& modem_service_path,
                         CellularDataPlanVector* data_plan_vector));

 private:
  void CheckDataPlans(const std::string& modem_service_path,
                      CellularDataPlanVector* data_plan_vector) {
    ASSERT_EQ(expected_data_plan_vector_->size(), data_plan_vector->size());
    for (size_t i = 0; i != data_plan_vector->size(); ++i) {
      EXPECT_EQ((*expected_data_plan_vector_)[i]->plan_name,
                (*data_plan_vector)[i]->plan_name);
      EXPECT_EQ((*expected_data_plan_vector_)[i]->plan_type,
                (*data_plan_vector)[i]->plan_type);
      EXPECT_EQ((*expected_data_plan_vector_)[i]->update_time,
                (*data_plan_vector)[i]->update_time);
      EXPECT_EQ((*expected_data_plan_vector_)[i]->plan_start_time,
                (*data_plan_vector)[i]->plan_start_time);
      EXPECT_EQ((*expected_data_plan_vector_)[i]->plan_end_time,
                (*data_plan_vector)[i]->plan_end_time);
      EXPECT_EQ((*expected_data_plan_vector_)[i]->plan_data_bytes,
                (*data_plan_vector)[i]->plan_data_bytes);
      EXPECT_EQ((*expected_data_plan_vector_)[i]->data_bytes_used,
                (*data_plan_vector)[i]->data_bytes_used);
    }
    delete data_plan_vector;
  }

  const CellularDataPlanVector* expected_data_plan_vector_;
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

  // Mock NetworkOperationCallback.
  MOCK_METHOD3(MockNetworkOperationCallback,
               void(const std::string& path,
                    NetworkMethodErrorType error,
                    const std::string& error_message));

  // Does nothing.  Used as an argument.
  static void OnSmsReceived(void* object,
                            const char* modem_device_path,
                            const SMS* message) {}

 protected:
  ScopedGValue argument_gvalue_;
  ScopedGHashTable argument_ghash_table_;
  ScopedGHashTable result_ghash_table_;
  base::Callback<void(const char* path,
                      const char* key,
                      const GValue* gvalue)> property_changed_callback_;
};

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosActivateCellularModem) {
  const std::string service_path = "/";
  const std::string carrier = "carrier";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              ActivateCellularModem(StrEq(service_path), StrEq(carrier)))
      .Times(1);
  CrosActivateCellularModem(service_path, carrier);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosSetNetworkServiceProperty) {
  const std::string service_path = "/";
  const std::string property = "property";
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      SetNetworkServicePropertyGValue(StrEq(service_path), StrEq(property), _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnSetNetworkPropertyGValue));
  const std::string key1= "key1";
  const std::string string1 = "string1";
  const std::string key2 = "key2";
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
      g_hash_table_lookup(ghash_table, key1.c_str())));
  EXPECT_EQ(string2, static_cast<const char*>(
      g_hash_table_lookup(ghash_table, key2.c_str())));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosClearNetworkServiceProperty) {
  const std::string service_path = "/";
  const std::string property = "property";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              ClearNetworkServiceProperty(StrEq(service_path),
                                          StrEq(property))).Times(1);
  CrosClearNetworkServiceProperty(service_path, property);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosSetNetworkDeviceProperty) {
  const std::string device_path = "/";
  const std::string property = "property";
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      SetNetworkDevicePropertyGValue(StrEq(device_path), StrEq(property), _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnSetNetworkPropertyGValue));
  const bool kBool = true;
  const base::FundamentalValue value(kBool);
  CrosSetNetworkDeviceProperty(device_path, property, value);
  EXPECT_EQ(kBool, g_value_get_boolean(argument_gvalue_.get()));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosSetNetworkIPConfigProperty) {
  const std::string ipconfig_path = "/";
  const std::string property = "property";
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      SetNetworkIPConfigPropertyGValue(StrEq(ipconfig_path),
                                       StrEq(property), _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnSetNetworkPropertyGValue));
  const int kInt = 1234;
  const base::FundamentalValue value(kInt);
  CrosSetNetworkIPConfigProperty(ipconfig_path, property, value);
  EXPECT_EQ(kInt, g_value_get_int(argument_gvalue_.get()));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosSetNetworkManagerProperty) {
  const std::string property = "property";
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      SetNetworkManagerPropertyGValue(StrEq(property), _))
      .WillOnce(Invoke(
          this,
          &CrosNetworkFunctionsLibcrosTest::OnSetNetworkManagerPropertyGValue));
  const std::string str = "string";
  const base::StringValue value(str);
  CrosSetNetworkManagerProperty(property, value);
  EXPECT_EQ(str, g_value_get_string(argument_gvalue_.get()));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestCellularDataPlanUpdate) {
  const std::string modem_service_path = "/modem/service/path";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              RequestCellularDataPlanUpdate(StrEq(modem_service_path)))
      .Times(1);
  CrosRequestCellularDataPlanUpdate(modem_service_path);
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
              MonitorNetworkServiceProperties(_, StrEq(path), _))
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
              MonitorNetworkDeviceProperties(_, StrEq(path), _))
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
  DataPlanUpdateMonitor monitor = NULL;  // dummy
  const std::string modem_service_path = "/modem/path";
  CellularDataPlanInfo data_plan = {};
  data_plan.plan_name = "plan name";
  data_plan.update_time = 123456;
  data_plan.plan_start_time = 234567;
  data_plan.plan_end_time = 345678;
  data_plan.plan_data_bytes = 1024*1024;
  data_plan.data_bytes_used = 12345;
  CellularDataPlanList data_plans = {};
  data_plans.plans = &data_plan;
  data_plans.plans_size = 1;
  data_plans.data_plan_size = sizeof(data_plan);

  CellularDataPlanVector result;
  result.push_back(new CellularDataPlan(data_plan));

  // Set expectations.
  DataPlanUpdateWatcherCallback callback =
      MockDataPlanUpdateWatcherCallback::CreateCallback(modem_service_path,
                                                        result);
  MonitorDataPlanCallback arg_callback = NULL;
  void* arg_object = NULL;
  EXPECT_CALL(*MockChromeOSNetwork::Get(), MonitorCellularDataPlan(_, _))
      .WillOnce(DoAll(SaveArg<0>(&arg_callback), SaveArg<1>(&arg_object),
                      Return(monitor)));

  // Start monitoring.
  CrosNetworkWatcher* watcher = CrosMonitorCellularDataPlan(callback);

  // Run callback.
  arg_callback(arg_object, modem_service_path.c_str(), &data_plans);

  // Stop monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              DisconnectDataPlanUpdateMonitor(monitor)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosMonitorSMS) {
  const std::string modem_device_path = "/modem/device/path";
  MonitorSMSCallback callback = &CrosNetworkFunctionsLibcrosTest::OnSmsReceived;
  void* object = this;

  // Start monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              MonitorSMS(StrEq(modem_device_path), callback, object))
      .Times(1);
  CrosNetworkWatcher* watcher = CrosMonitorSMS(
      modem_device_path, callback, object);

  // Stop monitoring.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              DisconnectSMSMonitor(_)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestNetworkServiceConnect) {
  const std::string service_path = "service path";

  NetworkActionCallback callback = NULL;
  void* object = NULL;
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              RequestNetworkServiceConnect(StrEq(service_path), _, _))
      .WillOnce(DoAll(SaveArg<1>(&callback), SaveArg<2>(&object)));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      service_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);

  CrosRequestNetworkServiceConnect(
      service_path,
      base::Bind(&CrosNetworkFunctionsLibcrosTest::MockNetworkOperationCallback,
                 base::Unretained(this)));

  ASSERT_TRUE(callback);
  callback(object, service_path.c_str(), NETWORK_METHOD_ERROR_NONE, NULL);
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
      RequestNetworkServiceProperties(StrEq(service_path), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties3));

  CrosRequestNetworkServiceProperties(
      service_path,
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
      RequestNetworkDeviceProperties(StrEq(device_path), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties3));

  CrosRequestNetworkDeviceProperties(
      device_path,
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
      RequestNetworkProfileProperties(StrEq(profile_path), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties3));

  CrosRequestNetworkProfileProperties(
      profile_path,
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
      RequestNetworkProfileEntryProperties(
          StrEq(profile_path), StrEq(profile_entry_path), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties4));

  CrosRequestNetworkProfileEntryProperties(
      profile_path,
      profile_entry_path,
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
      RequestHiddenWifiNetworkProperties(
          StrEq(ssid), StrEq(security), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties4));

  CrosRequestHiddenWifiNetworkProperties(
      ssid, security,
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
              RequestVirtualNetworkProperties(
                  StrEq(service_name), StrEq(server_hostname),
                  StrEq(provider_name), _, _))
      .WillOnce(Invoke(
          this, &CrosNetworkFunctionsLibcrosTest::OnRequestNetworkProperties5));

  CrosRequestVirtualNetworkProperties(
      service_name,
      server_hostname,
      provider_name,
      MockNetworkPropertiesCallback::CreateCallback(kExamplePath, result));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestNetworkServiceDisconnect) {
  const std::string service_path = "/service/path";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              RequestNetworkServiceDisconnect(StrEq(service_path))).Times(1);
  CrosRequestNetworkServiceDisconnect(service_path);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestRemoveNetworkService) {
  const std::string service_path = "/service/path";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              RequestRemoveNetworkService(StrEq(service_path))).Times(1);
  CrosRequestRemoveNetworkService(service_path);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestNetworkScan) {
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              RequestNetworkScan(StrEq(flimflam::kTypeWifi))).Times(1);
  CrosRequestNetworkScan(flimflam::kTypeWifi);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestNetworkDeviceEnable) {
  const bool kEnable = true;
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              RequestNetworkDeviceEnable(StrEq(flimflam::kTypeWifi), kEnable))
      .Times(1);
  CrosRequestNetworkDeviceEnable(flimflam::kTypeWifi, kEnable);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestRequirePin) {
  const std::string device_path = "/device/path";
  const std::string pin = "123456";
  const bool kEnable = true;

  NetworkActionCallback callback = NULL;
  void* object = this;
  EXPECT_CALL(*MockChromeOSNetwork::Get(), RequestRequirePin(
      StrEq(device_path), StrEq(pin), kEnable, _, _))
      .WillOnce(DoAll(SaveArg<3>(&callback), SaveArg<4>(&object)));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);

  CrosRequestRequirePin(
      device_path, pin, kEnable,
      base::Bind(&CrosNetworkFunctionsLibcrosTest::MockNetworkOperationCallback,
                 base::Unretained(this)));

  ASSERT_TRUE(callback);
  callback(object, device_path.c_str(), NETWORK_METHOD_ERROR_NONE, NULL);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestEnterPin) {
  const std::string device_path = "/device/path";
  const std::string pin = "123456";

  NetworkActionCallback callback = NULL;
  void* object = this;
  EXPECT_CALL(*MockChromeOSNetwork::Get(), RequestEnterPin(
      StrEq(device_path), StrEq(pin), _, _))
      .WillOnce(DoAll(SaveArg<2>(&callback), SaveArg<3>(&object)));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);

  CrosRequestEnterPin(
      device_path, pin,
      base::Bind(&CrosNetworkFunctionsLibcrosTest::MockNetworkOperationCallback,
                 base::Unretained(this)));

  ASSERT_TRUE(callback);
  callback(object, device_path.c_str(), NETWORK_METHOD_ERROR_NONE, NULL);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestUnblockPin) {
  const std::string device_path = "/device/path";
  const std::string unblock_code = "987654";
  const std::string pin = "123456";

  NetworkActionCallback callback = NULL;
  void* object = this;
  EXPECT_CALL(*MockChromeOSNetwork::Get(), RequestUnblockPin(
      StrEq(device_path), StrEq(unblock_code), StrEq(pin), _, _))
      .WillOnce(DoAll(SaveArg<3>(&callback), SaveArg<4>(&object)));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);

  CrosRequestUnblockPin(
      device_path, unblock_code, pin,
      base::Bind(&CrosNetworkFunctionsLibcrosTest::MockNetworkOperationCallback,
                 base::Unretained(this)));

  ASSERT_TRUE(callback);
  callback(object, device_path.c_str(), NETWORK_METHOD_ERROR_NONE, NULL);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestChangePin) {
  const std::string device_path = "/device/path";
  const std::string old_pin = "123456";
  const std::string new_pin = "234567";

  NetworkActionCallback callback = NULL;
  void* object = this;
  EXPECT_CALL(*MockChromeOSNetwork::Get(), RequestChangePin(
      StrEq(device_path), StrEq(old_pin), StrEq(new_pin), _, _))
      .WillOnce(DoAll(SaveArg<3>(&callback), SaveArg<4>(&object)));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);

  CrosRequestChangePin(
      device_path, old_pin, new_pin,
      base::Bind(&CrosNetworkFunctionsLibcrosTest::MockNetworkOperationCallback,
                 base::Unretained(this)));

  ASSERT_TRUE(callback);
  callback(object, device_path.c_str(), NETWORK_METHOD_ERROR_NONE, NULL);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosProposeScan) {
  const std::string device_path = "/device/path";
  EXPECT_CALL(*MockChromeOSNetwork::Get(), ProposeScan(StrEq(device_path)))
      .Times(1);
  CrosProposeScan(device_path);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRequestCellularRegister) {
  const std::string device_path = "/device/path";
  const std::string network_id = "networkid";

  NetworkActionCallback callback = NULL;
  void* object = this;
  EXPECT_CALL(*MockChromeOSNetwork::Get(), RequestCellularRegister(
      StrEq(device_path), StrEq(network_id), _, _))
      .WillOnce(DoAll(SaveArg<2>(&callback), SaveArg<3>(&object)));
  EXPECT_CALL(*this, MockNetworkOperationCallback(
      device_path, NETWORK_METHOD_ERROR_NONE, _)).Times(1);

  CrosRequestCellularRegister(
      device_path, network_id,
      base::Bind(&CrosNetworkFunctionsLibcrosTest::MockNetworkOperationCallback,
                 base::Unretained(this)));

  ASSERT_TRUE(callback);
  callback(object, device_path.c_str(), NETWORK_METHOD_ERROR_NONE, NULL);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosSetOfflineMode) {
  const bool kOffline = true;
  EXPECT_CALL(*MockChromeOSNetwork::Get(), SetOfflineMode(kOffline)).Times(1);
  CrosSetOfflineMode(kOffline);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosListIPConfigs) {
  const std::string device_path = "/device/path";
  const std::string ipconfig_path = "/ipconfig/path";
  const IPConfigType kType = IPCONFIG_TYPE_DHCP;
  const std::string address = "address";
  const int kMtu = 123;
  const std::string netmask = "netmask";
  const std::string broadcast = "broadcast";
  const std::string peer_address = "peer address";
  const std::string gateway = "gateway";
  const std::string domainname = "domainname";
  const std::string name_servers = "name servers";
  const std::string hardware_address = "hardware address";
  IPConfig ipconfig = {};
  ipconfig.path = ipconfig_path.c_str();
  ipconfig.type = kType;
  ipconfig.address = address.c_str();
  ipconfig.mtu = kMtu;
  ipconfig.netmask = netmask.c_str();
  ipconfig.broadcast = broadcast.c_str();
  ipconfig.peer_address = peer_address.c_str();
  ipconfig.gateway = gateway.c_str();
  ipconfig.domainname = domainname.c_str();
  ipconfig.name_servers = name_servers.c_str();
  IPConfigStatus ipconfig_status = {};
  ipconfig_status.ips = &ipconfig;
  ipconfig_status.hardware_address = hardware_address.c_str();
  ipconfig_status.size = 1;

  EXPECT_CALL(*MockChromeOSNetwork::Get(), ListIPConfigs(StrEq(device_path)))
      .WillOnce(Return(&ipconfig_status));
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              FreeIPConfigStatus(&ipconfig_status)).Times(1);

  NetworkIPConfigVector ipconfigs;
  std::vector<std::string> ipconfig_paths;
  std::string result_hardware_address;
  EXPECT_TRUE(CrosListIPConfigs(device_path, &ipconfigs, &ipconfig_paths,
                                &result_hardware_address));

  EXPECT_EQ(hardware_address, result_hardware_address);
  EXPECT_EQ(ipconfig_status.size, static_cast<int>(ipconfigs.size()));
  EXPECT_EQ(device_path, ipconfigs[0].device_path);
  EXPECT_EQ(kType, ipconfigs[0].type);
  EXPECT_EQ(address, ipconfigs[0].address);
  EXPECT_EQ(netmask, ipconfigs[0].netmask);
  EXPECT_EQ(gateway, ipconfigs[0].gateway);
  EXPECT_EQ(name_servers, ipconfigs[0].name_servers);
  EXPECT_EQ(ipconfig_status.size, static_cast<int>(ipconfig_paths.size()));
  EXPECT_EQ(ipconfig_path, ipconfig_paths[0]);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosAddIPConfig) {
  const std::string device_path = "/device/path";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              AddIPConfig(StrEq(device_path), IPCONFIG_TYPE_DHCP)).Times(1);
  CrosAddIPConfig(device_path, IPCONFIG_TYPE_DHCP);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosRemoveIPConfig) {
  const std::string path = "/path";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              RemoveIPConfig(Pointee(IsIPConfigPathEqualTo(path))))
      .WillOnce(Return(true));
  EXPECT_TRUE(CrosRemoveIPConfig(path));
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosGetWifiAccessPoints) {
  // Preapare data.
  std::vector<DeviceNetworkInfo> networks(1);
  networks[0].device_path = "/device/path";
  networks[0].network_path = "/network/path";
  networks[0].address = "address";
  networks[0].name = "name";
  networks[0].strength = 42;
  networks[0].channel = 24;
  networks[0].connected = true;
  networks[0].age_seconds = 12345;

  DeviceNetworkList network_list;
  network_list.network_size = networks.size();
  network_list.networks = &networks[0];

  const base::Time expected_timestamp =
      base::Time::Now() - base::TimeDelta::FromSeconds(networks[0].age_seconds);
  const base::TimeDelta acceptable_timestamp_range =
      base::TimeDelta::FromSeconds(1);

  // Set expectations.
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              GetDeviceNetworkList()).WillOnce(Return(&network_list));
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              FreeDeviceNetworkList(&network_list)).Times(1);

  // Call function.
  WifiAccessPointVector aps;
  ASSERT_TRUE(CrosGetWifiAccessPoints(&aps));
  ASSERT_EQ(1U, aps.size());
  EXPECT_EQ(networks[0].address, aps[0].mac_address);
  EXPECT_EQ(networks[0].name, aps[0].name);
  EXPECT_LE(expected_timestamp - acceptable_timestamp_range, aps[0].timestamp);
  EXPECT_GE(expected_timestamp + acceptable_timestamp_range, aps[0].timestamp);
  EXPECT_EQ(networks[0].strength, aps[0].signal_strength);
  EXPECT_EQ(networks[0].channel, aps[0].channel);
}

TEST_F(CrosNetworkFunctionsLibcrosTest, CrosConfigureService) {
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              ConfigureService(_, _, _, _))
              .WillOnce(Invoke(
                  this, &CrosNetworkFunctionsLibcrosTest::OnConfigureService));
  const std::string key1 = "key1";
  const std::string string1 = "string1";
  const std::string key2 = "key2";
  const std::string string2 = "string2";
  base::DictionaryValue value;
  value.SetString(key1, string1);
  value.SetString(key2, string2);
  CrosConfigureService(value);

  // Check the argument GHashTable.
  const GValue* string1_gvalue = static_cast<const GValue*>(
      g_hash_table_lookup(argument_ghash_table_.get(), key1.c_str()));
  EXPECT_EQ(string1, g_value_get_string(string1_gvalue));
  const GValue* string2_gvalue = static_cast<const GValue*>(
      g_hash_table_lookup(argument_ghash_table_.get(), key2.c_str()));
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
    mock_cashew_client_ = mock_dbus_thread_manager->mock_cashew_client();
    mock_device_client_ =
        mock_dbus_thread_manager->mock_flimflam_device_client();
    mock_ipconfig_client_ =
        mock_dbus_thread_manager->mock_flimflam_ipconfig_client();
    mock_manager_client_ =
        mock_dbus_thread_manager->mock_flimflam_manager_client();
    mock_network_client_ =
        mock_dbus_thread_manager->mock_flimflam_network_client();
    mock_profile_client_ =
        mock_dbus_thread_manager->mock_flimflam_profile_client();
    mock_service_client_ =
        mock_dbus_thread_manager->mock_flimflam_service_client();
    mock_gsm_sms_client_ = mock_dbus_thread_manager->mock_gsm_sms_client();
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

  // Mock NetworkOperationCallback.
  MOCK_METHOD3(MockNetworkOperationCallback,
               void(const std::string& path,
                    NetworkMethodErrorType error,
                    const std::string& error_message));

  // Mock MonitorSMSCallback.
  MOCK_METHOD3(MockMonitorSMSCallback, void(void* object,
                                            const char* modem_device_path,
                                            const SMS* message));
  static void MockMonitorSMSCallbackThunk(void* object,
                                          const char* modem_device_path,
                                          const SMS* message) {
    static_cast<CrosNetworkFunctionsTest*>(object)->MockMonitorSMSCallback(
        object, modem_device_path, message);
  }

 protected:
  MockCashewClient* mock_cashew_client_;
  MockFlimflamDeviceClient* mock_device_client_;
  MockFlimflamIPConfigClient* mock_ipconfig_client_;
  MockFlimflamManagerClient* mock_manager_client_;
  MockFlimflamNetworkClient* mock_network_client_;
  MockFlimflamProfileClient* mock_profile_client_;
  MockFlimflamServiceClient* mock_service_client_;
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
                          IsEqualTo(&value), _)).Times(1);

  CrosSetNetworkServiceProperty(service_path, property, value);
}

TEST_F(CrosNetworkFunctionsTest, CrosClearNetworkServiceProperty) {
  const std::string service_path = "/";
  const std::string property = "property";
  EXPECT_CALL(*mock_service_client_,
              ClearProperty(dbus::ObjectPath(service_path), property, _))
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
                          IsEqualTo(&value), _)).Times(1);

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
              SetProperty(property, IsEqualTo(&value), _)).Times(1);

  CrosSetNetworkManagerProperty(property, value);
}

TEST_F(CrosNetworkFunctionsTest, CrosDeleteServiceFromProfile) {
  const std::string profile_path("/profile/path");
  const std::string service_path("/service/path");
  EXPECT_CALL(*mock_profile_client_,
              DeleteEntry(dbus::ObjectPath(profile_path), service_path, _))
      .Times(1);
  CrosDeleteServiceFromProfile(profile_path, service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestCellularDataPlanUpdate) {
  const std::string modem_service_path = "/modem/service/path";
  EXPECT_CALL(*mock_cashew_client_,
              RequestDataPlansUpdate(modem_service_path)).Times(1);
  CrosRequestCellularDataPlanUpdate(modem_service_path);
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

TEST_F(CrosNetworkFunctionsTest, CrosMonitorNetworkServiceProperties) {
  const dbus::ObjectPath path("/path");
  const std::string key = "key";
  const int kValue = 42;
  const base::FundamentalValue value(kValue);
  // Start monitoring.
  FlimflamClientHelper::PropertyChangedHandler handler;
  EXPECT_CALL(*mock_service_client_, SetPropertyChangedHandler(path, _))
      .WillOnce(SaveArg<1>(&handler));
  NetworkPropertiesWatcherCallback callback =
      MockNetworkPropertiesWatcherCallback::CreateCallback(path.value(),
                                                           key, value);
  CrosNetworkWatcher* watcher = CrosMonitorNetworkServiceProperties(
      callback, path.value());
  // Call callback.
  handler.Run(key, value);
  // Stop monitoring.
  EXPECT_CALL(*mock_service_client_,
              ResetPropertyChangedHandler(path)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsTest, CrosMonitorNetworkDeviceProperties) {
  const dbus::ObjectPath path("/path");
  const std::string key = "key";
  const int kValue = 42;
  const base::FundamentalValue value(kValue);
  // Start monitoring.
  FlimflamClientHelper::PropertyChangedHandler handler;
  EXPECT_CALL(*mock_device_client_, SetPropertyChangedHandler(path, _))
      .WillOnce(SaveArg<1>(&handler));
  NetworkPropertiesWatcherCallback callback =
      MockNetworkPropertiesWatcherCallback::CreateCallback(path.value(),
                                                           key, value);
  CrosNetworkWatcher* watcher = CrosMonitorNetworkDeviceProperties(
      callback, path.value());
  // Call callback.
  handler.Run(key, value);
  // Stop monitoring.
  EXPECT_CALL(*mock_device_client_,
              ResetPropertyChangedHandler(path)).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsTest, CrosMonitorCellularDataPlan) {
  const std::string modem_service_path = "/modem/path";
  const int64 kUpdateTime = 123456;
  const int64 kPlanStartTime = 234567;
  const int64 kPlanEndTime = 345678;

  CellularDataPlan* data_plan = new CellularDataPlan;
  CellularDataPlanVector data_plans;
  data_plan->plan_name = "plan name";
  data_plan->plan_type = CELLULAR_DATA_PLAN_METERED_PAID;
  data_plan->update_time = base::Time::FromInternalValue(kUpdateTime);
  data_plan->plan_start_time = base::Time::FromInternalValue(kPlanStartTime);
  data_plan->plan_end_time = base::Time::FromInternalValue(kPlanEndTime);
  data_plan->plan_data_bytes = 1024*1024;
  data_plan->data_bytes_used = 12345;
  data_plans.push_back(data_plan);

  base::DictionaryValue* data_plan_dictionary = new base::DictionaryValue;
  data_plan_dictionary->SetWithoutPathExpansion(
      cashew::kCellularPlanNameProperty,
      base::Value::CreateStringValue(data_plan->plan_name));
  data_plan_dictionary->SetWithoutPathExpansion(
      cashew::kCellularPlanTypeProperty,
      base::Value::CreateStringValue(cashew::kCellularDataPlanMeteredPaid));
  data_plan_dictionary->SetWithoutPathExpansion(
      cashew::kCellularPlanUpdateTimeProperty,
      base::Value::CreateDoubleValue(kUpdateTime));
  data_plan_dictionary->SetWithoutPathExpansion(
      cashew::kCellularPlanStartProperty,
      base::Value::CreateDoubleValue(kPlanStartTime));
  data_plan_dictionary->SetWithoutPathExpansion(
      cashew::kCellularPlanEndProperty,
      base::Value::CreateDoubleValue(kPlanEndTime));
  data_plan_dictionary->SetWithoutPathExpansion(
      cashew::kCellularPlanDataBytesProperty,
      base::Value::CreateDoubleValue(data_plan->plan_data_bytes));
  data_plan_dictionary->SetWithoutPathExpansion(
      cashew::kCellularDataBytesUsedProperty,
      base::Value::CreateDoubleValue(data_plan->data_bytes_used));

  base::ListValue data_plans_list;
  data_plans_list.Append(data_plan_dictionary);

  // Set expectations.
  DataPlanUpdateWatcherCallback callback =
      MockDataPlanUpdateWatcherCallback::CreateCallback(modem_service_path,
                                                        data_plans);
  CashewClient::DataPlansUpdateHandler arg_callback;
  EXPECT_CALL(*mock_cashew_client_, SetDataPlansUpdateHandler(_))
      .WillOnce(SaveArg<0>(&arg_callback));

  // Start monitoring.
  CrosNetworkWatcher* watcher = CrosMonitorCellularDataPlan(callback);

  // Run callback.
  arg_callback.Run(modem_service_path, data_plans_list);

  // Stop monitoring.
  EXPECT_CALL(*mock_cashew_client_, ResetDataPlansUpdateHandler()).Times(1);
  delete watcher;
}

TEST_F(CrosNetworkFunctionsTest, CrosMonitorSMS) {
  const std::string dbus_connection = ":1.1";
  const dbus::ObjectPath object_path("/object/path");
  base::DictionaryValue device_properties;
  device_properties.SetWithoutPathExpansion(
      flimflam::kDBusConnectionProperty,
      base::Value::CreateStringValue(dbus_connection));
  device_properties.SetWithoutPathExpansion(
      flimflam::kDBusObjectProperty,
      base::Value::CreateStringValue(object_path.value()));

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
      SMSWatcher::kNumberKey, base::Value::CreateStringValue(number));
  sms_dictionary->SetWithoutPathExpansion(
      SMSWatcher::kTextKey, base::Value::CreateStringValue(text));
  sms_dictionary->SetWithoutPathExpansion(
      SMSWatcher::kTimestampKey,
      base::Value::CreateStringValue(timestamp_string));
  sms_dictionary->SetWithoutPathExpansion(SMSWatcher::kSmscKey,
                                         base::Value::CreateStringValue(smsc));
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

  void* object = this;
  const std::string modem_device_path = "/modem/device/path";

  // Set expectations.
  FlimflamDeviceClient::DictionaryValueCallback get_properties_callback;
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
      object, StrEq(modem_device_path), Pointee(IsSMSEqualTo(sms)))).Times(2);

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
      &CrosNetworkFunctionsTest::MockMonitorSMSCallbackThunk,
      object);
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
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
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
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(profile_path), _)).WillOnce(
                  Invoke(this, &CrosNetworkFunctionsTest::OnGetProperties));

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
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  // Set expectations.
  dictionary_value_result_ = &result;
  EXPECT_CALL(*mock_profile_client_,
              GetEntry(dbus::ObjectPath(profile_path), profile_entry_path, _))
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
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  dictionary_value_result_ = &result;
  // Create expected argument to FlimflamManagerClient::GetService.
  base::DictionaryValue properties;
  properties.SetWithoutPathExpansion(
      flimflam::kModeProperty,
      base::Value::CreateStringValue(flimflam::kModeManaged));
  properties.SetWithoutPathExpansion(
      flimflam::kTypeProperty,
      base::Value::CreateStringValue(flimflam::kTypeWifi));
  properties.SetWithoutPathExpansion(
      flimflam::kSSIDProperty,
      base::Value::CreateStringValue(ssid));
  properties.SetWithoutPathExpansion(
      flimflam::kSecurityProperty,
      base::Value::CreateStringValue(security));
  // Set expectations.
  const dbus::ObjectPath service_path("/service/path");
  FlimflamClientHelper::ObjectPathCallback callback;
  EXPECT_CALL(*mock_manager_client_, GetService(IsEqualTo(&properties), _))
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
  callback.Run(DBUS_METHOD_CALL_SUCCESS, service_path);
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
  result.SetWithoutPathExpansion(key1, base::Value::CreateStringValue(value1));
  result.SetWithoutPathExpansion(key2, base::Value::CreateStringValue(value2));
  dictionary_value_result_ = &result;
  // Create expected argument to FlimflamManagerClient::GetService.
  base::DictionaryValue properties;
  properties.SetWithoutPathExpansion(
      flimflam::kTypeProperty, base::Value::CreateStringValue("vpn"));
  properties.SetWithoutPathExpansion(
      flimflam::kProviderNameProperty,
      base::Value::CreateStringValue(service_name));
  properties.SetWithoutPathExpansion(
      flimflam::kProviderHostProperty,
      base::Value::CreateStringValue(server_hostname));
  properties.SetWithoutPathExpansion(
      flimflam::kProviderTypeProperty,
      base::Value::CreateStringValue(provider_type));
  properties.SetWithoutPathExpansion(
      flimflam::kVPNDomainProperty,
      base::Value::CreateStringValue(service_name));

  // Set expectations.
  const dbus::ObjectPath service_path("/service/path");
  FlimflamClientHelper::ObjectPathCallback callback;
  EXPECT_CALL(*mock_manager_client_, GetService(IsEqualTo(&properties), _))
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
  callback.Run(DBUS_METHOD_CALL_SUCCESS, service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkServiceDisconnect) {
  const std::string service_path = "/service/path";
  EXPECT_CALL(*mock_service_client_,
              Disconnect(dbus::ObjectPath(service_path), _)).Times(1);
  CrosRequestNetworkServiceDisconnect(service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestRemoveNetworkService) {
  const std::string service_path = "/service/path";
  EXPECT_CALL(*mock_service_client_,
              Remove(dbus::ObjectPath(service_path), _)).Times(1);
  CrosRequestRemoveNetworkService(service_path);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkScan) {
  EXPECT_CALL(*mock_manager_client_,
              RequestScan(flimflam::kTypeWifi, _)).Times(1);
  CrosRequestNetworkScan(flimflam::kTypeWifi);
}

TEST_F(CrosNetworkFunctionsTest, CrosRequestNetworkDeviceEnable) {
  const bool kEnable = true;
  EXPECT_CALL(*mock_manager_client_,
              EnableTechnology(flimflam::kTypeWifi, _)).Times(1);
  CrosRequestNetworkDeviceEnable(flimflam::kTypeWifi, kEnable);

  const bool kDisable = false;
  EXPECT_CALL(*mock_manager_client_,
              DisableTechnology(flimflam::kTypeWifi, _)).Times(1);
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
      flimflam::kOfflineModeProperty, IsEqualTo(&value), _)).Times(1);
  CrosSetOfflineMode(kOffline);
}

TEST_F(CrosNetworkFunctionsTest, CrosListIPConfigs) {
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
  ipconfigs->Append(base::Value::CreateStringValue(ipconfig_path));
  base::DictionaryValue* device_properties = new base::DictionaryValue;
  device_properties->SetWithoutPathExpansion(
      flimflam::kIPConfigsProperty, ipconfigs);
  device_properties->SetWithoutPathExpansion(
      flimflam::kAddressProperty,
      base::Value::CreateStringValue(hardware_address));

  base::ListValue* name_servers_list = new base::ListValue;
  name_servers_list->Append(base::Value::CreateStringValue(name_server1));
  name_servers_list->Append(base::Value::CreateStringValue(name_server2));
  base::DictionaryValue* ipconfig_properties = new base::DictionaryValue;
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kMethodProperty,
      base::Value::CreateStringValue(flimflam::kTypeDHCP));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kAddressProperty,
      base::Value::CreateStringValue(address));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kMtuProperty,
      base::Value::CreateIntegerValue(kMtu));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kPrefixlenProperty,
      base::Value::CreateIntegerValue(kPrefixlen));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kBroadcastProperty,
      base::Value::CreateStringValue(broadcast));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kPeerAddressProperty,
      base::Value::CreateStringValue(peer_address));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kGatewayProperty,
      base::Value::CreateStringValue(gateway));
  ipconfig_properties->SetWithoutPathExpansion(
      flimflam::kDomainNameProperty,
      base::Value::CreateStringValue(domainname));
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
      CrosListIPConfigs(device_path, &result_ipconfigs, &result_ipconfig_paths,
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

TEST_F(CrosNetworkFunctionsTest, CrosAddIPConfig) {
  const std::string device_path = "/device/path";
  const dbus::ObjectPath result_path("/result/path");
  EXPECT_CALL(*mock_device_client_,
              CallAddIPConfigAndBlock(dbus::ObjectPath(device_path),
                                      flimflam::kTypeDHCP))
      .WillOnce(Return(result_path));
  EXPECT_TRUE(CrosAddIPConfig(device_path, IPCONFIG_TYPE_DHCP));
}

TEST_F(CrosNetworkFunctionsTest, CrosRemoveIPConfig) {
  const std::string path = "/path";
  EXPECT_CALL(*mock_ipconfig_client_,
              CallRemoveAndBlock(dbus::ObjectPath(path))).Times(1);
  CrosRemoveIPConfig(path);
}

TEST_F(CrosNetworkFunctionsTest, CrosGetWifiAccessPoints) {
  const std::string device_path = "/device/path";
  base::ListValue* devices = new base::ListValue;
  devices->Append(base::Value::CreateStringValue(device_path));
  base::DictionaryValue* manager_properties = new base::DictionaryValue;
  manager_properties->SetWithoutPathExpansion(
      flimflam::kDevicesProperty, devices);

  const int kScanInterval = 42;
  const std::string network_path = "/network/path";
  base::ListValue* networks = new base::ListValue;
  networks->Append(base::Value::CreateStringValue(network_path));
  base::DictionaryValue* device_properties = new base::DictionaryValue;
  device_properties->SetWithoutPathExpansion(
      flimflam::kNetworksProperty, networks);
  device_properties->SetWithoutPathExpansion(
      flimflam::kPoweredProperty, base::Value::CreateBooleanValue(true));
  device_properties->SetWithoutPathExpansion(
      flimflam::kScanIntervalProperty,
      base::Value::CreateIntegerValue(kScanInterval));

  const int kSignalStrength = 10;
  const int kWifiChannel = 3;
  const std::string address = "address";
  const std::string name = "name";
  const base::Time expected_timestamp =
      base::Time::Now() - base::TimeDelta::FromSeconds(kScanInterval);
  const base::TimeDelta acceptable_timestamp_range =
      base::TimeDelta::FromSeconds(1);

  base::DictionaryValue* network_properties = new base::DictionaryValue;
  network_properties->SetWithoutPathExpansion(
      flimflam::kAddressProperty, base::Value::CreateStringValue(address));
  network_properties->SetWithoutPathExpansion(
      flimflam::kNameProperty, base::Value::CreateStringValue(name));
  network_properties->SetWithoutPathExpansion(
      flimflam::kSignalStrengthProperty,
      base::Value::CreateIntegerValue(kSignalStrength));
  network_properties->SetWithoutPathExpansion(
      flimflam::kWifiChannelProperty,
      base::Value::CreateIntegerValue(kWifiChannel));

  // Set expectations.
  EXPECT_CALL(*mock_manager_client_, CallGetPropertiesAndBlock())
      .WillOnce(Return(manager_properties));
  EXPECT_CALL(*mock_device_client_,
              CallGetPropertiesAndBlock(dbus::ObjectPath(device_path)))
      .WillOnce(Return(device_properties));
  EXPECT_CALL(*mock_network_client_,
              CallGetPropertiesAndBlock(dbus::ObjectPath(network_path)))
      .WillOnce(Return(network_properties));

  // Call function.
  WifiAccessPointVector aps;
  ASSERT_TRUE(CrosGetWifiAccessPoints(&aps));
  ASSERT_EQ(1U, aps.size());
  EXPECT_EQ(address, aps[0].mac_address);
  EXPECT_EQ(name, aps[0].name);
  EXPECT_LE(expected_timestamp - acceptable_timestamp_range, aps[0].timestamp);
  EXPECT_GE(expected_timestamp + acceptable_timestamp_range, aps[0].timestamp);
  EXPECT_EQ(kSignalStrength, aps[0].signal_strength);
  EXPECT_EQ(kWifiChannel, aps[0].channel);
}

TEST_F(CrosNetworkFunctionsTest, CrosConfigureService) {
  const std::string key1 = "key1";
  const std::string string1 = "string1";
  const std::string key2 = "key2";
  const std::string string2 = "string2";
  base::DictionaryValue value;
  value.SetString(key1, string1);
  value.SetString(key2, string2);
  EXPECT_CALL(*mock_manager_client_, ConfigureService(IsEqualTo(&value), _))
      .Times(1);
  CrosConfigureService(value);
}

}  // namespace chromeos
