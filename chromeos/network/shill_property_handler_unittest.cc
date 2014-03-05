// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_property_handler.h"

#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void DoNothingWithCallStatus(DBusMethodCallStatus call_status) {
}

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

class TestListener : public internal::ShillPropertyHandler::Listener {
 public:
  TestListener() : technology_list_updates_(0),
                   errors_(0) {
  }

  virtual void UpdateManagedList(ManagedState::ManagedType type,
                                 const base::ListValue& entries) OVERRIDE {
    UpdateEntries(GetTypeString(type), entries);
  }

  virtual void UpdateManagedStateProperties(
      ManagedState::ManagedType type,
      const std::string& path,
      const base::DictionaryValue& properties) OVERRIDE {
    AddInitialPropertyUpdate(GetTypeString(type), path);
  }

  virtual void ProfileListChanged() OVERRIDE {
  }

  virtual void UpdateNetworkServiceProperty(
      const std::string& service_path,
      const std::string& key,
      const base::Value& value) OVERRIDE {
    AddPropertyUpdate(shill::kServicesProperty, service_path);
  }

  virtual void UpdateDeviceProperty(
      const std::string& device_path,
      const std::string& key,
      const base::Value& value) OVERRIDE {
    AddPropertyUpdate(shill::kDevicesProperty, device_path);
  }

  virtual void TechnologyListChanged() OVERRIDE {
    ++technology_list_updates_;
  }

  virtual void CheckPortalListChanged(
      const std::string& check_portal_list) OVERRIDE {
  }

  virtual void ManagedStateListChanged(
      ManagedState::ManagedType type) OVERRIDE {
    AddStateListUpdate(GetTypeString(type));
  }

  virtual void DefaultNetworkServiceChanged(
      const std::string& service_path) OVERRIDE {
  }

  std::vector<std::string>& entries(const std::string& type) {
    return entries_[type];
  }
  std::map<std::string, int>& property_updates(const std::string& type) {
    return property_updates_[type];
  }
  std::map<std::string, int>& initial_property_updates(
      const std::string& type) {
    return initial_property_updates_[type];
  }
  int list_updates(const std::string& type) { return list_updates_[type]; }
  void reset_list_updates() { list_updates_.clear(); }
  int technology_list_updates() { return technology_list_updates_; }
  int errors() { return errors_; }

 private:
  std::string GetTypeString(ManagedState::ManagedType type) {
    if (type == ManagedState::MANAGED_TYPE_NETWORK) {
      return shill::kServicesProperty;
    } else if (type == ManagedState::MANAGED_TYPE_FAVORITE) {
      return shill::kServiceCompleteListProperty;
    } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
      return shill::kDevicesProperty;
    }
    LOG(ERROR) << "UpdateManagedList called with unrecognized type: " << type;
    ++errors_;
    return std::string();
  }

  void UpdateEntries(const std::string& type, const base::ListValue& entries) {
    if (type.empty())
      return;
    entries_[type].clear();
    for (base::ListValue::const_iterator iter = entries.begin();
         iter != entries.end(); ++iter) {
      std::string path;
      if ((*iter)->GetAsString(&path))
        entries_[type].push_back(path);
    }
  }

  void AddPropertyUpdate(const std::string& type, const std::string& path) {
    if (type.empty())
      return;
    property_updates(type)[path] += 1;
  }

  void AddInitialPropertyUpdate(const std::string& type,
                                const std::string& path) {
    if (type.empty())
      return;
    initial_property_updates(type)[path] += 1;
  }

  void AddStateListUpdate(const std::string& type) {
    if (type.empty())
      return;
    list_updates_[type] += 1;
  }

  // Map of list-type -> paths
  std::map<std::string, std::vector<std::string> > entries_;
  // Map of list-type -> map of paths -> update counts
  std::map<std::string, std::map<std::string, int> > property_updates_;
  std::map<std::string, std::map<std::string, int> > initial_property_updates_;
  // Map of list-type -> list update counts
  std::map<std::string, int > list_updates_;
  int technology_list_updates_;
  int errors_;
};

}  // namespace

class ShillPropertyHandlerTest : public testing::Test {
 public:
  ShillPropertyHandlerTest()
      : manager_test_(NULL),
        device_test_(NULL),
        service_test_(NULL),
        profile_test_(NULL) {
  }
  virtual ~ShillPropertyHandlerTest() {
  }

