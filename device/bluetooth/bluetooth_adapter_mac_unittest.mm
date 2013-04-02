// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <IOBluetooth/IOBluetoothUtilities.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>

#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface TestIOBluetoothDevice : IOBluetoothDevice {
 @private
  NSString* device_name_;
  NSString* device_address_;
  BOOL paired_;
}

+ (TestIOBluetoothDevice*)deviceWithName:(NSString*)device_name
                                 address:(NSString*)device_address
                                  paired:(BOOL)paired;
- (id)initWithName:(NSString*)device_name
           address:(NSString*)device_address
            paired:(BOOL)paired;

@property (nonatomic, copy, getter=name) NSString* deviceName;
@property (nonatomic, copy, getter=addressString) NSString* deviceAddress;
@property (nonatomic, assign, getter=isPaired) BOOL paired;

@end

@implementation TestIOBluetoothDevice

@synthesize deviceName = device_name_;
@synthesize deviceAddress = device_address_;
@synthesize paired = paired_;

+ (TestIOBluetoothDevice*)deviceWithName:(NSString*)device_name
                                 address:(NSString*)device_address
                                  paired:(BOOL)paired {
  return [[[TestIOBluetoothDevice alloc] initWithName:device_name
                                              address:device_address
                                               paired:paired] autorelease];
}

- (id)initWithName:(NSString*)device_name
           address:(NSString*)device_address
            paired:(BOOL)paired {
  if (self = [super init]) {
    [self setDeviceName:device_name];
    [self setDeviceAddress:device_address];
    [self setPaired:paired];
  }

  return self;
}

- (void)dealloc {
  [self setDeviceName:nil];
  [self setDeviceAddress:nil];
  [super dealloc];
}

@end

namespace {

class AdapterObserver : public device::BluetoothAdapter::Observer {
 public:
  AdapterObserver() {
    Clear();
  }

  void Clear() {
    num_device_added_ = 0;
    num_device_changed_ = 0;
    num_device_removed_ = 0;
  }

  virtual void DeviceAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothDevice* device) OVERRIDE {
    num_device_added_++;
  }

  virtual void DeviceChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothDevice* device) OVERRIDE {
    num_device_changed_++;
  }

  virtual void DeviceRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothDevice* device) OVERRIDE {
    num_device_removed_++;
  }

  int num_device_added() const {
    return num_device_added_;
  }

  int num_device_changed() const {
    return num_device_changed_;
  }

  int num_device_removed() const {
    return num_device_removed_;
  }

 private:
  int num_device_added_;
  int num_device_changed_;
  int num_device_removed_;
};

}  // namespace

namespace device {

class BluetoothAdapterMacTest : public testing::Test {
 public:
  BluetoothAdapterMacTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        adapter_(new BluetoothAdapterMac()),
        adapter_mac_(static_cast<BluetoothAdapterMac*>(adapter_.get())) {
    adapter_mac_->InitForTest(ui_task_runner_);
    adapter_mac_->RemoveUnpairedDevices([NSArray array]);
    adapter_observer_.Clear();
  }

  virtual void SetUp() OVERRIDE {
    adapter_mac_->AddObserver(&adapter_observer_);
  }

  virtual void TearDown() OVERRIDE {
    adapter_mac_->RemoveObserver(&adapter_observer_);
  }

  void AddDevices(NSArray* devices) {
    adapter_mac_->AddDevices(devices);
  }

  void RemoveUnpairedDevices(NSArray* devices) {
    adapter_mac_->RemoveUnpairedDevices(devices);
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothAdapter> adapter_;
  BluetoothAdapterMac* adapter_mac_;
  AdapterObserver adapter_observer_;
};

TEST_F(BluetoothAdapterMacTest, Poll) {
  EXPECT_FALSE(ui_task_runner_->GetPendingTasks().empty());
}

TEST_F(BluetoothAdapterMacTest, AddDevices) {
  TestIOBluetoothDevice* android_device =
      [TestIOBluetoothDevice deviceWithName:@"android"
                                    address:@"11:22:33:44:55:66"
                                     paired:false];
  TestIOBluetoothDevice* laptop_device =
      [TestIOBluetoothDevice deviceWithName:@"laptop"
                                    address:@"77:88:99:aa:bb:cc"
                                     paired:false];
  TestIOBluetoothDevice* iphone_device =
      [TestIOBluetoothDevice deviceWithName:@"iphone"
                                    address:@"dd:ee:ff:11:22:33"
                                     paired:false];
  NSMutableArray* devices = [NSMutableArray arrayWithCapacity:3];
  [devices addObject:android_device];
  [devices addObject:laptop_device];
  [devices addObject:iphone_device];
  AddDevices(devices);
  EXPECT_EQ(3, adapter_observer_.num_device_added());
  adapter_observer_.Clear();

  AddDevices(devices);
  EXPECT_EQ(0, adapter_observer_.num_device_added());
  EXPECT_EQ(0, adapter_observer_.num_device_changed());

  [iphone_device setDeviceName:@"apple phone"];
  AddDevices(devices);
  EXPECT_EQ(0, adapter_observer_.num_device_added());
  EXPECT_EQ(1, adapter_observer_.num_device_changed());
  adapter_observer_.Clear();
}

TEST_F(BluetoothAdapterMacTest, RemoveUnpairedDevices) {
  TestIOBluetoothDevice* android_device =
      [TestIOBluetoothDevice deviceWithName:@"android"
                                    address:@"11:22:33:44:55:66"
                                     paired:true];
  TestIOBluetoothDevice* laptop_device =
      [TestIOBluetoothDevice deviceWithName:@"laptop"
                                    address:@"77:88:99:aa:bb:cc"
                                     paired:false];
  NSMutableArray* devices = [NSMutableArray arrayWithCapacity:2];
  [devices addObject:android_device];
  [devices addObject:laptop_device];
  AddDevices(devices);
  adapter_observer_.Clear();

  RemoveUnpairedDevices([NSArray arrayWithObject:android_device]);
  EXPECT_EQ(0, adapter_observer_.num_device_removed());

  RemoveUnpairedDevices([NSArray array]);
  EXPECT_EQ(1, adapter_observer_.num_device_removed());

}

}  // namespace device
