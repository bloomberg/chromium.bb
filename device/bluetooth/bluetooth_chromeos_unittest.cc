// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;

namespace chromeos {

class TestObserver : public BluetoothAdapter::Observer {
 public:
  TestObserver(scoped_refptr<BluetoothAdapter> adapter)
      : present_changed_count_(0),
        powered_changed_count_(0),
        discoverable_changed_count_(0),
        discovering_changed_count_(0),
        last_present_(false),
        last_powered_(false),
        last_discovering_(false),
        device_added_count_(0),
        device_changed_count_(0),
        device_removed_count_(0),
        last_device_(NULL),
        adapter_(adapter) {
  }
  virtual ~TestObserver() {}

  virtual void AdapterPresentChanged(BluetoothAdapter* adapter,
                                     bool present) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++present_changed_count_;
    last_present_ = present;
  }

  virtual void AdapterPoweredChanged(BluetoothAdapter* adapter,
                                     bool powered) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++powered_changed_count_;
    last_powered_ = powered;
  }

  virtual void AdapterDiscoverableChanged(BluetoothAdapter* adapter,
                                          bool discoverable) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++discoverable_changed_count_;
  }

  virtual void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                         bool discovering) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++discovering_changed_count_;
    last_discovering_ = discovering;
  }

  virtual void DeviceAdded(BluetoothAdapter* adapter,
                           BluetoothDevice* device) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++device_added_count_;
    last_device_ = device;
    last_device_address_ = device->GetAddress();

    QuitMessageLoop();
  }

  virtual void DeviceChanged(BluetoothAdapter* adapter,
                             BluetoothDevice* device) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++device_changed_count_;
    last_device_ = device;
    last_device_address_ = device->GetAddress();

    QuitMessageLoop();
  }

  virtual void DeviceRemoved(BluetoothAdapter* adapter,
                             BluetoothDevice* device) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++device_removed_count_;
    // Can't save device, it may be freed
    last_device_address_ = device->GetAddress();

    QuitMessageLoop();
  }

  int present_changed_count_;
  int powered_changed_count_;
  int discoverable_changed_count_;
  int discovering_changed_count_;
  bool last_present_;
  bool last_powered_;
  bool last_discovering_;
  int device_added_count_;
  int device_changed_count_;
  int device_removed_count_;
  BluetoothDevice* last_device_;
  std::string last_device_address_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
  }

  scoped_refptr<BluetoothAdapter> adapter_;
};

class TestPairingDelegate : public BluetoothDevice::PairingDelegate {
 public:
  TestPairingDelegate()
      : call_count_(0),
        request_pincode_count_(0),
        request_passkey_count_(0),
        display_pincode_count_(0),
        display_passkey_count_(0),
        keys_entered_count_(0),
        confirm_passkey_count_(0),
        dismiss_count_(0),
        last_passkey_(9999999U),
        last_entered_(999U) {}
  virtual ~TestPairingDelegate() {}

  virtual void RequestPinCode(BluetoothDevice* device) OVERRIDE {
    ++call_count_;
    ++request_pincode_count_;
    QuitMessageLoop();
  }

  virtual void RequestPasskey(BluetoothDevice* device) OVERRIDE {
    ++call_count_;
    ++request_passkey_count_;
    QuitMessageLoop();
  }

  virtual void DisplayPinCode(BluetoothDevice* device,
                              const std::string& pincode) OVERRIDE {
    ++call_count_;
    ++display_pincode_count_;
    last_pincode_ = pincode;
    QuitMessageLoop();
  }

  virtual void DisplayPasskey(BluetoothDevice* device,
                              uint32 passkey) OVERRIDE {
    ++call_count_;
    ++display_passkey_count_;
    last_passkey_ = passkey;
    QuitMessageLoop();
  }

  virtual void KeysEntered(BluetoothDevice* device, uint32 entered) OVERRIDE {
    ++call_count_;
    ++keys_entered_count_;
    last_entered_ = entered;
    QuitMessageLoop();
  }

  virtual void ConfirmPasskey(BluetoothDevice* device,
                              uint32 passkey) OVERRIDE {
    ++call_count_;
    ++confirm_passkey_count_;
    last_passkey_ = passkey;
    QuitMessageLoop();
  }

  virtual void DismissDisplayOrConfirm() OVERRIDE {
    ++call_count_;
    ++dismiss_count_;
    QuitMessageLoop();
  }

