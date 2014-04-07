// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_socket_event_dispatcher.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "chrome/common/extensions/api/bluetooth/bluetooth_manifest_data.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_service_record.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/io_buffer.h"

using content::BrowserContext;
using content::BrowserThread;

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothProfile;
using device::BluetoothServiceRecord;
using device::BluetoothSocket;

using extensions::BluetoothApiSocket;

namespace AddProfile = extensions::api::bluetooth::AddProfile;
namespace bluetooth = extensions::api::bluetooth;
namespace Connect = extensions::api::bluetooth::Connect;
namespace Disconnect = extensions::api::bluetooth::Disconnect;
namespace GetDevice = extensions::api::bluetooth::GetDevice;
namespace GetDevices = extensions::api::bluetooth::GetDevices;
namespace RemoveProfile = extensions::api::bluetooth::RemoveProfile;
namespace SetOutOfBandPairingData =
    extensions::api::bluetooth::SetOutOfBandPairingData;
namespace Send = extensions::api::bluetooth::Send;

namespace {

const char kCouldNotGetLocalOutOfBandPairingData[] =
    "Could not get local Out Of Band Pairing Data";
const char kCouldNotSetOutOfBandPairingData[] =
    "Could not set Out Of Band Pairing Data";
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

extensions::BluetoothEventRouter* GetEventRouter(BrowserContext* context) {
  // Note: |context| is valid on UI thread only.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return extensions::BluetoothAPI::Get(context)->event_router();
}

linked_ptr<bluetooth::Socket> CreateSocketInfo(int socket_id,
                                               BluetoothApiSocket* socket) {
  DCHECK(BrowserThread::CurrentlyOn(BluetoothApiSocket::kThreadId));
  linked_ptr<bluetooth::Socket> socket_info(new bluetooth::Socket());
  // This represents what we know about the socket, and does not call through
  // to the system.
  socket_info->id = socket_id;
  if (!socket->name().empty()) {
    socket_info->name.reset(new std::string(socket->name()));
  }
  socket_info->persistent = socket->persistent();
  if (socket->buffer_size() > 0) {
    socket_info->buffer_size.reset(new int(socket->buffer_size()));
  }
  socket_info->paused = socket->paused();
  socket_info->device.address = socket->device_address();
  socket_info->uuid = socket->uuid().canonical_value();

  return socket_info;
}

void SetSocketProperties(extensions::BluetoothApiSocket* socket,
                         bluetooth::SocketProperties* properties) {
  if (properties->name.get()) {
    socket->set_name(*properties->name.get());
  }
  if (properties->persistent.get()) {
    socket->set_persistent(*properties->persistent.get());
  }
  if (properties->buffer_size.get()) {
    // buffer size is validated when issuing the actual Recv operation
    // on the socket.
    socket->set_buffer_size(*properties->buffer_size.get());
  }
}

static void DispatchConnectionEventWorker(
    void* browser_context_id,
    const std::string& extension_id,
    const device::BluetoothUUID& profile_uuid,
    const device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothSocket> socket) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  if (!extensions::ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;

  extensions::BluetoothAPI* bluetooth_api =
      extensions::BluetoothAPI::Get(context);
  if (!bluetooth_api)
    return;

  bluetooth_api->DispatchConnectionEvent(
      extension_id, profile_uuid, device, socket);
}

}  // namespace

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return GetFactoryInstance()->Get(context);
}

BluetoothAPI::ConnectionParams::ConnectionParams() {}

BluetoothAPI::ConnectionParams::~ConnectionParams() {}

BluetoothAPI::BluetoothAPI(content::BrowserContext* context)
    : browser_context_(context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ExtensionSystem::Get(browser_context_)->event_router()->RegisterObserver(
      this, bluetooth::OnAdapterStateChanged::kEventName);
  ExtensionSystem::Get(browser_context_)->event_router()->RegisterObserver(
      this, bluetooth::OnDeviceAdded::kEventName);
  ExtensionSystem::Get(browser_context_)->event_router()->RegisterObserver(
      this, bluetooth::OnDeviceChanged::kEventName);
  ExtensionSystem::Get(browser_context_)->event_router()->RegisterObserver(
      this, bluetooth::OnDeviceRemoved::kEventName);
}

