// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/p2p/ipc_socket_factory.h"

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/renderer/p2p/socket_client.h"
#include "chrome/renderer/p2p/socket_dispatcher.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"

namespace {

const size_t kIPv4AddressSize = 4;

// Chromium and libjingle represent socket addresses differently. The
// following two functions are used to convert addresses from one
// representation to another.
bool ChromeToLibjingleSocketAddress(const net::IPEndPoint& address_chrome,
                                    talk_base::SocketAddress* address_lj) {
  if (address_chrome.GetFamily() != AF_INET) {
    LOG(ERROR) << "Only IPv4 addresses are supported.";
    return false;
  }
  uint32 ip_as_int = ntohl(*reinterpret_cast<const uint32*>(
      &address_chrome.address()[0]));
  *address_lj =  talk_base::SocketAddress(ip_as_int, address_chrome.port());
  return true;
}

bool LibjingleToIPEndPoint(const talk_base::SocketAddress& address_lj,
                           net::IPEndPoint* address_chrome) {
  uint32 ip = htonl(address_lj.ip());
  net::IPAddressNumber address;
  address.resize(kIPv4AddressSize);
  memcpy(&address[0], &ip, kIPv4AddressSize);
  *address_chrome = net::IPEndPoint(address, address_lj.port());
  return true;
}

// IpcPacketSocket implements talk_base::AsyncPacketSocket interface
// using P2PSocketClient that works over IPC-channel. It must be used
// on the thread it was created.
class IpcPacketSocket : public talk_base::AsyncPacketSocket,
                        public P2PSocketClient::Delegate {
 public:
  IpcPacketSocket();
  virtual ~IpcPacketSocket();

  bool Init(P2PSocketType type, P2PSocketClient* client,
            const talk_base::SocketAddress& address);

  // talk_base::AsyncPacketSocket interface.
  virtual talk_base::SocketAddress GetLocalAddress(bool* allocated) const;
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

  // P2PSocketClient::Delegate
  virtual void OnOpen(const net::IPEndPoint& address);
  virtual void OnError();
  virtual void OnDataReceived(const net::IPEndPoint& address,
                              const std::vector<char>& data);

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_OPENING,
    STATE_OPEN,
    STATE_CLOSED,
    STATE_ERROR,
  };

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
                           const talk_base::SocketAddress& address) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  client_ = client;
  remote_address_ = address;
  state_ = STATE_OPENING;

  net::IPEndPoint address_chrome;
  if (!LibjingleToIPEndPoint(address, &address_chrome)) {
    return false;
  }

  client_->Init(type, address_chrome, this,
                base::MessageLoopProxy::CreateForCurrentThread());

  return true;
}

// talk_base::AsyncPacketSocket interface.
talk_base::SocketAddress IpcPacketSocket::GetLocalAddress(
     bool* allocated) const {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  *allocated = address_initialized_;
  return local_address_;
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
  if (!LibjingleToIPEndPoint(address, &address_chrome)) {
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

  if (!ChromeToLibjingleSocketAddress(address, &local_address_)) {
    // Always expect correct IPv4 address to be allocated.
    NOTREACHED();
  }
  SignalAddressReady(this, local_address_);
  address_initialized_ = true;
  state_ = STATE_OPEN;
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
  if (!ChromeToLibjingleSocketAddress(address, &address_lj)) {
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
                    talk_base::SocketAddress())) {
    return NULL;
  }

  // Socket increments reference count if Init() was successful.
  return socket.release();
}

talk_base::AsyncPacketSocket* IpcPacketSocketFactory::CreateServerTcpSocket(
    const talk_base::SocketAddress& local_address, int min_port, int max_port,
    bool listen, bool ssl) {
  // TODO(sergeyu): Implement this;
  NOTIMPLEMENTED();
  return NULL;
}

talk_base::AsyncPacketSocket* IpcPacketSocketFactory::CreateClientTcpSocket(
    const talk_base::SocketAddress& local_address,
    const talk_base::SocketAddress& remote_address,
    const talk_base::ProxyInfo& proxy_info,
    const std::string& user_agent, bool ssl) {
  // TODO(sergeyu): Implement this;
  NOTIMPLEMENTED();
  return NULL;
}
