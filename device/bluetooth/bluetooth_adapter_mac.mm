// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/location.h"
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

#endif  // MAC_OS_X_VERSION_10_7

namespace {

const int kPollIntervalMs = 500;

}  // namespace

namespace device {

BluetoothAdapterMac::BluetoothAdapterMac()
    : BluetoothAdapter(),
      powered_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

BluetoothAdapterMac::~BluetoothAdapterMac() {
}

void BluetoothAdapterMac::AddObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAdapterMac::RemoveObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
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
  return false;
}

bool BluetoothAdapterMac::IsScanning() const {
  return false;
}

void BluetoothAdapterMac::StartDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
}

void BluetoothAdapterMac::StopDiscovering(const base::Closure& callback,
                                          const ErrorCallback& error_callback) {
}

void BluetoothAdapterMac::ReadLocalOutOfBandPairingData(
    const BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
}

void BluetoothAdapterMac::TrackDefaultAdapter() {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  PollAdapter();
}

void BluetoothAdapterMac::TrackTestAdapter(
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

}  // namespace device
