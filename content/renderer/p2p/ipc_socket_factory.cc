// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/ipc_socket_factory.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "content/renderer/p2p/socket_client.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "jingle/glue/utils.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"

namespace {

// IpcPacketSocket implements talk_base::AsyncPacketSocket interface
// using P2PSocketClient that works over IPC-channel. It must be used
// on the thread it was created.
class IpcPacketSocket : public talk_base::AsyncPacketSocket,
                        public P2PSocketClient::Delegate {
 public:
  IpcPacketSocket();
  virtual ~IpcPacketSocket();

  // Always takes ownership of client even if initialization fails.
  bool Init(P2PSocketType type, P2PSocketClient* client,
            const talk_base::SocketAddress& local_address,
            const talk_base::SocketAddress& remote_address);

  // talk_base::AsyncPacketSocket interface.
  virtual bool GetLocalAddress(talk_base::SocketAddress* address) const;
  virtual talk_base::SocketAddress GetRemoteAddress() const;
  virtual int Send(const void *pv, size_t cb);
  virtual int SendTo(const void *pv, size_t cb,
                     const talk_base::SocketAddress& addr);
  virtual int Close();
  virtual talk_base::Socket::ConnState GetState() const;
  virtual int GetOption(talk_base::Socket::Option opt, int* value);
  virtual int SetOption(talk_base::Socket::Option opt, int value);
  virtual int GetError() const;
  virtual void SetError(int error);

  // P2PSocketClient::Delegate implementation.
  virtual void OnOpen(const net::IPEndPoint& address) OVERRIDE;
  virtual void OnIncomingTcpConnection(const net::IPEndPoint& address,
                                       P2PSocketClient* client) OVERRIDE;
  virtual void OnError();
  virtual void OnDataReceived(const net::IPEndPoint& address,
                              const std::vector<char>& data) OVERRIDE;

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_OPENING,
    STATE_OPEN,
    STATE_CLOSED,
    STATE_ERROR,
  };

  void InitAcceptedTcp(P2PSocketClient* client,
                       const talk_base::SocketAddress& local_address,
                       const talk_base::SocketAddress& remote_address);

  // Message loop on which this socket was created and being used.
  MessageLoop* message_loop_;

  // Corresponding P2P socket client.
  scoped_refptr<P2PSocketClient> client_;

  // Local address is allocated by the browser process, and the
  // renderer side doesn't know the address until it receives OnOpen()
  // event from the browser.
  talk_base::SocketAddress local_address_;
  bool address_initialized_;

  // Remote address for client TCP connections.
  talk_base::SocketAddress remote_address_;

  // Current state of the object.
  State state_;

  // Current error code. Valid when state_ == STATE_ERROR.
  int error_;

  DISALLOW_COPY_AND_ASSIGN(IpcPacketSocket);
};

IpcPacketSocket::IpcPacketSocket()
    : message_loop_(MessageLoop::current()),
      address_initialized_(false),
      state_(STATE_UNINITIALIZED), error_(0) {
}

IpcPacketSocket::~IpcPacketSocket() {
  if (state_ == STATE_OPENING || state_ == STATE_OPEN ||
      state_ == STATE_ERROR) {
    Close();
  }
}

bool IpcPacketSocket::Init(P2PSocketType type, P2PSocketClient* client,
                           const talk_base::SocketAddress& local_address,
                           const talk_base::SocketAddress& remote_address) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  client_ = client;
  local_address_ = local_address;
  remote_address_ = remote_address;
  state_ = STATE_OPENING;

  net::IPEndPoint local_endpoint;
  if (!jingle_glue::SocketAddressToIPEndPoint(local_address, &local_endpoint)) {
    return false;
  }

  net::IPEndPoint remote_endpoint;
  if (!jingle_glue::SocketAddressToIPEndPoint(
          remote_address, &remote_endpoint)) {
    return false;
  }

  client_->Init(type, local_endpoint, remote_endpoint, this,
                base::MessageLoopProxy::CreateForCurrentThread());

  return true;
}

void IpcPacketSocket::InitAcceptedTcp(
    P2PSocketClient* client,
    const talk_base::SocketAddress& local_address,
    const talk_base::SocketAddress& remote_address) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  client_ = client;
  local_address_ = local_address;
  remote_address_ = remote_address;
  state_ = STATE_OPEN;
  client_->set_delegate(this);
}

// talk_base::AsyncPacketSocket interface.
bool IpcPacketSocket::GetLocalAddress(talk_base::SocketAddress* address) const {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (!address_initialized_)
    return false;

  *address = local_address_;
  return true;
}

talk_base::SocketAddress IpcPacketSocket::GetRemoteAddress() const {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  return remote_address_;
}

int IpcPacketSocket::Send(const void *data, size_t data_size) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  return SendTo(data, data_size, remote_address_);
}

int IpcPacketSocket::SendTo(const void *data, size_t data_size,
                            const talk_base::SocketAddress& address) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  switch (state_) {
    case STATE_UNINITIALIZED:
      NOTREACHED();
      return EWOULDBLOCK;
    case STATE_OPENING:
      return EWOULDBLOCK;
    case STATE_CLOSED:
      return ENOTCONN;
    case STATE_ERROR:
      return error_;
    case STATE_OPEN:
      // Continue sending the packet.
      break;
  }

  const char* data_char = reinterpret_cast<const char*>(data);
  std::vector<char> data_vector(data_char, data_char + data_size);

  net::IPEndPoint address_chrome;
  if (!jingle_glue::SocketAddressToIPEndPoint(address, &address_chrome)) {
    // Just drop the packet if we failed to convert the address.
    return 0;
  }

  client_->Send(address_chrome, data_vector);

  // Fake successful send. The caller ignores result anyway.
  return data_size;
}

