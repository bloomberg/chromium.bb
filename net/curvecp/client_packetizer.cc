// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/client_packetizer.h"

#include "base/bind.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"
#include "net/curvecp/protocol.h"
#include "net/udp/udp_client_socket.h"

namespace {

const int kMaxHelloAttempts = 8;
const int kHelloTimeoutMs[kMaxHelloAttempts] = {
  1000,  // 1 second, with 1.5x increase for each retry.
  1500,
  2250,
  3375,
  5063,
  7594,
 11391,
 17086,
};

}  // namespace

namespace net {

ClientPacketizer::ClientPacketizer()
    : Packetizer(),
      next_state_(NONE),
      listener_(NULL),
      user_callback_(NULL),
      current_address_(NULL),
      hello_attempts_(0),
      initiate_sent_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &ClientPacketizer::OnIOComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  // TODO(mbelshe): Initialize our keys and such properly.
  //     for now we use random values to keep them unique.
  for (int i = 0; i < 32; ++i)
    shortterm_public_key_[i] = rand() % 26 + 'a';
}

ClientPacketizer::~ClientPacketizer() {
}

int ClientPacketizer::Connect(const AddressList& server,
                              Packetizer::Listener* listener,
                              OldCompletionCallback* callback) {
  DCHECK(!user_callback_);
  DCHECK(!socket_.get());
  DCHECK(!listener_);

  listener_ = listener;

  addresses_ = server;

  user_callback_ = callback;
  next_state_ = LOOKUP_COOKIE;

  return DoLoop(OK);
}

int ClientPacketizer::SendMessage(ConnectionKey key,
                                  const char* data,
                                  size_t length,
                                  OldCompletionCallback* callback) {
  // We can't send messages smaller than 16 bytes.
  if (length < 16)
    return ERR_UNEXPECTED;

  if (!initiate_sent_) {
    const size_t kMaxMessageInInitiatePacket =
        kMaxPacketLength - sizeof(InitiatePacket);

    if (length > kMaxMessageInInitiatePacket) {
      NOTREACHED();
      return ERR_UNEXPECTED;
    }

    initiate_sent_ = true;

    // Bundle this message into the Initiate Packet.
    scoped_refptr<IOBuffer> buffer(new IOBuffer(kMaxPacketLength));
    InitiatePacket* packet = reinterpret_cast<InitiatePacket*>(buffer->data());
    memset(packet, 0, sizeof(InitiatePacket));
    memcpy(packet->id, "QvnQ5XlI", 8);
    memcpy(packet->client_shortterm_public_key, shortterm_public_key_,
           sizeof(shortterm_public_key_));
    // TODO(mbelshe): Fill in rest of Initiate fields here
    // TODO(mbelshe): Fill in rest of message
    //
    // TODO(mbelshe) - this is just broken to make it work with cleartext
    memcpy(&buffer->data()[sizeof(InitiatePacket)], data, length);
    int packet_length = sizeof(InitiatePacket) + length;
    int rv = socket_->Write(buffer, packet_length, &io_callback_);
    if (rv <= 0)
      return rv;
    CHECK_EQ(packet_length, rv);  // We must send all data.
    return length;
  }

  if (length > static_cast<size_t>(kMaxMessageLength)) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  // Bundle this message into the Initiate Packet.
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kMaxPacketLength));
  ClientMessagePacket* packet =
      reinterpret_cast<ClientMessagePacket*>(buffer->data());
  memset(packet, 0, sizeof(ClientMessagePacket));
  memcpy(packet->id, "QvnQ5XlM", 8);
  memcpy(packet->client_shortterm_public_key, shortterm_public_key_,
         sizeof(shortterm_public_key_));
  // TODO(mbelshe): Fill in rest of Initiate fields here
  // TODO(mbelshe): Fill in rest of message
  memcpy(&buffer->data()[sizeof(ClientMessagePacket)], data, length);
  int packet_length = sizeof(ClientMessagePacket) + length;
  int rv = socket_->Write(buffer, packet_length, &io_callback_);
  if (rv <= 0)
    return rv;
  CHECK_EQ(packet_length, rv);  // We must send all data.
  return length;
}

