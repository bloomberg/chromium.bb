// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"

#if defined(OS_CHROMEOS)
#include <errno.h>
#endif

#include <string>

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/experimental_bluetooth.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "base/memory/ref_counted.h"
#include "base/safe_strerror_posix.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"
#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"
#include "chromeos/dbus/bluetooth_out_of_band_client.h"

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

const char kCouldNotGetLocalOutOfBandPairingData[] =
    "Could not get local Out Of Band Pairing Data";
const char kCouldNotSetOutOfBandPairingData[] =
    "Could not set Out Of Band Pairing Data";
const char kFailedToConnect[] = "Connection failed";
const char kInvalidDevice[] = "Invalid device";
const char kServiceDiscoveryFailed[] = "Service discovery failed";
const char kSocketNotFoundError[] = "Socket not found: invalid socket id";
const char kStartDiscoveryFailed[] =
    "Starting discovery failed, or already discovering";
const char kStopDiscoveryFailed[] = "Failed to stop discovery";

}  // namespace

namespace Connect = extensions::api::experimental_bluetooth::Connect;
namespace Disconnect = extensions::api::experimental_bluetooth::Disconnect;
namespace GetDevices = extensions::api::experimental_bluetooth::GetDevices;
namespace GetServices = extensions::api::experimental_bluetooth::GetServices;
namespace Read = extensions::api::experimental_bluetooth::Read;
namespace SetOutOfBandPairingData =
    extensions::api::experimental_bluetooth::SetOutOfBandPairingData;
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

BluetoothGetDevicesFunction::BluetoothGetDevicesFunction()
    : callbacks_pending_(0) {}

void BluetoothGetDevicesFunction::AddDeviceIfTrueCallback(
    ListValue* list,
    const chromeos::BluetoothDevice* device,
    bool shouldAdd) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (shouldAdd)
    list->Append(experimental_bluetooth::BluetoothDeviceToValue(*device));

  callbacks_pending_--;
  if (callbacks_pending_ == -1) {
    SendResponse(true);
    Release();  // Added in RunImpl
  }
}

bool BluetoothGetDevicesFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<GetDevices::Params> params(GetDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const experimental_bluetooth::GetDevicesOptions& options = params->options;

  ListValue* matches = new ListValue;
  result_.reset(matches);

  CHECK_EQ(0, callbacks_pending_);

  // This will be released when callbacks_pending_ == -1.  The count is checked
  // for -1 because of the extra decrement after the for-loop, which ensures
  // that all requests have been made before the Release happens.
  AddRef();

  chromeos::BluetoothAdapter::DeviceList devices =
      GetMutableAdapter(profile())->GetDevices();
  for (chromeos::BluetoothAdapter::DeviceList::iterator i = devices.begin();
      i != devices.end(); ++i) {
    chromeos::BluetoothDevice* device = *i;

    if (options.uuid.get() != NULL &&
        !(device->ProvidesServiceWithUUID(*(options.uuid))))
      continue;

    if (options.name.get() == NULL) {
      matches->Append(experimental_bluetooth::BluetoothDeviceToValue(*device));
      continue;
    }

    callbacks_pending_++;
    device->ProvidesServiceWithName(
        *(options.name),
        base::Bind(&BluetoothGetDevicesFunction::AddDeviceIfTrueCallback,
                   this,
                   matches,
                   device));
  }
  callbacks_pending_--;

  if (callbacks_pending_ == -1) {
    SendResponse(true);
    Release();
  }

  return true;
}

void BluetoothGetServicesFunction::GetServiceRecordsCallback(
    base::ListValue* services,
    const chromeos::BluetoothDevice::ServiceRecordList& records) {
  for (chromeos::BluetoothDevice::ServiceRecordList::const_iterator i =
      records.begin(); i != records.end(); ++i) {
    const chromeos::BluetoothServiceRecord& record = **i;
    experimental_bluetooth::ServiceRecord api_record;
    api_record.name = record.name();
    if (!record.uuid().empty())
      api_record.uuid.reset(new std::string(record.uuid()));
    services->Append(api_record.ToValue().release());
  }

  SendResponse(true);
  Release();  // Added in RunImpl
}

void BluetoothGetServicesFunction::OnErrorCallback() {
  SetError(kServiceDiscoveryFailed);
  SendResponse(false);
  Release();  // Added in RunImpl
}