BluetoothAPI::~BluetoothAPI() {}

BluetoothEventRouter* BluetoothAPI::event_router() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!event_router_) {
    event_router_.reset(new BluetoothEventRouter(browser_context_));
  }
  return event_router_.get();
}

scoped_refptr<BluetoothAPI::SocketData> BluetoothAPI::socket_data() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!socket_data_) {
    ApiResourceManager<BluetoothApiSocket>* socket_manager =
        ApiResourceManager<BluetoothApiSocket>::Get(browser_context_);
    DCHECK(socket_manager)
        << "There is no socket manager. "
           "If this assertion is failing during a test, then it is likely that "
           "TestExtensionSystem is failing to provide an instance of "
           "ApiResourceManager<BluetoothApiSocket>.";

    socket_data_ = socket_manager->data_;
  }
  return socket_data_;
}

scoped_refptr<api::BluetoothSocketEventDispatcher>
BluetoothAPI::socket_event_dispatcher() {
  if (!socket_event_dispatcher_) {
    socket_event_dispatcher_ = new api::BluetoothSocketEventDispatcher(
        browser_context_, socket_data());
  }
  return socket_event_dispatcher_;
}

void BluetoothAPI::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ExtensionSystem::Get(browser_context_)->event_router()->UnregisterObserver(
      this);
}

void BluetoothAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (event_router()->IsBluetoothSupported())
    event_router()->OnListenerAdded();
}

void BluetoothAPI::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (event_router()->IsBluetoothSupported())
    event_router()->OnListenerRemoved();
}

void BluetoothAPI::DispatchConnectionEvent(
    const std::string& extension_id,
    const device::BluetoothUUID& uuid,
    const device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothSocket> socket) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!event_router()->HasProfile(uuid))
    return;

  extensions::BluetoothAPI::ConnectionParams params;
  params.browser_context_id = browser_context_;
  params.thread_id = BluetoothApiSocket::kThreadId;
  params.extension_id = extension_id;
  params.uuid = uuid;
  params.device_address = device->GetAddress();
  params.socket = socket;
  params.socket_data = socket_data();
  BrowserThread::PostTask(
      params.thread_id, FROM_HERE, base::Bind(&RegisterSocket, params));
}

// static
void BluetoothAPI::RegisterSocket(
    const BluetoothAPI::ConnectionParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  BluetoothApiSocket* api_socket = new BluetoothApiSocket(
      params.extension_id, params.socket, params.device_address, params.uuid);
  int socket_id = params.socket_data->Add(api_socket);

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&RegisterSocketUI, params, socket_id));
}