void ClientPacketizer::Close(ConnectionKey key) {
  // TODO(mbelshe): implement me
  NOTIMPLEMENTED();
}

int ClientPacketizer::GetPeerAddress(IPEndPoint* endpoint) const {
  return socket_->GetPeerAddress(endpoint);
}

int ClientPacketizer::max_message_payload() const {
  if (!initiate_sent_)
    return kMaxPacketLength - sizeof(InitiatePacket) - sizeof(Message);
  return kMaxMessageLength - sizeof(Message);
}

int ClientPacketizer::DoLoop(int result) {
  DCHECK(next_state_ != NONE);
  int rv = result;
  do {
    switch (next_state_) {
      case LOOKUP_COOKIE:
        rv = DoLookupCookie();
        break;
      case LOOKUP_COOKIE_COMPLETE:
        rv = DoLookupCookieComplete(rv);
        break;
      case SENDING_HELLO:
        rv = DoSendingHello();
        break;
      case SENDING_HELLO_COMPLETE:
        rv = DoSendingHelloComplete(rv);
        break;
      case WAITING_COOKIE:
        rv = DoWaitingCookie();
        break;
      case WAITING_COOKIE_COMPLETE:
        rv = DoWaitingCookieComplete(rv);
        break;
      case CONNECTED:
        rv = DoConnected(rv);
        break;
      default:
        NOTREACHED();
        break;
    }
  } while (rv > ERR_IO_PENDING && next_state_ != CONNECTED);

  return rv;
}

int ClientPacketizer::DoLookupCookie() {
  // Eventually, we'll use this state to see if we have persisted the cookie
  // in the disk cache.  For now, we don't do any persistence, even in memory.
  next_state_ = LOOKUP_COOKIE_COMPLETE;
  return OK;
}

int ClientPacketizer::DoLookupCookieComplete(int rv) {
  // TODO(mbelshe): If we got a cookie, goto state WAITING_COOKIE_COMPLETE
  next_state_ = SENDING_HELLO;
  return rv;
}

int ClientPacketizer::DoSendingHello() {
  next_state_ = SENDING_HELLO_COMPLETE;

  if (hello_attempts_ == kMaxHelloAttempts)
    return ERR_TIMED_OUT;

  // Connect to the next socket
  int rv = ConnectNextAddress();
  if (rv < 0) {
    LOG(ERROR) << "Could not get next address!";
    return rv;
  }

  // Construct Hello Packet
  scoped_refptr<IOBuffer> buffer(new IOBuffer(sizeof(struct HelloPacket)));
  struct HelloPacket* data =
      reinterpret_cast<struct HelloPacket*>(buffer->data());
  memset(data, 0, sizeof(struct HelloPacket));
  memcpy(data->id, "QvnQ5XlH", 8);
  memcpy(data->client_shortterm_public_key, shortterm_public_key_,
         sizeof(shortterm_public_key_));
  // TODO(mbelshe): populate all other fields of the HelloPacket.

  return socket_->Write(buffer, sizeof(struct HelloPacket), &io_callback_);
}

int ClientPacketizer::DoSendingHelloComplete(int rv) {
  next_state_ = NONE;

  if (rv < 0)
    return rv;

  // Writing to UDP should not result in a partial datagram.
  if (rv != sizeof(struct HelloPacket))
    return ERR_FAILED;

  next_state_ = WAITING_COOKIE;
  return OK;
}

int ClientPacketizer::DoWaitingCookie() {
  next_state_ = WAITING_COOKIE_COMPLETE;

  StartHelloTimer(kHelloTimeoutMs[hello_attempts_++]);

  read_buffer_ = new IOBuffer(kMaxPacketLength);
  return socket_->Read(read_buffer_, kMaxPacketLength, &io_callback_);
}

