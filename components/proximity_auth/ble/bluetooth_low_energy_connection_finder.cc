// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattConnection;

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
  if (device) {
    VLOG(1) << "New device found: " << device->GetName();
    HandleDeviceAdded(device);
  }
}

void BluetoothLowEnergyConnectionFinder::OnAdapterInitialized(
    scoped_refptr<BluetoothAdapter> adapter) {
  VLOG(1) << "Adapter ready";

  adapter_ = adapter;
  adapter_->AddObserver(this);

  std::vector<BluetoothDevice*> devices = adapter_->GetDevices();
  for (auto iter = devices.begin(); iter != devices.end(); iter++) {
    HandleDeviceAdded((*iter));
  }

  StartDiscoverySession();
}

void BluetoothLowEnergyConnectionFinder::HandleDeviceAdded(
    BluetoothDevice* remote_device) {
  if (!connected_ && HasService(remote_device)) {
    CreateConnection(remote_device);
  }
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
    std::vector<device::BluetoothUUID> uuids = remote_device->GetUUIDs();
    for (auto iter = uuids.begin(); iter != uuids.end(); iter++) {
      if (remote_service_uuid_ == *iter) {
        return true;
      }
    }
  }
  return false;
}

void BluetoothLowEnergyConnectionFinder::OnCreateConnectionError(
    BluetoothDevice::ConnectErrorCode error_code) {
  VLOG(1) << "Error creating connection";
}

void BluetoothLowEnergyConnectionFinder::OnConnectionCreated(
    scoped_ptr<BluetoothGattConnection> connection) {
  VLOG(1) << "Connection created";
  connected_ = true;
  StopDiscoverySession();
  connection_callback_.Run(connection.Pass());
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
                 weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace proximity_auth
