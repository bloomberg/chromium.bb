// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_socket.h"

#include <errno.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/posix/eintr_wrapper.h"
#include "net/base/address_list.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/socket_libevent.h"
#include "net/socket/socket_net_log_params.h"

// If we don't have a definition for TCPI_OPT_SYN_DATA, create one.
#ifndef TCPI_OPT_SYN_DATA
#define TCPI_OPT_SYN_DATA 32
#endif

namespace net {

namespace {

// SetTCPNoDelay turns on/off buffering in the kernel. By default, TCP sockets
// will wait up to 200ms for more data to complete a packet before transmitting.
// After calling this function, the kernel will not wait. See TCP_NODELAY in
// `man 7 tcp`.
bool SetTCPNoDelay(int fd, bool no_delay) {
  int on = no_delay ? 1 : 0;
  int error = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  return error == 0;
}

// SetTCPKeepAlive sets SO_KEEPALIVE.
bool SetTCPKeepAlive(int fd, bool enable, int delay) {
  int on = enable ? 1 : 0;
  if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on))) {
    PLOG(ERROR) << "Failed to set SO_KEEPALIVE on fd: " << fd;
    return false;
  }

  // If we disabled TCP keep alive, our work is done here.
  if (!enable)
    return true;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  // Set seconds until first TCP keep alive.
  if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &delay, sizeof(delay))) {
    PLOG(ERROR) << "Failed to set TCP_KEEPIDLE on fd: " << fd;
    return false;
  }
  // Set seconds between TCP keep alives.
  if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &delay, sizeof(delay))) {
    PLOG(ERROR) << "Failed to set TCP_KEEPINTVL on fd: " << fd;
    return false;
  }
#endif
  return true;
}

}  // namespace

//-----------------------------------------------------------------------------

TCPSocketLibevent::TCPSocketLibevent(NetLog* net_log,
                                     const NetLog::Source& source)
    : use_tcp_fastopen_(IsTCPFastOpenEnabled()),
      tcp_fastopen_connected_(false),
      fast_open_status_(FAST_OPEN_STATUS_UNKNOWN),
      logging_multiple_connect_attempts_(false),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE,
                      source.ToEventParametersCallback());
}

TCPSocketLibevent::~TCPSocketLibevent() {
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE);
  if (tcp_fastopen_connected_) {
    UMA_HISTOGRAM_ENUMERATION("Net.TcpFastOpenSocketConnection",
                              fast_open_status_, FAST_OPEN_MAX_VALUE);
  }
}

int TCPSocketLibevent::Open(AddressFamily family) {
  DCHECK(!socket_);
  socket_.reset(new SocketLibevent);
  int rv = socket_->Open(ConvertAddressFamily(family));
  if (rv != OK)
    socket_.reset();
  return rv;
}

int TCPSocketLibevent::AdoptConnectedSocket(int socket_fd,
                                            const IPEndPoint& peer_address) {
  DCHECK(!socket_);

  SockaddrStorage storage;
  if (!peer_address.ToSockAddr(storage.addr, &storage.addr_len) &&
      // For backward compatibility, allows the empty address.
      !(peer_address == IPEndPoint())) {
    return ERR_ADDRESS_INVALID;
  }

  socket_.reset(new SocketLibevent);
  int rv = socket_->AdoptConnectedSocket(socket_fd, storage);
  if (rv != OK)
    socket_.reset();
  return rv;
}

int TCPSocketLibevent::Bind(const IPEndPoint& address) {
  DCHECK(socket_);

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  return socket_->Bind(storage);
}

int TCPSocketLibevent::Listen(int backlog) {
  DCHECK(socket_);
  return socket_->Listen(backlog);
}

int TCPSocketLibevent::Accept(scoped_ptr<TCPSocketLibevent>* tcp_socket,
                              IPEndPoint* address,
                              const CompletionCallback& callback) {
  DCHECK(tcp_socket);
  DCHECK(!callback.is_null());
  DCHECK(socket_);
  DCHECK(!accept_socket_);

  net_log_.BeginEvent(NetLog::TYPE_TCP_ACCEPT);

  int rv = socket_->Accept(
      &accept_socket_,
      base::Bind(&TCPSocketLibevent::AcceptCompleted,
                 base::Unretained(this), tcp_socket, address, callback));
  if (rv != ERR_IO_PENDING)
    rv = HandleAcceptCompleted(tcp_socket, address, rv);
  return rv;
}

