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

BluetoothTestBase::BluetoothTestBase() {
}

BluetoothTestBase::~BluetoothTestBase() {
}

void BluetoothTestBase::Callback() {
  ++callback_count_;
}

void BluetoothTestBase::DiscoverySessionCallback(
    scoped_ptr<BluetoothDiscoverySession> discovery_session) {
  ++callback_count_;
  discovery_sessions_.push_back(discovery_session.release());
}

void BluetoothTestBase::ErrorCallback() {
  ++error_callback_count_;
}

base::Closure BluetoothTestBase::GetCallback() {
  return base::Bind(&BluetoothTestBase::Callback, base::Unretained(this));
}

BluetoothAdapter::DiscoverySessionCallback
BluetoothTestBase::GetDiscoverySessionCallback() {
  return base::Bind(&BluetoothTestBase::DiscoverySessionCallback,
                    base::Unretained(this));
}

BluetoothAdapter::ErrorCallback BluetoothTestBase::GetErrorCallback() {
  return base::Bind(&BluetoothTestBase::ErrorCallback, base::Unretained(this));
}

}  // namespace device
