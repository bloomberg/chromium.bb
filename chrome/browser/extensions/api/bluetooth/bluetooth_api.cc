// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/experimental_bluetooth.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"

using chromeos::BluetoothAdapter;
using chromeos::BluetoothDevice;

#endif

namespace GetDevicesWithServiceUUID =
    extensions::api::experimental_bluetooth::GetDevicesWithServiceUUID;
namespace GetDevicesWithServiceName =
    extensions::api::experimental_bluetooth::GetDevicesWithServiceName;

namespace extensions {
namespace api {

#if defined(OS_CHROMEOS)

const chromeos::BluetoothAdapter* BluetoothExtensionFunction::adapter() const {
  return profile()->GetExtensionService()->bluetooth_event_router()->adapter();
}

chromeos::BluetoothAdapter* BluetoothExtensionFunction::GetMutableAdapter() {
  return profile()->GetExtensionService()->bluetooth_event_router()->
      GetMutableAdapter();
}

const chromeos::BluetoothAdapter*
AsyncBluetoothExtensionFunction::adapter() const {
  return profile()->GetExtensionService()->bluetooth_event_router()->adapter();
}

chromeos::BluetoothAdapter*
AsyncBluetoothExtensionFunction::GetMutableAdapter() {
  return profile()->GetExtensionService()->bluetooth_event_router()->
      GetMutableAdapter();
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

bool BluetoothGetDevicesWithServiceUUIDFunction::RunImpl() {
  scoped_ptr<GetDevicesWithServiceUUID::Params> params(
      GetDevicesWithServiceUUID::Params::Create(*args_));

  const BluetoothAdapter::ConstDeviceList& devices = adapter()->GetDevices();
  ListValue* matches = new ListValue;
  for (BluetoothAdapter::ConstDeviceList::const_iterator i =
      devices.begin(); i != devices.end(); ++i) {
    if ((*i)->ProvidesServiceWithUUID(params->uuid)) {
      experimental_bluetooth::Device device;
      device.name = UTF16ToUTF8((*i)->GetName());
      device.address = (*i)->address();
      matches->Append(device.ToValue().release());
    }
  }
  result_.reset(matches);

  return true;
}

BluetoothGetDevicesWithServiceNameFunction::
    BluetoothGetDevicesWithServiceNameFunction() : callbacks_pending_(0) {}

void BluetoothGetDevicesWithServiceNameFunction::AddDeviceIfTrue(
    ListValue* list, const BluetoothDevice* device, bool result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (result) {
    experimental_bluetooth::Device device_result;
    device_result.name = UTF16ToUTF8(device->GetName());
    device_result.address = device->address();
    list->Append(device_result.ToValue().release());
  }

  callbacks_pending_--;
  if (callbacks_pending_ == 0) {
    SendResponse(true);
    Release();  // Added in RunImpl
  }
}

bool BluetoothGetDevicesWithServiceNameFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<GetDevicesWithServiceName::Params> params(
      GetDevicesWithServiceName::Params::Create(*args_));

  ListValue* matches = new ListValue;
  result_.reset(matches);

  BluetoothAdapter::DeviceList devices = GetMutableAdapter()->GetDevices();
  callbacks_pending_ = 0;
  for (BluetoothAdapter::DeviceList::iterator i = devices.begin();
      i != devices.end(); ++i) {
    (*i)->ProvidesServiceWithName(params->name,
        base::Bind(&BluetoothGetDevicesWithServiceNameFunction::AddDeviceIfTrue,
                   this,
                   matches,
                   *i));
    callbacks_pending_++;
  }

  if (callbacks_pending_)
    AddRef();  // Released in AddDeviceIfTrue when callbacks_pending_ == 0
  else
    SendResponse(true);

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

bool BluetoothGetDevicesWithServiceUUIDFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothGetDevicesWithServiceNameFunction::RunImpl() {
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
