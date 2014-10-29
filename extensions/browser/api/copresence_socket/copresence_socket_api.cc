// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/copresence_socket/copresence_socket_api.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/string_piece.h"
#include "components/copresence_sockets/public/copresence_peer.h"
#include "components/copresence_sockets/public/copresence_socket.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/copresence_socket/copresence_socket_resources.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/copresence_socket.h"
#include "net/base/io_buffer.h"

using copresence_sockets::CopresencePeer;
using copresence_sockets::CopresenceSocket;

namespace extensions {

namespace {

const size_t kSizeBytes = 2;
const char kToField[] = "to";
const char kDataField[] = "data";
const char kReplyToField[] = "replyTo";

bool Base64DecodeWithoutPadding(const std::string& data, std::string* out) {
  std::string ret = data;
  while (ret.size() % 4)
    ret.push_back('=');

  if (!base::Base64Decode(ret, &ret))
    return false;

  out->swap(ret);
  return true;
}

std::string Base64EncodeWithoutPadding(const std::string& data) {
  std::string ret = data;
  base::Base64Encode(ret, &ret);
  while (*(ret.end() - 1) == '=')
    ret.erase(ret.end() - 1);
  return ret;
}

// Create a message to send to another peer.
std::string CreateMessage(const std::string& data) {
  base::DictionaryValue dict;
  dict.SetInteger(kToField, 1);
  dict.SetString(kDataField, Base64EncodeWithoutPadding(data));
  // We have only one peer at the moment, always with the ID 1.
  dict.SetInteger(kReplyToField, 1);

  std::string json;
  base::JSONWriter::Write(&dict, &json);

  std::string message;
  message.push_back(static_cast<unsigned char>(json.size() & 0xff));
  message.push_back(static_cast<unsigned char>(json.size() >> 8));

  message.append(json);
  return message;
}

// Parse a message received from another peer.
bool ParseReceivedMessage(const std::string& message, std::string* data) {
  // The format of a received message is, first two bytes = size of the
  // message, the rest is a string with the message in JSON. Since we don't
  // have multi-part messages yet, we'll ignore the size bytes.
  base::StringPiece json(message.c_str() + kSizeBytes,
                         message.size() - kSizeBytes);
  scoped_ptr<base::Value> value(base::JSONReader::Read(json));

  // Check to see that we have a valid dictionary.
  base::DictionaryValue* dict = nullptr;
  if (!value || !value->GetAsDictionary(&dict) || !dict->HasKey(kDataField)) {
    LOG(WARNING) << "Invalid JSON: " << json;
    return false;
  }

  // The fields in the json string are,
  // to: Peer Id this message is meant for (unused atm).
  // data: Data content of the message.
  // replyTo: Sender of this message (in the locator data format - unused atm).
  dict->GetStringASCII(kDataField, data);
  if (!Base64DecodeWithoutPadding(*data, data))
    return false;
  return true;
}

}  // namespace

// CopresenceSocketFunction public methods:

CopresenceSocketFunction::CopresenceSocketFunction()
    : peers_manager_(nullptr), sockets_manager_(nullptr) {
}

void CopresenceSocketFunction::DispatchOnConnectedEvent(
    int peer_id,
    scoped_ptr<copresence_sockets::CopresenceSocket> socket) {
  // Save socket's pointer since we'll lose the scoper once we pass it to
  // AddSocket.
  copresence_sockets::CopresenceSocket* socket_ptr = socket.get();
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

  socket_ptr->Receive(base::Bind(
      base::Bind(&CopresenceSocketFunction::OnDataReceived, this, socket_id)));
}

// CopresenceSocketFunction protected methods:

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

ExtensionFunction::ResponseAction CopresenceSocketFunction::Run() {
  Initialize();
  return Execute();
}

// CopresenceSocketFunction private methods:

CopresenceSocketFunction::~CopresenceSocketFunction() {
}

void CopresenceSocketFunction::Initialize() {
  peers_manager_ =
      ApiResourceManager<CopresencePeerResource>::Get(browser_context());
  sockets_manager_ =
      ApiResourceManager<CopresenceSocketResource>::Get(browser_context());
}

void CopresenceSocketFunction::OnDataReceived(
    int socket_id,
    const scoped_refptr<net::IOBuffer>& buffer,
    int size) {
  CopresenceSocketResource* socket = GetSocket(socket_id);
  if (!socket) {
    VLOG(2) << "Receiving socket not found. ID = " << socket_id;
    return;
  }

  socket->add_to_packet(std::string(buffer->data(), size));
  const std::string& packet = socket->packet();
  if (packet.size() >= kSizeBytes) {
    int size =
        static_cast<int>(packet[1]) << 8 | (static_cast<int>(packet[0]) & 0xff);
    if (packet.size() >= (size + kSizeBytes)) {
      std::string message_data;
      if (ParseReceivedMessage(packet, &message_data))
        DispatchOnReceiveEvent(socket_id, message_data);
      socket->clear_packet();
    }
  }
}

void CopresenceSocketFunction::DispatchOnReceiveEvent(int socket_id,
                                                      const std::string& data) {
  core_api::copresence_socket::ReceiveInfo info;
  info.socket_id = socket_id;
  info.data = data;
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

  socket->socket()->Send(new net::StringIOBuffer(CreateMessage(params->data)),
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