int TCPSocketLibevent::Connect(const IPEndPoint& address,
                               const CompletionCallback& callback) {
  DCHECK(socket_);

  if (!logging_multiple_connect_attempts_)
    LogConnectBegin(AddressList(address));

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT,
                      CreateNetLogIPEndPointCallback(&address));

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  if (use_tcp_fastopen_) {
    // With TCP FastOpen, we pretend that the socket is connected.
    DCHECK(!tcp_fastopen_connected_);
    socket_->SetPeerAddress(storage);
    return OK;
  }

  int rv = socket_->Connect(storage,
                            base::Bind(&TCPSocketLibevent::ConnectCompleted,
                                       base::Unretained(this), callback));
  if (rv != ERR_IO_PENDING)
    rv = HandleConnectCompleted(rv);
  return rv;
}

bool TCPSocketLibevent::IsConnected() const {
  if (!socket_)
    return false;

  if (use_tcp_fastopen_ && !tcp_fastopen_connected_ &&
      socket_->HasPeerAddress()) {
    // With TCP FastOpen, we pretend that the socket is connected.
    // This allows GetPeerAddress() to return peer_address_.
    return true;
  }

  return socket_->IsConnected();
}

bool TCPSocketLibevent::IsConnectedAndIdle() const {
  // TODO(wtc): should we also handle the TCP FastOpen case here,
  // as we do in IsConnected()?
  return socket_ && socket_->IsConnectedAndIdle();
}

int TCPSocketLibevent::Read(IOBuffer* buf,
                            int buf_len,
                            const CompletionCallback& callback) {
  DCHECK(socket_);
  DCHECK(!callback.is_null());

  int rv = socket_->Read(
      buf, buf_len,
      base::Bind(&TCPSocketLibevent::ReadCompleted,
                 // Grab a reference to |buf| so that ReadCompleted() can still
                 // use it when Read() completes, as otherwise, this transfers
                 // ownership of buf to socket.
                 base::Unretained(this), make_scoped_refptr(buf), callback));
  if (rv >= 0)
    RecordFastOpenStatus();
  if (rv != ERR_IO_PENDING)
    rv = HandleReadCompleted(buf, rv);
  return rv;
}

int TCPSocketLibevent::Write(IOBuffer* buf,
                             int buf_len,
                             const CompletionCallback& callback) {
  DCHECK(socket_);
  DCHECK(!callback.is_null());

  CompletionCallback write_callback =
      base::Bind(&TCPSocketLibevent::WriteCompleted,
                 // Grab a reference to |buf| so that WriteCompleted() can still
                 // use it when Write() completes, as otherwise, this transfers
                 // ownership of buf to socket.
                 base::Unretained(this), make_scoped_refptr(buf), callback);
  int rv;
  if (use_tcp_fastopen_ && !tcp_fastopen_connected_) {
    rv = TcpFastOpenWrite(buf, buf_len, write_callback);
  } else {
    rv = socket_->Write(buf, buf_len, write_callback);
  }

  if (rv != ERR_IO_PENDING)
    rv = HandleWriteCompleted(buf, rv);
  return rv;
}

int TCPSocketLibevent::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(address);

  if (!socket_)
    return ERR_SOCKET_NOT_CONNECTED;

  SockaddrStorage storage;
  int rv = socket_->GetLocalAddress(&storage);
  if (rv != OK)
    return rv;

  if (!address->FromSockAddr(storage.addr, storage.addr_len))
    return ERR_ADDRESS_INVALID;

  return OK;
}

int TCPSocketLibevent::GetPeerAddress(IPEndPoint* address) const {
  DCHECK(address);

  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;

  SockaddrStorage storage;
  int rv = socket_->GetPeerAddress(&storage);
  if (rv != OK)
    return rv;

  if (!address->FromSockAddr(storage.addr, storage.addr_len))
    return ERR_ADDRESS_INVALID;

  return OK;
}

int TCPSocketLibevent::SetDefaultOptionsForServer() {
  DCHECK(socket_);
  return SetAddressReuse(true);
}

