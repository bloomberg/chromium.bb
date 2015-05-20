// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider.h"

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothDiscoverySession;
using device::BluetoothUUID;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothDiscoverySession;
using testing::Invoke;
using testing::Return;
using testing::NiceMock;
using testing::_;

namespace {
// Invokes Run() on the k-th argument of the function with no arguments.
ACTION_TEMPLATE(RunCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  return ::testing::get<k>(args).Run();
}

// Invokes Run() on the k-th argument of the function with the result
// of |func| as an argument.
ACTION_TEMPLATE(RunCallbackWithResult,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(func)) {
  return ::testing::get<k>(args).Run(func());
}
}

namespace content {

// static
scoped_refptr<BluetoothAdapter>
LayoutTestBluetoothAdapterProvider::GetBluetoothAdapter(
    const std::string& fake_adapter_name) {
  // TODO(ortuno): Remove RejectRequestDevice once LayoutTests are modified
  if (fake_adapter_name == "RejectRequestDevice_NotFoundError" ||
      fake_adapter_name == "EmptyAdapter") {
    return GetEmptyAdapter();
  }
  // TODO(ortuno): Remove "Single Empty Device" once LayoutTests are modified
  else if (fake_adapter_name == "Single Empty Device" ||
           fake_adapter_name == "SingleEmptyDeviceAdapter") {
    return GetSingleEmptyDeviceAdapter();
  } else if (fake_adapter_name == "") {
    return NULL;
  }
  NOTREACHED();
  return NULL;
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetEmptyAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(
      new NiceMock<MockBluetoothAdapter>());

  ON_CALL(*adapter, StartDiscoverySession(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          []() { return GetDiscoverySession(); }));

  // Using Invoke allows the adapter returned from this method to be futher
  // modified and have devices added to it. The call to ::GetDevices will
  // invoke ::GetConstMockDevices, returning all devices added up to that time.
  ON_CALL(*adapter, GetDevices())
      .WillByDefault(
          Invoke(adapter.get(), &MockBluetoothAdapter::GetConstMockDevices));

  return adapter.Pass();
}

// static
scoped_refptr<NiceMock<MockBluetoothAdapter>>
LayoutTestBluetoothAdapterProvider::GetSingleEmptyDeviceAdapter() {
  scoped_refptr<NiceMock<MockBluetoothAdapter>> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetEmptyDevice(adapter.get()));

  return adapter.Pass();
}

// static
scoped_ptr<NiceMock<MockBluetoothDevice>>
LayoutTestBluetoothAdapterProvider::GetEmptyDevice(
    MockBluetoothAdapter* adapter) {
  scoped_ptr<NiceMock<MockBluetoothDevice>> empty_device(
      new NiceMock<MockBluetoothDevice>(
          adapter, 0x1F00 /* Bluetooth Class */, "Empty Mock Device name",
          "Empty Mock Device instanceID", true /* Paired */,
          true /* Connected */));

  ON_CALL(*empty_device, GetVendorIDSource())
      .WillByDefault(Return(BluetoothDevice::VENDOR_ID_BLUETOOTH));
  ON_CALL(*empty_device, GetVendorID()).WillByDefault(Return(0xFFFF));
  ON_CALL(*empty_device, GetProductID()).WillByDefault(Return(1));
  ON_CALL(*empty_device, GetDeviceID()).WillByDefault(Return(2));

  BluetoothDevice::UUIDList list;
  list.push_back(BluetoothUUID("1800"));
  list.push_back(BluetoothUUID("1801"));
  ON_CALL(*empty_device, GetUUIDs()).WillByDefault(Return(list));
  return empty_device.Pass();
}

// static
scoped_ptr<NiceMock<MockBluetoothDiscoverySession>>
LayoutTestBluetoothAdapterProvider::GetDiscoverySession() {
  scoped_ptr<NiceMock<MockBluetoothDiscoverySession>> discovery_session(
      new NiceMock<MockBluetoothDiscoverySession>());

  ON_CALL(*discovery_session, Stop(_, _))
      .WillByDefault(RunCallback<0 /* success_callback */>());

  return discovery_session.Pass();
}

}  // namespace content
