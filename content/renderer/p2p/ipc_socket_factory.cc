// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/ipc_socket_factory.h"

#include <algorithm>
#include <deque>

#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/public/renderer/p2p_socket_client_delegate.h"
#include "content/renderer/p2p/host_address_request.h"
#include "content/renderer/p2p/socket_client_impl.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "jingle/glue/utils.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"

namespace content {

namespace {

const int kDefaultNonSetOptionValue = -1;

bool IsTcpClientSocket(P2PSocketType type) {
  return (type == P2P_SOCKET_STUN_TCP_CLIENT) ||
         (type == P2P_SOCKET_TCP_CLIENT) ||
         (type == P2P_SOCKET_STUN_SSLTCP_CLIENT) ||
         (type == P2P_SOCKET_SSLTCP_CLIENT) ||
         (type == P2P_SOCKET_TLS_CLIENT) ||
         (type == P2P_SOCKET_STUN_TLS_CLIENT);
}

bool JingleSocketOptionToP2PSocketOption(talk_base::Socket::Option option,
                                         P2PSocketOption* ipc_option) {
  switch (option) {
    case talk_base::Socket::OPT_RCVBUF:
      *ipc_option = P2P_SOCKET_OPT_RCVBUF;
      break;
    case talk_base::Socket::OPT_SNDBUF:
      *ipc_option = P2P_SOCKET_OPT_SNDBUF;
      break;
    case talk_base::Socket::OPT_DSCP:
      *ipc_option = P2P_SOCKET_OPT_DSCP;
      break;
    case talk_base::Socket::OPT_DONTFRAGMENT:
    case talk_base::Socket::OPT_NODELAY:
    case talk_base::Socket::OPT_IPV6_V6ONLY:
    case talk_base::Socket::OPT_RTP_SENDTIME_EXTN_ID:
      return false;  // Not supported by the chrome sockets.
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

// TODO(miu): This needs tuning.  http://crbug.com/237960
const size_t kMaximumInFlightBytes = 64 * 1024;  // 64 KB

// IpcPacketSocket implements talk_base::AsyncPacketSocket interface
// using P2PSocketClient that works over IPC-channel. It must be used
// on the thread it was created.
class IpcPacketSocket : public talk_base::AsyncPacketSocket,
                        public P2PSocketClientDelegate {
 public:
  IpcPacketSocket();
  virtual ~IpcPacketSocket();

  // Always takes ownership of client even if initialization fails.
  bool Init(P2PSocketType type, P2PSocketClientImpl* client,
            const talk_base::SocketAddress& local_address,
            const talk_base::SocketAddress& remote_address);

  // talk_base::AsyncPacketSocket interface.
  virtual talk_base::SocketAddress GetLocalAddress() const OVERRIDE;
  virtual talk_base::SocketAddress GetRemoteAddress() const OVERRIDE;
  virtual int Send(const void *pv, size_t cb,
                   const talk_base::PacketOptions& options) OVERRIDE;
  virtual int SendTo(const void *pv, size_t cb,
                     const talk_base::SocketAddress& addr,
                     const talk_base::PacketOptions& options) OVERRIDE;
  virtual int Close() OVERRIDE;
  virtual State GetState() const OVERRIDE;
  virtual int GetOption(talk_base::Socket::Option option, int* value) OVERRIDE;
  virtual int SetOption(talk_base::Socket::Option option, int value) OVERRIDE;
  virtual int GetError() const OVERRIDE;
  virtual void SetError(int error) OVERRIDE;

  // P2PSocketClientDelegate implementation.
  virtual void OnOpen(const net::IPEndPoint& address) OVERRIDE;
  virtual void OnIncomingTcpConnection(
      const net::IPEndPoint& address,
      P2PSocketClient* client) OVERRIDE;
  virtual void OnSendComplete() OVERRIDE;
  virtual void OnError() OVERRIDE;
  virtual void OnDataReceived(const net::IPEndPoint& address,
                              const std::vector<char>& data,
                              const base::TimeTicks& timestamp) OVERRIDE;

 private:
  enum InternalState {
    IS_UNINITIALIZED,
    IS_OPENING,
    IS_OPEN,
    IS_CLOSED,
    IS_ERROR,
  };

  // Update trace of send throttling internal state. This should be called
  // immediately after any changes to |send_bytes_available_| and/or
  // |in_flight_packet_sizes_|.
  void TraceSendThrottlingState() const;

  void InitAcceptedTcp(P2PSocketClient* client,
                       const talk_base::SocketAddress& local_address,
                       const talk_base::SocketAddress& remote_address);

  int DoSetOption(P2PSocketOption option, int value);

  P2PSocketType type_;

  // Message loop on which this socket was created and being used.
  base::MessageLoop* message_loop_;

  // Corresponding P2P socket client.
  scoped_refptr<P2PSocketClient> client_;

  // Local address is allocated by the browser process, and the
  // renderer side doesn't know the address until it receives OnOpen()
  // event from the browser.
  talk_base::SocketAddress local_address_;

  // Remote address for client TCP connections.
  talk_base::SocketAddress remote_address_;

  // Current state of the object.
  InternalState state_;

  // Track the number of bytes allowed to be sent non-blocking. This is used to
  // throttle the sending of packets to the browser process. For each packet
  // sent, the value is decreased. As callbacks to OnSendComplete() (as IPCs
  // from the browser process) are made, the value is increased back. This
  // allows short bursts of high-rate sending without dropping packets, but
  // quickly restricts the client to a sustainable steady-state rate.
  size_t send_bytes_available_;
  std::deque<size_t> in_flight_packet_sizes_;

  // Set to true once EWOULDBLOCK was returned from Send(). Indicates that the
  // caller expects SignalWritable notification.
  bool writable_signal_expected_;

  // Current error code. Valid when state_ == IS_ERROR.
  int error_;
  int options_[P2P_SOCKET_OPT_MAX];

  DISALLOW_COPY_AND_ASSIGN(IpcPacketSocket);
};

IpcPacketSocket::IpcPacketSocket()
    : type_(P2P_SOCKET_UDP),
      message_loop_(base::MessageLoop::current()),
      state_(IS_UNINITIALIZED),
      send_bytes_available_(kMaximumInFlightBytes),
      writable_signal_expected_(false),
      error_(0) {
  COMPILE_ASSERT(kMaximumInFlightBytes > 0, would_send_at_zero_rate);
  std::fill_n(options_, static_cast<int> (P2P_SOCKET_OPT_MAX),
              kDefaultNonSetOptionValue);
}

IpcPacketSocket::~IpcPacketSocket() {
  if (state_ == IS_OPENING || state_ == IS_OPEN ||
      state_ == IS_ERROR) {
    Close();
  }
}

void IpcPacketSocket::TraceSendThrottlingState() const {
  TRACE_COUNTER_ID1("p2p", "P2PSendBytesAvailable", local_address_.port(),
                    send_bytes_available_);
  TRACE_COUNTER_ID1("p2p", "P2PSendPacketsInFlight", local_address_.port(),
                    in_flight_packet_sizes_.size());
}

bool IpcPacketSocket::Init(P2PSocketType type,
                           P2PSocketClientImpl* client,
                           const talk_base::SocketAddress& local_address,
                           const talk_base::SocketAddress& remote_address) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  DCHECK_EQ(state_, IS_UNINITIALIZED);

  type_ = type;
  client_ = client;
  local_address_ = local_address;
  remote_address_ = remote_address;
  state_ = IS_OPENING;

  net::IPEndPoint local_endpoint;
  if (!jingle_glue::SocketAddressToIPEndPoint(
          local_address, &local_endpoint)) {
    return false;
  }

  net::IPEndPoint remote_endpoint;
  if (!remote_address.IsNil() &&
      !jingle_glue::SocketAddressToIPEndPoint(
          remote_address, &remote_endpoint)) {
    return false;
  }

  client->Init(type, local_endpoint, remote_endpoint, this);

  return true;
}

void IpcPacketSocket::InitAcceptedTcp(
    P2PSocketClient* client,
    const talk_base::SocketAddress& local_address,
    const talk_base::SocketAddress& remote_address) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  DCHECK_EQ(state_, IS_UNINITIALIZED);

  client_ = client;
  local_address_ = local_address;
  remote_address_ = remote_address;
  state_ = IS_OPEN;
  TraceSendThrottlingState();
  client_->SetDelegate(this);
}

// talk_base::AsyncPacketSocket interface.
talk_base::SocketAddress IpcPacketSocket::GetLocalAddress() const {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  return local_address_;
}

talk_base::SocketAddress IpcPacketSocket::GetRemoteAddress() const {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  return remote_address_;
}

int IpcPacketSocket::Send(const void *data, size_t data_size,
                          const talk_base::PacketOptions& options) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  return SendTo(data, data_size, remote_address_, options);
}

int IpcPacketSocket::SendTo(const void *data, size_t data_size,
                            const talk_base::SocketAddress& address,
                            const talk_base::PacketOptions& options) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  switch (state_) {
    case IS_UNINITIALIZED:
      NOTREACHED();
      return EWOULDBLOCK;
    case IS_OPENING:
      return EWOULDBLOCK;
    case IS_CLOSED:
      return ENOTCONN;
    case IS_ERROR:
      return error_;
    case IS_OPEN:
      // Continue sending the packet.
      break;
  }