int ClientPacketizer::DoWaitingCookieComplete(int rv) {
  // TODO(mbelshe): Add Histogram for hello_attempts_.
  RevokeHelloTimer();

  // TODO(mbelshe): Validate the cookie here
  if (rv < 0)
    return rv;

  if (rv == 0) {  // Does this happen?
    NOTREACHED();
    return ERR_FAILED;
  }

  if (rv !=  sizeof(struct CookiePacket))
    return ERR_FAILED;  // TODO(mbelshe): more specific error message.

  // TODO(mbelshe): verify contents of Cookie

  listener_->OnConnection(shortterm_public_key_);

  next_state_ = CONNECTED;

  // Start reading for messages
  rv = ReadPackets();
  if (rv == ERR_IO_PENDING)
    rv = OK;
  return rv;
}

int ClientPacketizer::DoConnected(int rv) {
  DCHECK(next_state_ == CONNECTED);
  if (rv > 0)
    ProcessRead(rv);
  return ReadPackets();
}

void ClientPacketizer::DoCallback(int result) {
  DCHECK_NE(result, ERR_IO_PENDING);
  DCHECK(user_callback_);

  OldCompletionCallback* callback = user_callback_;
  user_callback_ = NULL;
  callback->Run(result);
}

int ClientPacketizer::ConnectNextAddress() {
  // TODO(mbelshe): plumb Netlog information

  DCHECK(addresses_.head());

  socket_.reset(new UDPClientSocket(DatagramSocket::DEFAULT_BIND,
                                    RandIntCallback(),
                                    NULL,
                                    NetLog::Source()));

  // Rotate to next address in the list.
  if (current_address_)
    current_address_ = current_address_->ai_next;
  if (!current_address_)
    current_address_ = addresses_.head();

  IPEndPoint endpoint;
  if (!endpoint.FromSockAddr(current_address_->ai_addr,
                             current_address_->ai_addrlen))
    return ERR_FAILED;

  int rv = socket_->Connect(endpoint);
  DCHECK_NE(ERR_IO_PENDING, rv);

  return rv;
}

void ClientPacketizer::StartHelloTimer(int milliseconds) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ClientPacketizer::OnHelloTimeout, weak_factory_.GetWeakPtr()),
      milliseconds);
}

void ClientPacketizer::RevokeHelloTimer() {
  weak_factory_.InvalidateWeakPtrs();
}

void ClientPacketizer::OnHelloTimeout() {
  DCHECK_EQ(WAITING_COOKIE_COMPLETE, next_state_);
  next_state_ = SENDING_HELLO;
  DLOG(INFO) << "HelloTimeout #" << hello_attempts_;
  int rv = DoLoop(OK);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

void ClientPacketizer::ProcessRead(int result) {
  DCHECK_GT(result, 0);
  DCHECK(listener_);

  // The smallest packet we can receive is a ServerMessagePacket.
  if (result < static_cast<int>(sizeof(ServerMessagePacket)) ||
      result > kMaxPacketLength)
    return;

  // Packets are always 16 byte padded.
  if (result & 15)
    return;

  Packet *packet = reinterpret_cast<Packet*>(read_buffer_->data());

  // The only type of packet we should receive at this point is a Message
  // packet.
  // TODO(mbelshe): what happens if the server sends a new Cookie packet?
  if (memcmp(packet->id, "RL3aNMXM", 8))
    return;

  uchar* msg = reinterpret_cast<uchar*>(packet);
  int length = result - sizeof(ServerMessagePacket);
  listener_->OnMessage(this,
                       shortterm_public_key_,
                       &msg[sizeof(ServerMessagePacket)],
                       length);
}

int ClientPacketizer::ReadPackets() {
  DCHECK(socket_.get());

  int rv;
  while (true) {
    rv = socket_->Read(read_buffer_,
                       kMaxPacketLength,
                       &io_callback_);
    if (rv <= 0) {
      if (rv != ERR_IO_PENDING)
        LOG(ERROR) << "Error reading socket:" << rv;
      return rv;
    }
    ProcessRead(rv);
  }
  return rv;
}

void ClientPacketizer::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

}  // namespace net
