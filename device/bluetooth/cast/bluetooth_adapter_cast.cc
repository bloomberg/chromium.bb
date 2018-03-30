// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_adapter_cast.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager.h"
#include "chromecast/device/bluetooth/le/le_scan_manager.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/cast/bluetooth_device_cast.h"

namespace device {
namespace {

// The classes in //device/bluetooth expect addresses to be in the canonical
// format: "AA:BB:CC:DD:EE:FF". Use this utility whenever an address from the
// Cast stack is converted.
std::string GetCanonicalAddress(
    const chromecast::bluetooth_v2_shlib::Addr& addr) {
  return BluetoothDevice::CanonicalizeAddress(
      chromecast::bluetooth::util::AddrToString(addr));
}

}  // namespace

BluetoothAdapterCast::BluetoothAdapterCast(
    chromecast::bluetooth::GattClientManager* gatt_client_manager,
    chromecast::bluetooth::LeScanManager* le_scan_manager)
    : gatt_client_manager_(gatt_client_manager),
      le_scan_manager_(le_scan_manager),
      weak_factory_(this) {
  DCHECK(gatt_client_manager_);
  DCHECK(le_scan_manager_);
  gatt_client_manager_->AddObserver(this);
  le_scan_manager_->AddObserver(this);
}

BluetoothAdapterCast::~BluetoothAdapterCast() {
  gatt_client_manager_->RemoveObserver(this);
  le_scan_manager_->RemoveObserver(this);
}

std::string BluetoothAdapterCast::GetAddress() const {
  // TODO(slan|bcf): Right now, we aren't surfacing the address of the GATT
  // client to the caller, because there is no apparent need and this
  // information is potentially PII. Implement this when it's needed.
  return std::string();
}

std::string BluetoothAdapterCast::GetName() const {
  return name_;
}

void BluetoothAdapterCast::SetName(const std::string& name,
                                   const base::Closure& callback,
                                   const ErrorCallback& error_callback) {
  name_ = name;
  callback.Run();
}

bool BluetoothAdapterCast::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return initialized_;
}

bool BluetoothAdapterCast::IsPresent() const {
  // The BluetoothAdapter is always present on Cast devices.
  return true;
}

bool BluetoothAdapterCast::IsPowered() const {
  // The BluetoothAdapter is always powered on Cast devices.
  return true;
}

void BluetoothAdapterCast::SetPowered(bool powered,
                                      const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
  // This class cannot change the powered state of the BT stack. Assume that it
  // is always powered.
  if (powered) {
    callback.Run();
  } else {
    LOG(ERROR) << "Cannot change the powered state of the BT stack.";
    error_callback.Run();
  }
}

bool BluetoothAdapterCast::IsDiscoverable() const {
  VLOG(2) << __func__ << " GATT server mode not supported";
  return false;
}

void BluetoothAdapterCast::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  error_callback.Run();
}

bool BluetoothAdapterCast::IsDiscovering() const {
  return true;
}

BluetoothAdapter::UUIDList BluetoothAdapterCast::GetUUIDs() const {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  return UUIDList();
}

void BluetoothAdapterCast::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  error_callback.Run("Not Implemented");
}

void BluetoothAdapterCast::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  error_callback.Run("Not Implemented");
}

void BluetoothAdapterCast::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const AdvertisementErrorCallback& error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  error_callback.Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

void BluetoothAdapterCast::SetAdvertisingInterval(
    const base::TimeDelta& min,
    const base::TimeDelta& max,
    const base::Closure& callback,
    const AdvertisementErrorCallback& error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  error_callback.Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

void BluetoothAdapterCast::ResetAdvertising(
    const base::Closure& callback,
    const AdvertisementErrorCallback& error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  error_callback.Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

BluetoothLocalGattService* BluetoothAdapterCast::GetGattService(
    const std::string& identifier) const {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  return nullptr;
}

bool BluetoothAdapterCast::SetPoweredImpl(bool powered) {
  NOTREACHED() << "This method is not invoked when SetPowered() is overridden.";
  return true;
}

void BluetoothAdapterCast::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  // TODO(slan): Implement this or properly stub.
  NOTIMPLEMENTED();
  error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_IMPLEMENTED);
}

void BluetoothAdapterCast::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  // TODO(slan): Implement this or properly stub.
  NOTIMPLEMENTED();
  error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_IMPLEMENTED);
}

void BluetoothAdapterCast::SetDiscoveryFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  // TODO(slan): Implement this or properly stub.
  NOTIMPLEMENTED();
  error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_IMPLEMENTED);
}

void BluetoothAdapterCast::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  // TODO(slan): Implement this or properly stub.
  NOTIMPLEMENTED();
}

void BluetoothAdapterCast::OnConnectChanged(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> device,
    bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto address = GetCanonicalAddress(device->addr());
  VLOG(1) << __func__ << " " << address << " connected: " << connected;

  // This method could be called before this device is detected in a scan and
  // GetDevice() is called. Add it if needed.
  if (devices_.find(address) == devices_.end())
    AddDevice(std::move(device));

  BluetoothDeviceCast* cast_device = GetCastDevice(address);
  cast_device->SetConnected(connected);
}

void BluetoothAdapterCast::OnMtuChanged(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> device,
    int mtu) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(3) << __func__ << " " << GetCanonicalAddress(device->addr())
           << " mtu: " << mtu;
}

void BluetoothAdapterCast::OnServicesUpdated(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> device,
    std::vector<scoped_refptr<chromecast::bluetooth::RemoteService>> services) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = devices_.find(GetCanonicalAddress(device->addr()));
  if (it == devices_.end())
    return;
  it->second->SetGattServicesDiscoveryComplete(true);
}

