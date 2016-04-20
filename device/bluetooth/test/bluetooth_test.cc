// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
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
      base::WrapUnique(new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE)),
      GetDiscoverySessionCallback(Call::EXPECTED),
      GetErrorCallback(Call::NOT_EXPECTED));
  base::RunLoop().RunUntilIdle();
}

void BluetoothTestBase::StartLowEnergyDiscoverySessionExpectedToFail() {
  adapter_->StartDiscoverySessionWithFilter(
      base::WrapUnique(new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE)),
      GetDiscoverySessionCallback(Call::NOT_EXPECTED),
      GetErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();
}

void BluetoothTestBase::TearDown() {
  EXPECT_EQ(expected_success_callback_calls_, actual_success_callback_calls_);
  EXPECT_EQ(expected_error_callback_calls_, actual_error_callback_calls_);
  EXPECT_FALSE(unexpected_success_callback_);
  EXPECT_FALSE(unexpected_error_callback_);
}

bool BluetoothTestBase::DenyPermission() {
  return false;
}

BluetoothDevice* BluetoothTestBase::DiscoverLowEnergyDevice(
    int device_ordinal) {
  NOTIMPLEMENTED();
  return nullptr;
}

void BluetoothTestBase::DeleteDevice(BluetoothDevice* device) {
  adapter_->DeleteDeviceForTesting(device->GetAddress());
}

void BluetoothTestBase::Callback(Call expected) {
  ++callback_count_;

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::DiscoverySessionCallback(
    Call expected,
    std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
  ++callback_count_;
  discovery_sessions_.push_back(discovery_session.release());

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::GattConnectionCallback(
    Call expected,
    std::unique_ptr<BluetoothGattConnection> connection) {
  ++callback_count_;
  gatt_connections_.push_back(connection.release());

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::NotifyCallback(
    Call expected,
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  ++callback_count_;
  notify_sessions_.push_back(notify_session.release());

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::ReadValueCallback(Call expected,
                                          const std::vector<uint8_t>& value) {
  ++callback_count_;
  last_read_value_ = value;

  if (expected == Call::EXPECTED)
    ++actual_success_callback_calls_;
  else
    unexpected_success_callback_ = true;
}

void BluetoothTestBase::ErrorCallback(Call expected) {
  ++error_callback_count_;

  if (expected == Call::EXPECTED)
    ++actual_error_callback_calls_;
  else
    unexpected_error_callback_ = true;
}

void BluetoothTestBase::ConnectErrorCallback(
    Call expected,
    enum BluetoothDevice::ConnectErrorCode error_code) {
  ++error_callback_count_;
  last_connect_error_code_ = error_code;

  if (expected == Call::EXPECTED)
    ++actual_error_callback_calls_;
  else
    unexpected_error_callback_ = true;
}

void BluetoothTestBase::GattErrorCallback(
    Call expected,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  ++error_callback_count_;
  last_gatt_error_code_ = error_code;

  if (expected == Call::EXPECTED)
    ++actual_error_callback_calls_;
  else
    unexpected_error_callback_ = true;
}

base::Closure BluetoothTestBase::GetCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::Callback, weak_factory_.GetWeakPtr(),
                    expected);
}

BluetoothAdapter::DiscoverySessionCallback
BluetoothTestBase::GetDiscoverySessionCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::DiscoverySessionCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothDevice::GattConnectionCallback
BluetoothTestBase::GetGattConnectionCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::GattConnectionCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothRemoteGattCharacteristic::NotifySessionCallback
BluetoothTestBase::GetNotifyCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::NotifyCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothRemoteGattCharacteristic::ValueCallback
BluetoothTestBase::GetReadValueCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_success_callback_calls_;
  return base::Bind(&BluetoothTestBase::ReadValueCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothAdapter::ErrorCallback BluetoothTestBase::GetErrorCallback(
    Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_error_callback_calls_;
  return base::Bind(&BluetoothTestBase::ErrorCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

BluetoothDevice::ConnectErrorCallback
BluetoothTestBase::GetConnectErrorCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_error_callback_calls_;
  return base::Bind(&BluetoothTestBase::ConnectErrorCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

base::Callback<void(BluetoothRemoteGattService::GattErrorCode)>
BluetoothTestBase::GetGattErrorCallback(Call expected) {
  if (expected == Call::EXPECTED)
    ++expected_error_callback_calls_;
  return base::Bind(&BluetoothTestBase::GattErrorCallback,
                    weak_factory_.GetWeakPtr(), expected);
}

void BluetoothTestBase::ResetEventCounts() {
  last_connect_error_code_ = BluetoothDevice::ERROR_UNKNOWN;
  callback_count_ = 0;
  error_callback_count_ = 0;
  gatt_connection_attempts_ = 0;
  gatt_disconnection_attempts_ = 0;
  gatt_discovery_attempts_ = 0;
  gatt_notify_characteristic_attempts_ = 0;
  gatt_read_characteristic_attempts_ = 0;
  gatt_write_characteristic_attempts_ = 0;
  gatt_read_descriptor_attempts_ = 0;
  gatt_write_descriptor_attempts_ = 0;
}

}  // namespace device
