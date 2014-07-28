// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_tcp_server/sockets_tcp_server_api.h"

#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/sockets_tcp_server/tcp_server_socket_event_dispatcher.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "net/base/net_errors.h"

using content::SocketPermissionRequest;
using extensions::ResumableTCPServerSocket;
using extensions::core_api::sockets_tcp_server::SocketInfo;
using extensions::core_api::sockets_tcp_server::SocketProperties;

namespace {

const char kSocketNotFoundError[] = "Socket not found";
const char kPermissionError[] = "Does not have permission";
const int kDefaultListenBacklog = SOMAXCONN;

linked_ptr<SocketInfo> CreateSocketInfo(int socket_id,
                                        ResumableTCPServerSocket* socket) {
  linked_ptr<SocketInfo> socket_info(new SocketInfo());
  // This represents what we know about the socket, and does not call through
  // to the system.
  socket_info->socket_id = socket_id;
  if (!socket->name().empty()) {
    socket_info->name.reset(new std::string(socket->name()));
  }
  socket_info->persistent = socket->persistent();
  socket_info->paused = socket->paused();

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    socket_info->local_address.reset(
        new std::string(localAddress.ToStringWithoutPort()));
    socket_info->local_port.reset(new int(localAddress.port()));
  }

  return socket_info;
}

void SetSocketProperties(ResumableTCPServerSocket* socket,
                         SocketProperties* properties) {
  if (properties->name.get()) {
    socket->set_name(*properties->name.get());
  }
  if (properties->persistent.get()) {
    socket->set_persistent(*properties->persistent.get());
  }
}

}  // namespace

namespace extensions {
namespace core_api {

TCPServerSocketAsyncApiFunction::~TCPServerSocketAsyncApiFunction() {}

scoped_ptr<SocketResourceManagerInterface>
TCPServerSocketAsyncApiFunction::CreateSocketResourceManager() {
  return scoped_ptr<SocketResourceManagerInterface>(
             new SocketResourceManager<ResumableTCPServerSocket>()).Pass();
}

ResumableTCPServerSocket* TCPServerSocketAsyncApiFunction::GetTcpSocket(
    int socket_id) {
  return static_cast<ResumableTCPServerSocket*>(GetSocket(socket_id));
}

SocketsTcpServerCreateFunction::SocketsTcpServerCreateFunction() {}

SocketsTcpServerCreateFunction::~SocketsTcpServerCreateFunction() {}

bool SocketsTcpServerCreateFunction::Prepare() {
  params_ = sockets_tcp_server::Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerCreateFunction::Work() {
  ResumableTCPServerSocket* socket =
      new ResumableTCPServerSocket(extension_->id());

  sockets_tcp_server::SocketProperties* properties =
      params_.get()->properties.get();
  if (properties) {
    SetSocketProperties(socket, properties);
  }

  sockets_tcp_server::CreateInfo create_info;
  create_info.socket_id = AddSocket(socket);
  results_ = sockets_tcp_server::Create::Results::Create(create_info);
}

SocketsTcpServerUpdateFunction::SocketsTcpServerUpdateFunction() {}

SocketsTcpServerUpdateFunction::~SocketsTcpServerUpdateFunction() {}

bool SocketsTcpServerUpdateFunction::Prepare() {
  params_ = sockets_tcp_server::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerUpdateFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  SetSocketProperties(socket, &params_.get()->properties);
  results_ = sockets_tcp_server::Update::Results::Create();
}

SocketsTcpServerSetPausedFunction::SocketsTcpServerSetPausedFunction()
    : socket_event_dispatcher_(NULL) {}

SocketsTcpServerSetPausedFunction::~SocketsTcpServerSetPausedFunction() {}

bool SocketsTcpServerSetPausedFunction::Prepare() {
  params_ = core_api::sockets_tcp_server::SetPaused::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  socket_event_dispatcher_ =
      TCPServerSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPServerSocketEventDispatcher.";
  return socket_event_dispatcher_ != NULL;
}

void SocketsTcpServerSetPausedFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  if (socket->paused() != params_->paused) {
    socket->set_paused(params_->paused);
    if (socket->IsConnected() && !params_->paused) {
      socket_event_dispatcher_->OnServerSocketResume(extension_->id(),
                                                     params_->socket_id);
    }
  }

  results_ = sockets_tcp_server::SetPaused::Results::Create();
}

SocketsTcpServerListenFunction::SocketsTcpServerListenFunction()
    : socket_event_dispatcher_(NULL) {}

SocketsTcpServerListenFunction::~SocketsTcpServerListenFunction() {}

bool SocketsTcpServerListenFunction::Prepare() {
  params_ = core_api::sockets_tcp_server::Listen::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  socket_event_dispatcher_ =
      TCPServerSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPServerSocketEventDispatcher.";
  return socket_event_dispatcher_ != NULL;
}

void SocketsTcpServerListenFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  SocketPermissionRequest param(
      SocketPermissionRequest::TCP_LISTEN, params_->address, params_->port);
  if (!SocketsManifestData::CheckRequest(extension(), param)) {
    error_ = kPermissionError;
    return;
  }

