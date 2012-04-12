// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/experimental.bluetooth.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"

using chromeos::BluetoothAdapter;
using chromeos::BluetoothDevice;
#endif

namespace GetDevicesWithService =
    extensions::api::experimental_bluetooth::GetDevicesWithService;

namespace extensions {
namespace api {

#if defined(OS_CHROMEOS)
const chromeos::BluetoothAdapter* BluetoothExtensionFunction::adapter() const {
  return profile()->GetExtensionService()->bluetooth_event_router()->adapter();
}

bool BluetoothIsAvailableFunction::RunImpl() {
  result_.reset(Value::CreateBooleanValue(adapter()->IsPresent()));
  return true;
}

bool BluetoothIsPoweredFunction::RunImpl() {
  result_.reset(Value::CreateBooleanValue(adapter()->IsPowered()));
  return true;
}

bool BluetoothGetAddressFunction::RunImpl() {
  result_.reset(Value::CreateStringValue(adapter()->address()));
  return true;
}

bool BluetoothGetDevicesWithServiceFunction::RunImpl() {
  scoped_ptr<GetDevicesWithService::Params> params(
      GetDevicesWithService::Params::Create(*args_));

  BluetoothAdapter::ConstDeviceList devices = adapter()->GetDevices();

  ListValue* matches = new ListValue;
  for (BluetoothAdapter::ConstDeviceList::const_iterator i =
      devices.begin(); i != devices.end(); ++i) {
    const BluetoothDevice::ServiceList& services = (*i)->GetServices();
    for (BluetoothDevice::ServiceList::const_iterator j = services.begin();
        j != services.end(); ++j) {
      if (*j == params->service) {
        experimental_bluetooth::Device device;
        device.name = UTF16ToUTF8((*i)->GetName());
        device.address = (*i)->address();
        matches->Append(device.ToValue().release());
      }
    }
  }

  result_.reset(matches);
  return true;
}

#else

// -----------------------------------------------------------------------------
// NIY stubs
// -----------------------------------------------------------------------------
bool BluetoothIsAvailableFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothIsPoweredFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothGetAddressFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothGetDevicesWithServiceFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

#endif

bool BluetoothDisconnectFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothReadFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothSetOutOfBandPairingDataFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothGetOutOfBandPairingDataFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothWriteFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothConnectFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

}  // namespace api
}  // namespace extensions
