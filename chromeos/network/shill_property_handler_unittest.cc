// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_property_handler.h"

#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

class TestListener : public internal::ShillPropertyHandler::Listener {
 public:
  TestListener() : manager_updates_(0), errors_(0) {
  }

  virtual void UpdateManagedList(ManagedState::ManagedType type,
                                 const base::ListValue& entries) OVERRIDE {
    UpdateEntries(GetTypeString(type), entries);
  }

  virtual void UpdateAvailableTechnologies(
      const base::ListValue& technologies) OVERRIDE {
    UpdateEntries(flimflam::kAvailableTechnologiesProperty, technologies);
  }

  virtual void UpdateEnabledTechnologies(
      const base::ListValue& technologies) OVERRIDE {
    UpdateEntries(flimflam::kEnabledTechnologiesProperty, technologies);
  }

  virtual void UpdateManagedStateProperties(
      ManagedState::ManagedType type,
      const std::string& path,
      const base::DictionaryValue& properties) OVERRIDE {
    AddPropertyUpdate(GetTypeString(type), path);
  }

  virtual void UpdateNetworkServiceProperty(
      const std::string& service_path,
      const std::string& key,
      const base::Value& value) OVERRIDE {
    AddPropertyUpdate(flimflam::kServicesProperty, service_path);
  }

  virtual void ManagerPropertyChanged() OVERRIDE {
    ++manager_updates_;
  }

  virtual void UpdateNetworkServiceIPAddress(
      const std::string& service_path,
      const std::string& ip_address) OVERRIDE {
    AddPropertyUpdate(flimflam::kServicesProperty, service_path);
  }

  virtual void ManagedStateListChanged(
      ManagedState::ManagedType type) OVERRIDE {
    AddStateListUpdate(GetTypeString(type));
  }

  std::vector<std::string>& entries(const std::string& type) {
    return entries_[type];
  }
  std::map<std::string, int>& property_updates(const std::string& type) {
    return property_updates_[type];
  }
  int list_updates(const std::string& type) { return list_updates_[type]; }
  int manager_updates() { return manager_updates_; }
  int errors() { return errors_; }

 private:
  std::string GetTypeString(ManagedState::ManagedType type) {
    if (type == ManagedState::MANAGED_TYPE_NETWORK) {
      return flimflam::kServicesProperty;
    } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
      return flimflam::kDevicesProperty;
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

  void AddStateListUpdate(const std::string& type) {
    if (type.empty())
      return;
    list_updates_[type] += 1;
  }

  // Map of list-type -> paths
  std::map<std::string, std::vector<std::string> > entries_;
  // Map of list-type -> map of paths -> update counts
  std::map<std::string, std::map<std::string, int> > property_updates_;
  // Map of list-type -> list update counts
  std::map<std::string, int > list_updates_;
  int manager_updates_;
  int errors_;
};

}  // namespace

class ShillPropertyHandlerTest : public testing::Test {
 public:
  ShillPropertyHandlerTest()
      : manager_test_(NULL),
        device_test_(NULL),
        service_test_(NULL) {
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
  }

  virtual void TearDown() OVERRIDE {
    shill_property_handler_.reset();
    listener_.reset();
    DBusThreadManager::Shutdown();
  }

  void AddDevice(const std::string& type, const std::string& id) {
    ASSERT_TRUE(IsValidType(type));
    manager_test_->AddDevice(id);
    device_test_->AddDevice(id, type, std::string("/device/" + id));
  }

  void RemoveDevice(const std::string& id) {
    manager_test_->RemoveDevice(id);
    device_test_->RemoveDevice(id);
  }

  void AddService(const std::string& type,
                  const std::string& id,
                  const std::string& state,
                  bool add_to_watch_list) {
    ASSERT_TRUE(IsValidType(type));
    manager_test_->AddService(id, add_to_watch_list);
    service_test_->AddService(id, id, type, state);
  }

  void RemoveService(const std::string& id) {
    manager_test_->RemoveService(id);
    service_test_->RemoveService(id);
  }

  // Call this after any initial Shill client setup
  void SetupShillPropertyHandler() {
    listener_.reset(new TestListener);
    shill_property_handler_.reset(
        new internal::ShillPropertyHandler(listener_.get()));
    shill_property_handler_->Init();
  }