  int call_count_;
  int request_pincode_count_;
  int request_passkey_count_;
  int display_pincode_count_;
  int display_passkey_count_;
  int keys_entered_count_;
  int confirm_passkey_count_;
  int dismiss_count_;
  uint32 last_passkey_;
  uint32 last_entered_;
  std::string last_pincode_;

  private:
   // Some tests use a message loop since background processing is simulated;
   // break out of those loops.
   void QuitMessageLoop() {
     if (base::MessageLoop::current() &&
         base::MessageLoop::current()->is_running())
       base::MessageLoop::current()->Quit();
   }
};

class BluetoothChromeOSTest : public testing::Test {
 public:
  virtual void SetUp() {
    FakeDBusThreadManager* fake_dbus_thread_manager = new FakeDBusThreadManager;
    fake_bluetooth_adapter_client_ = new FakeBluetoothAdapterClient;
    fake_dbus_thread_manager->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(fake_bluetooth_adapter_client_));
    fake_bluetooth_device_client_ = new FakeBluetoothDeviceClient;
    fake_dbus_thread_manager->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(fake_bluetooth_device_client_));
    fake_dbus_thread_manager->SetBluetoothInputClient(
        scoped_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    fake_dbus_thread_manager->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));
    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);

    callback_count_ = 0;
    error_callback_count_ = 0;
    last_connect_error_ = BluetoothDevice::ERROR_UNKNOWN;
  }

  virtual void TearDown() {
    adapter_ = NULL;
    DBusThreadManager::Shutdown();
  }

  // Generic callbacks
  void Callback() {
    ++callback_count_;
  }

  void ErrorCallback() {
    ++error_callback_count_;
  }

  void ConnectErrorCallback(enum BluetoothDevice::ConnectErrorCode error) {
    ++error_callback_count_;
    last_connect_error_ = error;
  }

  // Call to fill the adapter_ member with a BluetoothAdapter instance.
  void GetAdapter() {
    adapter_ = new BluetoothAdapterChromeOS();
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(adapter_->IsInitialized());
  }

  // Run a discovery phase until the named device is detected, or if the named
  // device is not created, the discovery process ends without finding it.
  //
  // The correct behavior of discovery is tested by the "Discovery" test case
  // without using this function.
  void DiscoverDevice(const std::string& address) {
    ASSERT_TRUE(adapter_.get() != NULL);

    if (base::MessageLoop::current() == NULL) {
      base::MessageLoop message_loop;
      DiscoverDevices();
      return;
    }

    fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

    TestObserver observer(adapter_);
    adapter_->AddObserver(&observer);

    adapter_->SetPowered(
        true,
        base::Bind(&BluetoothChromeOSTest::Callback,
                   base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));
    adapter_->StartDiscovering(
        base::Bind(&BluetoothChromeOSTest::Callback,
                   base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));
    ASSERT_EQ(2, callback_count_);
    ASSERT_EQ(0, error_callback_count_);
    callback_count_ = 0;

    ASSERT_TRUE(adapter_->IsPowered());
    ASSERT_TRUE(adapter_->IsDiscovering());

    while (!observer.device_removed_count_ &&
           observer.last_device_address_ != address)
      base::MessageLoop::current()->Run();

    adapter_->StopDiscovering(
        base::Bind(&BluetoothChromeOSTest::Callback,
                   base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));
    ASSERT_EQ(1, callback_count_);
    ASSERT_EQ(0, error_callback_count_);
    callback_count_ = 0;

    ASSERT_FALSE(adapter_->IsDiscovering());

    adapter_->RemoveObserver(&observer);
  }

  // Run a discovery phase so we have devices that can be paired with.
  void DiscoverDevices() {
    // Pass an invalid address for the device so that the discovery process
    // completes with all devices.
    DiscoverDevice("does not exist");
  }

 protected:
  FakeBluetoothAdapterClient* fake_bluetooth_adapter_client_;
  FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  scoped_refptr<BluetoothAdapter> adapter_;

  int callback_count_;
  int error_callback_count_;
  enum BluetoothDevice::ConnectErrorCode last_connect_error_;
};

TEST_F(BluetoothChromeOSTest, AlreadyPresent) {
  GetAdapter();

  // This verifies that the class gets the list of adapters when created;
  // and initializes with an existing adapter if there is one.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());
  EXPECT_FALSE(adapter_->IsDiscovering());

  // There should be a device
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  EXPECT_EQ(1U, devices.size());
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());
}

