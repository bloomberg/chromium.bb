// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class TestObserver : public NetworkDeviceHandler::Observer {
 public:
  TestObserver() : device_updates_(0) {}

  virtual void NetworkDevicesUpdated(const DeviceMap& devices) {
    ++device_updates_;
    devices_ = devices;
  }

  int device_updates() { return device_updates_; }
  const DeviceMap& devices() { return devices_; }

 private:
  int device_updates_;
  DeviceMap devices_;
};

}  // namespace

class NetworkDeviceHandlerTest : public testing::Test {
 public:
  NetworkDeviceHandlerTest()
      : manager_test_(NULL),
        device_test_(NULL) {
  }
  virtual ~NetworkDeviceHandlerTest() {
  }

  virtual void SetUp() OVERRIDE {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::InitializeWithStub();
    // Get the test interface for manager / device.
    manager_test_ =
        DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
    ASSERT_TRUE(manager_test_);
    device_test_ =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    ASSERT_TRUE(device_test_);
  }

  virtual void TearDown() OVERRIDE {
    device_handler_->RemoveObserver(observer_.get());
    observer_.reset();
    device_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  void AddDevice(const std::string& type, const std::string& id) {
    manager_test_->AddDevice(id);
    device_test_->AddDevice(id, type, std::string("/device/" + id), "/stub");
  }

  void RemoveDevice(const std::string& id) {
    manager_test_->RemoveDevice(id);
    device_test_->RemoveDevice(id);
  }

  // Call this after any initial Shill client setup
  void SetupNetworkDeviceHandler() {
    device_handler_.reset(new NetworkDeviceHandler());
    device_handler_->Init();
    observer_.reset(new TestObserver());
    device_handler_->AddObserver(observer_.get());
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<TestObserver> observer_;
  scoped_ptr<NetworkDeviceHandler> device_handler_;
  ShillManagerClient::TestInterface* manager_test_;
  ShillDeviceClient::TestInterface* device_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkDeviceHandlerTest);
};

TEST_F(NetworkDeviceHandlerTest, NetworkDeviceHandlerStub) {
  SetupNetworkDeviceHandler();
  EXPECT_FALSE(device_handler_->devices_ready());

  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, observer_->device_updates());
  EXPECT_TRUE(device_handler_->devices_ready());
  // ShillManagerClient default stub entries are in shill_manager_client.cc.
  // TODO(stevenjb): Eliminate default stub entries and add them explicitly.
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            device_handler_->devices().size());
}

TEST_F(NetworkDeviceHandlerTest, NetworkDeviceHandlerPropertyChanged) {
  // This relies on the stub dbus implementations for ShillManagerClient,
  SetupNetworkDeviceHandler();

  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, observer_->device_updates());
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices, observer_->devices().size());

  // Add a device.
  const std::string kTestDevicePath("test_wifi_device1");
  AddDevice(flimflam::kTypeWifi, kTestDevicePath);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(2, observer_->device_updates());
  EXPECT_EQ(kNumShillManagerClientStubImplDevices + 1,
            observer_->devices().size());

  // Remove a device
  RemoveDevice(kTestDevicePath);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(3, observer_->device_updates());
  EXPECT_EQ(kNumShillManagerClientStubImplDevices, observer_->devices().size());
}

}  // namespace chromeos
