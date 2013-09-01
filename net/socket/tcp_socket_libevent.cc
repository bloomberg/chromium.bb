// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_socket_libevent.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <netinet/in.h>
#endif

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_net_log_params.h"

namespace net {

TCPSocketLibevent::TCPSocketLibevent(NetLog* net_log,
                                     const NetLog::Source& source)
    : socket_(kInvalidSocket),
      accept_socket_(NULL),
      accept_address_(NULL),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE,
                      source.ToEventParametersCallback());
}

TCPSocketLibevent::~TCPSocketLibevent() {
  if (socket_ != kInvalidSocket)
    Close();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE);
}

int TCPSocketLibevent::Create(AddressFamily family) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(socket_, kInvalidSocket);

  socket_ = CreatePlatformSocket(ConvertAddressFamily(family), SOCK_STREAM,
                                 IPPROTO_TCP);
  if (socket_ < 0) {
    PLOG(ERROR) << "CreatePlatformSocket() returned an error";
    return MapSystemError(errno);
  }

  if (SetNonBlocking(socket_)) {
    int result = MapSystemError(errno);
    Close();
    return result;
  }

  return OK;
}

int TCPSocketLibevent::Adopt(int socket) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(socket_, kInvalidSocket);

  socket_ = socket;

  if (SetNonBlocking(socket_)) {
    int result = MapSystemError(errno);
    Close();
    return result;
  }

  return OK;
}

int TCPSocketLibevent::Release() {
  DCHECK(CalledOnValidThread());
  DCHECK(accept_callback_.is_null());

  int result = socket_;
  socket_ = kInvalidSocket;
  return result;
}

int TCPSocketLibevent::Bind(const IPEndPoint& address) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(socket_, kInvalidSocket);

  SockaddrStorage storage;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_ADDRESS_INVALID;

  int result = bind(socket_, storage.addr, storage.addr_len);
  if (result < 0) {
    PLOG(ERROR) << "bind() returned an error";
    return MapSystemError(errno);
  }

  return OK;
}

int TCPSocketLibevent::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);

  SockaddrStorage storage;
  if (getsockname(socket_, storage.addr, &storage.addr_len) < 0)
    return MapSystemError(errno);
  if (!address->FromSockAddr(storage.addr, storage.addr_len))
    return ERR_FAILED;

  return OK;
}

int TCPSocketLibevent::Listen(int backlog) {
  DCHECK(CalledOnValidThread());
  DCHECK_GT(backlog, 0);
  DCHECK_NE(socket_, kInvalidSocket);

  int result = listen(socket_, backlog);
  if (result < 0) {
    PLOG(ERROR) << "listen() returned an error";
    return MapSystemError(errno);
  }

  return OK;
}

int TCPSocketLibevent::Accept(scoped_ptr<TCPSocketLibevent>* socket,
                              IPEndPoint* address,
                              const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket);
  DCHECK(address);
  DCHECK(!callback.is_null());
  DCHECK(accept_callback_.is_null());

  net_log_.BeginEvent(NetLog::TYPE_TCP_ACCEPT);

  int result = AcceptInternal(socket, address);

  if (result == ERR_IO_PENDING) {
    if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
            socket_, true, base::MessageLoopForIO::WATCH_READ,
            &accept_socket_watcher_, this)) {
      PLOG(ERROR) << "WatchFileDescriptor failed on read";
      return MapSystemError(errno);
    }

    accept_socket_ = socket;
    accept_address_ = address;
    accept_callback_ = callback;
  }

  return result;
}

int TCPSocketLibevent::SetDefaultOptionsForServer() {
  return SetAddressReuse(true);
}

int TCPSocketLibevent::SetAddressReuse(bool allow) {
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
  int rv = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &boolean_value,
                      sizeof(boolean_value));
  if (rv < 0)
    return MapSystemError(errno);
  return OK;
}

void TCPSocketLibevent::Close() {
  if (socket_ != kInvalidSocket) {
    bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    if (HANDLE_EINTR(close(socket_)) < 0)
      PLOG(ERROR) << "close";
    socket_ = kInvalidSocket;
  }
}

int TCPSocketLibevent::AcceptInternal(scoped_ptr<TCPSocketLibevent>* socket,
                                      IPEndPoint* address) {
  SockaddrStorage storage;
  int new_socket = HANDLE_EINTR(accept(socket_,
                                       storage.addr,
                                       &storage.addr_len));
  if (new_socket < 0) {
    int net_error = MapSystemError(errno);
    if (net_error != ERR_IO_PENDING)
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, net_error);
    return net_error;
  }

  IPEndPoint ip_end_point;
  if (!ip_end_point.FromSockAddr(storage.addr, storage.addr_len)) {
    NOTREACHED();
    if (HANDLE_EINTR(close(new_socket)) < 0)
      PLOG(ERROR) << "close";
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, ERR_FAILED);
    return ERR_FAILED;
  }
  scoped_ptr<TCPSocketLibevent> tcp_socket(new TCPSocketLibevent(
      net_log_.net_log(), net_log_.source()));
  int adopt_result = tcp_socket->Adopt(new_socket);
  if (adopt_result != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, adopt_result);
    return adopt_result;
  }
  *socket = tcp_socket.Pass();
  *address = ip_end_point;
  net_log_.EndEvent(NetLog::TYPE_TCP_ACCEPT,
                    CreateNetLogIPEndPointCallback(&ip_end_point));
  return OK;
}

void TCPSocketLibevent::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(CalledOnValidThread());

  int result = AcceptInternal(accept_socket_, accept_address_);
  if (result != ERR_IO_PENDING) {
    accept_socket_ = NULL;
    accept_address_ = NULL;
    bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    CompletionCallback callback = accept_callback_;
    accept_callback_.Reset();
    callback.Run(result);
  }
}

void TCPSocketLibevent::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace net