bool BluetoothGetServicesFunction::RunImpl() {
  scoped_ptr<GetServices::Params> params(GetServices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const experimental_bluetooth::GetServicesOptions& options = params->options;

  chromeos::BluetoothDevice* device =
      GetMutableAdapter(profile())->GetDevice(options.device_address);
  if (!device) {
    SendResponse(false);
    SetError(kInvalidDevice);
    return false;
  }

  ListValue* services = new ListValue;
  result_.reset(services);

  AddRef();  // Released in GetServiceRecordsCallback or OnErrorCallback
  device->GetServiceRecords(
      base::Bind(&BluetoothGetServicesFunction::GetServiceRecordsCallback,
                 this,
                 services),
      base::Bind(&BluetoothGetServicesFunction::OnErrorCallback,
                 this));

  return true;
}

void BluetoothConnectFunction::ConnectToServiceCallback(
    const chromeos::BluetoothDevice* device,
    const std::string& service_uuid,
    scoped_refptr<chromeos::BluetoothSocket> socket) {
  if (socket.get()) {
    int socket_id = GetEventRouter(profile())->RegisterSocket(socket);

    experimental_bluetooth::Socket result_socket;
    experimental_bluetooth::BluetoothDeviceToApiDevice(
        *device, &result_socket.device);
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
  const experimental_bluetooth::ConnectOptions& options = params->options;

  chromeos::BluetoothDevice* device =
      GetMutableAdapter(profile())->GetDevice(options.device_address);
  if (!device) {
    SendResponse(false);
    SetError(kInvalidDevice);
    return false;
  }

  AddRef();  // Released in ConnectToServiceCallback
  device->ConnectToService(options.service_uuid,
      base::Bind(&BluetoothConnectFunction::ConnectToServiceCallback,
                 this,
                 device,
                 options.service_uuid));
  return true;
}

bool BluetoothDisconnectFunction::RunImpl() {
  scoped_ptr<Disconnect::Params> params(Disconnect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const experimental_bluetooth::DisconnectOptions& options = params->options;
  return GetEventRouter(profile())->ReleaseSocket(options.socket_id);
}

bool BluetoothReadFunction::Prepare() {
  scoped_ptr<Read::Params> params(Read::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const experimental_bluetooth::ReadOptions& options = params->options;

  socket_ = GetEventRouter(profile())->GetSocket(options.socket_id);
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
  DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));
  int socket_id;
  EXTENSION_FUNCTION_VALIDATE(options->GetInteger("socketId", &socket_id));

  socket_ = GetEventRouter(profile())->GetSocket(socket_id);
  if (socket_.get() == NULL) {
    SetError(kSocketNotFoundError);
    return false;
  }

  base::BinaryValue* tmp_data;
  EXTENSION_FUNCTION_VALIDATE(options->GetBinary("data", &tmp_data));
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

void BluetoothSetOutOfBandPairingDataFunction::OnSuccessCallback() {
  SendResponse(true);
  Release();  // Added in RunImpl
}

void BluetoothSetOutOfBandPairingDataFunction::OnErrorCallback() {
  SetError(kCouldNotSetOutOfBandPairingData);
  SendResponse(false);
  Release();  // Added in RunImpl
}

bool BluetoothSetOutOfBandPairingDataFunction::RunImpl() {
  // TODO(bryeung): update to new-style parameter passing when ArrayBuffer
  // support is added
  DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));
  std::string address;
  EXTENSION_FUNCTION_VALIDATE(options->GetString("deviceAddress", &address));

  chromeos::BluetoothDevice* device =
      GetMutableAdapter(profile())->GetDevice(address);
  if (!device) {
    SendResponse(false);
    SetError(kInvalidDevice);
    return false;
  }

  AddRef();  // Released in OnSuccessCallback or OnErrorCallback
  if (options->HasKey("data")) {
    DictionaryValue* data_in;
    EXTENSION_FUNCTION_VALIDATE(options->GetDictionary("data", &data_in));

    chromeos::BluetoothOutOfBandPairingData data_out;

    base::BinaryValue* tmp_data;
    EXTENSION_FUNCTION_VALIDATE(data_in->GetBinary("hash", &tmp_data));
    EXTENSION_FUNCTION_VALIDATE(
        tmp_data->GetSize() == chromeos::kBluetoothOutOfBandPairingDataSize);
    memcpy(data_out.hash,
        reinterpret_cast<uint8_t*>(tmp_data->GetBuffer()),
        chromeos::kBluetoothOutOfBandPairingDataSize);

    EXTENSION_FUNCTION_VALIDATE(data_in->GetBinary("randomizer", &tmp_data));
    EXTENSION_FUNCTION_VALIDATE(
        tmp_data->GetSize() == chromeos::kBluetoothOutOfBandPairingDataSize);
    memcpy(data_out.randomizer,
        reinterpret_cast<uint8_t*>(tmp_data->GetBuffer()),
        chromeos::kBluetoothOutOfBandPairingDataSize);

    device->SetOutOfBandPairingData(
        data_out,
        base::Bind(&BluetoothSetOutOfBandPairingDataFunction::OnSuccessCallback,
            this),
        base::Bind(&BluetoothSetOutOfBandPairingDataFunction::OnErrorCallback,
            this));
  } else {
    device->ClearOutOfBandPairingData(
        base::Bind(&BluetoothSetOutOfBandPairingDataFunction::OnSuccessCallback,
            this),
        base::Bind(&BluetoothSetOutOfBandPairingDataFunction::OnErrorCallback,
            this));
  }

  return true;
}

void BluetoothGetLocalOutOfBandPairingDataFunction::ReadCallback(
    const chromeos::BluetoothOutOfBandPairingData& data) {
  base::BinaryValue* hash = base::BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data.hash),
      chromeos::kBluetoothOutOfBandPairingDataSize);
  base::BinaryValue* randomizer = base::BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data.randomizer),
      chromeos::kBluetoothOutOfBandPairingDataSize);

  // TODO(bryeung): convert to experimental_bluetooth::OutOfBandPairingData
  // when ArrayBuffer support within objects is completed.
  DictionaryValue* result = new DictionaryValue();
  result->Set("hash", hash);
  result->Set("randomizer", randomizer);

  result_.reset(result);

  SendResponse(true);
  Release();  // Added in RunImpl
}

