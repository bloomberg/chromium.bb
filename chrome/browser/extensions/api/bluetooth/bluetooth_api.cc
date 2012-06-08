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
#include "base/memory/ref_counted.h"
#include "base/safe_strerror_posix.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"
#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"

#include <errno.h>

using chromeos::BluetoothAdapter;
using chromeos::BluetoothDevice;

namespace {

chromeos::ExtensionBluetoothEventRouter* GetEventRouter(Profile* profile) {
  return profile->GetExtensionService()->bluetooth_event_router();
}

const chromeos::BluetoothAdapter* GetAdapter(Profile* profile) {
  return GetEventRouter(profile)->adapter();
}

chromeos::BluetoothAdapter* GetMutableAdapter(Profile* profile) {
  return GetEventRouter(profile)->GetMutableAdapter();
}

}  // namespace
#endif

namespace {

const char kFailedToConnect[] = "Connection failed";
const char kInvalidDevice[] = "Invalid device";
const char kSocketNotFoundError[] = "Socket not found: invalid socket id";

}  // namespace

namespace Connect = extensions::api::experimental_bluetooth::Connect;
namespace Disconnect = extensions::api::experimental_bluetooth::Disconnect;
namespace GetDevicesWithServiceName =
    extensions::api::experimental_bluetooth::GetDevicesWithServiceName;
namespace GetDevicesWithServiceUUID =
    extensions::api::experimental_bluetooth::GetDevicesWithServiceUUID;
namespace Read = extensions::api::experimental_bluetooth::Read;
namespace Write = extensions::api::experimental_bluetooth::Write;

