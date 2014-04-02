// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "chrome/common/extensions/api/bluetooth/bluetooth_manifest_data.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_service_record.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/io_buffer.h"

using content::BrowserContext;
using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothProfile;
using device::BluetoothServiceRecord;
using device::BluetoothSocket;

namespace {

extensions::BluetoothEventRouter* GetEventRouter(BrowserContext* context) {
  return extensions::BluetoothAPI::Get(context)->bluetooth_event_router();
}

}  // namespace

namespace {

const char kCouldNotGetLocalOutOfBandPairingData[] =
    "Could not get local Out Of Band Pairing Data";
const char kCouldNotSetOutOfBandPairingData[] =
    "Could not set Out Of Band Pairing Data";
const char kFailedToConnect[] = "Connection failed";
const char kInvalidDevice[] = "Invalid device";
const char kInvalidUuid[] = "Invalid UUID";
const char kPermissionDenied[] = "Permission to add profile denied.";
const char kProfileAlreadyRegistered[] =
    "This profile has already been registered";
const char kProfileNotFound[] = "Profile not found: invalid uuid";
const char kProfileRegistrationFailed[] = "Profile registration failed";
const char kSocketNotFoundError[] = "Socket not found: invalid socket id";
const char kStartDiscoveryFailed[] = "Starting discovery failed";
const char kStopDiscoveryFailed[] = "Failed to stop discovery";

}  // namespace

namespace AddProfile = extensions::api::bluetooth::AddProfile;
namespace bluetooth = extensions::api::bluetooth;
namespace Connect = extensions::api::bluetooth::Connect;
namespace Disconnect = extensions::api::bluetooth::Disconnect;
namespace GetDevice = extensions::api::bluetooth::GetDevice;
namespace GetDevices = extensions::api::bluetooth::GetDevices;
namespace Read = extensions::api::bluetooth::Read;
namespace RemoveProfile = extensions::api::bluetooth::RemoveProfile;
namespace SetOutOfBandPairingData =
    extensions::api::bluetooth::SetOutOfBandPairingData;
