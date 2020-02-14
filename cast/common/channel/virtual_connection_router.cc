// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_router.h"

#include "cast/common/channel/cast_message_handler.h"
#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {

using ::cast::channel::CastMessage;

VirtualConnectionRouter::VirtualConnectionRouter(
    VirtualConnectionManager* vc_manager)
    : vc_manager_(vc_manager) {
  OSP_DCHECK(vc_manager);
}

VirtualConnectionRouter::~VirtualConnectionRouter() = default;

bool VirtualConnectionRouter::AddHandlerForLocalId(
    std::string local_id,
    CastMessageHandler* endpoint) {
  return endpoints_.emplace(std::move(local_id), endpoint).second;
}

bool VirtualConnectionRouter::RemoveHandlerForLocalId(
    const std::string& local_id) {
  return endpoints_.erase(local_id) == 1u;
}

void VirtualConnectionRouter::TakeSocket(SocketErrorHandler* error_handler,
                                         std::unique_ptr<CastSocket> socket) {
  int id = socket->socket_id();
  socket->SetClient(this);
  sockets_.emplace(id, SocketWithHandler{std::move(socket), error_handler});
}

void VirtualConnectionRouter::CloseSocket(int id) {
  auto it = sockets_.find(id);
  if (it != sockets_.end()) {
    vc_manager_->RemoveConnectionsBySocketId(
        id, VirtualConnection::kTransportClosed);
    std::unique_ptr<CastSocket> socket = std::move(it->second.socket);
    SocketErrorHandler* error_handler = it->second.error_handler;
    sockets_.erase(it);
    error_handler->OnClose(socket.get());
  }
}

Error VirtualConnectionRouter::SendMessage(VirtualConnection virtual_conn,
                                           CastMessage message) {
  // TODO(btolsch): Check for broadcast message.
  if (!IsTransportNamespace(message.namespace_()) &&
      !vc_manager_->GetConnectionData(virtual_conn)) {
    return Error::Code::kNoActiveConnection;
  }
  auto it = sockets_.find(virtual_conn.socket_id);
  if (it == sockets_.end()) {
    return Error::Code::kItemNotFound;
  }
  message.set_source_id(std::move(virtual_conn.local_id));
  message.set_destination_id(std::move(virtual_conn.peer_id));
  return it->second.socket->SendMessage(message);
}

void VirtualConnectionRouter::OnError(CastSocket* socket, Error error) {
  int id = socket->socket_id();
  auto it = sockets_.find(id);
  if (it != sockets_.end()) {
    vc_manager_->RemoveConnectionsBySocketId(id, VirtualConnection::kUnknown);
    std::unique_ptr<CastSocket> socket_owned = std::move(it->second.socket);
    SocketErrorHandler* error_handler = it->second.error_handler;
    sockets_.erase(it);
    error_handler->OnError(socket, error);
  }
}

void VirtualConnectionRouter::OnMessage(CastSocket* socket,
                                        CastMessage message) {
  // TODO(btolsch): Check for broadcast message.
  VirtualConnection virtual_conn{message.destination_id(), message.source_id(),
                                 socket->socket_id()};
  if (!IsTransportNamespace(message.namespace_()) &&
      !vc_manager_->GetConnectionData(virtual_conn)) {
    return;
  }
  const std::string& local_id = message.destination_id();
  auto it = endpoints_.find(local_id);
  if (it != endpoints_.end()) {
    it->second->OnMessage(this, socket, std::move(message));
  }
}

}  // namespace cast
}  // namespace openscreen
