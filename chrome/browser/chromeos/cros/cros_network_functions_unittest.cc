// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_network_functions.h"
#include "chrome/browser/chromeos/cros/sms_watcher.h"
#include "chromeos/dbus/mock_cashew_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_shill_device_client.h"
#include "chromeos/dbus/mock_shill_ipconfig_client.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_network_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/mock_shill_service_client.h"
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
    mock_cashew_client_ = mock_dbus_thread_manager->mock_cashew_client();
    mock_device_client_ =
        mock_dbus_thread_manager->mock_shill_device_client();
    mock_ipconfig_client_ =
        mock_dbus_thread_manager->mock_shill_ipconfig_client();
    mock_manager_client_ =
        mock_dbus_thread_manager->mock_shill_manager_client();
    mock_network_client_ =
        mock_dbus_thread_manager->mock_shill_network_client();
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

  // Handles responses for GetEntry method calls.
  void OnGetEntry(
      const dbus::ObjectPath& profile_path,
      const std::string& entry_path,
      const ShillClientHelper::DictionaryValueCallback& callback) {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *dictionary_value_result_);
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
  MockCashewClient* mock_cashew_client_;
  MockShillDeviceClient* mock_device_client_;
  MockShillIPConfigClient* mock_ipconfig_client_;
  MockShillManagerClient* mock_manager_client_;
  MockShillNetworkClient* mock_network_client_;
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
  ShillClientHelper::PropertyChangedHandler handler;
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
  ShillClientHelper::PropertyChangedHandler handler;
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
  ShillClientHelper::PropertyChangedHandler handler;
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
  // Create expected argument to ShillManagerClient::GetService.
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
  ObjectPathDBusMethodCallback callback;
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
  // Create expected argument to ShillManagerClient::GetService.
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
  ObjectPathDBusMethodCallback callback;
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

TEST_F(CrosNetworkFunctionsTest, NetmaskToPrefixLength) {
  // Valid netmasks
  EXPECT_EQ(32, CrosNetmaskToPrefixLength("255.255.255.255"));
  EXPECT_EQ(31, CrosNetmaskToPrefixLength("255.255.255.254"));
  EXPECT_EQ(30, CrosNetmaskToPrefixLength("255.255.255.252"));
  EXPECT_EQ(29, CrosNetmaskToPrefixLength("255.255.255.248"));
  EXPECT_EQ(28, CrosNetmaskToPrefixLength("255.255.255.240"));
  EXPECT_EQ(27, CrosNetmaskToPrefixLength("255.255.255.224"));
  EXPECT_EQ(26, CrosNetmaskToPrefixLength("255.255.255.192"));
  EXPECT_EQ(25, CrosNetmaskToPrefixLength("255.255.255.128"));
  EXPECT_EQ(24, CrosNetmaskToPrefixLength("255.255.255.0"));
  EXPECT_EQ(23, CrosNetmaskToPrefixLength("255.255.254.0"));
  EXPECT_EQ(22, CrosNetmaskToPrefixLength("255.255.252.0"));
  EXPECT_EQ(21, CrosNetmaskToPrefixLength("255.255.248.0"));
  EXPECT_EQ(20, CrosNetmaskToPrefixLength("255.255.240.0"));
  EXPECT_EQ(19, CrosNetmaskToPrefixLength("255.255.224.0"));
  EXPECT_EQ(18, CrosNetmaskToPrefixLength("255.255.192.0"));
  EXPECT_EQ(17, CrosNetmaskToPrefixLength("255.255.128.0"));
  EXPECT_EQ(16, CrosNetmaskToPrefixLength("255.255.0.0"));
  EXPECT_EQ(15, CrosNetmaskToPrefixLength("255.254.0.0"));
  EXPECT_EQ(14, CrosNetmaskToPrefixLength("255.252.0.0"));
  EXPECT_EQ(13, CrosNetmaskToPrefixLength("255.248.0.0"));
  EXPECT_EQ(12, CrosNetmaskToPrefixLength("255.240.0.0"));
  EXPECT_EQ(11, CrosNetmaskToPrefixLength("255.224.0.0"));
  EXPECT_EQ(10, CrosNetmaskToPrefixLength("255.192.0.0"));
  EXPECT_EQ(9, CrosNetmaskToPrefixLength("255.128.0.0"));
  EXPECT_EQ(8, CrosNetmaskToPrefixLength("255.0.0.0"));
  EXPECT_EQ(7, CrosNetmaskToPrefixLength("254.0.0.0"));
  EXPECT_EQ(6, CrosNetmaskToPrefixLength("252.0.0.0"));
  EXPECT_EQ(5, CrosNetmaskToPrefixLength("248.0.0.0"));
  EXPECT_EQ(4, CrosNetmaskToPrefixLength("240.0.0.0"));
  EXPECT_EQ(3, CrosNetmaskToPrefixLength("224.0.0.0"));
  EXPECT_EQ(2, CrosNetmaskToPrefixLength("192.0.0.0"));
  EXPECT_EQ(1, CrosNetmaskToPrefixLength("128.0.0.0"));
  EXPECT_EQ(0, CrosNetmaskToPrefixLength("0.0.0.0"));
  // Invalid netmasks
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255.255.255"));
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255.255.255.255.255"));
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255.255.255.255.0"));
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255.255.255.256"));
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255.255.255.1"));
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255.255.240.255"));
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255.0.0.255"));
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255.255.255.FF"));
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255,255,255,255"));
  EXPECT_EQ(-1, CrosNetmaskToPrefixLength("255 255 255 255"));
}