namespace Write = extensions::api::bluetooth::Write;

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<BluetoothAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BluetoothAPI>*
BluetoothAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
BluetoothAPI* BluetoothAPI::Get(BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

BluetoothAPI::BluetoothAPI(BrowserContext* context)
    : browser_context_(context) {
  ExtensionSystem::Get(browser_context_)->event_router()->RegisterObserver(
      this, bluetooth::OnAdapterStateChanged::kEventName);
  ExtensionSystem::Get(browser_context_)->event_router()->RegisterObserver(
      this, bluetooth::OnDeviceAdded::kEventName);
  ExtensionSystem::Get(browser_context_)->event_router()->RegisterObserver(
      this, bluetooth::OnDeviceChanged::kEventName);
  ExtensionSystem::Get(browser_context_)->event_router()->RegisterObserver(
      this, bluetooth::OnDeviceRemoved::kEventName);
}

BluetoothAPI::~BluetoothAPI() {
}

BluetoothEventRouter* BluetoothAPI::bluetooth_event_router() {
  if (!bluetooth_event_router_)
    bluetooth_event_router_.reset(new BluetoothEventRouter(browser_context_));

  return bluetooth_event_router_.get();
}

void BluetoothAPI::Shutdown() {
  ExtensionSystem::Get(browser_context_)->event_router()->UnregisterObserver(
      this);
}

void BluetoothAPI::OnListenerAdded(const EventListenerInfo& details) {
  if (bluetooth_event_router()->IsBluetoothSupported())
    bluetooth_event_router()->OnListenerAdded();
}

void BluetoothAPI::OnListenerRemoved(const EventListenerInfo& details) {
  if (bluetooth_event_router()->IsBluetoothSupported())
    bluetooth_event_router()->OnListenerRemoved();
}

namespace api {

BluetoothAddProfileFunction::BluetoothAddProfileFunction() {
}

bool BluetoothAddProfileFunction::RunImpl() {
  scoped_ptr<AddProfile::Params> params(AddProfile::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  if (!BluetoothDevice::IsUUIDValid(params->profile.uuid)) {
    SetError(kInvalidUuid);
    return false;
  }

  BluetoothPermissionRequest param(params->profile.uuid);
  if (!BluetoothManifestData::CheckRequest(GetExtension(), param)) {
    error_ = kPermissionDenied;
    return false;
  }

  uuid_ = device::bluetooth_utils::CanonicalUuid(params->profile.uuid);

  if (GetEventRouter(browser_context())->HasProfile(uuid_)) {
    SetError(kProfileAlreadyRegistered);
    return false;
  }

  BluetoothProfile::Options options;
  if (params->profile.name.get())
    options.name = *params->profile.name.get();
  if (params->profile.channel.get())
    options.channel = *params->profile.channel.get();
  if (params->profile.psm.get())
    options.psm = *params->profile.psm.get();
  if (params->profile.require_authentication.get()) {
    options.require_authentication =
        *params->profile.require_authentication.get();
  }
  if (params->profile.require_authorization.get()) {
    options.require_authorization =
        *params->profile.require_authorization.get();
  }
  if (params->profile.auto_connect.get())
    options.auto_connect = *params->profile.auto_connect.get();
  if (params->profile.version.get())
    options.version = *params->profile.version.get();
  if (params->profile.features.get())
    options.features = *params->profile.features.get();

  RegisterProfile(
      options,
      base::Bind(&BluetoothAddProfileFunction::OnProfileRegistered, this));

  return true;
}

void BluetoothAddProfileFunction::RegisterProfile(
    const BluetoothProfile::Options& options,
    const BluetoothProfile::ProfileCallback& callback) {
  BluetoothProfile::Register(uuid_, options, callback);
}

void BluetoothAddProfileFunction::OnProfileRegistered(
    BluetoothProfile* bluetooth_profile) {
  if (!bluetooth_profile) {
    SetError(kProfileRegistrationFailed);
    SendResponse(false);
    return;
  }

  if (GetEventRouter(browser_context())->HasProfile(uuid_)) {
    bluetooth_profile->Unregister();
    SetError(kProfileAlreadyRegistered);
    SendResponse(false);
    return;
  }

  bluetooth_profile->SetConnectionCallback(
      base::Bind(&BluetoothEventRouter::DispatchConnectionEvent,
                 base::Unretained(GetEventRouter(browser_context())),
                 extension_id(),
                 uuid_));
  GetEventRouter(browser_context())
      ->AddProfile(uuid_, extension_id(), bluetooth_profile);
  SendResponse(true);
}

bool BluetoothRemoveProfileFunction::RunImpl() {
  scoped_ptr<RemoveProfile::Params> params(
      RemoveProfile::Params::Create(*args_));

  if (!BluetoothDevice::IsUUIDValid(params->profile.uuid)) {
    SetError(kInvalidUuid);
    return false;
  }

  std::string uuid =
      device::bluetooth_utils::CanonicalUuid(params->profile.uuid);

  if (!GetEventRouter(browser_context())->HasProfile(uuid)) {
    SetError(kProfileNotFound);
    return false;
  }

  GetEventRouter(browser_context())->RemoveProfile(uuid);
  return true;
}

bool BluetoothGetAdapterStateFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  bluetooth::AdapterState state;
  PopulateAdapterState(*adapter.get(), &state);
  SetResult(state.ToValue().release());
  SendResponse(true);
  return true;
}

bool BluetoothGetDevicesFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ListValue* device_list = new base::ListValue;
  SetResult(device_list);

  BluetoothAdapter::DeviceList devices = adapter->GetDevices();
  for (BluetoothAdapter::DeviceList::const_iterator iter = devices.begin();
       iter != devices.end();
       ++iter) {
    const BluetoothDevice* device = *iter;
    DCHECK(device);

    bluetooth::Device extension_device;
    bluetooth::BluetoothDeviceToApiDevice(*device, &extension_device);

    device_list->Append(extension_device.ToValue().release());
  }

  SendResponse(true);

  return true;
}

bool BluetoothGetDeviceFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_ptr<GetDevice::Params> params(GetDevice::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const std::string& device_address = params->device_address;

  BluetoothDevice* device = adapter->GetDevice(device_address);
  if (device) {
    bluetooth::Device extension_device;
    bluetooth::BluetoothDeviceToApiDevice(*device, &extension_device);
    SetResult(extension_device.ToValue().release());
    SendResponse(true);
  } else {
    SetError(kInvalidDevice);
    SendResponse(false);
  }

  return false;
}

void BluetoothConnectFunction::OnSuccessCallback() {
  SendResponse(true);
}

void BluetoothConnectFunction::OnErrorCallback() {
  SetError(kFailedToConnect);
  SendResponse(false);
}

bool BluetoothConnectFunction::DoWork(scoped_refptr<BluetoothAdapter> adapter) {
  scoped_ptr<Connect::Params> params(Connect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const bluetooth::ConnectOptions& options = params->options;

  if (!BluetoothDevice::IsUUIDValid(options.profile.uuid)) {
    SetError(kInvalidUuid);
    SendResponse(false);
    return false;
  }

  BluetoothDevice* device = adapter->GetDevice(options.device.address);
  if (!device) {
    SetError(kInvalidDevice);
    SendResponse(false);
    return false;
  }

  std::string uuid = device::bluetooth_utils::CanonicalUuid(
      options.profile.uuid);

  BluetoothProfile* bluetooth_profile =
      GetEventRouter(browser_context())->GetProfile(uuid);
  if (!bluetooth_profile) {
    SetError(kProfileNotFound);
    SendResponse(false);
    return false;
  }

  device->ConnectToProfile(
      bluetooth_profile,
      base::Bind(&BluetoothConnectFunction::OnSuccessCallback, this),
      base::Bind(&BluetoothConnectFunction::OnErrorCallback, this));

  return true;
}

bool BluetoothDisconnectFunction::RunImpl() {
  scoped_ptr<Disconnect::Params> params(Disconnect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const bluetooth::DisconnectOptions& options = params->options;
  return GetEventRouter(browser_context())->ReleaseSocket(options.socket.id);
}

BluetoothReadFunction::BluetoothReadFunction() : success_(false) {}
BluetoothReadFunction::~BluetoothReadFunction() {}

bool BluetoothReadFunction::Prepare() {
  scoped_ptr<Read::Params> params(Read::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const bluetooth::ReadOptions& options = params->options;

  socket_ = GetEventRouter(browser_context())->GetSocket(options.socket.id);
  if (socket_.get() == NULL) {
    SetError(kSocketNotFoundError);
    return false;
  }

  success_ = false;
  return true;
}

void BluetoothReadFunction::Work() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!socket_.get())
    return;

  scoped_refptr<net::GrowableIOBuffer> buffer(new net::GrowableIOBuffer);
  success_ = socket_->Receive(buffer.get());
  if (success_)
    SetResult(base::BinaryValue::CreateWithCopiedBuffer(buffer->StartOfBuffer(),
                                                        buffer->offset()));
  else
    SetError(socket_->GetLastErrorMessage());
}

bool BluetoothReadFunction::Respond() {
  return success_;
}

BluetoothWriteFunction::BluetoothWriteFunction()
    : success_(false),
      data_to_write_(NULL) {
}

BluetoothWriteFunction::~BluetoothWriteFunction() {}

bool BluetoothWriteFunction::Prepare() {
  // TODO(bryeung): update to new-style parameter passing when ArrayBuffer
  // support is added
  base::DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));

  base::DictionaryValue* socket;
  EXTENSION_FUNCTION_VALIDATE(options->GetDictionary("socket", &socket));

  int socket_id;
  EXTENSION_FUNCTION_VALIDATE(socket->GetInteger("id", &socket_id));

  socket_ = GetEventRouter(browser_context())->GetSocket(socket_id);
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (socket_.get() == NULL)
    return;

  scoped_refptr<net::WrappedIOBuffer> wrapped_io_buffer(
      new net::WrappedIOBuffer(data_to_write_->GetBuffer()));
  scoped_refptr<net::DrainableIOBuffer> drainable_io_buffer(
      new net::DrainableIOBuffer(wrapped_io_buffer.get(),
                                 data_to_write_->GetSize()));
  success_ = socket_->Send(drainable_io_buffer.get());
  if (success_) {
    if (drainable_io_buffer->BytesConsumed() > 0)
      SetResult(new base::FundamentalValue(
          drainable_io_buffer->BytesConsumed()));
    else
      results_.reset();
  } else {
    SetError(socket_->GetLastErrorMessage());
  }
}

