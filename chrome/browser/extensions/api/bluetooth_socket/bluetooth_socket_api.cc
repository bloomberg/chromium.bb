// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_socket/bluetooth_socket_api.h"

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_socket.h"
#include "chrome/browser/extensions/api/bluetooth_socket/bluetooth_socket_event_dispatcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"

using content::BrowserThread;
using extensions::BluetoothApiSocket;
using extensions::api::bluetooth_socket::SocketInfo;
using extensions::api::bluetooth_socket::SocketProperties;

namespace {

const char kSocketNotFoundError[] = "Socket not found";

linked_ptr<SocketInfo> CreateSocketInfo(int socket_id,
                                        BluetoothApiSocket* socket) {
  DCHECK(BrowserThread::CurrentlyOn(BluetoothApiSocket::kThreadId));
  linked_ptr<SocketInfo> socket_info(new SocketInfo());
  // This represents what we know about the socket, and does not call through
  // to the system.
  socket_info->socket_id = socket_id;
  if (!socket->name().empty()) {
    socket_info->name.reset(new std::string(socket->name()));
  }
  socket_info->persistent = socket->persistent();
  if (socket->buffer_size() > 0) {
    socket_info->buffer_size.reset(new int(socket->buffer_size()));
  }
  socket_info->paused = socket->paused();
  socket_info->connected = socket->IsConnected();

  // TODO(keybuk): These should not be present if socket isn't connected or
  // listening.
  socket_info->address.reset(new std::string(socket->device_address()));
  socket_info->uuid.reset(new std::string(socket->uuid().canonical_value()));

  return socket_info;
}

void SetSocketProperties(BluetoothApiSocket* socket,
                         SocketProperties* properties) {
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

}  // namespace

namespace extensions {
namespace api {

BluetoothSocketAsyncApiFunction::BluetoothSocketAsyncApiFunction() {}

BluetoothSocketAsyncApiFunction::~BluetoothSocketAsyncApiFunction() {}

bool BluetoothSocketAsyncApiFunction::RunImpl() {
  if (!PrePrepare() || !Prepare()) {
    return false;
  }
  AsyncWorkStart();
  return true;
}

bool BluetoothSocketAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<BluetoothApiSocket>::Get(browser_context());
  DCHECK(manager_)
      << "There is no socket manager. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<BluetoothApiSocket>.";
  return manager_ != NULL;
}

bool BluetoothSocketAsyncApiFunction::Respond() { return error_.empty(); }

void BluetoothSocketAsyncApiFunction::AsyncWorkCompleted() {
  SendResponse(Respond());
}

void BluetoothSocketAsyncApiFunction::Work() {}

void BluetoothSocketAsyncApiFunction::AsyncWorkStart() {
  Work();
  AsyncWorkCompleted();
}

int BluetoothSocketAsyncApiFunction::AddSocket(BluetoothApiSocket* socket) {
  return manager_->Add(socket);
}

content::BrowserThread::ID
BluetoothSocketAsyncApiFunction::work_thread_id() const {
  return BluetoothApiSocket::kThreadId;
}

BluetoothApiSocket* BluetoothSocketAsyncApiFunction::GetSocket(
    int api_resource_id) {
  return manager_->Get(extension_id(), api_resource_id);
}

void BluetoothSocketAsyncApiFunction::RemoveSocket(int api_resource_id) {
  manager_->Remove(extension_id(), api_resource_id);
}

base::hash_set<int>* BluetoothSocketAsyncApiFunction::GetSocketIds() {
  return manager_->GetResourceIds(extension_id());
}

BluetoothSocketCreateFunction::BluetoothSocketCreateFunction() {}

BluetoothSocketCreateFunction::~BluetoothSocketCreateFunction() {}

bool BluetoothSocketCreateFunction::Prepare() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  params_ = bluetooth_socket::Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void BluetoothSocketCreateFunction::Work() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));

  BluetoothApiSocket* socket = new BluetoothApiSocket(extension_id());

  bluetooth_socket::SocketProperties* properties =
      params_.get()->properties.get();
  if (properties) {
    SetSocketProperties(socket, properties);
  }

  bluetooth_socket::CreateInfo create_info;
  create_info.socket_id = AddSocket(socket);
  results_ = bluetooth_socket::Create::Results::Create(create_info);
  AsyncWorkCompleted();
}

BluetoothSocketUpdateFunction::BluetoothSocketUpdateFunction() {}

BluetoothSocketUpdateFunction::~BluetoothSocketUpdateFunction() {}

