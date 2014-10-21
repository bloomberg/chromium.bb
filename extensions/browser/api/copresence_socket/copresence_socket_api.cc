// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/copresence_socket/copresence_socket_api.h"

#include "base/lazy_instance.h"
#include "components/copresence_sockets/public/copresence_peer.h"
#include "components/copresence_sockets/public/copresence_socket.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/copresence_socket.h"
#include "net/base/io_buffer.h"

using copresence_sockets::CopresencePeer;
using copresence_sockets::CopresenceSocket;

namespace extensions {

class CopresencePeerResource : public ApiResource {
 public:
  // Takes ownership of peer.
  CopresencePeerResource(const std::string& owner_extension_id,
                         scoped_ptr<copresence_sockets::CopresencePeer> peer)
      : ApiResource(owner_extension_id), peer_(peer.Pass()) {}

  ~CopresencePeerResource() override {}

  copresence_sockets::CopresencePeer* peer() { return peer_.get(); }

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  scoped_ptr<copresence_sockets::CopresencePeer> peer_;

  DISALLOW_COPY_AND_ASSIGN(CopresencePeerResource);
};

class CopresenceSocketResource : public ApiResource {
 public:
  // Takes ownership of socket.
  CopresenceSocketResource(
      const std::string& owner_extension_id,
      scoped_ptr<copresence_sockets::CopresenceSocket> socket)
      : ApiResource(owner_extension_id), socket_(socket.Pass()) {}

  ~CopresenceSocketResource() override {}

  copresence_sockets::CopresenceSocket* socket() { return socket_.get(); }

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  scoped_ptr<copresence_sockets::CopresenceSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceSocketResource);
};

CopresenceSocketFunction::CopresenceSocketFunction()
    : peers_manager_(nullptr), sockets_manager_(nullptr) {
}

CopresenceSocketFunction::~CopresenceSocketFunction() {
  delete peers_manager_;
  delete sockets_manager_;
}

void CopresenceSocketFunction::Initialize() {
  peers_manager_ =
      new ApiResourceManager<CopresencePeerResource>(browser_context());
  sockets_manager_ =
      new ApiResourceManager<CopresenceSocketResource>(browser_context());
}

int CopresenceSocketFunction::AddPeer(CopresencePeerResource* peer) {
  return peers_manager_->Add(peer);
}

int CopresenceSocketFunction::AddSocket(CopresenceSocketResource* socket) {
  return sockets_manager_->Add(socket);
}

void CopresenceSocketFunction::ReplacePeer(const std::string& extension_id,
                                           int peer_id,
                                           CopresencePeerResource* peer) {
  peers_manager_->Replace(extension_id, peer_id, peer);
}

CopresencePeerResource* CopresenceSocketFunction::GetPeer(int peer_id) {
  return peers_manager_->Get(extension_id(), peer_id);
}

CopresenceSocketResource* CopresenceSocketFunction::GetSocket(int socket_id) {
  return sockets_manager_->Get(extension_id(), socket_id);
}

void CopresenceSocketFunction::RemovePeer(int peer_id) {
  peers_manager_->Remove(extension_id(), peer_id);
}

void CopresenceSocketFunction::RemoveSocket(int socket_id) {
  sockets_manager_->Remove(extension_id(), socket_id);
}

void CopresenceSocketFunction::DispatchOnReceiveEvent(
    int socket_id,
    const scoped_refptr<net::IOBuffer>& buffer,
    int size) {
  core_api::copresence_socket::ReceiveInfo info;
  info.socket_id = socket_id;
  info.data = std::string(buffer->data(), size);
  // Send the data to the client app.
  scoped_ptr<Event> event(
      new Event(core_api::copresence_socket::OnReceive::kEventName,
                core_api::copresence_socket::OnReceive::Create(info),
                browser_context()));
  EventRouter::Get(browser_context())
      ->DispatchEventToExtension(extension_id(), event.Pass());
  VLOG(2) << "Dispatched OnReceive event: socketId = " << socket_id
          << " and data = " << info.data;
}

