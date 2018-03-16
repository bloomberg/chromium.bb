// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_ble_discovery.h"

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
#include "device/fido/fido_ble_device.h"
#include "device/fido/fido_ble_uuids.h"

namespace device {

FidoBleDiscovery::FidoBleDiscovery() : weak_factory_(this) {}

FidoBleDiscovery::~FidoBleDiscovery() {
  if (adapter_)
    adapter_->RemoveObserver(this);

  // Pretend we are able to successfully stop a discovery session in case it is
  // still present.
  if (discovery_session_)
    OnStopped(true);
}

U2fTransportProtocol FidoBleDiscovery::GetTransportProtocol() const {
  return U2fTransportProtocol::kBluetoothLowEnergy;
}

void FidoBleDiscovery::Start() {
  auto& factory = BluetoothAdapterFactory::Get();
  factory.GetAdapter(
      base::Bind(&FidoBleDiscovery::OnGetAdapter, weak_factory_.GetWeakPtr()));
}

void FidoBleDiscovery::Stop() {
  DCHECK(adapter_);
  adapter_->RemoveObserver(this);

  DCHECK(discovery_session_);
  discovery_session_->Stop(base::Bind(&FidoBleDiscovery::OnStopped,
                                      weak_factory_.GetWeakPtr(), true),
                           base::Bind(&FidoBleDiscovery::OnStopped,
                                      weak_factory_.GetWeakPtr(), false));
}

// static
const BluetoothUUID& FidoBleDiscovery::FidoServiceUUID() {
  static const BluetoothUUID service_uuid(kFidoServiceUUID);
  return service_uuid;
}

void FidoBleDiscovery::OnGetAdapter(scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(!adapter_);
  adapter_ = std::move(adapter);
  DCHECK(adapter_);
  VLOG(2) << "Got adapter " << adapter_->GetAddress();

  adapter_->AddObserver(this);
  if (adapter_->IsPowered()) {
    OnSetPowered();
  } else {
    adapter_->SetPowered(
        true,
        base::Bind(&FidoBleDiscovery::OnSetPowered, weak_factory_.GetWeakPtr()),
        base::Bind(&FidoBleDiscovery::OnSetPoweredError,
                   weak_factory_.GetWeakPtr()));
  }
}

void FidoBleDiscovery::OnSetPowered() {
  DCHECK(adapter_);
  VLOG(2) << "Adapter " << adapter_->GetAddress() << " is powered on.";

  for (BluetoothDevice* device : adapter_->GetDevices()) {
    if (base::ContainsKey(device->GetUUIDs(), FidoServiceUUID())) {
      VLOG(2) << "U2F BLE device: " << device->GetAddress();
      AddDevice(std::make_unique<FidoBleDevice>(device->GetAddress()));
    }
  }

  auto filter = std::make_unique<BluetoothDiscoveryFilter>(
      BluetoothTransport::BLUETOOTH_TRANSPORT_LE);
  filter->AddUUID(FidoServiceUUID());

  adapter_->StartDiscoverySessionWithFilter(
      std::move(filter),
      base::Bind(&FidoBleDiscovery::OnStartDiscoverySessionWithFilter,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&FidoBleDiscovery::OnStartDiscoverySessionWithFilterError,
                 weak_factory_.GetWeakPtr()));
}

void FidoBleDiscovery::OnSetPoweredError() {
  DLOG(ERROR) << "Failed to power on the adapter.";
  NotifyDiscoveryStarted(false);
}

void FidoBleDiscovery::OnStartDiscoverySessionWithFilter(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  discovery_session_ = std::move(session);
  DVLOG(2) << "Discovery session started.";
  NotifyDiscoveryStarted(true);
}

void FidoBleDiscovery::OnStartDiscoverySessionWithFilterError() {
  DLOG(ERROR) << "Discovery session not started.";
  NotifyDiscoveryStarted(false);
}

void FidoBleDiscovery::DeviceAdded(BluetoothAdapter* adapter,
                                   BluetoothDevice* device) {
  if (base::ContainsKey(device->GetUUIDs(), FidoServiceUUID())) {
    VLOG(2) << "Discovered U2F BLE device: " << device->GetAddress();
    AddDevice(std::make_unique<FidoBleDevice>(device->GetAddress()));
  }
}

void FidoBleDiscovery::DeviceChanged(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (base::ContainsKey(device->GetUUIDs(), FidoServiceUUID()) &&
      !GetDevice(FidoBleDevice::GetId(device->GetAddress()))) {
    VLOG(2) << "Discovered U2F service on existing BLE device: "
            << device->GetAddress();
    AddDevice(std::make_unique<FidoBleDevice>(device->GetAddress()));
  }
}

void FidoBleDiscovery::DeviceRemoved(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (base::ContainsKey(device->GetUUIDs(), FidoServiceUUID())) {
    VLOG(2) << "U2F BLE device removed: " << device->GetAddress();
    RemoveDevice(FidoBleDevice::GetId(device->GetAddress()));
  }
}

void FidoBleDiscovery::OnStopped(bool success) {
  discovery_session_.reset();
  NotifyDiscoveryStopped(success);
}

}  // namespace device
