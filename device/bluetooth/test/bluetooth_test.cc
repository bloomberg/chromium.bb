// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

const std::string BluetoothTestBase::kTestAdapterName = "FakeBluetoothAdapter";
const std::string BluetoothTestBase::kTestAdapterAddress = "A1:B2:C3:D4:E5:F6";

const std::string BluetoothTestBase::kTestDeviceName = "FakeBluetoothDevice";
const std::string BluetoothTestBase::kTestDeviceNameEmpty = "";

const std::string BluetoothTestBase::kTestDeviceAddress1 = "01:00:00:90:1E:BE";
const std::string BluetoothTestBase::kTestDeviceAddress2 = "02:00:00:8B:74:63";

const std::string BluetoothTestBase::kTestUUIDGenericAccess = "1800";
const std::string BluetoothTestBase::kTestUUIDGenericAttribute = "1801";
const std::string BluetoothTestBase::kTestUUIDImmediateAlert = "1802";
const std::string BluetoothTestBase::kTestUUIDLinkLoss = "1803";

BluetoothTestBase::BluetoothTestBase() : weak_factory_(this) {}

BluetoothTestBase::~BluetoothTestBase() {
}

void BluetoothTestBase::StartLowEnergyDiscoverySession() {
  adapter_->StartDiscoverySessionWithFilter(
      make_scoped_ptr(new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE)),
      GetDiscoverySessionCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
}

BluetoothDevice* BluetoothTestBase::DiscoverLowEnergyDevice(
    int device_ordinal) {
  NOTIMPLEMENTED();
  return nullptr;
}

void BluetoothTestBase::DeleteDevice(BluetoothDevice* device) {
  adapter_->DeleteDeviceForTesting(device->GetAddress());
}

void BluetoothTestBase::Callback() {
  ++callback_count_;
}

void BluetoothTestBase::DiscoverySessionCallback(
    scoped_ptr<BluetoothDiscoverySession> discovery_session) {
  ++callback_count_;
  discovery_sessions_.push_back(discovery_session.release());
}

void BluetoothTestBase::GattConnectionCallback(
    scoped_ptr<BluetoothGattConnection> connection) {
  ++callback_count_;
  gatt_connections_.push_back(connection.release());
}

void BluetoothTestBase::ErrorCallback() {
  ++error_callback_count_;
}

void BluetoothTestBase::ConnectErrorCallback(
    enum BluetoothDevice::ConnectErrorCode error_code) {
  ++error_callback_count_;
  last_connect_error_code_ = error_code;
}

base::Closure BluetoothTestBase::GetCallback() {
  return base::Bind(&BluetoothTestBase::Callback, weak_factory_.GetWeakPtr());
}

BluetoothAdapter::DiscoverySessionCallback
BluetoothTestBase::GetDiscoverySessionCallback() {
  return base::Bind(&BluetoothTestBase::DiscoverySessionCallback,
                    weak_factory_.GetWeakPtr());
}

BluetoothDevice::GattConnectionCallback
BluetoothTestBase::GetGattConnectionCallback() {
  return base::Bind(&BluetoothTestBase::GattConnectionCallback,
                    weak_factory_.GetWeakPtr());
}

BluetoothAdapter::ErrorCallback BluetoothTestBase::GetErrorCallback() {
  return base::Bind(&BluetoothTestBase::ErrorCallback,
                    weak_factory_.GetWeakPtr());
}

BluetoothDevice::ConnectErrorCallback
BluetoothTestBase::GetConnectErrorCallback() {
  return base::Bind(&BluetoothTestBase::ConnectErrorCallback,
                    weak_factory_.GetWeakPtr());
}

void BluetoothTestBase::ResetEventCounts() {
  last_connect_error_code_ = BluetoothDevice::ERROR_UNKNOWN;
  callback_count_ = 0;
  error_callback_count_ = 0;
  gatt_connection_attempts_ = 0;
  gatt_disconnection_attempts_ = 0;
  gatt_discovery_attempts_ = 0;
}

}  // namespace device