namespace extensions {
namespace api {

#if defined(OS_CHROMEOS)

bool BluetoothIsAvailableFunction::RunImpl() {
  result_.reset(Value::CreateBooleanValue(GetAdapter(profile())->IsPresent()));
  return true;
}

bool BluetoothIsPoweredFunction::RunImpl() {
  result_.reset(Value::CreateBooleanValue(GetAdapter(profile())->IsPowered()));
  return true;
}

bool BluetoothGetAddressFunction::RunImpl() {
  result_.reset(Value::CreateStringValue(GetAdapter(profile())->address()));
  return true;
}

bool BluetoothGetDevicesWithServiceUUIDFunction::RunImpl() {
  scoped_ptr<GetDevicesWithServiceUUID::Params> params(
      GetDevicesWithServiceUUID::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  const BluetoothAdapter::ConstDeviceList& devices =
      GetAdapter(profile())->GetDevices();
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

  ListValue* matches = new ListValue;
  result_.reset(matches);

  BluetoothAdapter::DeviceList devices =
      GetMutableAdapter(profile())->GetDevices();
  if (devices.empty()) {
    SendResponse(true);
    return true;
  }

  callbacks_pending_ = devices.size();
  AddRef();  // Released in AddDeviceIfTrue when callbacks_pending_ == 0

  scoped_ptr<GetDevicesWithServiceName::Params> params(
      GetDevicesWithServiceName::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  for (BluetoothAdapter::DeviceList::iterator i = devices.begin();
      i != devices.end(); ++i) {
    (*i)->ProvidesServiceWithName(params->name,
        base::Bind(&BluetoothGetDevicesWithServiceNameFunction::AddDeviceIfTrue,
                   this,
                   matches,
                   *i));
  }

  return true;
}

void BluetoothConnectFunction::ConnectToServiceCallback(
    const chromeos::BluetoothDevice* device,
    const std::string& service_uuid,
    scoped_refptr<chromeos::BluetoothSocket> socket) {
  if (socket.get()) {
    int socket_id = GetEventRouter(profile())->RegisterSocket(socket);

    experimental_bluetooth::Socket result_socket;
    result_socket.device.address = device->address();
    result_socket.device.name = UTF16ToUTF8(device->GetName());
    result_socket.service_uuid = service_uuid;
    result_socket.id = socket_id;
    result_.reset(result_socket.ToValue().release());
    SendResponse(true);
  } else {
    SetError(kFailedToConnect);
    SendResponse(false);
  }

  Release();  // Added in RunImpl
}

bool BluetoothConnectFunction::RunImpl() {
  scoped_ptr<Connect::Params> params(Connect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  chromeos::BluetoothDevice* device =
      GetMutableAdapter(profile())->GetDevice(params->device.address);
  if (!device) {
    SendResponse(false);
    SetError(kInvalidDevice);
    return false;
  }

  AddRef();
  device->ConnectToService(params->service,
      base::Bind(&BluetoothConnectFunction::ConnectToServiceCallback,
                 this,
                 device,
                 params->service));
  return true;
}

bool BluetoothDisconnectFunction::RunImpl() {
  scoped_ptr<Disconnect::Params> params(Disconnect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  return GetEventRouter(profile())->ReleaseSocket(params->socket.id);
}

bool BluetoothReadFunction::Prepare() {
  scoped_ptr<Read::Params> params(Read::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  socket_ = GetEventRouter(profile())->GetSocket(params->socket.id);
  if (socket_.get() == NULL) {
    SetError(kSocketNotFoundError);
    return false;
  }

  success_ = false;
  return true;
}

void BluetoothReadFunction::Work() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  CHECK(socket_.get() != NULL);

  char* all_bytes = NULL;
  ssize_t buffer_size = 0;
  ssize_t total_bytes_read = 0;
  int errsv;
  while (true) {
    buffer_size += 1024;
    all_bytes = static_cast<char*>(realloc(all_bytes, buffer_size));
    CHECK(all_bytes) << "Failed to grow Bluetooth socket buffer";

    // bluetooth sockets are non-blocking, so read until we hit an error
    ssize_t bytes_read = read(socket_->fd(), all_bytes + total_bytes_read,
        buffer_size - total_bytes_read);
    errsv = errno;
    if (bytes_read <= 0)
      break;

    total_bytes_read += bytes_read;
  }

  if (total_bytes_read > 0) {
    success_ = true;
    result_.reset(base::BinaryValue::Create(all_bytes, total_bytes_read));
  } else {
    success_ = (errsv == EAGAIN || errsv == EWOULDBLOCK);
    free(all_bytes);
  }

  if (!success_)
    SetError(safe_strerror(errsv));
}

bool BluetoothReadFunction::Respond() {
  return success_;
}

bool BluetoothWriteFunction::Prepare() {
  // TODO(bryeung): update to new-style parameter passing when ArrayBuffer
  // support is added
  DictionaryValue* socket;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &socket));
  int socket_id;
  EXTENSION_FUNCTION_VALIDATE(socket->GetInteger("id", &socket_id));

  socket_ = GetEventRouter(profile())->GetSocket(socket_id);
  if (socket_.get() == NULL) {
    SetError(kSocketNotFoundError);
    return false;
  }

  base::BinaryValue* tmp_data;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(1, &tmp_data));
  data_to_write_ = tmp_data;

  success_ = false;
  return socket_.get() != NULL;
}

void BluetoothWriteFunction::Work() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (socket_.get() == NULL)
    return;

  ssize_t bytes_written = write(socket_->fd(),
      data_to_write_->GetBuffer(), data_to_write_->GetSize());
  int errsv = errno;

  if (bytes_written > 0) {
    result_.reset(Value::CreateIntegerValue(bytes_written));
    success_ = true;
  } else {
    result_.reset(0);
    success_ = (errsv == EAGAIN || errsv == EWOULDBLOCK);
  }

  if (!success_)
    SetError(safe_strerror(errsv));
}

bool BluetoothWriteFunction::Respond() {
  return success_;
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

bool BluetoothConnectFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothDisconnectFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothReadFunction::Prepare() {
  return true;
}

void BluetoothReadFunction::Work() {
}

bool BluetoothReadFunction::Respond() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothWriteFunction::Prepare() {
  return true;
}

void BluetoothWriteFunction::Work() {
}

bool BluetoothWriteFunction::Respond() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

#endif

BluetoothReadFunction::BluetoothReadFunction() {}
BluetoothReadFunction::~BluetoothReadFunction() {}

BluetoothWriteFunction::BluetoothWriteFunction() {}
BluetoothWriteFunction::~BluetoothWriteFunction() {}

bool BluetoothSetOutOfBandPairingDataFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothGetOutOfBandPairingDataFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

}  // namespace api
}  // namespace extensions