  bool IsValidType(const std::string& type) {
    return (type == flimflam::kTypeEthernet ||
            type == flimflam::kTypeWifi ||
            type == flimflam::kTypeWimax ||
            type == flimflam::kTypeBluetooth ||
            type == flimflam::kTypeCellular ||
            type == flimflam::kTypeVPN);
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<TestListener> listener_;
  scoped_ptr<internal::ShillPropertyHandler> shill_property_handler_;
  ShillManagerClient::TestInterface* manager_test_;
  ShillDeviceClient::TestInterface* device_test_;
  ShillServiceClient::TestInterface* service_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillPropertyHandlerTest);
};

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerStub) {
  SetupShillPropertyHandler();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->manager_updates());
  // ShillManagerClient default stub entries are in shill_manager_client.cc.
  // TODO(stevenjb): Eliminate default stub entries and add them explicitly.
  const size_t kNumShillManagerClientStubImplTechnologies = 3;
  EXPECT_EQ(kNumShillManagerClientStubImplTechnologies,
            listener_->entries(
                flimflam::kAvailableTechnologiesProperty).size());
  EXPECT_EQ(kNumShillManagerClientStubImplTechnologies,
            listener_->entries(
                flimflam::kEnabledTechnologiesProperty).size());
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            listener_->entries(flimflam::kDevicesProperty).size());
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            listener_->entries(flimflam::kServicesProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerTechnologyChanged) {
  // This relies on the stub dbus implementations for ShillManagerClient,
  SetupShillPropertyHandler();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->manager_updates());
  // Add a disabled technology.
  manager_test_->AddTechnology(flimflam::kTypeWimax, false);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(2, listener_->manager_updates());
  const size_t kNumShillManagerClientStubImplTechnologies = 3;
  EXPECT_EQ(kNumShillManagerClientStubImplTechnologies + 1,
            listener_->entries(
                flimflam::kAvailableTechnologiesProperty).size());
  EXPECT_EQ(kNumShillManagerClientStubImplTechnologies,
            listener_->entries(
                flimflam::kEnabledTechnologiesProperty).size());
  // Enable the technology.
  DBusThreadManager::Get()->GetShillManagerClient()->EnableTechnology(
      flimflam::kTypeWimax,
      base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(3, listener_->manager_updates());
  EXPECT_EQ(kNumShillManagerClientStubImplTechnologies + 1,
            listener_->entries(
                flimflam::kEnabledTechnologiesProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerDevicePropertyChanged) {
  // This relies on the stub dbus implementations for ShillManagerClient,
  SetupShillPropertyHandler();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->manager_updates());
  EXPECT_EQ(1, listener_->list_updates(flimflam::kDevicesProperty));
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            listener_->entries(flimflam::kDevicesProperty).size());
  // Add a device.
  const std::string kTestDevicePath("test_wifi_device1");
  AddDevice(flimflam::kTypeWifi, kTestDevicePath);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->manager_updates());  // No new manager updates.
  EXPECT_EQ(2, listener_->list_updates(flimflam::kDevicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplDevices + 1,
            listener_->entries(flimflam::kDevicesProperty).size());
  // Device changes are not observed.
  // Remove a device
  RemoveDevice(kTestDevicePath);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(3, listener_->list_updates(flimflam::kDevicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            listener_->entries(flimflam::kDevicesProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

TEST_F(ShillPropertyHandlerTest, ShillPropertyHandlerServicePropertyChanged) {
  // This relies on the stub dbus implementations for ShillManagerClient,
  SetupShillPropertyHandler();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->manager_updates());
  EXPECT_EQ(1, listener_->list_updates(flimflam::kServicesProperty));
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            listener_->entries(flimflam::kServicesProperty).size());

  // Add an unwatched service.
  const std::string kTestServicePath("test_wifi_service1");
  AddService(flimflam::kTypeWifi, kTestServicePath,
             flimflam::kStateIdle, false);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, listener_->manager_updates());  // No new manager updates.
  EXPECT_EQ(2, listener_->list_updates(flimflam::kServicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1,
            listener_->entries(flimflam::kServicesProperty).size());
  // Change a property.
  base::FundamentalValue scan_interval(3);
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(kTestServicePath),
      flimflam::kScanIntervalProperty,
      scan_interval,
      base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  message_loop_.RunUntilIdle();
  // Property change should NOT trigger an update.
  EXPECT_EQ(0, listener_->
            property_updates(flimflam::kServicesProperty)[kTestServicePath]);

  // Add the existing service to the watch list.
  AddService(flimflam::kTypeWifi, kTestServicePath,
             flimflam::kStateIdle, true);
  message_loop_.RunUntilIdle();
  // Service list update should be received when watch list changes.
  EXPECT_EQ(3, listener_->list_updates(flimflam::kServicesProperty));
  // Number of services shouldn't change.
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1,
            listener_->entries(flimflam::kServicesProperty).size());
  // Property update should be received when watched service is added.
  EXPECT_EQ(1, listener_->
            property_updates(flimflam::kServicesProperty)[kTestServicePath]);

  // Change a property.
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(kTestServicePath),
      flimflam::kScanIntervalProperty,
      scan_interval,
      base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  message_loop_.RunUntilIdle();
  // Property change should trigger another update.
  EXPECT_EQ(2, listener_->
            property_updates(flimflam::kServicesProperty)[kTestServicePath]);

  // Remove a service
  RemoveService(kTestServicePath);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(4, listener_->list_updates(flimflam::kServicesProperty));
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            listener_->entries(flimflam::kServicesProperty).size());

  EXPECT_EQ(0, listener_->errors());
}

// TODO(stevenjb): Test IP Configs.

}  // namespace chromeos
