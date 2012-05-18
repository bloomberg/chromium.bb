// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/server_packetizer.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/curvecp/protocol.h"
#include "net/udp/udp_server_socket.h"

namespace net {

ServerPacketizer::ServerPacketizer()
    : Packetizer(),
      state_(NONE),
      listener_(NULL),
      read_buffer_(new IOBuffer(kMaxPacketLength)) {
}

int ServerPacketizer::Listen(const IPEndPoint& endpoint,
                             Packetizer::Listener* listener) {
  DCHECK(!listener_);
  listener_ = listener;
  socket_.reset(new UDPServerSocket(NULL, NetLog::Source()));
  int rv = socket_->Listen(endpoint);
  if (rv != OK)
    return rv;

  return ReadPackets();
}

bool ServerPacketizer::Open(ConnectionKey key, Packetizer::Listener* listener) {
  DCHECK(listener_map_.find(key) == listener_map_.end());
  listener_map_[key] = listener;
  return true;
}

int ServerPacketizer::SendMessage(ConnectionKey key,
                                  const char* data,
                                  size_t length,
                                  const CompletionCallback& callback) {
  DCHECK(socket_.get());
  DCHECK_LT(0u, length);
  DCHECK_GT(kMaxPacketLength - sizeof(ServerMessagePacket), length);

  ConnectionMap::const_iterator it = connection_map_.find(key);
  if (it == connection_map_.end()) {
    LOG(ERROR) << "Unknown connection key";
    return ERR_FAILED;  // No route to the client!
  }
  IPEndPoint endpoint = it->second;

  // Build up a message packet.
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kMaxPacketLength));
  ServerMessagePacket* packet =
      reinterpret_cast<ServerMessagePacket*>(buffer->data());
  memset(packet, 0, sizeof(ServerMessagePacket));
  memcpy(packet->id, "RL3aNMXM", 8);
  memcpy(&buffer->data()[sizeof(ServerMessagePacket)], data, length);
  int packet_length = sizeof(ServerMessagePacket) + length;
  int rv = socket_->SendTo(buffer, packet_length, endpoint, callback);
  if (rv <= 0)
    return rv;
  CHECK_EQ(packet_length, rv);
  return length;  // The number of message bytes written.
}

void ServerPacketizer::Close(ConnectionKey key) {
  ListenerMap::iterator it = listener_map_.find(key);
  DCHECK(it != listener_map_.end());
  listener_map_.erase(it);
  socket_->Close();
  socket_.reset(NULL);
}

int ServerPacketizer::GetPeerAddress(IPEndPoint* endpoint) const {
  return socket_->GetPeerAddress(endpoint);
}

int ServerPacketizer::max_message_payload() const {
  return kMaxMessageLength - sizeof(Message);
}

void ServerPacketizer::ProcessRead(int result) {
  DCHECK_GT(result, 0);

  // The smallest packet we can receive is a ClientMessagePacket.
  if (result < static_cast<int>(sizeof(ClientMessagePacket)) ||
      result > kMaxPacketLength)
    return;

  // Packets are always 16 byte padded.
  if (result & 15)
    return;

  Packet *packet = reinterpret_cast<Packet*>(read_buffer_->data());
  if (memcmp(packet, "QvnQ5Xl", 7))
    return;

  switch (packet->id[7]) {
    case 'H':
      HandleHelloPacket(packet, result);
      break;
    case 'I':
      HandleInitiatePacket(packet, result);
      break;
    case 'M':
      HandleClientMessagePacket(packet, result);
      break;
  }
}