// static
void BluetoothAPI::RegisterSocketUI(const ConnectionParams& params,
                                    int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(params.browser_context_id);
  if (!extensions::ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;

  BluetoothAPI::Get(context)->event_router()->GetAdapter(
      base::Bind(&RegisterSocketWithAdapterUI, params, socket_id));
}

void BluetoothAPI::RegisterSocketWithAdapterUI(
    const ConnectionParams& params,
    int socket_id,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(params.browser_context_id);
  if (!extensions::ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;

  BluetoothDevice* device = adapter->GetDevice(params.device_address);
  if (!device)
    return;

  api::bluetooth::Socket result_socket;
  bluetooth::BluetoothDeviceToApiDevice(*device, &result_socket.device);
  result_socket.uuid = params.uuid.canonical_value();
  result_socket.id = socket_id;

  scoped_ptr<base::ListValue> args =
      bluetooth::OnConnection::Create(result_socket);
  scoped_ptr<Event> event(
      new Event(bluetooth::OnConnection::kEventName, args.Pass()));

  EventRouter* router = ExtensionSystem::Get(context)->event_router();
  if (router)
    router->DispatchEventToExtension(params.extension_id, event.Pass());
}

namespace api {

BluetoothSocketApiFunction::BluetoothSocketApiFunction() {}

BluetoothSocketApiFunction::~BluetoothSocketApiFunction() {}

bool BluetoothSocketApiFunction::RunImpl() {
  if (!PrePrepare() || !Prepare()) {
    return false;
  }
  AsyncWorkStart();
  return true;
}

bool BluetoothSocketApiFunction::PrePrepare() {
  socket_data_ = BluetoothAPI::Get(browser_context())->socket_data();
  socket_event_dispatcher_ =
      BluetoothAPI::Get(browser_context())->socket_event_dispatcher();
  return socket_data_ && socket_event_dispatcher_;
}

void BluetoothSocketApiFunction::AsyncWorkStart() {
  Work();
  AsyncWorkCompleted();
}

void BluetoothSocketApiFunction::Work() {}

void BluetoothSocketApiFunction::AsyncWorkCompleted() {
  SendResponse(Respond());
}

bool BluetoothSocketApiFunction::Respond() { return error_.empty(); }

BluetoothGetAdapterStateFunction::~BluetoothGetAdapterStateFunction() {}

bool BluetoothGetAdapterStateFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  bluetooth::AdapterState state;
  PopulateAdapterState(*adapter.get(), &state);
  results_ = bluetooth::GetAdapterState::Results::Create(state);
  SendResponse(true);
  return true;
}

BluetoothGetDevicesFunction::~BluetoothGetDevicesFunction() {}

bool BluetoothGetDevicesFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

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

BluetoothGetDeviceFunction::~BluetoothGetDeviceFunction() {}

bool BluetoothGetDeviceFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

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

BluetoothAddProfileFunction::BluetoothAddProfileFunction() {}

BluetoothAddProfileFunction::~BluetoothAddProfileFunction() {}

bool BluetoothAddProfileFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_ptr<AddProfile::Params> params(AddProfile::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);

  device::BluetoothUUID uuid(params->profile.uuid);

  if (!uuid.IsValid()) {
    SetError(kInvalidUuid);
    return false;
  }

  BluetoothPermissionRequest param(params->profile.uuid);
  if (!BluetoothManifestData::CheckRequest(GetExtension(), param)) {
    error_ = kPermissionDenied;
    return false;
  }

  uuid_ = uuid;

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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  BluetoothProfile::Register(uuid_, options, callback);
}

void BluetoothAddProfileFunction::OnProfileRegistered(
    BluetoothProfile* bluetooth_profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
      base::Bind(&DispatchConnectionEventWorker,
                 browser_context(),
                 extension_id(),
                 uuid_));
  GetEventRouter(browser_context())
      ->AddProfile(uuid_, extension_id(), bluetooth_profile);
  SendResponse(true);
}

BluetoothRemoveProfileFunction::~BluetoothRemoveProfileFunction() {}

bool BluetoothRemoveProfileFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_ptr<RemoveProfile::Params> params(
      RemoveProfile::Params::Create(*args_));

  device::BluetoothUUID uuid(params->profile.uuid);

  if (!uuid.IsValid()) {
    SetError(kInvalidUuid);
    return false;
  }

  if (!GetEventRouter(browser_context())->HasProfile(uuid)) {
    SetError(kProfileNotFound);
    return false;
  }

  GetEventRouter(browser_context())->RemoveProfile(uuid);
  return true;
}

BluetoothConnectFunction::~BluetoothConnectFunction() {}

bool BluetoothConnectFunction::DoWork(scoped_refptr<BluetoothAdapter> adapter) {
  scoped_ptr<Connect::Params> params(Connect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get() != NULL);
  const bluetooth::ConnectOptions& options = params->options;

  device::BluetoothUUID uuid(options.profile.uuid);

  if (!uuid.IsValid()) {
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

void BluetoothConnectFunction::OnSuccessCallback() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  SendResponse(true);
}

void BluetoothConnectFunction::OnErrorCallback(const std::string& error) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  SetError(error);
  SendResponse(false);
}

BluetoothDisconnectFunction::BluetoothDisconnectFunction() {}

BluetoothDisconnectFunction::~BluetoothDisconnectFunction() {}

bool BluetoothDisconnectFunction::Prepare() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  params_ = Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get() != NULL);
  return true;
}

void BluetoothDisconnectFunction::AsyncWorkStart() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  BluetoothApiSocket* socket =
      socket_data_->Get(extension_id(), params_->options.socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }
  socket->Disconnect(base::Bind(&BluetoothDisconnectFunction::OnSuccess, this));
}