  virtual void SetUp() OVERRIDE {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::InitializeWithStub();
    // Get the test interface for manager / device / service and clear the
    // default stub properties.
    manager_test_ =
        DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
    ASSERT_TRUE(manager_test_);
    device_test_ =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    ASSERT_TRUE(device_test_);
    service_test_ =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    ASSERT_TRUE(service_test_);
    profile_test_ =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    ASSERT_TRUE(profile_test_);
    SetupShillPropertyHandler();
    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    shill_property_handler_.reset();
    listener_.reset();
    DBusThreadManager::Shutdown();
  }

  void AddDevice(const std::string& type, const std::string& id) {
    ASSERT_TRUE(IsValidType(type));
    device_test_->AddDevice(id, type, std::string("/device/" + id));
  }

  void RemoveDevice(const std::string& id) {
    device_test_->RemoveDevice(id);
  }

  void AddService(const std::string& type,
                  const std::string& id,
                  const std::string& state,
                  bool add_to_watch_list) {
    ASSERT_TRUE(IsValidType(type));
    service_test_->AddService(id, id, type, state,
                              true /* visible */, add_to_watch_list);
  }

  void AddServiceWithIPConfig(const std::string& type,
                              const std::string& id,
                              const std::string& state,
                              const std::string& ipconfig_path,
                              bool add_to_watch_list) {
    ASSERT_TRUE(IsValidType(type));
    service_test_->AddServiceWithIPConfig(id, id, type, state,
                                          ipconfig_path,
                                          true /* visible */,
                                          add_to_watch_list);
  }

  void AddServiceToProfile(const std::string& type,
                           const std::string& id,
                           bool visible) {
    service_test_->AddService(id, id, type, shill::kStateIdle,
                              visible, false /* watch */);
    std::vector<std::string> profiles;
    profile_test_->GetProfilePaths(&profiles);
    ASSERT_TRUE(profiles.size() > 0);
    base::DictionaryValue properties;  // Empty entry
    profile_test_->AddService(profiles[0], id);
  }

  void RemoveService(const std::string& id) {
    service_test_->RemoveService(id);
  }

  // Call this after any initial Shill client setup
  void SetupShillPropertyHandler() {
    SetupDefaultShillState();
    listener_.reset(new TestListener);
    shill_property_handler_.reset(
        new internal::ShillPropertyHandler(listener_.get()));
    shill_property_handler_->Init();
  }

  bool IsValidType(const std::string& type) {
    return (type == shill::kTypeEthernet ||
            type == shill::kTypeEthernetEap ||
            type == shill::kTypeWifi ||
            type == shill::kTypeWimax ||
            type == shill::kTypeBluetooth ||
            type == shill::kTypeCellular ||
            type == shill::kTypeVPN);
  }

 protected:
  void SetupDefaultShillState() {
    message_loop_.RunUntilIdle();  // Process any pending updates
    device_test_->ClearDevices();
    AddDevice(shill::kTypeWifi, "stub_wifi_device1");
    AddDevice(shill::kTypeCellular, "stub_cellular_device1");
    service_test_->ClearServices();
    const bool add_to_watchlist = true;
    AddService(shill::kTypeEthernet, "stub_ethernet",
               shill::kStateOnline, add_to_watchlist);
    AddService(shill::kTypeWifi, "stub_wifi1",
               shill::kStateOnline, add_to_watchlist);
    AddService(shill::kTypeWifi, "stub_wifi2",
               shill::kStateIdle, add_to_watchlist);
    AddService(shill::kTypeCellular, "stub_cellular1",
               shill::kStateIdle, add_to_watchlist);
  }

