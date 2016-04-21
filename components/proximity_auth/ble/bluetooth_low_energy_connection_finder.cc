// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_connection.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_device_whitelist.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattConnection;
using device::BluetoothDiscoveryFilter;

namespace proximity_auth {
namespace {
const int kMinDiscoveryRSSI = -90;
}  // namespace

class BluetoothThrottler;

BluetoothLowEnergyConnectionFinder::BluetoothLowEnergyConnectionFinder(
    const RemoteDevice remote_device,
    const std::string& remote_service_uuid,
    FinderStrategy finder_strategy,
    const BluetoothLowEnergyDeviceWhitelist* device_whitelist,
    BluetoothThrottler* bluetooth_throttler,
    int max_number_of_tries)
    : remote_device_(remote_device),
      remote_service_uuid_(device::BluetoothUUID(remote_service_uuid)),
      finder_strategy_(finder_strategy),
      device_whitelist_(device_whitelist),
      bluetooth_throttler_(bluetooth_throttler),
      max_number_of_tries_(max_number_of_tries),
      weak_ptr_factory_(this) {
  DCHECK(finder_strategy_ == FIND_ANY_DEVICE ||
         !remote_device.bluetooth_address.empty());
}

BluetoothLowEnergyConnectionFinder::~BluetoothLowEnergyConnectionFinder() {
  if (discovery_session_) {
    StopDiscoverySession();
  }

  if (connection_) {
    connection_->RemoveObserver(this);
    connection_.reset();
  }

  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothLowEnergyConnectionFinder::Find(
    const ConnectionCallback& connection_callback) {
  if (!device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    PA_LOG(WARNING) << "Bluetooth is unsupported on this platform. Aborting.";
    return;
  }
  PA_LOG(INFO) << "Finding connection";

  connection_callback_ = connection_callback;

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnAdapterInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

// It's not necessary to observe |AdapterPresentChanged| too. When |adapter_| is
// present, but not powered, it's not possible to scan for new devices.
void BluetoothLowEnergyConnectionFinder::AdapterPoweredChanged(
    BluetoothAdapter* adapter,
    bool powered) {
  DCHECK_EQ(adapter_.get(), adapter);
  PA_LOG(INFO) << "Adapter powered: " << powered;

  // Important: do not rely on |adapter->IsDiscoverying()| to verify if there is
  // an active discovery session. We need to create our own with an specific
  // filter.
  if (powered && (!discovery_session_ || !discovery_session_->IsActive()))
    StartDiscoverySession();
}

void BluetoothLowEnergyConnectionFinder::DeviceAdded(BluetoothAdapter* adapter,
                                                     BluetoothDevice* device) {
  DCHECK_EQ(adapter_.get(), adapter);
  DCHECK(device);

  // Note: Only consider |device| when it was actually added/updated during a
  // scanning, otherwise the device is stale and the GATT connection will fail.
  // For instance, when |adapter_| change status from unpowered to powered,
  // |DeviceAdded| is called for each paired |device|.
  if (adapter_->IsPowered() && discovery_session_ &&
      discovery_session_->IsActive())
    HandleDeviceUpdated(device);
}

void BluetoothLowEnergyConnectionFinder::DeviceChanged(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DCHECK_EQ(adapter_.get(), adapter);
  DCHECK(device);

  // Note: Only consider |device| when it was actually added/updated during a
  // scanning, otherwise the device is stale and the GATT connection will fail.
  // For instance, when |adapter_| change status from unpowered to powered,
  // |DeviceAdded| is called for each paired |device|.
  if (adapter_->IsPowered() && discovery_session_ &&
      discovery_session_->IsActive())
    HandleDeviceUpdated(device);
}

void BluetoothLowEnergyConnectionFinder::HandleDeviceUpdated(
    BluetoothDevice* device) {
  // Ensuring only one call to |CreateConnection()| is made. A new |connection_|
  // can be created only when the previous one disconnects, triggering a call to
  // |OnConnectionStatusChanged|.
  if (connection_)
    return;

  if (IsRightDevice(device)) {
    PA_LOG(INFO) << "Connecting to device " << device->GetAddress()
                 << " with service (" << HasService(device)
                 << ") and is paired (" << device->IsPaired();

    connection_ = CreateConnection(device->GetAddress());
    connection_->AddObserver(this);
    connection_->Connect();

    StopDiscoverySession();
  }
}

bool BluetoothLowEnergyConnectionFinder::IsRightDevice(
    BluetoothDevice* device) {
  if (!device)
    return false;

  // TODO(sacomoto): Remove it when ProximityAuthBleSystem is not needed
  // anymore.
  if (device_whitelist_)
    return device->IsPaired() &&
           (HasService(device) ||
            device_whitelist_->HasDeviceWithAddress(device->GetAddress()));

  // The device should be paired when looking for BLE devices by bluetooth
  // address.
  if (finder_strategy_ == FIND_PAIRED_DEVICE)
    return device->IsPaired() &&
           device->GetAddress() == remote_device_.bluetooth_address;
  return HasService(device);
}

bool BluetoothLowEnergyConnectionFinder::HasService(
    BluetoothDevice* remote_device) {
  if (remote_device) {
    PA_LOG(INFO) << "Device " << remote_device->GetAddress() << " has "
                 << remote_device->GetUUIDs().size() << " services.";
    std::vector<device::BluetoothUUID> uuids = remote_device->GetUUIDs();
    for (const auto& service_uuid : uuids) {
      if (remote_service_uuid_ == service_uuid) {
        return true;
      }
    }
  }
  return false;
}

void BluetoothLowEnergyConnectionFinder::OnAdapterInitialized(
    scoped_refptr<BluetoothAdapter> adapter) {
  PA_LOG(INFO) << "Adapter ready";

  adapter_ = adapter;
  adapter_->AddObserver(this);

  // This is important for debugging. To eliminate the case where the device was
  // removed (forgotten) by the user, or BlueZ didn't load the device correctly.
  if (finder_strategy_ == FIND_PAIRED_DEVICE) {
    PA_LOG(INFO) << "Looking for paired device: "
                 << remote_device_.bluetooth_address;
    for (auto& device : adapter_->GetDevices()) {
      if (device->IsPaired())
        PA_LOG(INFO) << device->GetAddress() << " is paired";
    }
  }

  // Note: It's possible to connect to the paired directly, so when using
  // FIND_PAIRED_DEVICE strategy this is not necessary. However, the discovery
  // doesn't add a lot of latency, and the makes the code path for both
  // strategies more similar.
  StartDiscoverySession();
}

void BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStarted(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  PA_LOG(INFO) << "Discovery session started";
  discovery_session_ = std::move(discovery_session);
}

void BluetoothLowEnergyConnectionFinder::OnStartDiscoverySessionError() {
  PA_LOG(WARNING) << "Error starting discovery session";
}

void BluetoothLowEnergyConnectionFinder::StartDiscoverySession() {
  DCHECK(adapter_);
  if (discovery_session_ && discovery_session_->IsActive()) {
    PA_LOG(INFO) << "Discovery session already active";
    return;
  }

  // Discover only low energy (LE) devices with strong enough signal.
  std::unique_ptr<BluetoothDiscoveryFilter> filter(new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  filter->SetRSSI(kMinDiscoveryRSSI);

  adapter_->StartDiscoverySessionWithFilter(
      std::move(filter),
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStarted,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &BluetoothLowEnergyConnectionFinder::OnStartDiscoverySessionError,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyConnectionFinder::StopDiscoverySession() {
  PA_LOG(INFO) << "Stopping discovery session";
  // Destroying the discovery session also stops it.
  discovery_session_.reset();
}

std::unique_ptr<Connection>
BluetoothLowEnergyConnectionFinder::CreateConnection(
    const std::string& device_address) {
  DCHECK(remote_device_.bluetooth_address.empty() ||
         remote_device_.bluetooth_address == device_address);
  remote_device_.bluetooth_address = device_address;
  return base::WrapUnique(new BluetoothLowEnergyConnection(
      remote_device_, adapter_, remote_service_uuid_, bluetooth_throttler_,
      max_number_of_tries_));
}

void BluetoothLowEnergyConnectionFinder::OnConnectionStatusChanged(
    Connection* connection,
    Connection::Status old_status,
    Connection::Status new_status) {
  DCHECK_EQ(connection, connection_.get());
  PA_LOG(INFO) << "OnConnectionStatusChanged: " << old_status << " -> "
               << new_status;

  if (!connection_callback_.is_null() && connection_->IsConnected()) {
    adapter_->RemoveObserver(this);
    connection_->RemoveObserver(this);

    // If we invoke the callback now, the callback function may install its own
    // observer to |connection_|. Because we are in the ConnectionObserver
    // callstack, this new observer will receive this connection event.
    // Therefore, we need to invoke the callback or restart discovery
    // asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&BluetoothLowEnergyConnectionFinder::InvokeCallbackAsync,
                   weak_ptr_factory_.GetWeakPtr()));
  } else if (old_status == Connection::IN_PROGRESS) {
    PA_LOG(WARNING) << "Connection failed. Retrying.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            &BluetoothLowEnergyConnectionFinder::RestartDiscoverySessionAsync,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void BluetoothLowEnergyConnectionFinder::RestartDiscoverySessionAsync() {
  PA_LOG(INFO) << "Restarting discovery session.";
  connection_.reset();
  if (!discovery_session_ || !discovery_session_->IsActive())
    StartDiscoverySession();
}

BluetoothDevice* BluetoothLowEnergyConnectionFinder::GetDevice(
    const std::string& device_address) {
  // It's not possible to simply use
  // |adapter_->GetDevice(GetRemoteDeviceAddress())| to find the device with MAC
  // address |GetRemoteDeviceAddress()|. For paired devices,
  // BluetoothAdapter::GetDevice(XXX) searches for the temporary MAC address
  // XXX, whereas |remote_device_.bluetooth_address| is the real MAC address.
  // This is a bug in the way device::BluetoothAdapter is storing the devices
  // (see crbug.com/497841).
  std::vector<BluetoothDevice*> devices = adapter_->GetDevices();
  for (const auto& device : devices) {
    if (device->GetAddress() == device_address)
      return device;
  }
  return nullptr;
}

void BluetoothLowEnergyConnectionFinder::InvokeCallbackAsync() {
  connection_callback_.Run(std::move(connection_));
}

}  // namespace proximity_auth