void BluetoothDisconnectFunction::OnSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  socket_data_->Remove(extension_id(), params_->options.socket_id);
  results_ = bluetooth::Disconnect::Results::Create();
  AsyncWorkCompleted();
}

BluetoothSendFunction::BluetoothSendFunction() : io_buffer_size_(0) {}

BluetoothSendFunction::~BluetoothSendFunction() {}

bool BluetoothSendFunction::Prepare() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  params_ = Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get() != NULL);
  io_buffer_size_ = params_->data.size();
  io_buffer_ = new net::WrappedIOBuffer(params_->data.data());
  return true;
}

void BluetoothSendFunction::AsyncWorkStart() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  BluetoothApiSocket* socket =
      socket_data_->Get(extension_id(), params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }
  socket->Send(io_buffer_,
               io_buffer_size_,
               base::Bind(&BluetoothSendFunction::OnSendSuccess, this),
               base::Bind(&BluetoothSendFunction::OnSendError, this));
}

void BluetoothSendFunction::OnSendSuccess(int bytes_sent) {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  results_ = Send::Results::Create(bytes_sent);
  AsyncWorkCompleted();
}

void BluetoothSendFunction::OnSendError(const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  error_ = message;
  AsyncWorkCompleted();
}

BluetoothUpdateSocketFunction::BluetoothUpdateSocketFunction() {}

BluetoothUpdateSocketFunction::~BluetoothUpdateSocketFunction() {}

bool BluetoothUpdateSocketFunction::Prepare() {
  params_ = bluetooth::UpdateSocket::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void BluetoothUpdateSocketFunction::Work() {
  BluetoothApiSocket* socket =
      socket_data_->Get(extension_id(), params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  SetSocketProperties(socket, &params_.get()->properties);
  results_ = bluetooth::UpdateSocket::Results::Create();
}

BluetoothSetSocketPausedFunction::BluetoothSetSocketPausedFunction() {}

BluetoothSetSocketPausedFunction::~BluetoothSetSocketPausedFunction() {}

bool BluetoothSetSocketPausedFunction::Prepare() {
  params_ = bluetooth::SetSocketPaused::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void BluetoothSetSocketPausedFunction::Work() {
  BluetoothApiSocket* socket =
      socket_data_->Get(extension_id(), params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  if (socket->paused() != params_->paused) {
    socket->set_paused(params_->paused);
    if (!params_->paused) {
      socket_event_dispatcher_->OnSocketResume(extension_->id(),
                                               params_->socket_id);
    }
  }

  results_ = bluetooth::SetSocketPaused::Results::Create();
}

BluetoothGetSocketFunction::BluetoothGetSocketFunction() {}

BluetoothGetSocketFunction::~BluetoothGetSocketFunction() {}

bool BluetoothGetSocketFunction::Prepare() {
  params_ = bluetooth::GetSocket::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void BluetoothGetSocketFunction::Work() {
  BluetoothApiSocket* socket =
      socket_data_->Get(extension_id(), params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  linked_ptr<bluetooth::Socket> socket_info =
      CreateSocketInfo(params_->socket_id, socket);
  results_ = bluetooth::GetSocket::Results::Create(*socket_info);
}

BluetoothGetSocketsFunction::BluetoothGetSocketsFunction() {}

BluetoothGetSocketsFunction::~BluetoothGetSocketsFunction() {}

bool BluetoothGetSocketsFunction::Prepare() { return true; }

void BluetoothGetSocketsFunction::Work() {
  std::vector<linked_ptr<bluetooth::Socket> > socket_infos;
  base::hash_set<int>* resource_ids =
      socket_data_->GetResourceIds(extension_id());
  if (resource_ids != NULL) {
    for (base::hash_set<int>::iterator it = resource_ids->begin();
         it != resource_ids->end();
         ++it) {
      int socket_id = *it;
      BluetoothApiSocket* socket = socket_data_->Get(extension_id(), socket_id);
      if (socket) {
        socket_infos.push_back(CreateSocketInfo(socket_id, socket));
      }
    }
  }
  results_ = bluetooth::GetSockets::Results::Create(socket_infos);
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
