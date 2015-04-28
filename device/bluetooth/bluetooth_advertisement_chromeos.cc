// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_advertisement_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/bluetooth_le_advertising_manager_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

void UnregisterFailure(device::BluetoothAdvertisement::ErrorCode error) {
  LOG(ERROR)
      << "BluetoothAdvertisementChromeOS::Unregister failed with error code = "
      << error;
}

void ErrorCallbackConnector(
    const device::BluetoothAdapter::CreateAdvertisementErrorCallback&
        error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << "Error while registering advertisement. error_name = "
               << error_name << ", error_message = " << error_message;
  device::BluetoothAdvertisement::ErrorCode error_code;
  if (error_name == bluetooth_advertising_manager::kErrorFailed ||
      error_name == bluetooth_advertising_manager::kErrorAlreadyExists) {
    error_code = device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_ALREADY_EXISTS;
  } else if (error_name ==
             bluetooth_advertising_manager::kErrorInvalidArguments) {
    error_code = device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_INVALID_LENGTH;
  } else if (error_name == bluetooth_advertising_manager::kErrorDoesNotExist) {
    error_code = device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_DOES_NOT_EXIST;
  }

  error_callback.Run(error_code);
}

}  // namespace

namespace chromeos {

BluetoothAdvertisementChromeOS::BluetoothAdvertisementChromeOS(
    scoped_ptr<device::BluetoothAdvertisement::Data> data,
    scoped_refptr<BluetoothAdapterChromeOS> adapter)
    : adapter_(adapter) {
  dbus::ObjectPath advertisement_object_path = dbus::ObjectPath(
      "/org/chromium/bluetooth_advertisement/" + base::GenerateGUID());
  DCHECK(DBusThreadManager::Get());
  provider_ = BluetoothLEAdvertisementServiceProvider::Create(
      DBusThreadManager::Get()->GetSystemBus(), advertisement_object_path, this,
      static_cast<BluetoothLEAdvertisementServiceProvider::AdvertisementType>(
          data->type()),
      data->service_uuids().Pass(), data->manufacturer_data().Pass(),
      data->solicit_uuids().Pass(), data->service_data().Pass());
}

void BluetoothAdvertisementChromeOS::Register(
    const base::Closure& success_callback,
    const device::BluetoothAdapter::CreateAdvertisementErrorCallback&
        error_callback) {
  DCHECK(DBusThreadManager::Get());
  DBusThreadManager::Get()
      ->GetBluetoothLEAdvertisingManagerClient()
      ->RegisterAdvertisement(
          adapter_->object_path(), provider_->object_path(), success_callback,
          base::Bind(&ErrorCallbackConnector, error_callback));
}

BluetoothAdvertisementChromeOS::~BluetoothAdvertisementChromeOS() {
  Unregister(base::Bind(&base::DoNothing), base::Bind(&UnregisterFailure));
}

void BluetoothAdvertisementChromeOS::Unregister(
    const SuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  // If we don't have a provider, that means we have already been unregistered,
  // return an error.
  if (!provider_) {
    error_callback.Run(device::BluetoothAdvertisement::ErrorCode::
                           ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
    return;
  }

  DCHECK(DBusThreadManager::Get());
  DBusThreadManager::Get()
      ->GetBluetoothLEAdvertisingManagerClient()
      ->UnregisterAdvertisement(
          adapter_->object_path(), provider_->object_path(), success_callback,
          base::Bind(&ErrorCallbackConnector, error_callback));
  provider_.reset();
}

void BluetoothAdvertisementChromeOS::Released() {
  LOG(WARNING) << "Advertisement released.";
  provider_.reset();
  FOR_EACH_OBSERVER(BluetoothAdvertisement::Observer, observers_,
                    AdvertisementReleased(this));
}

}  // namespace chromeos
