// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <set>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/mock_shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_configuration_observer.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/shill_property_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;

// Matcher to match base::Value.
MATCHER_P(IsEqualTo, value, "") {
  return arg.Equals(value);
}

namespace chromeos {

namespace {

static std::string PrettyJson(const base::DictionaryValue& value) {
  std::string pretty;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &pretty);
  return pretty;
}

void DictionaryValueCallback(const std::string& expected_id,
                             const std::string& expected_json,
                             const std::string& service_path,
                             const base::DictionaryValue& dictionary) {
  std::string dict_str = PrettyJson(dictionary);
  EXPECT_EQ(expected_json, dict_str);
  EXPECT_EQ(expected_id, service_path);
}

void ErrorCallback(bool error_expected,
                   const std::string& expected_id,
                   const std::string& error_name,
                   std::unique_ptr<base::DictionaryValue> error_data) {
  EXPECT_TRUE(error_expected) << "Unexpected error: " << error_name
                              << " with associated data: \n"
                              << PrettyJson(*error_data);
}

void ServiceResultCallback(const std::string& expected_result,
                           const std::string& service_path,
                           const std::string& guid) {
  EXPECT_EQ(expected_result, service_path);
}

void DBusErrorCallback(const std::string& error_name,
                       const std::string& error_message) {
  EXPECT_TRUE(false) << "DBus Error: " << error_name << "(" << error_message
                     << ")";
}

class TestCallback {
 public:
  TestCallback() : run_count_(0) {}
  void Run() { ++run_count_; }
  int run_count() const { return run_count_; }

 private:
  int run_count_;
};

class TestNetworkConfigurationObserver : public NetworkConfigurationObserver {
 public:
  TestNetworkConfigurationObserver() {}
  ~TestNetworkConfigurationObserver() override {
  }

  // NetworkConfigurationObserver
  void OnConfigurationCreated(
      const std::string& service_path,
      const std::string& profile_path,
      const base::DictionaryValue& properties,
      NetworkConfigurationObserver::Source source) override {
    ASSERT_EQ(0u, configurations_.count(service_path));
    configurations_[service_path] = properties.CreateDeepCopy();
    profiles_[profile_path].insert(service_path);
  }

  void OnConfigurationRemoved(
      const std::string& service_path,
      const std::string& guid,
      NetworkConfigurationObserver::Source source) override {
    ASSERT_EQ(1u, configurations_.count(service_path));
    configurations_.erase(service_path);
    for (auto& p : profiles_) {
      p.second.erase(service_path);
    }
  }

  void OnConfigurationProfileChanged(
      const std::string& service_path,
      const std::string& profile_path,
      NetworkConfigurationObserver::Source source) override {
    for (auto& p : profiles_) {
      p.second.erase(service_path);
    }
    profiles_[profile_path].insert(service_path);
  }

  void OnPropertiesSet(const std::string& service_path,
                       const std::string& guid,
                       const base::DictionaryValue& set_properties,
                       NetworkConfigurationObserver::Source source) override {
    configurations_[service_path]->MergeDictionary(&set_properties);
  }

  bool HasConfiguration(const std::string& service_path) {
    return configurations_.count(service_path) == 1;
  }

  std::string GetStringProperty(const std::string& service_path,
                                const std::string& property) {
    if (!HasConfiguration(service_path))
      return "";
    std::string result;
    configurations_[service_path]->GetStringWithoutPathExpansion(property,
                                                                 &result);
    return result;
  }

  bool HasConfigurationInProfile(const std::string& service_path,
                                 const std::string& profile_path) {
    if (profiles_.count(profile_path) == 0)
      return false;
    return profiles_[profile_path].count(service_path) == 1;
  }

 private:
  std::map<std::string, std::unique_ptr<base::DictionaryValue>> configurations_;
  std::map<std::string, std::set<std::string>> profiles_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkConfigurationObserver);
};

}  // namespace

class NetworkConfigurationHandlerTest : public testing::Test {
 public:
  NetworkConfigurationHandlerTest()
      : mock_manager_client_(NULL),
        mock_profile_client_(NULL),
        mock_service_client_(NULL),
        dictionary_value_result_(NULL) {}
  ~NetworkConfigurationHandlerTest() override {}

