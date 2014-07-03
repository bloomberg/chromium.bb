// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/location.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_device_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace {

// The frequency with which to poll the adapter for updates.
const int kPollIntervalMs = 500;

// The length of time that must elapse since the last Inquiry response before a
// discovered Classic device is considered to be no longer available.
const NSTimeInterval kDiscoveryTimeoutSec = 3 * 60;  // 3 minutes

}  // namespace

namespace device {

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return BluetoothAdapterMac::CreateAdapter();
}

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapterMac::CreateAdapter() {
  BluetoothAdapterMac* adapter = new BluetoothAdapterMac();
  adapter->Init();
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

BluetoothAdapterMac::BluetoothAdapterMac()
    : BluetoothAdapter(),
      powered_(false),
      num_discovery_sessions_(0),
      classic_discovery_manager_(
          BluetoothDiscoveryManagerMac::CreateClassic(this)),
      weak_ptr_factory_(this) {
  DCHECK(classic_discovery_manager_.get());
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

std::string BluetoothAdapterMac::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterMac::GetName() const {
  return name_;
}

void BluetoothAdapterMac::SetName(const std::string& name,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsDiscoverable() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterMac::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsDiscovering() const {
  return classic_discovery_manager_->IsDiscovering();
}

void BluetoothAdapterMac::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingRfcomm(
      this, uuid, options, base::Bind(callback, socket), error_callback);
}

void BluetoothAdapterMac::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingL2cap(
      this, uuid, options, base::Bind(callback, socket), error_callback);
}

void BluetoothAdapterMac::DeviceFound(IOBluetoothDevice* device) {
  DeviceAdded(device);
}

void BluetoothAdapterMac::DiscoveryStopped(bool unexpected) {
  if (unexpected) {
    DVLOG(1) << "Discovery stopped unexpectedly";
    num_discovery_sessions_ = 0;
    MarkDiscoverySessionsAsInactive();
  }
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    AdapterDiscoveringChanged(this, false));
}

void BluetoothAdapterMac::DeviceConnected(IOBluetoothDevice* device) {
  // TODO(isherman): Investigate whether this method can be replaced with a call
  // to +registerForConnectNotifications:selector:.
  DVLOG(1) << "Adapter registered a new connection from device with address: "
           << BluetoothDeviceMac::GetDeviceAddress(device);
  DeviceAdded(device);
}

void BluetoothAdapterMac::AddDiscoverySession(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DVLOG(1) << __func__;
  if (num_discovery_sessions_ > 0) {
    DCHECK(IsDiscovering());
    num_discovery_sessions_++;
    callback.Run();
    return;
  }

  DCHECK_EQ(0, num_discovery_sessions_);

  if (!classic_discovery_manager_->StartDiscovery()) {
    DVLOG(1) << "Failed to add a discovery session";
    error_callback.Run();
    return;
  }

  DVLOG(1) << "Added a discovery session";
  num_discovery_sessions_++;
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    AdapterDiscoveringChanged(this, true));
  callback.Run();
}

void BluetoothAdapterMac::RemoveDiscoverySession(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DVLOG(1) << __func__;

  if (num_discovery_sessions_ > 1) {
    // There are active sessions other than the one currently being removed.
    DCHECK(IsDiscovering());
    num_discovery_sessions_--;
    callback.Run();
    return;
  }

  if (num_discovery_sessions_ == 0) {
    DVLOG(1) << "No active discovery sessions. Returning error.";
    error_callback.Run();
    return;
  }

  if (!classic_discovery_manager_->StopDiscovery()) {
    DVLOG(1) << "Failed to stop discovery";
    error_callback.Run();
    return;
  }

  DVLOG(1) << "Discovery stopped";
  num_discovery_sessions_--;
  callback.Run();
}

void BluetoothAdapterMac::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
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
  std::string name;
  std::string address;
  bool powered = false;
  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];

  if (controller != nil) {
    name = base::SysNSStringToUTF8([controller nameAsString]);
    address = BluetoothDevice::CanonicalizeAddress(
        base::SysNSStringToUTF8([controller addressAsString]));
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

  UpdateDevices();

  ui_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BluetoothAdapterMac::PollAdapter,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPollIntervalMs));
}

void BluetoothAdapterMac::DeviceAdded(IOBluetoothDevice* device) {
  std::string device_address = BluetoothDeviceMac::GetDeviceAddress(device);

  // Only notify observers once per device.
  if (devices_.count(device_address))
    return;

  devices_[device_address] = new BluetoothDeviceMac(device);
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    DeviceAdded(this, devices_[device_address]));
}

void BluetoothAdapterMac::UpdateDevices() {
  // Notify observers if any previously seen devices are no longer available,
  // i.e. if they are no longer paired, connected, nor recently discovered via
  // an inquiry.
  std::set<std::string> removed_devices;
  for (DevicesMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    BluetoothDevice* device = it->second;
    if (device->IsPaired() || device->IsConnected())
      continue;

    NSDate* last_inquiry_update =
        static_cast<BluetoothDeviceMac*>(device)->GetLastInquiryUpdate();
    if (last_inquiry_update &&
        -[last_inquiry_update timeIntervalSinceNow] < kDiscoveryTimeoutSec)
      continue;

    FOR_EACH_OBSERVER(
        BluetoothAdapter::Observer, observers_, DeviceRemoved(this, device));
    delete device;
    removed_devices.insert(it->first);
    // The device will be erased from the map in the loop immediately below.
  }
  for (const std::string& device_address : removed_devices) {
    size_t num_removed = devices_.erase(device_address);
    DCHECK_EQ(num_removed, 1U);
  }

  // Add any new paired devices.
  for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
    DeviceAdded(device);
  }
}

}  // namespace device