TEST_F(CrosNetworkFunctionsTest, PrefixLengthToNetmask) {
  // Valid Prefix Lengths
  EXPECT_EQ("255.255.255.255", CrosPrefixLengthToNetmask(32));
  EXPECT_EQ("255.255.255.254", CrosPrefixLengthToNetmask(31));
  EXPECT_EQ("255.255.255.252", CrosPrefixLengthToNetmask(30));
  EXPECT_EQ("255.255.255.248", CrosPrefixLengthToNetmask(29));
  EXPECT_EQ("255.255.255.240", CrosPrefixLengthToNetmask(28));
  EXPECT_EQ("255.255.255.224", CrosPrefixLengthToNetmask(27));
  EXPECT_EQ("255.255.255.192", CrosPrefixLengthToNetmask(26));
  EXPECT_EQ("255.255.255.128", CrosPrefixLengthToNetmask(25));
  EXPECT_EQ("255.255.255.0", CrosPrefixLengthToNetmask(24));
  EXPECT_EQ("255.255.254.0", CrosPrefixLengthToNetmask(23));
  EXPECT_EQ("255.255.252.0", CrosPrefixLengthToNetmask(22));
  EXPECT_EQ("255.255.248.0", CrosPrefixLengthToNetmask(21));
  EXPECT_EQ("255.255.240.0", CrosPrefixLengthToNetmask(20));
  EXPECT_EQ("255.255.224.0", CrosPrefixLengthToNetmask(19));
  EXPECT_EQ("255.255.192.0", CrosPrefixLengthToNetmask(18));
  EXPECT_EQ("255.255.128.0", CrosPrefixLengthToNetmask(17));
  EXPECT_EQ("255.255.0.0", CrosPrefixLengthToNetmask(16));
  EXPECT_EQ("255.254.0.0", CrosPrefixLengthToNetmask(15));
  EXPECT_EQ("255.252.0.0", CrosPrefixLengthToNetmask(14));
  EXPECT_EQ("255.248.0.0", CrosPrefixLengthToNetmask(13));
  EXPECT_EQ("255.240.0.0", CrosPrefixLengthToNetmask(12));
  EXPECT_EQ("255.224.0.0", CrosPrefixLengthToNetmask(11));
  EXPECT_EQ("255.192.0.0", CrosPrefixLengthToNetmask(10));
  EXPECT_EQ("255.128.0.0", CrosPrefixLengthToNetmask(9));
  EXPECT_EQ("255.0.0.0", CrosPrefixLengthToNetmask(8));
  EXPECT_EQ("254.0.0.0", CrosPrefixLengthToNetmask(7));
  EXPECT_EQ("252.0.0.0", CrosPrefixLengthToNetmask(6));
  EXPECT_EQ("248.0.0.0", CrosPrefixLengthToNetmask(5));
  EXPECT_EQ("240.0.0.0", CrosPrefixLengthToNetmask(4));
  EXPECT_EQ("224.0.0.0", CrosPrefixLengthToNetmask(3));
  EXPECT_EQ("192.0.0.0", CrosPrefixLengthToNetmask(2));
  EXPECT_EQ("128.0.0.0", CrosPrefixLengthToNetmask(1));
  EXPECT_EQ("0.0.0.0", CrosPrefixLengthToNetmask(0));
  // Invalid Prefix Lengths
  EXPECT_EQ("", CrosPrefixLengthToNetmask(-1));
  EXPECT_EQ("", CrosPrefixLengthToNetmask(33));
  EXPECT_EQ("", CrosPrefixLengthToNetmask(255));
}

}  // namespace chromeos