  void SetUp() override {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        DBusThreadManager::GetSetterForTesting();
    mock_manager_client_ = new MockShillManagerClient();
    mock_profile_client_ = new MockShillProfileClient();
    mock_service_client_ = new MockShillServiceClient();
    dbus_setter->SetShillManagerClient(
        std::unique_ptr<ShillManagerClient>(mock_manager_client_));
    dbus_setter->SetShillProfileClient(
        std::unique_ptr<ShillProfileClient>(mock_profile_client_));
    dbus_setter->SetShillServiceClient(
        std::unique_ptr<ShillServiceClient>(mock_service_client_));

    EXPECT_CALL(*mock_service_client_, GetProperties(_, _)).Times(AnyNumber());
    EXPECT_CALL(*mock_manager_client_, GetProperties(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_manager_client_, AddPropertyChangedObserver(_))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_manager_client_, RemovePropertyChangedObserver(_))
        .Times(AnyNumber());

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_configuration_handler_.reset(new NetworkConfigurationHandler());
    network_configuration_handler_->Init(network_state_handler_.get(),
                                         NULL /* network_device_handler */);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_handler_->Shutdown();
    network_configuration_handler_.reset();
    network_state_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  // Handles responses for GetProperties method calls.
  void OnGetProperties(
      const dbus::ObjectPath& path,
      const ShillClientHelper::DictionaryValueCallback& callback) {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *dictionary_value_result_);
  }

  // Handles responses for SetProperties method calls.
  void OnSetProperties(const dbus::ObjectPath& service_path,
                       const base::DictionaryValue& properties,
                       const base::Closure& callback,
                       const ShillClientHelper::ErrorCallback& error_callback) {
    callback.Run();
  }

  // Handles responses for ClearProperties method calls.
  void OnClearProperties(
      const dbus::ObjectPath& service_path,
      const std::vector<std::string>& names,
      const ShillClientHelper::ListValueCallback& callback,
      const ShillClientHelper::ErrorCallback& error_callback) {
    base::ListValue result;
    result.AppendBoolean(true);
    callback.Run(result);
  }

  // Handles responses for ClearProperties method calls, and simulates an error
  // result.
  void OnClearPropertiesError(
      const dbus::ObjectPath& service_path,
      const std::vector<std::string>& names,
      const ShillClientHelper::ListValueCallback& callback,
      const ShillClientHelper::ErrorCallback& error_callback) {
    base::ListValue result;
    result.AppendBoolean(false);
    callback.Run(result);
  }

  void OnConfigureService(
      const dbus::ObjectPath& profile_path,
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ShillClientHelper::ErrorCallback& error_callback) {
    callback.Run(dbus::ObjectPath("/service/2"));
  }

  void OnGetLoadableProfileEntries(
      const dbus::ObjectPath& service_path,
      const ShillClientHelper::DictionaryValueCallback& callback) {
    base::DictionaryValue entries;
    entries.SetString("profile1", "entry1");
    entries.SetString("profile2", "entry2");
    callback.Run(DBUS_METHOD_CALL_SUCCESS, entries);
  }

  void OnDeleteEntry(const dbus::ObjectPath& profile_path,
                     const std::string& entry_path,
                     const base::Closure& callback,
                     const ShillClientHelper::ErrorCallback& error_callback) {
    // Don't run the callback immediately to emulate actual behavior.
    message_loop_.task_runner()->PostTask(FROM_HERE, callback);
  }

  bool PendingProfileEntryDeleterForTest(const std::string& service_path) {
    return network_configuration_handler_->PendingProfileEntryDeleterForTest(
        service_path);
  }

  void CreateConfiguration(const std::string& service_path,
                           const base::DictionaryValue& properties) {
    network_configuration_handler_->CreateShillConfiguration(
        properties, NetworkConfigurationObserver::SOURCE_USER_ACTION,
        base::Bind(&ServiceResultCallback, service_path),
        base::Bind(&ErrorCallback, false, std::string()));
  }