void CopresenceSocketFunction::DispatchOnConnectedEvent(
    int peer_id,
    scoped_ptr<copresence_sockets::CopresenceSocket> socket) {
  int socket_id =
      AddSocket(new CopresenceSocketResource(extension_id(), socket.Pass()));

  // Send the messages to the client app.
  scoped_ptr<Event> event(new Event(
      core_api::copresence_socket::OnConnected::kEventName,
      core_api::copresence_socket::OnConnected::Create(peer_id, socket_id),
      browser_context()));
  EventRouter::Get(browser_context())
      ->DispatchEventToExtension(extension_id(), event.Pass());
  VLOG(2) << "Dispatched OnConnected event: peerId = " << peer_id
          << " and socketId = " << socket_id;

  socket->Receive(base::Bind(base::Bind(
      &CopresenceSocketFunction::DispatchOnReceiveEvent, this, peer_id)));
}

ExtensionFunction::ResponseAction CopresenceSocketFunction::Run() {
  Initialize();
  return Execute();
}

// CopresenceSocketCreatePeerFunction implementation:
ExtensionFunction::ResponseAction
CopresenceSocketCreatePeerFunction::Execute() {
  // Add an empty peer to create a placeholder peer_id. We will need to bind
  // this id to the OnConnected event dispatcher, so we need it before we
  // create the actual peer. Once we have the peer created, we'll replace the
  // placeholder with the actual peer object.
  int peer_id = AddPeer(new CopresencePeerResource(extension_id(), nullptr));

  scoped_ptr<CopresencePeer> peer = make_scoped_ptr(new CopresencePeer(
      base::Bind(&CopresenceSocketCreatePeerFunction::OnCreated, this, peer_id),
      base::Bind(
          &CopresenceSocketFunction::DispatchOnConnectedEvent, this, peer_id)));

  ReplacePeer(extension_id(),
              peer_id,
              new CopresencePeerResource(extension_id(), peer.Pass()));

  return RespondLater();
}

void CopresenceSocketCreatePeerFunction::OnCreated(int peer_id,
                                                   const std::string& locator) {
  core_api::copresence_socket::PeerInfo peer_info;
  peer_info.peer_id = peer_id;
  peer_info.locator = locator;
  Respond(ArgumentList(
      core_api::copresence_socket::CreatePeer::Results::Create(peer_info)));
}

// CopresenceSocketDestroyPeerFunction implementation:
ExtensionFunction::ResponseAction
CopresenceSocketDestroyPeerFunction::Execute() {
  scoped_ptr<core_api::copresence_socket::DestroyPeer::Params> params(
      core_api::copresence_socket::DestroyPeer::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  RemovePeer(params->peer_id);
  return RespondNow(NoArguments());
}

// CopresenceSocketSendFunction implementation:
ExtensionFunction::ResponseAction CopresenceSocketSendFunction::Execute() {
  scoped_ptr<core_api::copresence_socket::Send::Params> params(
      core_api::copresence_socket::Send::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopresenceSocketResource* socket = GetSocket(params->socket_id);
  if (!socket) {
    VLOG(1) << "Socket not found. ID = " << params->socket_id;
    return RespondNow(
        ArgumentList(core_api::copresence_socket::Send::Results::Create(
            core_api::copresence_socket::SOCKET_STATUS_INVALID_SOCKET)));
  }

  socket->socket()->Send(new net::StringIOBuffer(params->data),
                         params->data.size());
  return RespondNow(
      ArgumentList(core_api::copresence_socket::Send::Results::Create(
          core_api::copresence_socket::SOCKET_STATUS_NO_ERROR)));
}

// CopresenceSocketDisconnectFunction implementation:
ExtensionFunction::ResponseAction
CopresenceSocketDisconnectFunction::Execute() {
  scoped_ptr<core_api::copresence_socket::Disconnect::Params> params(
      core_api::copresence_socket::Disconnect::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return RespondLater();
}

}  // namespace extensions
