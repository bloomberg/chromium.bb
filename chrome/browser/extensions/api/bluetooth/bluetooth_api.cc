// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"
#endif

namespace extensions {
namespace api {

#if defined(OS_CHROMEOS)
bool BluetoothIsAvailableFunction::RunImpl() {
  const chromeos::BluetoothAdapter *adapter =
      profile()->GetExtensionService()->bluetooth_event_router()->adapter();
  result_.reset(Value::CreateBooleanValue(adapter->IsPresent()));
  return true;
}

bool BluetoothIsPoweredFunction::RunImpl() {
  const chromeos::BluetoothAdapter *adapter =
      profile()->GetExtensionService()->bluetooth_event_router()->adapter();
  result_.reset(Value::CreateBooleanValue(adapter->IsPowered()));
  return true;
}

bool BluetoothGetAddressFunction::RunImpl() {
  const chromeos::BluetoothAdapter *adapter =
      profile()->GetExtensionService()->bluetooth_event_router()->adapter();
  result_.reset(Value::CreateStringValue(adapter->address()));
  return false;
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

bool BluetoothGetDevicesWithServiceFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

}  // namespace api
}  // namespace extensions