 protected:
  MockShillManagerClient* mock_manager_client_;
  MockShillProfileClient* mock_profile_client_;
  MockShillServiceClient* mock_service_client_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  base::MessageLoopForUI message_loop_;
  base::DictionaryValue* dictionary_value_result_;
};

TEST_F(NetworkConfigurationHandlerTest, GetProperties) {
  std::string service_path = "/service/1";
  std::string expected_json = "{\n   \"SSID\": \"MyNetwork\"\n}\n";
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  std::unique_ptr<base::StringValue> networkNameValue(
      new base::StringValue(networkName));

  base::DictionaryValue value;
  value.Set(key, new base::StringValue(networkName));
  dictionary_value_result_ = &value;
  EXPECT_CALL(*mock_service_client_,
              SetProperty(dbus::ObjectPath(service_path), key,
                          IsEqualTo(networkNameValue.get()), _, _)).Times(1);
  mock_service_client_->SetProperty(
      dbus::ObjectPath(service_path), key, *networkNameValue,
      base::Bind(&base::DoNothing), base::Bind(&DBusErrorCallback));
  base::RunLoop().RunUntilIdle();

  ShillServiceClient::DictionaryValueCallback get_properties_callback;
  EXPECT_CALL(*mock_service_client_, GetProperties(_, _))
      .WillOnce(
          Invoke(this, &NetworkConfigurationHandlerTest::OnGetProperties));
  network_configuration_handler_->GetShillProperties(
      service_path,
      base::Bind(&DictionaryValueCallback, service_path, expected_json),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, SetProperties) {
  std::string service_path = "/service/1";
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  std::unique_ptr<base::StringValue> networkNameValue(
      new base::StringValue(networkName));

  base::DictionaryValue value;
  value.Set(key, new base::StringValue(networkName));
  dictionary_value_result_ = &value;
  EXPECT_CALL(*mock_service_client_, SetProperties(_, _, _, _))
      .WillOnce(
          Invoke(this, &NetworkConfigurationHandlerTest::OnSetProperties));
  network_configuration_handler_->SetShillProperties(
      service_path, value, NetworkConfigurationObserver::SOURCE_USER_ACTION,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, ClearProperties) {
  std::string service_path = "/service/1";
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  std::unique_ptr<base::StringValue> networkNameValue(
      new base::StringValue(networkName));

  // First set up a value to clear.
  base::DictionaryValue value;
  value.Set(key, new base::StringValue(networkName));
  dictionary_value_result_ = &value;
  EXPECT_CALL(*mock_service_client_, SetProperties(_, _, _, _))
      .WillOnce(
          Invoke(this, &NetworkConfigurationHandlerTest::OnSetProperties));
  network_configuration_handler_->SetShillProperties(
      service_path, value, NetworkConfigurationObserver::SOURCE_USER_ACTION,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();

  // Now clear it.
  std::vector<std::string> values_to_clear;
  values_to_clear.push_back(key);
  EXPECT_CALL(*mock_service_client_, ClearProperties(_, _, _, _))
      .WillOnce(
          Invoke(this, &NetworkConfigurationHandlerTest::OnClearProperties));
  network_configuration_handler_->ClearShillProperties(
      service_path, values_to_clear, base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, ClearPropertiesError) {
  std::string service_path = "/service/1";
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  std::unique_ptr<base::StringValue> networkNameValue(
      new base::StringValue(networkName));

  // First set up a value to clear.
  base::DictionaryValue value;
  value.Set(key, new base::StringValue(networkName));
  dictionary_value_result_ = &value;
  EXPECT_CALL(*mock_service_client_, SetProperties(_, _, _, _))
      .WillOnce(
          Invoke(this, &NetworkConfigurationHandlerTest::OnSetProperties));
  network_configuration_handler_->SetShillProperties(
      service_path, value, NetworkConfigurationObserver::SOURCE_USER_ACTION,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();

  // Now clear it.
  std::vector<std::string> values_to_clear;
  values_to_clear.push_back(key);
  EXPECT_CALL(*mock_service_client_, ClearProperties(_, _, _, _))
      .WillOnce(Invoke(
          this, &NetworkConfigurationHandlerTest::OnClearPropertiesError));
  network_configuration_handler_->ClearShillProperties(
      service_path, values_to_clear, base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, true, service_path));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, CreateConfiguration) {
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  std::string type = "wifi";
  std::string profile = "profile path";
  base::DictionaryValue value;
  shill_property_util::SetSSID(networkName, &value);
  value.SetWithoutPathExpansion(shill::kTypeProperty,
                                new base::StringValue(type));
  value.SetWithoutPathExpansion(shill::kProfileProperty,
                                new base::StringValue(profile));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(dbus::ObjectPath(profile), _, _, _))
      .WillOnce(
          Invoke(this, &NetworkConfigurationHandlerTest::OnConfigureService));
  CreateConfiguration("/service/2", value);
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, RemoveConfiguration) {
  std::string service_path = "/service/1";

  TestCallback test_callback;
  EXPECT_CALL(*mock_service_client_, GetLoadableProfileEntries(_, _))
      .WillOnce(Invoke(
          this, &NetworkConfigurationHandlerTest::OnGetLoadableProfileEntries));
  EXPECT_CALL(*mock_profile_client_, DeleteEntry(_, _, _, _))
      .WillRepeatedly(
          Invoke(this, &NetworkConfigurationHandlerTest::OnDeleteEntry));

  network_configuration_handler_->RemoveConfiguration(
      service_path, NetworkConfigurationObserver::SOURCE_USER_ACTION,
      base::Bind(&TestCallback::Run, base::Unretained(&test_callback)),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_callback.run_count());
  EXPECT_FALSE(PendingProfileEntryDeleterForTest(service_path));
}

////////////////////////////////////////////////////////////////////////////////
// Stub based tests

namespace {

class TestObserver : public chromeos::NetworkStateHandlerObserver {
 public:
  TestObserver() : network_list_changed_count_(0) {}
  ~TestObserver() override {}

  void NetworkListChanged() override { ++network_list_changed_count_; }

  void NetworkPropertiesUpdated(const NetworkState* network) override {
    property_updates_[network->path()]++;
  }

  size_t network_list_changed_count() { return network_list_changed_count_; }

  int PropertyUpdatesForService(const std::string& service_path) {
    return property_updates_[service_path];
  }

  void ClearPropertyUpdates() { property_updates_.clear(); }

 private:
  size_t network_list_changed_count_;
  std::map<std::string, int> property_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class NetworkConfigurationHandlerStubTest : public testing::Test {
 public:
  NetworkConfigurationHandlerStubTest() {}

  ~NetworkConfigurationHandlerStubTest() override {}

  void SetUp() override {
    DBusThreadManager::Initialize();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    test_observer_.reset(new TestObserver());
    network_state_handler_->AddObserver(test_observer_.get(), FROM_HERE);

    network_configuration_handler_.reset(new NetworkConfigurationHandler());
    network_configuration_handler_->Init(network_state_handler_.get(),
                                         NULL /* network_device_handler */);

    base::RunLoop().RunUntilIdle();
    test_observer_->ClearPropertyUpdates();
  }

  void TearDown() override {
    network_state_handler_->Shutdown();
    network_configuration_handler_.reset();
    network_state_handler_->RemoveObserver(test_observer_.get(), FROM_HERE);
    network_state_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  void SuccessCallback(const std::string& callback_name) {
    success_callback_name_ = callback_name;
  }

  void GetPropertiesCallback(const std::string& service_path,
                             const base::DictionaryValue& dictionary) {
    get_properties_path_ = service_path;
    get_properties_ = dictionary.CreateDeepCopy();
  }

  void CreateConfigurationCallback(const std::string& service_path,
                                   const std::string& guid) {
    create_service_path_ = service_path;
  }

  void CreateTestConfiguration(const std::string& service_path,
                               const std::string& type) {
    base::DictionaryValue properties;
    shill_property_util::SetSSID(service_path, &properties);
    properties.SetStringWithoutPathExpansion(shill::kNameProperty,
                                             service_path);
    properties.SetStringWithoutPathExpansion(shill::kGuidProperty,
                                             service_path);
    properties.SetStringWithoutPathExpansion(shill::kTypeProperty, type);
    properties.SetStringWithoutPathExpansion(shill::kStateProperty,
                                             shill::kStateIdle);
    properties.SetStringWithoutPathExpansion(
        shill::kProfileProperty, NetworkProfileHandler::GetSharedProfilePath());

    network_configuration_handler_->CreateShillConfiguration(
        properties, NetworkConfigurationObserver::SOURCE_USER_ACTION,
        base::Bind(
            &NetworkConfigurationHandlerStubTest::CreateConfigurationCallback,
            base::Unretained(this)),
        base::Bind(&ErrorCallback, false, service_path));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  bool GetServiceStringProperty(const std::string& service_path,
                                const std::string& key,
                                std::string* result) {
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    const base::DictionaryValue* properties =
        service_test->GetServiceProperties(service_path);
    if (properties && properties->GetStringWithoutPathExpansion(key, result))
      return true;
    return false;
  }

  bool GetReceivedStringProperty(const std::string& service_path,
                                 const std::string& key,
                                 std::string* result) {
    if (get_properties_path_ != service_path)
      return false;
    if (get_properties_ &&
        get_properties_->GetStringWithoutPathExpansion(key, result))
      return true;
    return false;
  }

  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<TestObserver> test_observer_;
  base::MessageLoopForUI message_loop_;
  std::string success_callback_name_;
  std::string get_properties_path_;
  std::unique_ptr<base::DictionaryValue> get_properties_;
  std::string create_service_path_;
};

TEST_F(NetworkConfigurationHandlerStubTest, StubSetAndClearProperties) {
  // TODO(stevenjb): Remove dependency on default Stub service.
  const std::string service_path("/service/wifi1");
  const std::string test_identity("test_identity");
  const std::string test_passphrase("test_passphrase");

  // Set Properties
  base::DictionaryValue properties_to_set;
  properties_to_set.SetStringWithoutPathExpansion(shill::kIdentityProperty,
                                                  test_identity);
  properties_to_set.SetStringWithoutPathExpansion(shill::kPassphraseProperty,
                                                  test_passphrase);
  network_configuration_handler_->SetShillProperties(
      service_path, properties_to_set,
      NetworkConfigurationObserver::SOURCE_USER_ACTION,
      base::Bind(&NetworkConfigurationHandlerStubTest::SuccessCallback,
                 base::Unretained(this), "SetProperties"),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("SetProperties", success_callback_name_);
  std::string identity, passphrase;
  EXPECT_TRUE(GetServiceStringProperty(service_path, shill::kIdentityProperty,
                                       &identity));
  EXPECT_TRUE(GetServiceStringProperty(service_path, shill::kPassphraseProperty,
                                       &passphrase));
  EXPECT_EQ(test_identity, identity);
  EXPECT_EQ(test_passphrase, passphrase);
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(service_path));

  // Clear Properties
  std::vector<std::string> properties_to_clear;
  properties_to_clear.push_back(shill::kIdentityProperty);
  properties_to_clear.push_back(shill::kPassphraseProperty);
  network_configuration_handler_->ClearShillProperties(
      service_path, properties_to_clear,
      base::Bind(&NetworkConfigurationHandlerStubTest::SuccessCallback,
                 base::Unretained(this), "ClearProperties"),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("ClearProperties", success_callback_name_);
  EXPECT_FALSE(GetServiceStringProperty(service_path, shill::kIdentityProperty,
                                        &identity));
  EXPECT_FALSE(GetServiceStringProperty(service_path, shill::kIdentityProperty,
                                        &passphrase));
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(service_path));
}

TEST_F(NetworkConfigurationHandlerStubTest, StubGetNameFromWifiHex) {
  // TODO(stevenjb): Remove dependency on default Stub service.
  const std::string service_path("/service/wifi1");
  std::string wifi_hex = "5468697320697320484558205353494421";
  std::string expected_name = "This is HEX SSID!";

  // Set Properties
  base::DictionaryValue properties_to_set;
  properties_to_set.SetStringWithoutPathExpansion(shill::kWifiHexSsid,
                                                  wifi_hex);
  network_configuration_handler_->SetShillProperties(
      service_path, properties_to_set,
      NetworkConfigurationObserver::SOURCE_USER_ACTION,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();
  std::string wifi_hex_result;
  EXPECT_TRUE(GetServiceStringProperty(service_path, shill::kWifiHexSsid,
                                       &wifi_hex_result));
  EXPECT_EQ(wifi_hex, wifi_hex_result);

  // Get Properties
  network_configuration_handler_->GetShillProperties(
      service_path,
      base::Bind(&NetworkConfigurationHandlerStubTest::GetPropertiesCallback,
                 base::Unretained(this)),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service_path, get_properties_path_);
  std::string name_result;
  EXPECT_TRUE(GetReceivedStringProperty(service_path, shill::kNameProperty,
                                        &name_result));
  EXPECT_EQ(expected_name, name_result);
}

TEST_F(NetworkConfigurationHandlerStubTest, StubCreateConfiguration) {
  const std::string service_path("/service/test_wifi");
  CreateTestConfiguration(service_path, shill::kTypeWifi);

  EXPECT_FALSE(create_service_path_.empty());

  std::string guid;
  EXPECT_TRUE(GetServiceStringProperty(create_service_path_,
                                       shill::kGuidProperty, &guid));
  EXPECT_EQ(service_path, guid);

  std::string actual_profile;
  EXPECT_TRUE(GetServiceStringProperty(
      create_service_path_, shill::kProfileProperty, &actual_profile));
  EXPECT_EQ(NetworkProfileHandler::GetSharedProfilePath(), actual_profile);
}

TEST_F(NetworkConfigurationHandlerStubTest, NetworkConfigurationObserver) {
  const std::string service_path("/service/test_wifi");
  const std::string test_passphrase("test_passphrase");

  std::unique_ptr<TestNetworkConfigurationObserver> test_observer(
      new TestNetworkConfigurationObserver);
  network_configuration_handler_->AddObserver(test_observer.get());
  CreateTestConfiguration(service_path, shill::kTypeWifi);

  EXPECT_TRUE(test_observer->HasConfiguration(service_path));
  EXPECT_TRUE(test_observer->HasConfigurationInProfile(
      service_path, NetworkProfileHandler::GetSharedProfilePath()));
  EXPECT_EQ(shill::kTypeWifi, test_observer->GetStringProperty(
                                  service_path, shill::kTypeProperty));

  base::DictionaryValue properties_to_set;
  properties_to_set.SetStringWithoutPathExpansion(shill::kPassphraseProperty,
                                                  test_passphrase);
  network_configuration_handler_->SetShillProperties(
      service_path, properties_to_set,
      NetworkConfigurationObserver::SOURCE_USER_ACTION,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_passphrase, test_observer->GetStringProperty(
                                 service_path, shill::kPassphraseProperty));

  std::string user_profile = "/profiles/user1";
  std::string userhash = "user1";
  DBusThreadManager::Get()
      ->GetShillProfileClient()
      ->GetTestInterface()
      ->AddProfile(user_profile, userhash);

  network_configuration_handler_->SetNetworkProfile(
      service_path, user_profile,
      NetworkConfigurationObserver::SOURCE_USER_ACTION,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_observer->HasConfiguration(service_path));
  EXPECT_FALSE(test_observer->HasConfigurationInProfile(
      service_path, NetworkProfileHandler::GetSharedProfilePath()));
  EXPECT_TRUE(
      test_observer->HasConfigurationInProfile(service_path, user_profile));

  network_configuration_handler_->RemoveConfiguration(
      service_path, NetworkConfigurationObserver::SOURCE_USER_ACTION,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_observer->HasConfiguration(service_path));
  EXPECT_FALSE(test_observer->HasConfigurationInProfile(
      service_path, NetworkProfileHandler::GetSharedProfilePath()));
  EXPECT_FALSE(
      test_observer->HasConfigurationInProfile(service_path, user_profile));

  network_configuration_handler_->RemoveObserver(test_observer.get());
}

}  // namespace chromeos
