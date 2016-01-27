// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_low_energy_device_mac.h"
#include "device/bluetooth/test/mock_bluetooth_central_manager_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ocmock/OCMock/OCMock.h"

#if defined(OS_IOS)
#import <CoreBluetooth/CoreBluetooth.h>
#else  // !defined(OS_IOS)
#import <IOBluetooth/IOBluetooth.h>
#endif  // defined(OS_IOS)

#import <Foundation/Foundation.h>

namespace {
// |kTestHashAddress| is the hash corresponding to identifier |kTestNSUUID|.
NSString* const kTestNSUUID = @"00000000-1111-2222-3333-444444444444";
const std::string kTestHashAddress = "D1:6F:E3:22:FD:5B";
const int kTestRssi = 0;
}  // namespace

namespace device {

class BluetoothAdapterMacTest : public testing::Test {
 public:
  BluetoothAdapterMacTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        adapter_(new BluetoothAdapterMac()),
        adapter_mac_(static_cast<BluetoothAdapterMac*>(adapter_.get())),
        callback_count_(0),
        error_callback_count_(0) {
    adapter_mac_->InitForTest(ui_task_runner_);
  }

  // Helper methods for setup and access to BluetoothAdapterMacTest's members.
  void PollAdapter() { adapter_mac_->PollAdapter(); }

  void LowEnergyDeviceUpdated(CBPeripheral* peripheral,
                              NSDictionary* advertisement_data,
                              int rssi) {
    adapter_mac_->LowEnergyDeviceUpdated(peripheral, advertisement_data, rssi);
  }

  BluetoothDevice* GetDevice(const std::string& address) {
    return adapter_->GetDevice(address);
  }

  CBPeripheral* CreateMockPeripheral(NSString* identifier) {
    if (!BluetoothAdapterMac::IsLowEnergyAvailable()) {
      LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
      return nil;
    }
    Class peripheral_class = NSClassFromString(@"CBPeripheral");
    id mock_peripheral =
        [[OCMockObject mockForClass:[peripheral_class class]] retain];
    [static_cast<CBPeripheral*>([[mock_peripheral stub]
        andReturnValue:@(CBPeripheralStateDisconnected)])
        performSelector:@selector(state)];
    [[[mock_peripheral stub] andReturn:[NSString string]] name];
    Class uuid_class = NSClassFromString(@"NSUUID");
    [[[mock_peripheral stub]
        andReturn:[[uuid_class performSelector:@selector(UUID)]
                      performSelector:@selector(initWithUUIDString:)
                           withObject:identifier]] identifier];

    return mock_peripheral;
  }

  NSDictionary* CreateAdvertisementData() {
    NSDictionary* advertisement_data = @{
      CBAdvertisementDataIsConnectable : @(YES),
      CBAdvertisementDataServiceDataKey : [NSDictionary dictionary],
    };
    [advertisement_data retain];
    return advertisement_data;
  }

  std::string GetHashAddress(CBPeripheral* peripheral) {
    return BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
  }

  void SetDeviceTimeGreaterThanTimeout(BluetoothLowEnergyDeviceMac* device) {
    device->last_update_time_.reset([[NSDate
        dateWithTimeInterval:-(BluetoothAdapterMac::kDiscoveryTimeoutSec + 1)
                   sinceDate:[NSDate date]] retain]);
  }

  void AddLowEnergyDevice(BluetoothLowEnergyDeviceMac* device) {
    adapter_mac_->devices_.set(device->GetAddress(),
                               scoped_ptr<BluetoothDevice>(device));
  }

  int NumDevices() { return adapter_mac_->devices_.size(); }

  bool DevicePresent(CBPeripheral* peripheral) {
    BluetoothDevice* device = adapter_mac_->GetDevice(
        BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral));
    return (device != NULL);
  }

  void RemoveTimedOutDevices() { adapter_mac_->RemoveTimedOutDevices(); }

  bool SetMockCentralManager(CBCentralManagerState desired_state) {
    if (!BluetoothAdapterMac::IsLowEnergyAvailable()) {
      LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
      return false;
    }
    mock_central_manager_ = [[MockCentralManager alloc] init];
    [mock_central_manager_ setState:desired_state];
    adapter_mac_->SetCentralManagerForTesting(mock_central_manager_);
    return true;
  }

  void AddDiscoverySession(BluetoothDiscoveryFilter* discovery_filter) {
    adapter_mac_->AddDiscoverySession(
        discovery_filter,
        base::Bind(&BluetoothAdapterMacTest::Callback, base::Unretained(this)),
        base::Bind(&BluetoothAdapterMacTest::DiscoveryErrorCallback,
                   base::Unretained(this)));
  }

  void RemoveDiscoverySession(BluetoothDiscoveryFilter* discovery_filter) {
    adapter_mac_->RemoveDiscoverySession(
        discovery_filter,
        base::Bind(&BluetoothAdapterMacTest::Callback, base::Unretained(this)),
        base::Bind(&BluetoothAdapterMacTest::DiscoveryErrorCallback,
                   base::Unretained(this)));
  }

  int NumDiscoverySessions() { return adapter_mac_->num_discovery_sessions_; }