int IpcPacketSocket::Close() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  client_->Close();
  state_ = STATE_CLOSED;

  return 0;
}

talk_base::Socket::ConnState IpcPacketSocket::GetState() const {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  switch (state_) {
    case STATE_UNINITIALIZED:
      NOTREACHED();
      return talk_base::Socket::CS_CONNECTING;

    case STATE_OPENING:
      return talk_base::Socket::CS_CONNECTING;

    case STATE_OPEN:
      return talk_base::Socket::CS_CONNECTED;

    case STATE_CLOSED:
    case STATE_ERROR:
      return talk_base::Socket::CS_CLOSED;
  }

  NOTREACHED();
  return talk_base::Socket::CS_CLOSED;
}

int IpcPacketSocket::GetOption(talk_base::Socket::Option opt, int* value) {
  // We don't support socket options for IPC sockets.
  return -1;
}

int IpcPacketSocket::SetOption(talk_base::Socket::Option opt, int value) {
  // We don't support socket options for IPC sockets.
  //
  // TODO(sergeyu): Make sure we set proper socket options on the
  // browser side.
  return -1;
}

int IpcPacketSocket::GetError() const {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  return error_;
}

void IpcPacketSocket::SetError(int error) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  error_ = error;
}

void IpcPacketSocket::OnOpen(const net::IPEndPoint& address) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (!jingle_glue::IPEndPointToSocketAddress(address, &local_address_)) {
    // Always expect correct IPv4 address to be allocated.
    NOTREACHED();
    OnError();
    return;
  }
  SignalAddressReady(this, local_address_);
  address_initialized_ = true;
  state_ = STATE_OPEN;
}

void IpcPacketSocket::OnIncomingTcpConnection(
    const net::IPEndPoint& address,
    P2PSocketClient* client) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  scoped_ptr<IpcPacketSocket> socket(new IpcPacketSocket());

  talk_base::SocketAddress remote_address;
  if (!jingle_glue::IPEndPointToSocketAddress(address, &remote_address)) {
    // Always expect correct IPv4 address to be allocated.
    NOTREACHED();
  }
  socket->InitAcceptedTcp(client, local_address_, remote_address);
  SignalNewConnection(this, socket.release());
}

void IpcPacketSocket::OnError() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  state_ = STATE_ERROR;
  error_ = ECONNABORTED;
}

void IpcPacketSocket::OnDataReceived(const net::IPEndPoint& address,
                                     const std::vector<char>& data) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  talk_base::SocketAddress address_lj;
  if (!jingle_glue::IPEndPointToSocketAddress(address, &address_lj)) {
    // We should always be able to convert address here because we
    // don't expect IPv6 address on IPv4 connections.
    NOTREACHED();
    return;
  }

  SignalReadPacket(this, &data[0], data.size(), address_lj);
}

}  // namespace

IpcPacketSocketFactory::IpcPacketSocketFactory(
    P2PSocketDispatcher* socket_dispatcher)
    : socket_dispatcher_(socket_dispatcher) {
}

IpcPacketSocketFactory::~IpcPacketSocketFactory() {
}

talk_base::AsyncPacketSocket* IpcPacketSocketFactory::CreateUdpSocket(
    const talk_base::SocketAddress& local_address, int min_port, int max_port) {
  talk_base::SocketAddress crome_address;
  P2PSocketClient* socket_client = new P2PSocketClient(socket_dispatcher_);
  scoped_ptr<IpcPacketSocket> socket(new IpcPacketSocket());
  // TODO(sergeyu): Respect local_address and port limits here (need
  // to pass them over IPC channel to the browser).
  if (!socket->Init(P2P_SOCKET_UDP, socket_client,
                    local_address, talk_base::SocketAddress())) {
    return NULL;
  }
  return socket.release();
}

talk_base::AsyncPacketSocket* IpcPacketSocketFactory::CreateServerTcpSocket(
    const talk_base::SocketAddress& local_address, int min_port, int max_port,
    bool ssl) {
  // TODO(sergeyu): Implement SSL support.
  if (ssl)
    return NULL;

  talk_base::SocketAddress crome_address;
  P2PSocketClient* socket_client = new P2PSocketClient(socket_dispatcher_);
  scoped_ptr<IpcPacketSocket> socket(new IpcPacketSocket());
  if (!socket->Init(P2P_SOCKET_TCP_SERVER, socket_client, local_address,
                    talk_base::SocketAddress())) {
    return NULL;
  }
  return socket.release();
}

talk_base::AsyncPacketSocket* IpcPacketSocketFactory::CreateClientTcpSocket(
    const talk_base::SocketAddress& local_address,
    const talk_base::SocketAddress& remote_address,
    const talk_base::ProxyInfo& proxy_info,
    const std::string& user_agent, bool ssl) {
  // TODO(sergeyu): Implement SSL support.
  if (ssl)
    return NULL;

  talk_base::SocketAddress crome_address;
  P2PSocketClient* socket_client = new P2PSocketClient(socket_dispatcher_);
  scoped_ptr<IpcPacketSocket> socket(new IpcPacketSocket());
  if (!socket->Init(P2P_SOCKET_TCP_CLIENT, socket_client, local_address,
                    remote_address))
    return NULL;
  return socket.release();
}
