// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattConnection;
using device::BluetoothDiscoveryFilter;

namespace proximity_auth {

BluetoothLowEnergyConnectionFinder::BluetoothLowEnergyConnectionFinder(
    const std::string& remote_service_uuid)
    : remote_service_uuid_(device::BluetoothUUID(remote_service_uuid)),
      connected_(false),
      weak_ptr_factory_(this) {
}

BluetoothLowEnergyConnectionFinder::~BluetoothLowEnergyConnectionFinder() {
  if (discovery_session_) {
    StopDiscoverySession();
  }
  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothLowEnergyConnectionFinder::Find(
    const BluetoothDevice::GattConnectionCallback& connection_callback) {
  if (!device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    VLOG(1) << "[BCF] Bluetooth is unsupported on this platform. Aborting.";
    return;
  }
  VLOG(1) << "Finding connection";

  connection_callback_ = connection_callback;

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnAdapterInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyConnectionFinder::Find(
    const ConnectionCallback& connection_callback) {
  NOTREACHED();
}

void BluetoothLowEnergyConnectionFinder::DeviceAdded(BluetoothAdapter* adapter,
                                                     BluetoothDevice* device) {
  DCHECK(device);
  VLOG(1) << "Device added: " << device->GetAddress();
  HandleDeviceUpdated(device);
}

void BluetoothLowEnergyConnectionFinder::DeviceChanged(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DCHECK(device);
  VLOG(1) << "Device changed: " << device->GetAddress();
  HandleDeviceUpdated(device);
}

void BluetoothLowEnergyConnectionFinder::HandleDeviceUpdated(
    BluetoothDevice* device) {
  if (connected_)
    return;
  const auto& i = pending_connections_.find(device);
  if (i != pending_connections_.end()) {
    VLOG(1) << "Pending connection to device " << device->GetAddress();
    return;
  }
  if (HasService(device)) {
    VLOG(1) << "Connecting to device " << device->GetAddress();
    pending_connections_.insert(device);
    CreateConnection(device);
  }
}

void BluetoothLowEnergyConnectionFinder::DeviceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  if (connected_)
    return;

  const auto& i = pending_connections_.find(device);
  if (i != pending_connections_.end()) {
    VLOG(1) << "Remove pending connection to  " << device->GetAddress();
    pending_connections_.erase(i);
  }
}

void BluetoothLowEnergyConnectionFinder::OnAdapterInitialized(
    scoped_refptr<BluetoothAdapter> adapter) {
  VLOG(1) << "Adapter ready";

  adapter_ = adapter;
  adapter_->AddObserver(this);

  // Note: Avoid trying to connect to existing devices as they may be stale.
  // The Bluetooth adapter will fire |OnDeviceChanged| notifications for all
  // not-stale Bluetooth Low Energy devices that are advertising.
  if (VLOG_IS_ON(1)) {
    std::vector<BluetoothDevice*> devices = adapter_->GetDevices();
    for (auto* device : devices) {
      VLOG(1) << "Ignoring device " << device->GetAddress()
              << " present when adapter was initialized.";
    }
  }

  StartDiscoverySession();
}

void BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStarted(
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  VLOG(1) << "Discovery session started";
  discovery_session_ = discovery_session.Pass();
}

void BluetoothLowEnergyConnectionFinder::OnStartDiscoverySessionError() {
  VLOG(1) << "Error starting discovery session";
}

void BluetoothLowEnergyConnectionFinder::StartDiscoverySession() {
  DCHECK(adapter_);
  if (discovery_session_ && discovery_session_->IsActive()) {
    VLOG(1) << "Discovery session already active";
    return;
  }

  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStarted,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &BluetoothLowEnergyConnectionFinder::OnStartDiscoverySessionError,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStopped() {
  VLOG(1) << "Discovery session stopped";
  discovery_session_.reset();
}

void BluetoothLowEnergyConnectionFinder::OnStopDiscoverySessionError() {
  VLOG(1) << "Error stopping discovery session";
}

void BluetoothLowEnergyConnectionFinder::StopDiscoverySession() {
  VLOG(1) << "Stopping discovery sesison";

  if (!adapter_) {
    VLOG(1) << "Adapter not initialized";
    return;
  }
  if (!discovery_session_ || !discovery_session_->IsActive()) {
    VLOG(1) << "No Active discovery session";
    return;
  }

  discovery_session_->Stop(
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStopped,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &BluetoothLowEnergyConnectionFinder::OnStopDiscoverySessionError,
          weak_ptr_factory_.GetWeakPtr()));
}

bool BluetoothLowEnergyConnectionFinder::HasService(
    BluetoothDevice* remote_device) {
  if (remote_device) {
    VLOG(1) << "Device " << remote_device->GetAddress() << " has "
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

void BluetoothLowEnergyConnectionFinder::OnCreateConnectionError(
    std::string device_address,
    BluetoothDevice::ConnectErrorCode error_code) {
  VLOG(1) << "Error creating connection to device " << device_address
          << " : error code = " << error_code;
}

void BluetoothLowEnergyConnectionFinder::OnConnectionCreated(
    scoped_ptr<BluetoothGattConnection> connection) {
  if (connected_) {
    CloseConnection(connection.Pass());
    return;
  }

  VLOG(1) << "Connection created";
  connected_ = true;
  pending_connections_.clear();
  if (!connection_callback_.is_null()) {
    connection_callback_.Run(connection.Pass());
    connection_callback_.Reset();
  }
  StopDiscoverySession();
}

void BluetoothLowEnergyConnectionFinder::CreateConnection(
    device::BluetoothDevice* remote_device) {
  VLOG(1) << "SmartLock service found ("
          << remote_service_uuid_.canonical_value() << ")\n"
          << "device = " << remote_device->GetAddress()
          << ", name = " << remote_device->GetName();
  remote_device->CreateGattConnection(
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnConnectionCreated,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnCreateConnectionError,
                 weak_ptr_factory_.GetWeakPtr(), remote_device->GetAddress()));
}

void BluetoothLowEnergyConnectionFinder::CloseConnection(
    scoped_ptr<device::BluetoothGattConnection> connection) {
  std::string device_address = connection->GetDeviceAddress();
  connection.reset();
  BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (device) {
    DCHECK(HasService(device));
    VLOG(1) << "Forget device " << device->GetAddress();
    device->Forget(base::Bind(&base::DoNothing));
  }
}

}  // namespace proximity_auth