void TCPSocketLibevent::SetDefaultOptionsForClient() {
  DCHECK(socket_);

  // This mirrors the behaviour on Windows. See the comment in
  // tcp_socket_win.cc after searching for "NODELAY".
  // If SetTCPNoDelay fails, we don't care.
  SetTCPNoDelay(socket_->socket_fd(), true);

  // TCP keep alive wakes up the radio, which is expensive on mobile. Do not
  // enable it there. It's useful to prevent TCP middleboxes from timing out
  // connection mappings. Packets for timed out connection mappings at
  // middleboxes will either lead to:
  // a) Middleboxes sending TCP RSTs. It's up to higher layers to check for this
  // and retry. The HTTP network transaction code does this.
  // b) Middleboxes just drop the unrecognized TCP packet. This leads to the TCP
  // stack retransmitting packets per TCP stack retransmission timeouts, which
  // are very high (on the order of seconds). Given the number of
  // retransmissions required before killing the connection, this can lead to
  // tens of seconds or even minutes of delay, depending on OS.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  const int kTCPKeepAliveSeconds = 45;

  SetTCPKeepAlive(socket_->socket_fd(), true, kTCPKeepAliveSeconds);
#endif
}

int TCPSocketLibevent::SetAddressReuse(bool allow) {
  DCHECK(socket_);

  // SO_REUSEADDR is useful for server sockets to bind to a recently unbound
  // port. When a socket is closed, the end point changes its state to TIME_WAIT
  // and wait for 2 MSL (maximum segment lifetime) to ensure the remote peer
  // acknowledges its closure. For server sockets, it is usually safe to
  // bind to a TIME_WAIT end point immediately, which is a widely adopted
  // behavior.
  //
  // Note that on *nix, SO_REUSEADDR does not enable the TCP socket to bind to
  // an end point that is already bound by another socket. To do that one must
  // set SO_REUSEPORT instead. This option is not provided on Linux prior
  // to 3.9.
  //
  // SO_REUSEPORT is provided in MacOS X and iOS.
  int boolean_value = allow ? 1 : 0;
  int rv = setsockopt(socket_->socket_fd(), SOL_SOCKET, SO_REUSEADDR,
                      &boolean_value, sizeof(boolean_value));
  if (rv < 0)
    return MapSystemError(errno);
  return OK;
}

int TCPSocketLibevent::SetReceiveBufferSize(int32 size) {
  DCHECK(socket_);
  int rv = setsockopt(socket_->socket_fd(), SOL_SOCKET, SO_RCVBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size));
  return (rv == 0) ? OK : MapSystemError(errno);
}

int TCPSocketLibevent::SetSendBufferSize(int32 size) {
  DCHECK(socket_);
  int rv = setsockopt(socket_->socket_fd(), SOL_SOCKET, SO_SNDBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size));
  return (rv == 0) ? OK : MapSystemError(errno);
}

bool TCPSocketLibevent::SetKeepAlive(bool enable, int delay) {
  DCHECK(socket_);
  return SetTCPKeepAlive(socket_->socket_fd(), enable, delay);
}

bool TCPSocketLibevent::SetNoDelay(bool no_delay) {
  DCHECK(socket_);
  return SetTCPNoDelay(socket_->socket_fd(), no_delay);
}

void TCPSocketLibevent::Close() {
  socket_.reset();
  tcp_fastopen_connected_ = false;
  fast_open_status_ = FAST_OPEN_STATUS_UNKNOWN;
}

bool TCPSocketLibevent::UsingTCPFastOpen() const {
  return use_tcp_fastopen_;
}

bool TCPSocketLibevent::IsValid() const {
  return socket_ != NULL && socket_->socket_fd() != kInvalidSocket;
}

void TCPSocketLibevent::StartLoggingMultipleConnectAttempts(
    const AddressList& addresses) {
  if (!logging_multiple_connect_attempts_) {
    logging_multiple_connect_attempts_ = true;
    LogConnectBegin(addresses);
  } else {
    NOTREACHED();
  }
}

void TCPSocketLibevent::EndLoggingMultipleConnectAttempts(int net_error) {
  if (logging_multiple_connect_attempts_) {
    LogConnectEnd(net_error);
    logging_multiple_connect_attempts_ = false;
  } else {
    NOTREACHED();
  }
}