bool BluetoothWriteFunction::Respond() {
  return success_;
}

void BluetoothSetOutOfBandPairingDataFunction::OnSuccessCallback() {
  SendResponse(true);
}

void BluetoothSetOutOfBandPairingDataFunction::OnErrorCallback() {
  SetError(kCouldNotSetOutOfBandPairingData);
  SendResponse(false);
}

bool BluetoothSetOutOfBandPairingDataFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  // TODO(bryeung): update to new-style parameter passing when ArrayBuffer
  // support is added
  base::DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));
  std::string address;
  EXTENSION_FUNCTION_VALIDATE(options->GetString("deviceAddress", &address));

  BluetoothDevice* device = adapter->GetDevice(address);
  if (!device) {
    SetError(kInvalidDevice);
    SendResponse(false);
    return false;
  }

  if (options->HasKey("data")) {
    base::DictionaryValue* data_in;
    EXTENSION_FUNCTION_VALIDATE(options->GetDictionary("data", &data_in));

    device::BluetoothOutOfBandPairingData data_out;

    base::BinaryValue* tmp_data;
    EXTENSION_FUNCTION_VALIDATE(data_in->GetBinary("hash", &tmp_data));
    EXTENSION_FUNCTION_VALIDATE(
        tmp_data->GetSize() == device::kBluetoothOutOfBandPairingDataSize);
    memcpy(data_out.hash,
        reinterpret_cast<uint8_t*>(tmp_data->GetBuffer()),
        device::kBluetoothOutOfBandPairingDataSize);

    EXTENSION_FUNCTION_VALIDATE(data_in->GetBinary("randomizer", &tmp_data));
    EXTENSION_FUNCTION_VALIDATE(
        tmp_data->GetSize() == device::kBluetoothOutOfBandPairingDataSize);
    memcpy(data_out.randomizer,
        reinterpret_cast<uint8_t*>(tmp_data->GetBuffer()),
        device::kBluetoothOutOfBandPairingDataSize);

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
    const device::BluetoothOutOfBandPairingData& data) {
  base::BinaryValue* hash = base::BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data.hash),
      device::kBluetoothOutOfBandPairingDataSize);
  base::BinaryValue* randomizer = base::BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data.randomizer),
      device::kBluetoothOutOfBandPairingDataSize);

  // TODO(bryeung): convert to bluetooth::OutOfBandPairingData
  // when ArrayBuffer support within objects is completed.
  base::DictionaryValue* result = new base::DictionaryValue();
  result->Set("hash", hash);
  result->Set("randomizer", randomizer);

  SetResult(result);

  SendResponse(true);
}