  base::MessageLoopForUI message_loop_;
  scoped_ptr<TestListener> listener_;
  scoped_ptr<internal::ShillPropertyHandler> shill_property_handler_;
  ShillManagerClient::TestInterface* manager_test_;
  ShillDeviceClient::TestInterface* device_test_;
  ShillServiceClient::TestInterface* service_test_;
  ShillProfileClient::TestInterface* profile_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillPropertyHandlerTest);
};

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerStub) {
  EXPECT_TRUE(shill_property_handler_->IsTechnologyAvailable(shill::kTypeWifi));
  EXPECT_TRUE(shill_property_handler_->IsTechnologyEnabled(shill::kTypeWifi));
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            listener_->entries(shill::kDevicesProperty).size());
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            listener_->entries(shill::kServicesProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerTechnologyChanged) {
  const int initial_technology_updates = 2;  // Available and Enabled lists
  EXPECT_EQ(initial_technology_updates, listener_->technology_list_updates());

  // Remove a technology. Updates both the Available and Enabled lists.
  manager_test_->RemoveTechnology(shill::kTypeWimax);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(initial_technology_updates + 2,
            listener_->technology_list_updates());

  // Add a disabled technology.
  manager_test_->AddTechnology(shill::kTypeWimax, false);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(initial_technology_updates + 3,
            listener_->technology_list_updates());
  EXPECT_TRUE(shill_property_handler_->IsTechnologyAvailable(
      shill::kTypeWimax));
  EXPECT_FALSE(shill_property_handler_->IsTechnologyEnabled(shill::kTypeWimax));

  // Enable the technology.
  DBusThreadManager::Get()->GetShillManagerClient()->EnableTechnology(
      shill::kTypeWimax,
      base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(initial_technology_updates + 4,
            listener_->technology_list_updates());
  EXPECT_TRUE(shill_property_handler_->IsTechnologyEnabled(shill::kTypeWimax));

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerDevicePropertyChanged) {
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            listener_->entries(shill::kDevicesProperty).size());
  // Add a device.
  listener_->reset_list_updates();
  const std::string kTestDevicePath("test_wifi_device1");
  AddDevice(shill::kTypeWifi, kTestDevicePath);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kDevicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplDevices + 1,
            listener_->entries(shill::kDevicesProperty).size());
  // Device changes are not observed.
  // Remove a device
  listener_->reset_list_updates();
  RemoveDevice(kTestDevicePath);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kDevicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            listener_->entries(shill::kDevicesProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerServicePropertyChanged) {
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            listener_->entries(shill::kServicesProperty).size());

  // Add an unwatched service.
  listener_->reset_list_updates();
  const std::string kTestServicePath("test_wifi_service1");
  AddService(shill::kTypeWifi, kTestServicePath, shill::kStateIdle, false);
  message_loop_.RunUntilIdle();
  // Watched and unwatched services trigger a service list update.
  EXPECT_EQ(1, listener_->list_updates(shill::kServicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1,
            listener_->entries(shill::kServicesProperty).size());
  // Service receives an initial property update.
  EXPECT_EQ(1, listener_->initial_property_updates(
      shill::kServicesProperty)[kTestServicePath]);
  // Change a property.
  base::FundamentalValue scan_interval(3);
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(kTestServicePath),
      shill::kScanIntervalProperty,
      scan_interval,
      base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  message_loop_.RunUntilIdle();
  // Property change triggers an update.
  EXPECT_EQ(1, listener_->property_updates(
      shill::kServicesProperty)[kTestServicePath]);

  // Add the existing service to the watch list.
  listener_->reset_list_updates();
  AddService(shill::kTypeWifi, kTestServicePath, shill::kStateIdle, true);
  message_loop_.RunUntilIdle();
  // Service list update should be received when watch list changes.
  EXPECT_EQ(1, listener_->list_updates(shill::kServicesProperty));
  // Number of services shouldn't change.
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1,
            listener_->entries(shill::kServicesProperty).size());

  // Change a property.
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(kTestServicePath),
      shill::kScanIntervalProperty,
      scan_interval,
      base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  message_loop_.RunUntilIdle();
  // Property change should trigger another update.
  EXPECT_EQ(2, listener_->property_updates(
      shill::kServicesProperty)[kTestServicePath]);

  // Remove a service
  listener_->reset_list_updates();
  RemoveService(kTestServicePath);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kServicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            listener_->entries(shill::kServicesProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerIPConfigPropertyChanged) {
  // Set the properties for an IP Config object.
  const std::string kTestIPConfigPath("test_ip_config_path");

  base::StringValue ip_address("192.168.1.1");
  DBusThreadManager::Get()->GetShillIPConfigClient()->SetProperty(
      dbus::ObjectPath(kTestIPConfigPath),
      shill::kAddressProperty, ip_address,
      base::Bind(&DoNothingWithCallStatus));
  base::ListValue dns_servers;
  dns_servers.Append(base::Value::CreateStringValue("192.168.1.100"));
  dns_servers.Append(base::Value::CreateStringValue("192.168.1.101"));
  DBusThreadManager::Get()->GetShillIPConfigClient()->SetProperty(
      dbus::ObjectPath(kTestIPConfigPath),
      shill::kNameServersProperty, dns_servers,
      base::Bind(&DoNothingWithCallStatus));
  base::FundamentalValue prefixlen(8);
  DBusThreadManager::Get()->GetShillIPConfigClient()->SetProperty(
      dbus::ObjectPath(kTestIPConfigPath),
      shill::kPrefixlenProperty, prefixlen,
      base::Bind(&DoNothingWithCallStatus));
  base::StringValue gateway("192.0.0.1");
  DBusThreadManager::Get()->GetShillIPConfigClient()->SetProperty(
      dbus::ObjectPath(kTestIPConfigPath),
      shill::kGatewayProperty, gateway,
      base::Bind(&DoNothingWithCallStatus));
  message_loop_.RunUntilIdle();

  // Add a service with an empty ipconfig and then update
  // its ipconfig property.
  const std::string kTestServicePath1("test_wifi_service1");
  AddService(shill::kTypeWifi, kTestServicePath1, shill::kStateIdle, true);
  message_loop_.RunUntilIdle();
  // This is the initial property update.
  EXPECT_EQ(1, listener_->initial_property_updates(
      shill::kServicesProperty)[kTestServicePath1]);
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(kTestServicePath1),
      shill::kIPConfigProperty,
      base::StringValue(kTestIPConfigPath),
      base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  message_loop_.RunUntilIdle();
  // IPConfig property change on the service should trigger property updates for
  // IP Address, DNS, prefixlen, and gateway.
  EXPECT_EQ(4, listener_->property_updates(
      shill::kServicesProperty)[kTestServicePath1]);

  // Now, Add a new watched service with the IPConfig already set.
  const std::string kTestServicePath2("test_wifi_service2");
  AddServiceWithIPConfig(shill::kTypeWifi, kTestServicePath2,
                         shill::kStateIdle, kTestIPConfigPath, true);
  message_loop_.RunUntilIdle();
  // A watched service with the IPConfig property already set must trigger
  // property updates for IP Address, DNS, prefixlen, and gateway when added.
  EXPECT_EQ(4, listener_->property_updates(
      shill::kServicesProperty)[kTestServicePath2]);
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerServiceCompleteList) {
  // Add a new entry to the profile only (triggers a Services update).
  const std::string kTestServicePath1("stub_wifi_profile_only1");
  AddServiceToProfile(shill::kTypeWifi, kTestServicePath1, false);
  message_loop_.RunUntilIdle();

  // Update the Manager properties. This should trigger a single list update
  // for both Services and ServiceCompleteList, and a single property update
  // for ServiceCompleteList.
  listener_->reset_list_updates();
  shill_property_handler_->UpdateManagerProperties();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kServicesProperty));
  EXPECT_EQ(1, listener_->list_updates(shill::kServiceCompleteListProperty));
  EXPECT_EQ(0, listener_->initial_property_updates(
      shill::kServicesProperty)[kTestServicePath1]);
  EXPECT_EQ(1, listener_->initial_property_updates(
      shill::kServiceCompleteListProperty)[kTestServicePath1]);
  EXPECT_EQ(0, listener_->property_updates(
      shill::kServicesProperty)[kTestServicePath1]);
  EXPECT_EQ(0, listener_->property_updates(
      shill::kServiceCompleteListProperty)[kTestServicePath1]);

  // Add a new entry to the services and the profile; should also trigger a
  // single list update for both Services and ServiceCompleteList, and should
  // trigger tow property updates for Services (one when the Profile propety
  // changes, and one for the Request) and one ServiceCompleteList change for
  // the Request.
  listener_->reset_list_updates();
  const std::string kTestServicePath2("stub_wifi_profile_only2");
  AddServiceToProfile(shill::kTypeWifi, kTestServicePath2, true);
  shill_property_handler_->UpdateManagerProperties();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->list_updates(shill::kServicesProperty));
  EXPECT_EQ(1, listener_->list_updates(shill::kServiceCompleteListProperty));
  EXPECT_EQ(1, listener_->initial_property_updates(
      shill::kServicesProperty)[kTestServicePath2]);
  EXPECT_EQ(1, listener_->initial_property_updates(
      shill::kServiceCompleteListProperty)[kTestServicePath2]);
  // Expect one property update for the Profile property of the Network.
  EXPECT_EQ(1, listener_->property_updates(
      shill::kServicesProperty)[kTestServicePath2]);
  EXPECT_EQ(0, listener_->property_updates(
      shill::kServiceCompleteListProperty)[kTestServicePath2]);

  // Change a property of a Network in a Profile.
  base::FundamentalValue scan_interval(3);
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(kTestServicePath2),
      shill::kScanIntervalProperty,
      scan_interval,
      base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  message_loop_.RunUntilIdle();
  // Property change should trigger an update for the Network only; no
  // property updates pushed by Shill affect Favorites.
  EXPECT_EQ(2, listener_->property_updates(
      shill::kServicesProperty)[kTestServicePath2]);
  EXPECT_EQ(0, listener_->property_updates(
      shill::kServiceCompleteListProperty)[kTestServicePath2]);
}

}  // namespace chromeos