  int net_result = socket->Listen(
      params_->address,
      params_->port,
      params_->backlog.get() ? *params_->backlog.get() : kDefaultListenBacklog,
      &error_);

  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);

  if (net_result == net::OK) {
    socket_event_dispatcher_->OnServerSocketListen(extension_->id(),
                                                   params_->socket_id);
  }

  results_ = sockets_tcp_server::Listen::Results::Create(net_result);
}

SocketsTcpServerDisconnectFunction::SocketsTcpServerDisconnectFunction() {}

SocketsTcpServerDisconnectFunction::~SocketsTcpServerDisconnectFunction() {}

bool SocketsTcpServerDisconnectFunction::Prepare() {
  params_ = sockets_tcp_server::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerDisconnectFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  socket->Disconnect();
  results_ = sockets_tcp_server::Disconnect::Results::Create();
}

SocketsTcpServerCloseFunction::SocketsTcpServerCloseFunction() {}

SocketsTcpServerCloseFunction::~SocketsTcpServerCloseFunction() {}

bool SocketsTcpServerCloseFunction::Prepare() {
  params_ = sockets_tcp_server::Close::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerCloseFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  RemoveSocket(params_->socket_id);
  results_ = sockets_tcp_server::Close::Results::Create();
}

SocketsTcpServerGetInfoFunction::SocketsTcpServerGetInfoFunction() {}

SocketsTcpServerGetInfoFunction::~SocketsTcpServerGetInfoFunction() {}

bool SocketsTcpServerGetInfoFunction::Prepare() {
  params_ = sockets_tcp_server::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerGetInfoFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  linked_ptr<sockets_tcp_server::SocketInfo> socket_info =
      CreateSocketInfo(params_->socket_id, socket);
  results_ = sockets_tcp_server::GetInfo::Results::Create(*socket_info);
}

SocketsTcpServerGetSocketsFunction::SocketsTcpServerGetSocketsFunction() {}

SocketsTcpServerGetSocketsFunction::~SocketsTcpServerGetSocketsFunction() {}

bool SocketsTcpServerGetSocketsFunction::Prepare() { return true; }

void SocketsTcpServerGetSocketsFunction::Work() {
  std::vector<linked_ptr<sockets_tcp_server::SocketInfo> > socket_infos;
  base::hash_set<int>* resource_ids = GetSocketIds();
  if (resource_ids != NULL) {
    for (base::hash_set<int>::iterator it = resource_ids->begin();
         it != resource_ids->end();
         ++it) {
      int socket_id = *it;
      ResumableTCPServerSocket* socket = GetTcpSocket(socket_id);
      if (socket) {
        socket_infos.push_back(CreateSocketInfo(socket_id, socket));
      }
    }
  }
  results_ = sockets_tcp_server::GetSockets::Results::Create(socket_infos);
}

}  // namespace core_api
}  // namespace extensions
