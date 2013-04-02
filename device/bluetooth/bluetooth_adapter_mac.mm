// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"

#import <IOBluetooth/objc/IOBluetoothDeviceInquiry.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_device_mac.h"

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface IOBluetoothHostController (LionSDKDeclarations)
- (NSString*)nameAsString;
- (BluetoothHCIPowerState)powerState;
@end

@interface IOBluetoothDevice (LionSDKDeclarations)
- (NSString*)addressString;
@end

@protocol IOBluetoothDeviceInquiryDelegate
- (void)deviceInquiryStarted:(IOBluetoothDeviceInquiry*)sender;
- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry*)sender
                          device:(IOBluetoothDevice*)device;
- (void)deviceInquiryComplete:(IOBluetoothDeviceInquiry*)sender
                        error:(IOReturn)error
                      aborted:(BOOL)aborted;
@end

#endif  // MAC_OS_X_VERSION_10_7

@interface BluetoothAdapterMacDelegate
    : NSObject <IOBluetoothDeviceInquiryDelegate> {
 @private
  device::BluetoothAdapterMac* adapter_;  // weak
}

- (id)initWithAdapter:(device::BluetoothAdapterMac*)adapter;

@end

@implementation BluetoothAdapterMacDelegate

- (id)initWithAdapter:(device::BluetoothAdapterMac*)adapter {
  if ((self = [super init]))
    adapter_ = adapter;

  return self;
}

- (void)deviceInquiryStarted:(IOBluetoothDeviceInquiry*)sender {
  adapter_->DeviceInquiryStarted(sender);
}

- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry*)sender
                          device:(IOBluetoothDevice*)device {
  adapter_->DeviceFound(sender, device);
}

- (void)deviceInquiryComplete:(IOBluetoothDeviceInquiry*)sender
                        error:(IOReturn)error
                      aborted:(BOOL)aborted {
  adapter_->DeviceInquiryComplete(sender, error, aborted);
}

@end

namespace {

const int kPollIntervalMs = 500;

}  // namespace

namespace device {

BluetoothAdapterMac::BluetoothAdapterMac()
    : BluetoothAdapter(),
      powered_(false),
      discovery_status_(NOT_DISCOVERING),
      adapter_delegate_(
          [[BluetoothAdapterMacDelegate alloc] initWithAdapter:this]),
      device_inquiry_(
          [[IOBluetoothDeviceInquiry
              inquiryWithDelegate:adapter_delegate_] retain]),
      weak_ptr_factory_(this) {
}

BluetoothAdapterMac::~BluetoothAdapterMac() {
  [device_inquiry_ release];
  [adapter_delegate_ release];
}

void BluetoothAdapterMac::AddObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAdapterMac::RemoveObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::string BluetoothAdapterMac::address() const {
  return address_;
}

std::string BluetoothAdapterMac::name() const {
  return name_;
}

bool BluetoothAdapterMac::IsInitialized() const {
  return true;
}

bool BluetoothAdapterMac::IsPresent() const {
  return !address_.empty();
}

bool BluetoothAdapterMac::IsPowered() const {
  return powered_;
}

void BluetoothAdapterMac::SetPowered(bool powered,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
}

bool BluetoothAdapterMac::IsDiscovering() const {
  return discovery_status_ == DISCOVERING ||
      discovery_status_ == DISCOVERY_STOPPING;
}

void BluetoothAdapterMac::StartDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (discovery_status_ == DISCOVERING) {
    num_discovery_listeners_++;
    callback.Run();
    return;
  }
  on_start_discovery_callbacks_.push_back(
      std::make_pair(callback, error_callback));
  MaybeStartDeviceInquiry();
}

void BluetoothAdapterMac::StopDiscovering(const base::Closure& callback,
                                          const ErrorCallback& error_callback) {
  if (discovery_status_ == NOT_DISCOVERING) {
    error_callback.Run();
    return;
  }
  on_stop_discovery_callbacks_.push_back(
      std::make_pair(callback, error_callback));
  MaybeStopDeviceInquiry();
}

void BluetoothAdapterMac::ReadLocalOutOfBandPairingData(
    const BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
}

void BluetoothAdapterMac::Init() {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  PollAdapter();
}

void BluetoothAdapterMac::InitForTest(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
  PollAdapter();
}

void BluetoothAdapterMac::PollAdapter() {
  bool was_present = IsPresent();
  std::string name = "";
  std::string address = "";
  bool powered = false;
  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];

  if (controller != nil) {
    name = base::SysNSStringToUTF8([controller nameAsString]);
    address = base::SysNSStringToUTF8([controller addressAsString]);
    powered = ([controller powerState] == kBluetoothHCIPowerStateON);
  }

  bool is_present = !address.empty();
  name_ = name;
  address_ = address;

  if (was_present != is_present) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPresentChanged(this, is_present));
  }
  if (powered_ != powered) {
    powered_ = powered;
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPoweredChanged(this, powered_));
  }

  NSArray* paired_devices = [IOBluetoothDevice pairedDevices];
  AddDevices(paired_devices);
  RemoveUnpairedDevices(paired_devices);

  ui_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BluetoothAdapterMac::PollAdapter,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPollIntervalMs));
}