TEST_F(BluetoothChromeOSTest, BecomePresent) {
  fake_bluetooth_adapter_client_->SetVisible(false);
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with true, and IsPresent() to return true.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  fake_bluetooth_adapter_client_->SetVisible(true);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_TRUE(observer.last_present_);

  EXPECT_TRUE(adapter_->IsPresent());

  // We should have had a device announced.
  EXPECT_EQ(1, observer.device_added_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            observer.last_device_address_);

  // Other callbacks shouldn't be called if the values are false.
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, BecomeNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_FALSE(observer.last_present_);

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had a device removed.
  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            observer.last_device_address_);

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, SecondAdapter) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer, then add a second adapter. Nothing should change,
  // we ignore the second adapter.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  fake_bluetooth_adapter_client_->SetSecondVisible(true);

  EXPECT_EQ(0, observer.present_changed_count_);

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());

  // Try removing the first adapter, we should now act as if the adapter
  // is no longer present rather than fall back to the second.
  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_FALSE(observer.last_present_);

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had a device removed.
  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            observer.last_device_address_);

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());

  observer.device_removed_count_ = 0;

  // Removing the second adapter shouldn't set anything either.
  fake_bluetooth_adapter_client_->SetSecondVisible(false);

  EXPECT_EQ(0, observer.device_removed_count_);
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.discovering_changed_count_);
}