void BluetoothAdapterCast::OnCharacteristicNotification(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> device,
    scoped_refptr<chromecast::bluetooth::RemoteCharacteristic> characteristic,
    std::vector<uint8_t> value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << __func__ << " " << GetCanonicalAddress(device->addr())
           << " updated.";

  // TODO(slan): Add an Observer interface to RemoteCharacteristc so this can be
  // wired directly to the BluetoothRemoteGattCharacteristicCast proxy, rather
  // than by performing a search of the services on device for |characteristc|.
  NOTIMPLEMENTED();
}

void BluetoothAdapterCast::OnNewScanResult(
    chromecast::bluetooth::LeScanManager::ScanResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto address = GetCanonicalAddress(result.addr);

  // If we haven't created a BluetoothDeviceCast for this address yet, we need
  // to send an async request to |gatt_client_manager_| for a handle to the
  // device.
  if (devices_.find(address) == devices_.end()) {
    // Only send a request if this is the first time we've seen this |address|
    // in a scan. This may happen if we pick up additional GAP advertisements
    // while the first request is in-flight.
    if (pending_scan_results_.find(address) == pending_scan_results_.end()) {
      gatt_client_manager_->GetDevice(
          result.addr, base::BindOnce(&BluetoothAdapterCast::OnGetDevice,
                                      weak_factory_.GetWeakPtr()));
    }

    // These results will be used to construct the BluetoothDeviceCast.
    pending_scan_results_[address].push_back(result);
    return;
  }

  // Update the device with the ScanResult.
  BluetoothDeviceCast* device = GetCastDevice(address);
  if (device->UpdateWithScanResult(result)) {
    for (auto& observer : observers_)
      observer.DeviceChanged(this, device);
  }
}

void BluetoothAdapterCast::OnScanEnableChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scan_enabled_ = enabled;
  if (enabled)
    return;

  // This is called when the state of |le_scan_manager_| changes. If it has been
  // disabled suddenly, log a warning.
  // TODO(slan): We may need to re-enable this on requestDevice() calls.
  LOG(WARNING) << "BLE scan has been disabled!";
}

BluetoothDeviceCast* BluetoothAdapterCast::GetCastDevice(
    const std::string& address) {
  return static_cast<BluetoothDeviceCast*>(devices_[address].get());
}

void BluetoothAdapterCast::AddDevice(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> remote_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This method should not be called if we already have a BluetoothDeviceCast
  // registered for this device.
  auto address = GetCanonicalAddress(remote_device->addr());
  DCHECK(devices_.find(address) == devices_.end());

  devices_[address] =
      std::make_unique<BluetoothDeviceCast>(this, remote_device);
  BluetoothDeviceCast* device = GetCastDevice(address);

  const auto scan_results = std::move(pending_scan_results_[address]);
  pending_scan_results_.erase(address);

  // Update the device with the ScanResults.
  for (const auto& result : scan_results)
    device->UpdateWithScanResult(result);

  // Update the observers of the new device.
  for (auto& observer : observers_)
    observer.DeviceAdded(this, device);
}

void BluetoothAdapterCast::OnScanEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled) {
    LOG(WARNING) << "Failed to start scan.";
    // TODO(slan): Retry the scan after some amount of time.
    return;
  }

  scan_enabled_ = true;
  le_scan_manager_->GetScanResults(base::BindOnce(
      &BluetoothAdapterCast::OnGetScanResults, weak_factory_.GetWeakPtr()));
}

void BluetoothAdapterCast::OnGetDevice(
    scoped_refptr<chromecast::bluetooth::RemoteDevice> remote_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto address = GetCanonicalAddress(remote_device->addr());

  // This callback could run before or after the device becomes connected and
  // OnConnectChanged() is called for a particular device. If that happened,
  // |remote_device| already has a handle. In this case, there should be no
  // |pending_scan_results_| and we should fast-return.
  if (devices_.find(address) != devices_.end()) {
    DCHECK(pending_scan_results_.find(address) == pending_scan_results_.end());
    return;
  }

  // If there is not a device already, there should be at least one ScanResult
  // which triggered the GetDevice() call.
  DCHECK(pending_scan_results_.find(address) != pending_scan_results_.end());
  AddDevice(std::move(remote_device));
}

void BluetoothAdapterCast::OnGetScanResults(
    std::vector<chromecast::bluetooth::LeScanManager::ScanResult> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& result : results)
    OnNewScanResult(result);
}

void BluetoothAdapterCast::InitializeAsynchronously(InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  initialized_ = true;
  le_scan_manager_->SetScanEnable(
      true, base::BindOnce(&BluetoothAdapterCast::OnScanEnabled,
                           weak_factory_.GetWeakPtr()));

  // Subsequent calls will get the scan results.
  std::move(callback).Run();
}

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapterCast::Create(
    InitCallback callback) {
  // TODO(slan): We need to figure out how to get these classes properly.
  chromecast::bluetooth::GattClientManager* gatt_client_manager = nullptr;
  chromecast::bluetooth::LeScanManager* le_scan_manager = nullptr;

  // TODO(slan): Consider life-cycle management. Currently this just leaks.
  auto* adapter =
      new BluetoothAdapterCast(gatt_client_manager, le_scan_manager);
  base::WeakPtr<BluetoothAdapterCast> weak_ptr =
      adapter->weak_factory_.GetWeakPtr();

  // BluetoothAdapterFactory assumes that |init_callback| will be called
  // asynchronously the first time, and that IsInitialized() will return false.
  // Post init_callback to this sequence so that it gets called back later.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothAdapterCast::InitializeAsynchronously,
                                weak_ptr, std::move(callback)));
  return weak_ptr;
}

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return BluetoothAdapterCast::Create(std::move(init_callback));
}

}  // namespace device