void ServerPacketizer::HandleHelloPacket(Packet* packet, int length) {
  if (length != sizeof(HelloPacket))
    return;

  LOG(ERROR) << "Received Hello Packet";

  HelloPacket* hello_packet = reinterpret_cast<HelloPacket*>(packet);

  // Handle HelloPacket
  scoped_refptr<IOBuffer> buffer(new IOBuffer(sizeof(struct CookiePacket)));
  struct CookiePacket* data =
      reinterpret_cast<struct CookiePacket*>(buffer->data());
  memset(data, 0, sizeof(struct CookiePacket));
  memcpy(data->id, "RL3aNMXK", 8);
  memcpy(data->client_extension, hello_packet->client_extension, 16);
  // TODO(mbelshe) Fill in the rest of the CookiePacket fields.

  // XXXMB - Can't have two pending writes at the same time...
  int rv = socket_->SendTo(buffer, sizeof(struct CookiePacket), recv_address_,
                           base::Bind(&ServerPacketizer::OnWriteComplete,
                                      base::Unretained(this)));
  DCHECK(rv == ERR_IO_PENDING || rv == sizeof(struct CookiePacket));
}

void ServerPacketizer::HandleInitiatePacket(Packet* packet, int length) {
  // Handle InitiatePacket
  LOG(ERROR) << "Received Initiate Packet";

  InitiatePacket* initiate_packet = reinterpret_cast<InitiatePacket*>(packet);

  // We have an active connection.
  AddConnection(initiate_packet->client_shortterm_public_key, recv_address_);

  listener_->OnConnection(initiate_packet->client_shortterm_public_key);

  // The initiate packet can carry a message.
  int message_length = length - sizeof(InitiatePacket);
  DCHECK_LT(0, message_length);
  if (message_length) {
    uchar* data = reinterpret_cast<uchar*>(packet);
    HandleMessage(initiate_packet->client_shortterm_public_key,
                  &data[sizeof(InitiatePacket)],
                  message_length);
  }
}

void ServerPacketizer::HandleClientMessagePacket(Packet* packet, int length) {
  // Handle Message
  if (length < 16)
    return;

  const int kMaxMessagePacketLength =
      kMaxMessageLength + sizeof(ClientMessagePacket);
  if (length > static_cast<int>(kMaxMessagePacketLength))
    return;

  ClientMessagePacket* message_packet =
      reinterpret_cast<ClientMessagePacket*>(packet);

  int message_length = length - sizeof(ClientMessagePacket);
  DCHECK_LT(0, message_length);
  if (message_length) {
    uchar* data = reinterpret_cast<uchar*>(packet);
    HandleMessage(message_packet->client_shortterm_public_key,
                  &data[sizeof(ClientMessagePacket)],
                  message_length);
  }
}

void ServerPacketizer::HandleMessage(ConnectionKey key,
                                     unsigned char* msg,
                                     int length) {
  ListenerMap::iterator it = listener_map_.find(key);
  if (it == listener_map_.end()) {
    // Received a message for a closed connection.
    return;
  }

  // Decode the message here

  Packetizer::Listener* listener = it->second;
  listener->OnMessage(this, key, msg, length);
}

void ServerPacketizer::AddConnection(ConnectionKey key,
                                     const IPEndPoint& endpoint) {
  DCHECK(connection_map_.find(key) == connection_map_.end());
  connection_map_[key] = endpoint;
}

void ServerPacketizer::RemoveConnection(ConnectionKey key) {
  DCHECK(connection_map_.find(key) != connection_map_.end());
  connection_map_.erase(key);
}

int ServerPacketizer::ReadPackets() {
  DCHECK(socket_.get());

  int rv;
  while (true) {
    rv = socket_->RecvFrom(read_buffer_,
                           kMaxPacketLength,
                           &recv_address_,
                           base::Bind(&ServerPacketizer::OnReadComplete,
                                      base::Unretained(this)));
    if (rv <= 0) {
      if (rv != ERR_IO_PENDING)
        LOG(ERROR) << "Error reading listen socket: " << rv;
      return rv;
    }

    ProcessRead(rv);
  }

  return rv;
}

ServerPacketizer::~ServerPacketizer() {}

void ServerPacketizer::OnReadComplete(int result) {
  if (result > 0)
    ProcessRead(result);
  ReadPackets();
}

void ServerPacketizer::OnWriteComplete(int result) {
  // TODO(mbelshe): do we need to do anything?
}

}