void TCPSocketLibevent::AcceptCompleted(
    scoped_ptr<TCPSocketLibevent>* tcp_socket,
    IPEndPoint* address,
    const CompletionCallback& callback,
    int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  callback.Run(HandleAcceptCompleted(tcp_socket, address, rv));
}

int TCPSocketLibevent::HandleAcceptCompleted(
    scoped_ptr<TCPSocketLibevent>* tcp_socket,
    IPEndPoint* address,
    int rv) {
  if (rv == OK)
    rv = BuildTcpSocketLibevent(tcp_socket, address);

  if (rv == OK) {
    net_log_.EndEvent(NetLog::TYPE_TCP_ACCEPT,
                      CreateNetLogIPEndPointCallback(address));
  } else {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, rv);
  }

  return rv;
}

int TCPSocketLibevent::BuildTcpSocketLibevent(
    scoped_ptr<TCPSocketLibevent>* tcp_socket,
    IPEndPoint* address) {
  DCHECK(accept_socket_);

  SockaddrStorage storage;
  if (accept_socket_->GetPeerAddress(&storage) != OK ||
      !address->FromSockAddr(storage.addr, storage.addr_len)) {
    accept_socket_.reset();
    return ERR_ADDRESS_INVALID;
  }

  tcp_socket->reset(new TCPSocketLibevent(net_log_.net_log(),
                                          net_log_.source()));
  (*tcp_socket)->socket_.reset(accept_socket_.release());
  return OK;
}

void TCPSocketLibevent::ConnectCompleted(const CompletionCallback& callback,
                                         int rv) const {
  DCHECK_NE(ERR_IO_PENDING, rv);
  callback.Run(HandleConnectCompleted(rv));
}

int TCPSocketLibevent::HandleConnectCompleted(int rv) const {
  // Log the end of this attempt (and any OS error it threw).
  if (rv != OK) {
    net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT,
                      NetLog::IntegerCallback("os_error", errno));
  } else {
    net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT);
  }

  // Give a more specific error when the user is offline.
  if (rv == ERR_ADDRESS_UNREACHABLE && NetworkChangeNotifier::IsOffline())
    rv = ERR_INTERNET_DISCONNECTED;

  if (!logging_multiple_connect_attempts_)
    LogConnectEnd(rv);

  return rv;
}

void TCPSocketLibevent::LogConnectBegin(const AddressList& addresses) const {
  base::StatsCounter connects("tcp.connect");
  connects.Increment();

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT,
                      addresses.CreateNetLogCallback());
}

void TCPSocketLibevent::LogConnectEnd(int net_error) const {
  if (net_error != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_CONNECT, net_error);
    return;
  }

  UpdateConnectionTypeHistograms(CONNECTION_ANY);

  SockaddrStorage storage;
  int rv = socket_->GetLocalAddress(&storage);
  if (rv != OK) {
    PLOG(ERROR) << "GetLocalAddress() [rv: " << rv << "] error: ";
    NOTREACHED();
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_CONNECT, rv);
    return;
  }

  net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT,
                    CreateNetLogSourceAddressCallback(storage.addr,
                                                      storage.addr_len));
}

void TCPSocketLibevent::ReadCompleted(const scoped_refptr<IOBuffer>& buf,
                                      const CompletionCallback& callback,
                                      int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  // Records fast open status regardless of error in asynchronous case.
  // TODO(rdsmith,jri): Change histogram name to indicate it could be called on
  // error.
  RecordFastOpenStatus();
  callback.Run(HandleReadCompleted(buf, rv));
}

int TCPSocketLibevent::HandleReadCompleted(IOBuffer* buf, int rv) {
  if (rv < 0) {
    net_log_.AddEvent(NetLog::TYPE_SOCKET_READ_ERROR,
                      CreateNetLogSocketErrorCallback(rv, errno));
    return rv;
  }

  base::StatsCounter read_bytes("tcp.read_bytes");
  read_bytes.Add(rv);
  net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_RECEIVED, rv,
                                buf->data());
  return rv;
}

void TCPSocketLibevent::WriteCompleted(const scoped_refptr<IOBuffer>& buf,
                                       const CompletionCallback& callback,
                                       int rv) const {
  DCHECK_NE(ERR_IO_PENDING, rv);
  callback.Run(HandleWriteCompleted(buf, rv));
}