  if (data_size == 0) {
    NOTREACHED();
    return 0;
  }

  if (data_size > send_bytes_available_) {
    TRACE_EVENT_INSTANT1("p2p", "MaxPendingBytesWouldBlock",
                         TRACE_EVENT_SCOPE_THREAD,
                         "id",
                         client_->GetSocketID());
    writable_signal_expected_ = true;
    error_ = EWOULDBLOCK;
    return -1;
  }

  net::IPEndPoint address_chrome;
  if (!jingle_glue::SocketAddressToIPEndPoint(address, &address_chrome)) {
    NOTREACHED();
    error_ = EINVAL;
    return -1;
  }

  send_bytes_available_ -= data_size;
  in_flight_packet_sizes_.push_back(data_size);
  TraceSendThrottlingState();

  const char* data_char = reinterpret_cast<const char*>(data);
  std::vector<char> data_vector(data_char, data_char + data_size);
  client_->SendWithOptions(address_chrome, data_vector, options);

  // Fake successful send. The caller ignores result anyway.
  return data_size;
}

int IpcPacketSocket::Close() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  client_->Close();
  state_ = IS_CLOSED;

  return 0;
}

talk_base::AsyncPacketSocket::State IpcPacketSocket::GetState() const {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  switch (state_) {
    case IS_UNINITIALIZED:
      NOTREACHED();
      return STATE_CLOSED;

    case IS_OPENING:
      return STATE_BINDING;

    case IS_OPEN:
      if (IsTcpClientSocket(type_)) {
        return STATE_CONNECTED;
      } else {
        return STATE_BOUND;
      }

    case IS_CLOSED:
    case IS_ERROR:
      return STATE_CLOSED;
  }

  NOTREACHED();
  return STATE_CLOSED;
}

