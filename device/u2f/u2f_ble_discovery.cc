// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_ble_discovery.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/u2f/u2f_apdu_command.h"
#include "device/u2f/u2f_ble_device.h"
#include "device/u2f/u2f_ble_uuids.h"

namespace device {

U2fBleDiscovery::U2fBleDiscovery() : weak_factory_(this) {}

U2fBleDiscovery::~U2fBleDiscovery() {
  adapter_->RemoveObserver(this);
}

void U2fBleDiscovery::Start() {
  auto& factory = BluetoothAdapterFactory::Get();
  factory.GetAdapter(
      base::Bind(&U2fBleDiscovery::OnGetAdapter, weak_factory_.GetWeakPtr()));
}

void U2fBleDiscovery::Stop() {
  adapter_->RemoveObserver(this);

  discovery_session_->Stop(
      base::Bind(&U2fBleDiscovery::OnStopped, weak_factory_.GetWeakPtr(), true),
      base::Bind(&U2fBleDiscovery::OnStopped, weak_factory_.GetWeakPtr(),
                 false));
}

// static
const BluetoothUUID& U2fBleDiscovery::U2fServiceUUID() {
  static const BluetoothUUID service_uuid(kU2fServiceUUID);
  return service_uuid;
}

void U2fBleDiscovery::OnGetAdapter(scoped_refptr<BluetoothAdapter> adapter) {
  adapter_ = std::move(adapter);
  DCHECK(adapter_);
  VLOG(2) << "Got adapter " << adapter_->GetAddress();

  adapter_->AddObserver(this);
  if (adapter_->IsPowered()) {
    OnSetPowered();
  } else {
    adapter_->SetPowered(
        true,
        base::Bind(&U2fBleDiscovery::OnSetPowered, weak_factory_.GetWeakPtr()),
        base::Bind(&U2fBleDiscovery::OnSetPoweredError,
                   weak_factory_.GetWeakPtr()));
  }
}

void U2fBleDiscovery::OnSetPowered() {
  VLOG(2) << "Adapter " << adapter_->GetAddress() << " is powered on.";

  for (BluetoothDevice* device : adapter_->GetDevices()) {
    if (base::ContainsKey(device->GetUUIDs(), U2fServiceUUID())) {
      VLOG(2) << "U2F BLE device: " << device->GetAddress();
      AddDevice(std::make_unique<U2fBleDevice>(device->GetAddress()));
    }
  }

  auto filter = std::make_unique<BluetoothDiscoveryFilter>(
      BluetoothTransport::BLUETOOTH_TRANSPORT_LE);
  filter->AddUUID(U2fServiceUUID());

  adapter_->StartDiscoverySessionWithFilter(
      std::move(filter),
      base::Bind(&U2fBleDiscovery::OnStartDiscoverySessionWithFilter,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&U2fBleDiscovery::OnStartDiscoverySessionWithFilterError,
                 weak_factory_.GetWeakPtr()));
}

void U2fBleDiscovery::OnSetPoweredError() {
  DLOG(ERROR) << "Failed to power on the adapter.";
  NotifyDiscoveryStarted(false);
}

void U2fBleDiscovery::OnStartDiscoverySessionWithFilter(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  discovery_session_ = std::move(session);
  DVLOG(2) << "Discovery session started.";
  NotifyDiscoveryStarted(true);
}

void U2fBleDiscovery::OnStartDiscoverySessionWithFilterError() {
  DLOG(ERROR) << "Discovery session not started.";
  NotifyDiscoveryStarted(false);
}

void U2fBleDiscovery::DeviceAdded(BluetoothAdapter* adapter,
                                  BluetoothDevice* device) {
  if (base::ContainsKey(device->GetUUIDs(), U2fServiceUUID())) {
    VLOG(2) << "Discovered U2F BLE device: " << device->GetAddress();
    AddDevice(std::make_unique<U2fBleDevice>(device->GetAddress()));
  }
}

void U2fBleDiscovery::DeviceChanged(BluetoothAdapter* adapter,
                                    BluetoothDevice* device) {
  if (base::ContainsKey(device->GetUUIDs(), U2fServiceUUID()) &&
      !GetDevice(U2fBleDevice::GetId(device->GetAddress()))) {
    VLOG(2) << "Discovered U2F service on existing BLE device: "
            << device->GetAddress();
    AddDevice(std::make_unique<U2fBleDevice>(device->GetAddress()));
  }
}

void U2fBleDiscovery::DeviceRemoved(BluetoothAdapter* adapter,
                                    BluetoothDevice* device) {
  if (base::ContainsKey(device->GetUUIDs(), U2fServiceUUID())) {
    VLOG(2) << "U2F BLE device removed: " << device->GetAddress();
    RemoveDevice(U2fBleDevice::GetId(device->GetAddress()));
  }
}

void U2fBleDiscovery::OnStopped(bool success) {
  discovery_session_.reset();
  NotifyDiscoveryStopped(success);
}

}  // namespace device