  // Generic callbacks.
  void Callback() { ++callback_count_; }
  void ErrorCallback() { ++error_callback_count_; }
  void DiscoveryErrorCallback(UMABluetoothDiscoverySessionOutcome) {
    ++error_callback_count_;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothAdapter> adapter_;
  BluetoothAdapterMac* adapter_mac_;

  // Owned by |adapter_mac_|.
  id mock_central_manager_ = NULL;

  int callback_count_;
  int error_callback_count_;
};

TEST_F(BluetoothAdapterMacTest, Poll) {
  PollAdapter();
  EXPECT_FALSE(ui_task_runner_->GetPendingTasks().empty());
}

TEST_F(BluetoothAdapterMacTest, AddDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(0, NumDiscoverySessions());

  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  // Check that adding a discovery session resulted in
  // scanForPeripheralsWithServices being called on the Central Manager.
  EXPECT_EQ(1, [mock_central_manager_ scanForPeripheralsCallCount]);
}

// TODO(krstnmnlsn): Test changing the filter when adding the second discovery
// session (once we have that ability).
TEST_F(BluetoothAdapterMacTest, AddSecondDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  // We replaced the success callback handed to AddDiscoverySession, so
  // |adapter_mac_| should remain in a discovering state indefinitely.
  EXPECT_TRUE(adapter_mac_->IsDiscovering());

  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(2, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(2, NumDiscoverySessions());
}

TEST_F(BluetoothAdapterMacTest, RemoveDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);

  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
  RemoveDiscoverySession(discovery_filter.get());
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, NumDiscoverySessions());

  // Check that removing the discovery session resulted in stopScan being called
  // on the Central Manager.
  EXPECT_EQ(1, [mock_central_manager_ stopScanCallCount]);
}

TEST_F(BluetoothAdapterMacTest, RemoveDiscoverySessionWithLowEnergyFilterFail) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
  EXPECT_EQ(0, NumDiscoverySessions());

  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  RemoveDiscoverySession(discovery_filter.get());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(0, NumDiscoverySessions());

  // Check that stopScan was not called.
  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
}

TEST_F(BluetoothAdapterMacTest, CheckGetPeripheralHashAddress) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  base::scoped_nsobject<id> mock_peripheral(CreateMockPeripheral(kTestNSUUID));
  if (mock_peripheral.get() == nil)
    return;
  EXPECT_EQ(kTestHashAddress, GetHashAddress(mock_peripheral));
}

TEST_F(BluetoothAdapterMacTest, LowEnergyDeviceUpdatedNewDevice) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  base::scoped_nsobject<id> mock_peripheral(CreateMockPeripheral(kTestNSUUID));
  if (mock_peripheral.get() == nil)
    return;
  base::scoped_nsobject<NSDictionary> advertisement_data(
      CreateAdvertisementData());

  EXPECT_EQ(0, NumDevices());
  EXPECT_FALSE(DevicePresent(mock_peripheral));
  LowEnergyDeviceUpdated(mock_peripheral, advertisement_data, kTestRssi);
  EXPECT_EQ(1, NumDevices());
  EXPECT_TRUE(DevicePresent(mock_peripheral));
}

TEST_F(BluetoothAdapterMacTest, LowEnergyDeviceUpdatedOldDevice) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  base::scoped_nsobject<id> mock_peripheral(CreateMockPeripheral(kTestNSUUID));
  if (mock_peripheral.get() == nil)
    return;
  base::scoped_nsobject<NSDictionary> advertisement_data(
      CreateAdvertisementData());

  // Update the device for the first time and check it was correctly added to
  // |devices_|.
  EXPECT_EQ(0, NumDevices());
  EXPECT_FALSE(DevicePresent(mock_peripheral));
  LowEnergyDeviceUpdated(mock_peripheral, advertisement_data, kTestRssi);
  EXPECT_EQ(1, NumDevices());
  EXPECT_TRUE(DevicePresent(mock_peripheral));
  // Search for the device by the address corresponding to |kTestNSUUID|.
  BluetoothDeviceMac* device =
      static_cast<BluetoothDeviceMac*>(GetDevice(kTestHashAddress));
  base::scoped_nsobject<NSDate> first_update_time(
      [device->GetLastUpdateTime() retain]);

  // Update the device a second time. The device should be updated in
  // |devices_| so check the time returned by GetLastUpdateTime() has increased.
  LowEnergyDeviceUpdated(mock_peripheral, advertisement_data, kTestRssi);
  EXPECT_EQ(1, NumDevices());
  EXPECT_TRUE(DevicePresent(mock_peripheral));
  device = static_cast<BluetoothDeviceMac*>(GetDevice(kTestHashAddress));
  EXPECT_TRUE([device->GetLastUpdateTime() compare:first_update_time] ==
              NSOrderedDescending);
}

TEST_F(BluetoothAdapterMacTest, UpdateDevicesRemovesLowEnergyDevice) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  base::scoped_nsobject<id> mock_peripheral(CreateMockPeripheral(kTestNSUUID));
  if (mock_peripheral.get() == nil)
    return;
  base::scoped_nsobject<NSDictionary> advertisement_data(
      CreateAdvertisementData());

  BluetoothLowEnergyDeviceMac* device = new BluetoothLowEnergyDeviceMac(
      adapter_mac_, mock_peripheral, advertisement_data, kTestRssi);
  SetDeviceTimeGreaterThanTimeout(device);

  EXPECT_EQ(0, NumDevices());
  AddLowEnergyDevice(device);
  EXPECT_EQ(1, NumDevices());
  EXPECT_TRUE(DevicePresent(mock_peripheral));

  // Check that object pointed to by |device| is deleted by the adapter.
  RemoveTimedOutDevices();
  EXPECT_EQ(0, NumDevices());
  EXPECT_FALSE(DevicePresent(mock_peripheral));
}

}  // namespace device