int IpcPacketSocket::GetOption(talk_base::Socket::Option option, int* value) {
  P2PSocketOption p2p_socket_option = P2P_SOCKET_OPT_MAX;
  if (!JingleSocketOptionToP2PSocketOption(option, &p2p_socket_option)) {
    // unsupported option.
    return -1;
  }

  *value = options_[p2p_socket_option];
  return 0;
}

int IpcPacketSocket::SetOption(talk_base::Socket::Option option, int value) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  P2PSocketOption p2p_socket_option = P2P_SOCKET_OPT_MAX;
  if (!JingleSocketOptionToP2PSocketOption(option, &p2p_socket_option)) {
    // Option is not supported.
    return -1;
  }

  options_[p2p_socket_option] = value;

  if (state_ == IS_OPEN) {
    // Options will be applied when state becomes IS_OPEN in OnOpen.
    return DoSetOption(p2p_socket_option, value);
  }
  return 0;
}

int IpcPacketSocket::DoSetOption(P2PSocketOption option, int value) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  DCHECK_EQ(state_, IS_OPEN);

  client_->SetOption(option, value);
  return 0;
}

int IpcPacketSocket::GetError() const {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  return error_;
}

void IpcPacketSocket::SetError(int error) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  error_ = error;
}

void IpcPacketSocket::OnOpen(const net::IPEndPoint& address) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  if (!jingle_glue::IPEndPointToSocketAddress(address, &local_address_)) {
    // Always expect correct IPv4 address to be allocated.
    NOTREACHED();
    OnError();
    return;
  }

  state_ = IS_OPEN;
  TraceSendThrottlingState();

  // Set all pending options if any.
  for (int i = 0; i < P2P_SOCKET_OPT_MAX; ++i) {
    if (options_[i] != kDefaultNonSetOptionValue)
      DoSetOption(static_cast<P2PSocketOption> (i), options_[i]);
  }

  SignalAddressReady(this, local_address_);
  if (IsTcpClientSocket(type_))
    SignalConnect(this);
}