void BluetoothAdapterMac::AddDevices(NSArray* devices) {
  for (IOBluetoothDevice* device in devices) {
    std::string device_address =
        base::SysNSStringToUTF8([device addressString]);
    DevicesMap::iterator found_device_iter = devices_.find(device_address);

    if (found_device_iter == devices_.end()) {
      devices_[device_address] = new BluetoothDeviceMac(device);
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceAdded(this, devices_[device_address]));
      continue;
    }
    BluetoothDeviceMac* device_mac =
        static_cast<BluetoothDeviceMac*>(found_device_iter->second);
    if (device_mac->device_fingerprint() !=
        BluetoothDeviceMac::ComputeDeviceFingerprint(device)) {
      devices_[device_address] = new BluetoothDeviceMac(device);
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceChanged(this, devices_[device_address]));
      delete device_mac;
    }
  }
}

void BluetoothAdapterMac::RemoveUnpairedDevices(NSArray* paired_devices) {
  base::hash_set<std::string> paired_device_address_list;
  for (IOBluetoothDevice* device in paired_devices) {
    paired_device_address_list.insert(
        base::SysNSStringToUTF8([device addressString]));
  }

  DevicesMap::iterator iter = devices_.begin();
  while (iter != devices_.end()) {
    DevicesMap::iterator device_iter = iter;
    ++iter;

    if (paired_device_address_list.find(device_iter->first) !=
        paired_device_address_list.end())
      continue;

    if (device_iter->second->IsPaired()) {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceRemoved(this, device_iter->second));
      delete device_iter->second;
      devices_.erase(device_iter);
    }
  }
}

void BluetoothAdapterMac::DeviceInquiryStarted(
    IOBluetoothDeviceInquiry* inquiry) {
  DCHECK(device_inquiry_ == inquiry);
  if (discovery_status_ == DISCOVERING)
    return;

  discovery_status_ = DISCOVERING;
  RunCallbacks(on_start_discovery_callbacks_, true);
  num_discovery_listeners_ = on_start_discovery_callbacks_.size();
  on_start_discovery_callbacks_.clear();

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterDiscoveringChanged(this, true));
  MaybeStopDeviceInquiry();
}

void BluetoothAdapterMac::DeviceFound(IOBluetoothDeviceInquiry* inquiry,
                                      IOBluetoothDevice* device) {
  DCHECK(device_inquiry_ == inquiry);
  std::string device_address = base::SysNSStringToUTF8([device addressString]);
  if (discovered_devices_.find(device_address) == discovered_devices_.end()) {
    scoped_ptr<BluetoothDeviceMac> device_mac(new BluetoothDeviceMac(device));
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceAdded(this, device_mac.get()));
    discovered_devices_.insert(device_address);
  }
}

void BluetoothAdapterMac::DeviceInquiryComplete(
    IOBluetoothDeviceInquiry* inquiry,
    IOReturn error,
    bool aborted) {
  DCHECK(device_inquiry_ == inquiry);
  if (discovery_status_ == DISCOVERING &&
      [device_inquiry_ start] == kIOReturnSuccess) {
    return;
  }

  // Device discovery is done.
  discovered_devices_.clear();
  discovery_status_ = NOT_DISCOVERING;
  RunCallbacks(on_stop_discovery_callbacks_, error == kIOReturnSuccess);
  num_discovery_listeners_ = 0;
  on_stop_discovery_callbacks_.clear();
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterDiscoveringChanged(this, false));
  MaybeStartDeviceInquiry();
}

void BluetoothAdapterMac::MaybeStartDeviceInquiry() {
  if (discovery_status_ == NOT_DISCOVERING &&
      !on_start_discovery_callbacks_.empty()) {
    discovery_status_ = DISCOVERY_STARTING;
    if ([device_inquiry_ start] != kIOReturnSuccess) {
      discovery_status_ = NOT_DISCOVERING;
      RunCallbacks(on_start_discovery_callbacks_, false);
      on_start_discovery_callbacks_.clear();
    }
  }
}

void BluetoothAdapterMac::MaybeStopDeviceInquiry() {
  if (discovery_status_ != DISCOVERING)
    return;

  if (on_stop_discovery_callbacks_.size() < num_discovery_listeners_) {
    RunCallbacks(on_stop_discovery_callbacks_, true);
    num_discovery_listeners_ -= on_stop_discovery_callbacks_.size();
    on_stop_discovery_callbacks_.clear();
    return;
  }

  discovery_status_ = DISCOVERY_STOPPING;
  if ([device_inquiry_ stop] != kIOReturnSuccess) {
    RunCallbacks(on_stop_discovery_callbacks_, false);
    on_stop_discovery_callbacks_.clear();
  }
}

void BluetoothAdapterMac::RunCallbacks(
    const DiscoveryCallbackList& callback_list, bool success) const {
  for (DiscoveryCallbackList::const_iterator iter = callback_list.begin();
       iter != callback_list.end();
       ++iter) {
    if (success)
      ui_task_runner_->PostTask(FROM_HERE, iter->first);
    else
      ui_task_runner_->PostTask(FROM_HERE, iter->second);
  }
}

}  // namespace device