void BluetoothGetLocalOutOfBandPairingDataFunction::ErrorCallback() {
  SetError(kCouldNotGetLocalOutOfBandPairingData);
  SendResponse(false);
}

bool BluetoothGetLocalOutOfBandPairingDataFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  adapter->ReadLocalOutOfBandPairingData(
      base::Bind(&BluetoothGetLocalOutOfBandPairingDataFunction::ReadCallback,
          this),
      base::Bind(&BluetoothGetLocalOutOfBandPairingDataFunction::ErrorCallback,
          this));

  return true;
}

void BluetoothStartDiscoveryFunction::OnSuccessCallback() {
  SendResponse(true);
}

void BluetoothStartDiscoveryFunction::OnErrorCallback() {
  SetError(kStartDiscoveryFailed);
  SendResponse(false);
}

bool BluetoothStartDiscoveryFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  GetEventRouter(browser_context())->StartDiscoverySession(
      adapter,
      extension_id(),
      base::Bind(&BluetoothStartDiscoveryFunction::OnSuccessCallback, this),
      base::Bind(&BluetoothStartDiscoveryFunction::OnErrorCallback, this));

  return true;
}

void BluetoothStopDiscoveryFunction::OnSuccessCallback() {
  SendResponse(true);
}

void BluetoothStopDiscoveryFunction::OnErrorCallback() {
  SetError(kStopDiscoveryFailed);
  SendResponse(false);
}

bool BluetoothStopDiscoveryFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  GetEventRouter(browser_context())->StopDiscoverySession(
      adapter,
      extension_id(),
      base::Bind(&BluetoothStopDiscoveryFunction::OnSuccessCallback, this),
      base::Bind(&BluetoothStopDiscoveryFunction::OnErrorCallback, this));

  return true;
}

}  // namespace api
}  // namespace extensions