TEST_F(BluetoothChromeOSTest, BecomePowered) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with true, and IsPowered() to return true.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_TRUE(observer.last_powered_);

  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothChromeOSTest, BecomeNotPowered) {
  GetAdapter();
  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with false, and IsPowered() to return false.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  adapter_->SetPowered(
      false,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_FALSE(observer.last_powered_);

  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothChromeOSTest, ChangeAdapterName) {
  GetAdapter();

  static const std::string new_name(".__.");

  adapter_->SetName(
      new_name,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(new_name, adapter_->GetName());
}

TEST_F(BluetoothChromeOSTest, BecomeDiscoverable) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with true, and IsDiscoverable() to return true.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  adapter_->SetDiscoverable(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count_);

  EXPECT_TRUE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothChromeOSTest, BecomeNotDiscoverable) {
  GetAdapter();
  adapter_->SetDiscoverable(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with false, and IsDiscoverable() to return false.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  adapter_->SetDiscoverable(
      false,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count_);

  EXPECT_FALSE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothChromeOSTest, StopDiscovery) {
  base::MessageLoop message_loop;

  GetAdapter();

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  adapter_->StartDiscovering(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  // Install an observer; aside from the callback, expect the
  // AdapterDiscoveringChanged method to be called and no longer to be
  // discovering,
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  adapter_->StopDiscovering(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);

  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, StopDiscoveryAfterTwoStarts) {
  base::MessageLoop message_loop;

  GetAdapter();

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  adapter_->StartDiscovering(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  // Install an observer and start discovering again; only the callback
  // should be called since we were already discovering to begin with.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  adapter_->StartDiscovering(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  EXPECT_EQ(0, observer.discovering_changed_count_);

  // Stop discovering; only the callback should be called since we're still
  // discovering. The adapter should be still discovering.
  adapter_->StopDiscovering(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  EXPECT_EQ(0, observer.discovering_changed_count_);

  EXPECT_TRUE(adapter_->IsDiscovering());

  // Stop discovering one more time; aside from the callback, expect the
  // AdapterDiscoveringChanged method to be called and no longer to be
  // discovering,
  adapter_->StopDiscovering(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);

  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, Discovery) {
  // Test a simulated discovery session.
  base::MessageLoop message_loop;

  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  adapter_->StartDiscovering(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  // First device to appear should be an Apple Mouse.
  message_loop.Run();

  EXPECT_EQ(1, observer.device_added_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kAppleMouseAddress,
            observer.last_device_address_);

  // Next we should get another two devices...
  message_loop.Run();
  EXPECT_EQ(3, observer.device_added_count_);

  // Okay, let's run forward until a device is actually removed...
  while (!observer.device_removed_count_)
    message_loop.Run();

  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kVanishingDeviceAddress,
            observer.last_device_address_);
}

TEST_F(BluetoothChromeOSTest, PoweredAndDiscovering) {
  base::MessageLoop message_loop;

  GetAdapter();
  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  adapter_->StartDiscovering(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  fake_bluetooth_adapter_client_->SetVisible(false);
  ASSERT_FALSE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged,
  // AdapterPoweredChanged and AdapterDiscoveringChanged methods to be called
  // with true, and IsPresent(), IsPowered() and IsDiscovering() to all
  // return true.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  fake_bluetooth_adapter_client_->SetVisible(true);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_TRUE(observer.last_present_);
  EXPECT_TRUE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_TRUE(observer.last_powered_);
  EXPECT_TRUE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());

  observer.present_changed_count_ = 0;
  observer.powered_changed_count_ = 0;
  observer.discovering_changed_count_ = 0;

  // Now mark the adapter not present again. Expect the methods to be called
  // again, to reset the properties back to false
  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_FALSE(observer.last_present_);
  EXPECT_FALSE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_FALSE(observer.last_powered_);
  EXPECT_FALSE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, DeviceProperties) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  // Verify the other device properties.
  EXPECT_EQ(base::UTF8ToUTF16(FakeBluetoothDeviceClient::kPairedDeviceName),
            devices[0]->GetName());
  EXPECT_EQ(BluetoothDevice::DEVICE_COMPUTER, devices[0]->GetDeviceType());
  EXPECT_TRUE(devices[0]->IsPaired());
  EXPECT_FALSE(devices[0]->IsConnected());
  EXPECT_FALSE(devices[0]->IsConnecting());

  // Non HID devices are always connectable.
  EXPECT_TRUE(devices[0]->IsConnectable());

  BluetoothDevice::ServiceList uuids = devices[0]->GetServices();
  ASSERT_EQ(2U, uuids.size());
  EXPECT_EQ(uuids[0], "00001800-0000-1000-8000-00805f9b34fb");
  EXPECT_EQ(uuids[1], "00001801-0000-1000-8000-00805f9b34fb");

  EXPECT_EQ(0x05ac, devices[0]->GetVendorID());
  EXPECT_EQ(0x030d, devices[0]->GetProductID());
  EXPECT_EQ(0x0306, devices[0]->GetDeviceID());
}

TEST_F(BluetoothChromeOSTest, DeviceClassChanged) {
  // Simulate a change of class of a device, as sometimes occurs
  // during discovery.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());
  ASSERT_EQ(BluetoothDevice::DEVICE_COMPUTER, devices[0]->GetDeviceType());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->bluetooth_class.ReplaceValue(0x002580);

  EXPECT_EQ(1, observer.device_changed_count_);
  EXPECT_EQ(devices[0], observer.last_device_);

  EXPECT_EQ(BluetoothDevice::DEVICE_MOUSE, devices[0]->GetDeviceType());
}

TEST_F(BluetoothChromeOSTest, DeviceNameChanged) {
  // Simulate a change of name of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());
  ASSERT_EQ(base::UTF8ToUTF16(FakeBluetoothDeviceClient::kPairedDeviceName),
            devices[0]->GetName());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the alias of the device.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  static const std::string new_name("New Device Name");
  properties->alias.ReplaceValue(new_name);

  EXPECT_EQ(1, observer.device_changed_count_);
  EXPECT_EQ(devices[0], observer.last_device_);

  EXPECT_EQ(base::UTF8ToUTF16(new_name), devices[0]->GetName());
}

TEST_F(BluetoothChromeOSTest, DeviceUuidsChanged) {
  // Simulate a change of advertised services of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  BluetoothDevice::ServiceList uuids = devices[0]->GetServices();
  ASSERT_EQ(2U, uuids.size());
  ASSERT_EQ(uuids[0], "00001800-0000-1000-8000-00805f9b34fb");
  ASSERT_EQ(uuids[1], "00001801-0000-1000-8000-00805f9b34fb");

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  uuids.push_back("0000110c-0000-1000-8000-00805f9b34fb");
  uuids.push_back("0000110e-0000-1000-8000-00805f9b34fb");
  uuids.push_back("0000110a-0000-1000-8000-00805f9b34fb");

  properties->uuids.ReplaceValue(uuids);

  EXPECT_EQ(1, observer.device_changed_count_);
  EXPECT_EQ(devices[0], observer.last_device_);

  // Fetching the value should give the new one.
  uuids = devices[0]->GetServices();
  ASSERT_EQ(5U, uuids.size());
  EXPECT_EQ(uuids[0], "00001800-0000-1000-8000-00805f9b34fb");
  EXPECT_EQ(uuids[1], "00001801-0000-1000-8000-00805f9b34fb");
  EXPECT_EQ(uuids[2], "0000110c-0000-1000-8000-00805f9b34fb");
  EXPECT_EQ(uuids[3], "0000110e-0000-1000-8000-00805f9b34fb");
  EXPECT_EQ(uuids[4], "0000110a-0000-1000-8000-00805f9b34fb");
}

TEST_F(BluetoothChromeOSTest, ForgetDevice) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  std::string address = devices[0]->GetAddress();

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  devices[0]->Forget(
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(address, observer.last_device_address_);

  // GetDevices shouldn't return the device either.
  devices = adapter_->GetDevices();
  ASSERT_EQ(0U, devices.size());
}

TEST_F(BluetoothChromeOSTest, ForgetUnpairedDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kMicrosoftMouseAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  // Connect the device so it becomes trusted and remembered.
  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kMicrosoftMousePath));
  ASSERT_TRUE(properties->trusted.value());

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  device->Forget(
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kMicrosoftMouseAddress,
            observer.last_device_address_);

  // GetDevices shouldn't return the device either.
  device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kMicrosoftMouseAddress);
  EXPECT_FALSE(device != NULL);
}

TEST_F(BluetoothChromeOSTest, ConnectPairedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_TRUE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  // Connect without a pairing delegate; since the device is already Paired
  // this should succeed and the device should become connected.
  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one for connected and one for for trusted
  // after connecting.
  EXPECT_EQ(4, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_F(BluetoothChromeOSTest, ConnectUnpairableDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kMicrosoftMouseAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  // Connect without a pairing delegate; since the device does not require
  // pairing, this should succeed and the device should become connected.
  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one for connected, one for for trusted after
  // connection, and one for the reconnect mode (IsConnectable).
  EXPECT_EQ(5, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kMicrosoftMousePath));
  EXPECT_TRUE(properties->trusted.value());

  // Verify is a HID device and is not connectable.
  BluetoothDevice::ServiceList uuids = device->GetServices();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], "00001124-0000-1000-8000-00805f9b34fb");
  EXPECT_FALSE(device->IsConnectable());
}

