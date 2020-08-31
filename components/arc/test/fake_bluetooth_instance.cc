// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/arc/test/fake_bluetooth_instance.h"

namespace arc {

FakeBluetoothInstance::FakeBluetoothInstance() = default;
FakeBluetoothInstance::~FakeBluetoothInstance() = default;

FakeBluetoothInstance::GattDBResult::GattDBResult(
    mojom::BluetoothAddressPtr&& remote_addr,
    std::vector<mojom::BluetoothGattDBElementPtr>&& db)
    : remote_addr_(std::move(remote_addr)), db_(std::move(db)) {}

FakeBluetoothInstance::GattDBResult::~GattDBResult() {}

FakeBluetoothInstance::LEDeviceFoundData::LEDeviceFoundData(
    mojom::BluetoothAddressPtr addr,
    int32_t rssi,
    std::vector<mojom::BluetoothAdvertisingDataPtr> adv_data,
    const std::vector<uint8_t>& eir)
    : addr_(std::move(addr)),
      rssi_(rssi),
      adv_data_(std::move(adv_data)),
      eir_(std::move(eir)) {}

FakeBluetoothInstance::LEDeviceFoundData::~LEDeviceFoundData() {}

FakeBluetoothInstance::ConnectionStateChangedData::ConnectionStateChangedData(
    mojom::BluetoothAddressPtr addr,
    device::BluetoothTransport device_type,
    bool connected)
    : addr_(std::move(addr)),
      device_type_(device_type),
      connected_(connected) {}

FakeBluetoothInstance::ConnectionStateChangedData::
    ~ConnectionStateChangedData() = default;

FakeBluetoothInstance::LEConnectionStateChangeData::LEConnectionStateChangeData(
    mojom::BluetoothAddressPtr addr,
    bool connected)
    : addr_(std::move(addr)), connected_(connected) {}

FakeBluetoothInstance::LEConnectionStateChangeData::
    ~LEConnectionStateChangeData() = default;

void FakeBluetoothInstance::InitDeprecated(mojom::BluetoothHostPtr host_ptr) {
  Init(std::move(host_ptr), base::DoNothing());
}

void FakeBluetoothInstance::Init(mojom::BluetoothHostPtr host_ptr,
                                 InitCallback callback) {
  host_ = std::move(host_ptr);
  std::move(callback).Run();
}

void FakeBluetoothInstance::OnAdapterProperties(
    mojom::BluetoothStatus status,
    std::vector<mojom::BluetoothPropertyPtr> properties) {}

void FakeBluetoothInstance::OnDeviceFound(
    std::vector<mojom::BluetoothPropertyPtr> properties) {
  device_found_data_.push_back(std::move(properties));
}

void FakeBluetoothInstance::OnDevicePropertiesChanged(
    mojom::BluetoothAddressPtr remote_addr,
    std::vector<mojom::BluetoothPropertyPtr> properties) {
  device_properties_changed_data_.push_back(std::move(properties));
}

void FakeBluetoothInstance::OnDiscoveryStateChanged(
    mojom::BluetoothDiscoveryState state) {}

void FakeBluetoothInstance::OnBondStateChanged(
    mojom::BluetoothStatus status,
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothBondState state) {}

void FakeBluetoothInstance::OnConnectionStateChanged(
    mojom::BluetoothAddressPtr remote_addr,
    device::BluetoothTransport device_type,
    bool connected) {
  connection_state_changed_data_.push_back(
      std::make_unique<ConnectionStateChangedData>(std::move(remote_addr),
                                                   device_type, connected));
}

void FakeBluetoothInstance::OnLEDeviceFoundForN(
    mojom::BluetoothAddressPtr addr,
    int32_t rssi,
    std::vector<mojom::BluetoothAdvertisingDataPtr> adv_data) {
  le_device_found_data_.push_back(std::make_unique<LEDeviceFoundData>(
      std::move(addr), rssi, std::move(adv_data), std::vector<uint8_t>()));
}

void FakeBluetoothInstance::OnLEDeviceFound(mojom::BluetoothAddressPtr addr,
                                            int32_t rssi,
                                            const std::vector<uint8_t>& eir) {
  le_device_found_data_.push_back(std::make_unique<LEDeviceFoundData>(
      std::move(addr), rssi, std::vector<mojom::BluetoothAdvertisingDataPtr>(),
      eir));
}

void FakeBluetoothInstance::OnLEConnectionStateChange(
    mojom::BluetoothAddressPtr remote_addr,
    bool connected) {
  le_connection_state_change_data_.push_back(
      std::make_unique<LEConnectionStateChangeData>(std::move(remote_addr),
                                                    connected));
}

void FakeBluetoothInstance::OnLEDeviceAddressChange(
    mojom::BluetoothAddressPtr old_addr,
    mojom::BluetoothAddressPtr new_addr) {}

void FakeBluetoothInstance::OnSearchComplete(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattStatus status) {}

void FakeBluetoothInstance::OnGetGattDB(
    mojom::BluetoothAddressPtr remote_addr,
    std::vector<mojom::BluetoothGattDBElementPtr> db) {
  gatt_db_result_.push_back(
      std::make_unique<GattDBResult>(std::move(remote_addr), std::move(db)));
}

void FakeBluetoothInstance::OnGattNotify(
    mojom::BluetoothAddressPtr remote_addr,
    mojom::BluetoothGattServiceIDPtr service_id,
    mojom::BluetoothGattIDPtr char_id,
    bool is_notify,
    const std::vector<uint8_t>& value) {}

void FakeBluetoothInstance::RequestGattRead(
    mojom::BluetoothAddressPtr address,
    int32_t attribute_handle,
    int32_t offset,
    bool is_long,
    mojom::BluetoothGattDBAttributeType attribute_type,
    RequestGattReadCallback callback) {}

void FakeBluetoothInstance::RequestGattWrite(
    mojom::BluetoothAddressPtr address,
    int32_t attribute_handle,
    int32_t offset,
    const std::vector<uint8_t>& value,
    mojom::BluetoothGattDBAttributeType attribute_type,
    bool is_prepare,
    RequestGattWriteCallback callback) {}

void FakeBluetoothInstance::RequestGattExecuteWrite(
    mojom::BluetoothAddressPtr address,
    bool execute,
    RequestGattExecuteWriteCallback callback) {}

void FakeBluetoothInstance::OnGetSdpRecords(
    mojom::BluetoothStatus status,
    mojom::BluetoothAddressPtr remote_addr,
    const device::BluetoothUUID& target_uuid,
    std::vector<mojom::BluetoothSdpRecordPtr> records) {}

void FakeBluetoothInstance::OnMTUReceived(
    mojom::BluetoothAddressPtr remote_addr,
    uint16_t mtu) {}

}  // namespace arc
