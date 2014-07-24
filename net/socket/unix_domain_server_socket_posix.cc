// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/unix_domain_server_socket_posix.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_libevent.h"
#include "net/socket/unix_domain_client_socket_posix.h"

namespace net {

UnixDomainServerSocket::UnixDomainServerSocket(
    const AuthCallback& auth_callback,
    bool use_abstract_namespace)
    : auth_callback_(auth_callback),
      use_abstract_namespace_(use_abstract_namespace) {
  DCHECK(!auth_callback_.is_null());
}

UnixDomainServerSocket::~UnixDomainServerSocket() {
}

// static
bool UnixDomainServerSocket::GetPeerIds(SocketDescriptor socket,
                                        uid_t* user_id,
                                        gid_t* group_id) {
#if defined(OS_LINUX) || defined(OS_ANDROID)
  struct ucred user_cred;
  socklen_t len = sizeof(user_cred);
  if (getsockopt(socket, SOL_SOCKET, SO_PEERCRED, &user_cred, &len) < 0)
    return false;
  *user_id = user_cred.uid;
  *group_id = user_cred.gid;
  return true;
#else
  return getpeereid(socket, user_id, group_id) == 0;
#endif
}

int UnixDomainServerSocket::Listen(const IPEndPoint& address, int backlog) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int UnixDomainServerSocket::ListenWithAddressAndPort(
    const std::string& unix_domain_path,
    int port_unused,
    int backlog) {
  DCHECK(!listen_socket_);

  SockaddrStorage address;
  if (!UnixDomainClientSocket::FillAddress(unix_domain_path,
                                           use_abstract_namespace_,
                                           &address)) {
    return ERR_ADDRESS_INVALID;
  }

  listen_socket_.reset(new SocketLibevent);
  int rv = listen_socket_->Open(AF_UNIX);
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv != OK)
    return rv;

  rv = listen_socket_->Bind(address);
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv != OK) {
    PLOG(ERROR)
        << "Could not bind unix domain socket to " << unix_domain_path
        << (use_abstract_namespace_ ? " (with abstract namespace)" : "");
    return rv;
  }

  return listen_socket_->Listen(backlog);
}

int UnixDomainServerSocket::GetLocalAddress(IPEndPoint* address) const {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int UnixDomainServerSocket::Accept(scoped_ptr<StreamSocket>* socket,
                                   const CompletionCallback& callback) {
  DCHECK(socket);
  DCHECK(!callback.is_null());
  DCHECK(listen_socket_);
  DCHECK(!accept_socket_);

  while (true) {
    int rv = listen_socket_->Accept(
        &accept_socket_,
        base::Bind(&UnixDomainServerSocket::AcceptCompleted,
                   base::Unretained(this), socket, callback));
    if (rv != OK)
      return rv;
    if (AuthenticateAndGetStreamSocket(socket))
      return OK;
    // Accept another socket because authentication error should be transparent
    // to the caller.
  }
}

void UnixDomainServerSocket::AcceptCompleted(scoped_ptr<StreamSocket>* socket,
                                             const CompletionCallback& callback,
                                             int rv) {
  if (rv != OK) {
    callback.Run(rv);
    return;
  }

  if (AuthenticateAndGetStreamSocket(socket)) {
    callback.Run(OK);
    return;
  }

  // Accept another socket because authentication error should be transparent
  // to the caller.
  rv = Accept(socket, callback);
  if (rv != ERR_IO_PENDING)
    callback.Run(rv);
}

bool UnixDomainServerSocket::AuthenticateAndGetStreamSocket(
    scoped_ptr<StreamSocket>* socket) {
  DCHECK(accept_socket_);

  uid_t user_id;
  gid_t group_id;
  if (!GetPeerIds(accept_socket_->socket_fd(), &user_id, &group_id) ||
      !auth_callback_.Run(user_id, group_id)) {
    accept_socket_.reset();
    return false;
  }

  socket->reset(new UnixDomainClientSocket(accept_socket_.Pass()));
  return true;
}

}  // namespace net