TEST_F(BluetoothChromeOSTest, ConnectConnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_TRUE(device->IsPaired());

  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());

  // Connect again; since the device is already Connected, this shouldn't do
  // anything to initiate the connection.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // The observer will be called because Connecting will toggle true and false,
  // and the trusted property will be updated to true.
  EXPECT_EQ(3, observer.device_changed_count_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_F(BluetoothChromeOSTest, ConnectDeviceFails) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kAppleMouseAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  // Connect without a pairing delegate; since the device requires pairing,
  // this should fail with an error.
  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  EXPECT_EQ(2, observer.device_changed_count_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_F(BluetoothChromeOSTest, DisconnectDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_TRUE(device->IsPaired());

  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  device->Disconnect(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothChromeOSTest, DisconnectUnconnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_TRUE(device->IsPaired());
  ASSERT_FALSE(device->IsConnected());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  device->Disconnect(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.device_changed_count_);

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothChromeOSTest, PairAppleMouse) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // The Apple Mouse requires no PIN or Passkey to pair; this is equivalent
  // to Simple Secure Pairing or a device with a fixed 0000 PIN.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kAppleMouseAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  message_loop.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::ServiceList uuids = device->GetServices();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], "00001124-0000-1000-8000-00805f9b34fb");
  EXPECT_TRUE(device->IsConnectable());

  // Pairing dialog should be dismissed
  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kAppleMousePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairAppleKeyboard) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // The Apple Keyboard requires that we display a randomly generated
  // PIN on the screen.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kAppleKeyboardAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.display_pincode_count_);
  EXPECT_EQ("123456", pairing_delegate.last_pincode_);
  EXPECT_TRUE(device->IsConnecting());

  message_loop.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::ServiceList uuids = device->GetServices();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], "00001124-0000-1000-8000-00805f9b34fb");
  EXPECT_TRUE(device->IsConnectable());

  // Pairing dialog should be dismissed
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kAppleKeyboardPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairMotorolaKeyboard) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // The Motorola Keyboard requires that we display a randomly generated
  // Passkey on the screen, and notifies us as it's typed in.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kMotorolaKeyboardAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  // One call for DisplayPasskey() and one for KeysEntered().
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.display_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);
  EXPECT_EQ(1, pairing_delegate.keys_entered_count_);
  EXPECT_EQ(0U, pairing_delegate.last_entered_);

  EXPECT_TRUE(device->IsConnecting());

  // One call to KeysEntered() for each key, including [enter].
  for(int i = 1; i <= 7; ++i) {
    message_loop.Run();

    EXPECT_EQ(2 + i, pairing_delegate.call_count_);
    EXPECT_EQ(1 + i, pairing_delegate.keys_entered_count_);
    EXPECT_EQ(static_cast<uint32_t>(i), pairing_delegate.last_entered_);
  }

  message_loop.Run();

  // 8 KeysEntered notifications (0 to 7, inclusive). Two aditional calls for
  // DisplayPasskey() and DismissDisplayOrConfirm().
  EXPECT_EQ(10, pairing_delegate.call_count_);
  EXPECT_EQ(8, pairing_delegate.keys_entered_count_);
  EXPECT_EQ(7U, pairing_delegate.last_entered_);

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device.
  BluetoothDevice::ServiceList uuids = device->GetServices();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], "00001124-0000-1000-8000-00805f9b34fb");

  // Fake MotorolaKeyboard is not connectable.
  EXPECT_FALSE(device->IsConnectable());

  // Pairing dialog should be dismissed
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kMotorolaKeyboardPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairSonyHeadphones) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // The Sony Headphones fake requires that the user enters a PIN for them.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kSonyHeadphonesAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Set the PIN.
  device->SetPinCode("1234");
  message_loop.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is not a HID device.
  BluetoothDevice::ServiceList uuids = device->GetServices();
  ASSERT_EQ(0U, uuids.size());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Pairing dialog should be dismissed
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kSonyHeadphonesPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairPhone) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // The fake phone requests that we confirm a displayed passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPhoneAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);
  EXPECT_TRUE(device->IsConnecting());

  // Confirm the passkey.
  device->ConfirmPairing();
  message_loop.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Pairing dialog should be dismissed
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPhonePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairWeirdDevice) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Use the "weird device" fake that requires that the user enters a Passkey,
  // this would be some kind of device that has a display, but doesn't use
  // "just works" - maybe a car?
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kWeirdDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Set the Passkey.
  device->SetPasskey(1234);
  message_loop.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Pairing dialog should be dismissed
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kWeirdDevicePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairUnpairableDeviceFails) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kUnpairableDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Run the loop to get the error..
  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);
}