void BluetoothGetLocalOutOfBandPairingDataFunction::ErrorCallback() {
  SetError(kCouldNotGetLocalOutOfBandPairingData);
  SendResponse(false);
  Release();  // Added in RunImpl
}

bool BluetoothGetLocalOutOfBandPairingDataFunction::RunImpl() {
  AddRef();  // Released in one of the callbacks below
  GetMutableAdapter(profile())->ReadLocalOutOfBandPairingData(
      base::Bind(&BluetoothGetLocalOutOfBandPairingDataFunction::ReadCallback,
          this),
      base::Bind(&BluetoothGetLocalOutOfBandPairingDataFunction::ErrorCallback,
          this));
  return true;
}

void BluetoothStartDiscoveryFunction::OnSuccessCallback() {
  SendResponse(true);
  Release();  // Added in RunImpl
}

void BluetoothStartDiscoveryFunction::OnErrorCallback() {
  SetError(kStartDiscoveryFailed);
  SendResponse(false);
  Release();  // Added in RunImpl
}

bool BluetoothStartDiscoveryFunction::RunImpl() {
  GetEventRouter(profile())->SetSendDiscoveryEvents(true);

  // BluetoothAdapter will throw an error if we SetDiscovering(true) when
  // discovery is already in progress
  if (GetMutableAdapter(profile())->IsDiscovering()) {
    SendResponse(true);
    return true;
  }

  AddRef();  // Removed in whichever callback is called.
  GetMutableAdapter(profile())->SetDiscovering(true,
      base::Bind(&BluetoothStartDiscoveryFunction::OnSuccessCallback, this),
      base::Bind(&BluetoothStartDiscoveryFunction::OnErrorCallback, this));
  return true;
}

void BluetoothStopDiscoveryFunction::OnSuccessCallback() {
  SendResponse(true);
  Release();  // Added in RunImpl
}

void BluetoothStopDiscoveryFunction::OnErrorCallback() {
  SetError(kStopDiscoveryFailed);
  SendResponse(false);
  Release();  // Added in RunImpl
}

bool BluetoothStopDiscoveryFunction::RunImpl() {
  GetEventRouter(profile())->SetSendDiscoveryEvents(false);
  AddRef();  // Removed in whichever callback is called.
  GetMutableAdapter(profile())->SetDiscovering(false,
      base::Bind(&BluetoothStopDiscoveryFunction::OnSuccessCallback, this),
      base::Bind(&BluetoothStopDiscoveryFunction::OnErrorCallback, this));
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

bool BluetoothGetDevicesFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothGetServicesFunction::RunImpl() {
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

bool BluetoothStartDiscoveryFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothStopDiscoveryFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothSetOutOfBandPairingDataFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

bool BluetoothGetLocalOutOfBandPairingDataFunction::RunImpl() {
  NOTREACHED() << "Not implemented yet";
  return false;
}

#endif

BluetoothReadFunction::BluetoothReadFunction() {}
BluetoothReadFunction::~BluetoothReadFunction() {}

BluetoothWriteFunction::BluetoothWriteFunction() {}
BluetoothWriteFunction::~BluetoothWriteFunction() {}

}  // namespace api
}  // namespace extensions