int TCPSocketLibevent::HandleWriteCompleted(IOBuffer* buf, int rv) const {
  if (rv < 0) {
    net_log_.AddEvent(NetLog::TYPE_SOCKET_WRITE_ERROR,
                      CreateNetLogSocketErrorCallback(rv, errno));
    return rv;
  }

  base::StatsCounter write_bytes("tcp.write_bytes");
  write_bytes.Add(rv);
  net_log_.AddByteTransferEvent(NetLog::TYPE_SOCKET_BYTES_SENT, rv,
                                buf->data());
  return rv;
}

int TCPSocketLibevent::TcpFastOpenWrite(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  SockaddrStorage storage;
  int rv = socket_->GetPeerAddress(&storage);
  if (rv != OK)
    return rv;

  int flags = 0x20000000;  // Magic flag to enable TCP_FASTOPEN.
#if defined(OS_LINUX)
  // sendto() will fail with EPIPE when the system doesn't support TCP Fast
  // Open. Theoretically that shouldn't happen since the caller should check
  // for system support on startup, but users may dynamically disable TCP Fast
  // Open via sysctl.
  flags |= MSG_NOSIGNAL;
#endif // defined(OS_LINUX)
  rv = HANDLE_EINTR(sendto(socket_->socket_fd(),
                           buf->data(),
                           buf_len,
                           flags,
                           storage.addr,
                           storage.addr_len));
  tcp_fastopen_connected_ = true;

  if (rv >= 0) {
    fast_open_status_ = FAST_OPEN_FAST_CONNECT_RETURN;
    return rv;
  }

  DCHECK_NE(EPIPE, errno);

  // If errno == EINPROGRESS, that means the kernel didn't have a cookie
  // and would block. The kernel is internally doing a connect() though.
  // Remap EINPROGRESS to EAGAIN so we treat this the same as our other
  // asynchronous cases. Note that the user buffer has not been copied to
  // kernel space.
  if (errno == EINPROGRESS) {
    rv = ERR_IO_PENDING;
  } else {
    rv = MapSystemError(errno);
  }

  if (rv != ERR_IO_PENDING) {
    fast_open_status_ = FAST_OPEN_ERROR;
    return rv;
  }

  fast_open_status_ = FAST_OPEN_SLOW_CONNECT_RETURN;
  return socket_->WaitForWrite(buf, buf_len, callback);
}

void TCPSocketLibevent::RecordFastOpenStatus() {
  if (use_tcp_fastopen_ &&
      (fast_open_status_ == FAST_OPEN_FAST_CONNECT_RETURN ||
       fast_open_status_ == FAST_OPEN_SLOW_CONNECT_RETURN)) {
    DCHECK_NE(FAST_OPEN_STATUS_UNKNOWN, fast_open_status_);
    bool getsockopt_success(false);
    bool server_acked_data(false);
#if defined(TCP_INFO)
    // Probe to see the if the socket used TCP Fast Open.
    tcp_info info;
    socklen_t info_len = sizeof(tcp_info);
    getsockopt_success =
        getsockopt(socket_->socket_fd(), IPPROTO_TCP, TCP_INFO,
                   &info, &info_len) == 0 &&
        info_len == sizeof(tcp_info);
    server_acked_data = getsockopt_success &&
        (info.tcpi_options & TCPI_OPT_SYN_DATA);
#endif
    if (getsockopt_success) {
      if (fast_open_status_ == FAST_OPEN_FAST_CONNECT_RETURN) {
        fast_open_status_ = (server_acked_data ? FAST_OPEN_SYN_DATA_ACK :
                             FAST_OPEN_SYN_DATA_NACK);
      } else {
        fast_open_status_ = (server_acked_data ? FAST_OPEN_NO_SYN_DATA_ACK :
                             FAST_OPEN_NO_SYN_DATA_NACK);
      }
    } else {
      fast_open_status_ = (fast_open_status_ == FAST_OPEN_FAST_CONNECT_RETURN ?
                           FAST_OPEN_SYN_DATA_FAILED :
                           FAST_OPEN_NO_SYN_DATA_FAILED);
    }
  }
}

}  // namespace net