TEST_F(BluetoothChromeOSTest, PairingFails) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(FakeBluetoothDeviceClient::kVanishingDeviceAddress);

  // The vanishing device times out during pairing
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kVanishingDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Run the loop to get the error..
  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_TIMEOUT, last_connect_error_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);
}

TEST_F(BluetoothChromeOSTest, PairingFailsAtConnection) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Everything seems to go according to plan with the unconnectable device;
  // it pairs, but then you can't make connections to it after.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  // Two changes for connecting, one for paired and one for trusted after
  // pairing. The device should not be connected.
  EXPECT_EQ(4, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);

  // Make sure the trusted property has been set to true still (since pairing
  // worked).
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(
              FakeBluetoothDeviceClient::kUnconnectableDevicePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairingRejectedAtPinCode) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kSonyHeadphonesAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);
}

TEST_F(BluetoothChromeOSTest, PairingCancelledAtPinCode) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kSonyHeadphonesAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);
}

TEST_F(BluetoothChromeOSTest, PairingRejectedAtPasskey) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kWeirdDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);
}

TEST_F(BluetoothChromeOSTest, PairingCancelledAtPasskey) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kWeirdDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);
}

TEST_F(BluetoothChromeOSTest, PairingRejectedAtConfirmation) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for passkey confirmation.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPhoneAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);
}

TEST_F(BluetoothChromeOSTest, PairingCancelledAtConfirmation) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPhoneAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);
}

TEST_F(BluetoothChromeOSTest, PairingCancelledInFlight) {
  base::MessageLoop message_loop;
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing while we're waiting for the remote host.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kAppleMouseAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());

  // Pairing dialog should be dismissed
  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.dismiss_count_);
}

}  // namespace chromeos