bool BluetoothSocketUpdateFunction::Prepare() {
  params_ = bluetooth_socket::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void BluetoothSocketUpdateFunction::Work() {
  BluetoothApiSocket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  SetSocketProperties(socket, &params_.get()->properties);
  results_ = bluetooth_socket::Update::Results::Create();
}

BluetoothSocketSetPausedFunction::BluetoothSocketSetPausedFunction()
    : socket_event_dispatcher_(NULL) {}

BluetoothSocketSetPausedFunction::~BluetoothSocketSetPausedFunction() {}

bool BluetoothSocketSetPausedFunction::Prepare() {
  params_ = bluetooth_socket::SetPaused::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  socket_event_dispatcher_ =
      BluetoothSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "BluetoothSocketEventDispatcher.";
  return socket_event_dispatcher_ != NULL;
}

void BluetoothSocketSetPausedFunction::Work() {
  BluetoothApiSocket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  if (socket->paused() != params_->paused) {
    socket->set_paused(params_->paused);
    if (!params_->paused) {
      socket_event_dispatcher_->OnSocketResume(extension_id(),
                                               params_->socket_id);
    }
  }

  results_ = bluetooth_socket::SetPaused::Results::Create();
}

bool BluetoothSocketListenUsingRfcommFunction::RunImpl() {
  // TODO(keybuk): Implement.
  SetError("Not yet implemented.");
  return false;
}

bool BluetoothSocketListenUsingInsecureRfcommFunction::RunImpl() {
  // TODO(keybuk): Implement.
  SetError("Not yet implemented.");
  return false;
}

bool BluetoothSocketListenUsingL2capFunction::RunImpl() {
  // TODO(keybuk): Implement.
  SetError("Not yet implemented.");
  return false;
}

bool BluetoothSocketConnectFunction::RunImpl() {
  // TODO(keybuk): Implement.
  SetError("Not yet implemented.");
  return false;
}

BluetoothSocketDisconnectFunction::BluetoothSocketDisconnectFunction() {}

BluetoothSocketDisconnectFunction::~BluetoothSocketDisconnectFunction() {}

bool BluetoothSocketDisconnectFunction::Prepare() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  params_ = bluetooth_socket::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void BluetoothSocketDisconnectFunction::AsyncWorkStart() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  BluetoothApiSocket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  socket->Disconnect(base::Bind(&BluetoothSocketDisconnectFunction::OnSuccess,
                                this));
}

void BluetoothSocketDisconnectFunction::OnSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  results_ = bluetooth_socket::Disconnect::Results::Create();
  AsyncWorkCompleted();
}

BluetoothSocketCloseFunction::BluetoothSocketCloseFunction() {}

BluetoothSocketCloseFunction::~BluetoothSocketCloseFunction() {}

bool BluetoothSocketCloseFunction::Prepare() {
  params_ = bluetooth_socket::Close::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void BluetoothSocketCloseFunction::Work() {
  BluetoothApiSocket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  RemoveSocket(params_->socket_id);
  results_ = bluetooth_socket::Close::Results::Create();
}

BluetoothSocketSendFunction::BluetoothSocketSendFunction()
    : io_buffer_size_(0) {}

BluetoothSocketSendFunction::~BluetoothSocketSendFunction() {}

bool BluetoothSocketSendFunction::Prepare() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  params_ = bluetooth_socket::Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  io_buffer_size_ = params_->data.size();
  io_buffer_ = new net::WrappedIOBuffer(params_->data.data());
  return true;
}

void BluetoothSocketSendFunction::AsyncWorkStart() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  BluetoothApiSocket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  socket->Send(io_buffer_,
               io_buffer_size_,
               base::Bind(&BluetoothSocketSendFunction::OnSuccess, this),
               base::Bind(&BluetoothSocketSendFunction::OnError, this));
}

void BluetoothSocketSendFunction::OnSuccess(int bytes_sent) {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  results_ = bluetooth_socket::Send::Results::Create(bytes_sent);
  AsyncWorkCompleted();
}

void BluetoothSocketSendFunction::OnError(
    BluetoothApiSocket::ErrorReason reason,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id()));
  error_ = message;
  AsyncWorkCompleted();
}

BluetoothSocketGetInfoFunction::BluetoothSocketGetInfoFunction() {}

BluetoothSocketGetInfoFunction::~BluetoothSocketGetInfoFunction() {}

bool BluetoothSocketGetInfoFunction::Prepare() {
  params_ = bluetooth_socket::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void BluetoothSocketGetInfoFunction::Work() {
  BluetoothApiSocket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  linked_ptr<bluetooth_socket::SocketInfo> socket_info =
      CreateSocketInfo(params_->socket_id, socket);
  results_ = bluetooth_socket::GetInfo::Results::Create(*socket_info);
}

BluetoothSocketGetSocketsFunction::BluetoothSocketGetSocketsFunction() {}

BluetoothSocketGetSocketsFunction::~BluetoothSocketGetSocketsFunction() {}

bool BluetoothSocketGetSocketsFunction::Prepare() { return true; }

void BluetoothSocketGetSocketsFunction::Work() {
  std::vector<linked_ptr<bluetooth_socket::SocketInfo> > socket_infos;
  base::hash_set<int>* resource_ids = GetSocketIds();
  if (resource_ids != NULL) {
    for (base::hash_set<int>::iterator it = resource_ids->begin();
         it != resource_ids->end();
         ++it) {
      int socket_id = *it;
      BluetoothApiSocket* socket = GetSocket(socket_id);
      if (socket) {
        socket_infos.push_back(CreateSocketInfo(socket_id, socket));
      }
    }
  }
  results_ = bluetooth_socket::GetSockets::Results::Create(socket_infos);
}

}  // namespace api
}  // namespace extensions