void IpcPacketSocket::OnIncomingTcpConnection(
    const net::IPEndPoint& address,
    P2PSocketClient* client) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  scoped_ptr<IpcPacketSocket> socket(new IpcPacketSocket());

  talk_base::SocketAddress remote_address;
  if (!jingle_glue::IPEndPointToSocketAddress(address, &remote_address)) {
    // Always expect correct IPv4 address to be allocated.
    NOTREACHED();
  }
  socket->InitAcceptedTcp(client, local_address_, remote_address);
  SignalNewConnection(this, socket.release());
}

void IpcPacketSocket::OnSendComplete() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  CHECK(!in_flight_packet_sizes_.empty());
  send_bytes_available_ += in_flight_packet_sizes_.front();
  DCHECK_LE(send_bytes_available_, kMaximumInFlightBytes);
  in_flight_packet_sizes_.pop_front();
  TraceSendThrottlingState();

  if (writable_signal_expected_ && send_bytes_available_ > 0) {
    SignalReadyToSend(this);
    writable_signal_expected_ = false;
  }
}

void IpcPacketSocket::OnError() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
  state_ = IS_ERROR;
  error_ = ECONNABORTED;
}

void IpcPacketSocket::OnDataReceived(const net::IPEndPoint& address,
                                     const std::vector<char>& data,
                                     const base::TimeTicks& timestamp) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  talk_base::SocketAddress address_lj;
  if (!jingle_glue::IPEndPointToSocketAddress(address, &address_lj)) {
    // We should always be able to convert address here because we
    // don't expect IPv6 address on IPv4 connections.
    NOTREACHED();
    return;
  }

  talk_base::PacketTime packet_time(timestamp.ToInternalValue(), 0);
  SignalReadPacket(this, &data[0], data.size(), address_lj,
                   packet_time);
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
  P2PSocketClientImpl* socket_client =
      new P2PSocketClientImpl(socket_dispatcher_);
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
    int opts) {
  // TODO(sergeyu): Implement SSL support.
  if (opts & talk_base::PacketSocketFactory::OPT_SSLTCP)
    return NULL;

  P2PSocketType type = (opts & talk_base::PacketSocketFactory::OPT_STUN) ?
      P2P_SOCKET_STUN_TCP_SERVER : P2P_SOCKET_TCP_SERVER;
  P2PSocketClientImpl* socket_client =
      new P2PSocketClientImpl(socket_dispatcher_);
  scoped_ptr<IpcPacketSocket> socket(new IpcPacketSocket());
  if (!socket->Init(type, socket_client, local_address,
                    talk_base::SocketAddress())) {
    return NULL;
  }
  return socket.release();
}

talk_base::AsyncPacketSocket* IpcPacketSocketFactory::CreateClientTcpSocket(
    const talk_base::SocketAddress& local_address,
    const talk_base::SocketAddress& remote_address,
    const talk_base::ProxyInfo& proxy_info,
    const std::string& user_agent, int opts) {
  P2PSocketType type;
  if (opts & talk_base::PacketSocketFactory::OPT_SSLTCP) {
    type = (opts & talk_base::PacketSocketFactory::OPT_STUN) ?
        P2P_SOCKET_STUN_SSLTCP_CLIENT : P2P_SOCKET_SSLTCP_CLIENT;
  } else if (opts & talk_base::PacketSocketFactory::OPT_TLS) {
    type = (opts & talk_base::PacketSocketFactory::OPT_STUN) ?
        P2P_SOCKET_STUN_TLS_CLIENT : P2P_SOCKET_TLS_CLIENT;
  } else {
    type = (opts & talk_base::PacketSocketFactory::OPT_STUN) ?
        P2P_SOCKET_STUN_TCP_CLIENT : P2P_SOCKET_TCP_CLIENT;
  }
  P2PSocketClientImpl* socket_client =
      new P2PSocketClientImpl(socket_dispatcher_);
  scoped_ptr<IpcPacketSocket> socket(new IpcPacketSocket());
  if (!socket->Init(type, socket_client, local_address, remote_address))
    return NULL;
  return socket.release();
}

talk_base::AsyncResolverInterface*
IpcPacketSocketFactory::CreateAsyncResolver() {
  return new P2PAsyncAddressResolver(socket_dispatcher_);
}

}  // namespace content
