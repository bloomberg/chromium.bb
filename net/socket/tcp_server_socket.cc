// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_server_socket.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_client_socket.h"

namespace net {

TCPServerSocket::TCPServerSocket(NetLog* net_log, const NetLog::Source& source)
    : socket_(net_log, source),
      pending_accept_(false) {
}

TCPServerSocket::~TCPServerSocket() {
}

int TCPServerSocket::Listen(const IPEndPoint& address, int backlog) {
  int result = socket_.Create(address.GetFamily());
  if (result != OK)
    return result;

  result = socket_.SetDefaultOptionsForServer();
  if (result != OK) {
    socket_.Close();
    return result;
  }

  result = socket_.Bind(address);
  if (result != OK) {
    socket_.Close();
    return result;
  }

  result = socket_.Listen(backlog);
  if (result != OK) {
    socket_.Close();
    return result;
  }

  return OK;
}

int TCPServerSocket::GetLocalAddress(IPEndPoint* address) const {
  return socket_.GetLocalAddress(address);
}

int TCPServerSocket::Accept(scoped_ptr<StreamSocket>* socket,
                            const CompletionCallback& callback) {
  DCHECK(socket);
  DCHECK(!callback.is_null());

  if (pending_accept_) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  // It is safe to use base::Unretained(this). |socket_| is owned by this class,
  // and the callback won't be run after |socket_| is destroyed.
  CompletionCallback accept_callback =
      base::Bind(&TCPServerSocket::OnAcceptCompleted, base::Unretained(this),
                 socket, callback);
  int result = socket_.Accept(&accepted_socket_, &accepted_address_,
                              accept_callback);
  if (result != ERR_IO_PENDING) {
    // |accept_callback| won't be called so we need to run
    // ConvertAcceptedSocket() ourselves in order to do the conversion from
    // |accepted_socket_| to |socket|.
    result = ConvertAcceptedSocket(result, socket);
  } else {
    pending_accept_ = true;
  }

  return result;
}

int TCPServerSocket::ConvertAcceptedSocket(
    int result,
    scoped_ptr<StreamSocket>* output_accepted_socket) {
  // Make sure the TCPSocket object is destroyed in any case.
  scoped_ptr<TCPSocket> temp_accepted_socket(accepted_socket_.Pass());
  if (result != OK)
    return result;

  scoped_ptr<TCPClientSocket> client_socket(new TCPClientSocket(
      AddressList(accepted_address_),
      temp_accepted_socket->net_log().net_log(),
      temp_accepted_socket->net_log().source()));
  // TODO(yzshen): Once we switch TCPClientSocket::AdoptSocket() to take a
  // TCPSocket object, we don't need to do platform-specific handling.
#if defined(OS_WIN)
  SOCKET raw_socket = temp_accepted_socket->Release();
#elif defined(OS_POSIX)
  int raw_socket = temp_accepted_socket->Release();
#endif
  result = client_socket->AdoptSocket(raw_socket);
  if (result != OK) {
    // |client_socket| won't take ownership of |raw_socket| on failure.
    // Therefore, we put it back into |temp_accepted_socket| to close it.
    temp_accepted_socket->Adopt(raw_socket);
    return result;
  }

  *output_accepted_socket = client_socket.Pass();
  return OK;
}

void TCPServerSocket::OnAcceptCompleted(
    scoped_ptr<StreamSocket>* output_accepted_socket,
    const CompletionCallback& forward_callback,
    int result) {
  result = ConvertAcceptedSocket(result, output_accepted_socket);
  pending_accept_ = false;
  forward_callback.Run(result);
}

}  // namespace net
