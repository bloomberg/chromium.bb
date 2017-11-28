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
#include "device/u2f/u2f_ble_uuids.h"
#include "device/u2f/u2f_device.h"

namespace device {

namespace {

// TODO(crbug/763303): Remove this once a U2fDevice for BLE is implemented.
class U2fFakeBleDevice : public U2fDevice {
 public:
  static std::string GetId(base::StringPiece address) {
    std::string result = "ble:";
    result.append(address.data(), address.size());
    return result;
  }

  explicit U2fFakeBleDevice(base::StringPiece address)
      : id_(GetId(address)), weak_factory_(this) {}

  ~U2fFakeBleDevice() override = default;

  void TryWink(WinkCallback callback) override {}
  std::string GetId() const override { return id_; }

 protected:
  void DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                      DeviceCallback callback) override {
    std::move(callback).Run(false, nullptr);
  }

  base::WeakPtr<U2fDevice> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::string id_;

  base::WeakPtrFactory<U2fFakeBleDevice> weak_factory_;
};

}  // namespace

U2fBleDiscovery::U2fBleDiscovery() : weak_factory_(this) {}

U2fBleDiscovery::~U2fBleDiscovery() {
  adapter_->RemoveObserver(this);
}

void U2fBleDiscovery::Start() {
  auto& factory = BluetoothAdapterFactory::Get();
  factory.GetAdapter(base::Bind(&U2fBleDiscovery::GetAdapterCallback,
                                weak_factory_.GetWeakPtr()));
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
  static const BluetoothUUID service_uuid(U2F_SERVICE_UUID);
  return service_uuid;
}

void U2fBleDiscovery::GetAdapterCallback(
    scoped_refptr<BluetoothAdapter> adapter) {
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
        base::Bind(
            [](base::WeakPtr<Delegate> delegate) {
              LOG(ERROR) << "Failed to power on the adapter.";
              if (delegate)
                delegate->OnStarted(false);
            },
            delegate_));
  }
}

void U2fBleDiscovery::OnSetPowered() {
  VLOG(2) << "Adapter " << adapter_->GetAddress() << " is powered on.";

  for (BluetoothDevice* device : adapter_->GetDevices()) {
    if (base::ContainsKey(device->GetUUIDs(), U2fServiceUUID())) {
      VLOG(2) << "U2F BLE device: " << device->GetAddress();
      if (delegate_) {
        delegate_->OnDeviceAdded(
            std::make_unique<U2fFakeBleDevice>(device->GetAddress()));
      }
    }
  }

  auto filter = std::make_unique<BluetoothDiscoveryFilter>(
      BluetoothTransport::BLUETOOTH_TRANSPORT_LE);
  filter->AddUUID(U2fServiceUUID());

  adapter_->StartDiscoverySessionWithFilter(
      std::move(filter),
      base::Bind(&U2fBleDiscovery::DiscoverySessionStarted,
                 weak_factory_.GetWeakPtr()),
      base::Bind(
          [](base::WeakPtr<Delegate> delegate) {
            LOG(ERROR) << "Discovery session not started.";
            if (delegate)
              delegate->OnStarted(false);
          },
          delegate_));
}

void U2fBleDiscovery::DiscoverySessionStarted(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  discovery_session_ = std::move(session);
  VLOG(2) << "Discovery session started.";
  if (delegate_)
    delegate_->OnStarted(true);
}

void U2fBleDiscovery::DeviceAdded(BluetoothAdapter* adapter,
                                  BluetoothDevice* device) {
  if (base::ContainsKey(device->GetUUIDs(), U2fServiceUUID())) {
    VLOG(2) << "Discovered U2F BLE device: " << device->GetAddress();
    if (delegate_) {
      delegate_->OnDeviceAdded(
          std::make_unique<U2fFakeBleDevice>(device->GetAddress()));
    }
  }
}

void U2fBleDiscovery::DeviceRemoved(BluetoothAdapter* adapter,
                                    BluetoothDevice* device) {
  if (base::ContainsKey(device->GetUUIDs(), U2fServiceUUID())) {
    VLOG(2) << "U2F BLE device removed: " << device->GetAddress();
    if (delegate_)
      delegate_->OnDeviceRemoved(U2fFakeBleDevice::GetId(device->GetAddress()));
  }
}

void U2fBleDiscovery::OnStopped(bool success) {
  discovery_session_.reset();
  if (delegate_)
    delegate_->OnStopped(success);
}

}  // namespace device
