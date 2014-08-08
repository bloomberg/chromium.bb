// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/unix_domain_listen_socket_posix.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/unix_domain_client_socket_posix.h"

namespace net {
namespace deprecated {

namespace {

int CreateAndBind(const std::string& socket_path,
                  bool use_abstract_namespace,
                  SocketDescriptor* socket_fd) {
  DCHECK(socket_fd);

  SockaddrStorage address;
  if (!UnixDomainClientSocket::FillAddress(socket_path,
                                           use_abstract_namespace,
                                           &address)) {
    return ERR_ADDRESS_INVALID;
  }

  SocketDescriptor fd = CreatePlatformSocket(PF_UNIX, SOCK_STREAM, 0);
  if (fd == kInvalidSocket)
    return errno ? MapSystemError(errno) : ERR_UNEXPECTED;

  if (bind(fd, address.addr, address.addr_len) < 0) {
    int rv = MapSystemError(errno);
    close(fd);
    PLOG(ERROR) << "Could not bind unix domain socket to " << socket_path
                << (use_abstract_namespace ? " (with abstract namespace)" : "");
    return rv;
  }

  *socket_fd = fd;
  return OK;
}

}  // namespace

// static
scoped_ptr<UnixDomainListenSocket>
UnixDomainListenSocket::CreateAndListenInternal(
    const std::string& path,
    const std::string& fallback_path,
    StreamListenSocket::Delegate* del,
    const AuthCallback& auth_callback,
    bool use_abstract_namespace) {
  SocketDescriptor socket_fd = kInvalidSocket;
  int rv = CreateAndBind(path, use_abstract_namespace, &socket_fd);
  if (rv != OK && !fallback_path.empty())
    rv = CreateAndBind(fallback_path, use_abstract_namespace, &socket_fd);
  if (rv != OK)
    return scoped_ptr<UnixDomainListenSocket>();
  scoped_ptr<UnixDomainListenSocket> sock(
      new UnixDomainListenSocket(socket_fd, del, auth_callback));
  sock->Listen();
  return sock.Pass();
}

// static
scoped_ptr<UnixDomainListenSocket> UnixDomainListenSocket::CreateAndListen(
    const std::string& path,
    StreamListenSocket::Delegate* del,
    const AuthCallback& auth_callback) {
  return CreateAndListenInternal(path, "", del, auth_callback, false);
}

#if defined(SOCKET_ABSTRACT_NAMESPACE_SUPPORTED)
// static
scoped_ptr<UnixDomainListenSocket>
UnixDomainListenSocket::CreateAndListenWithAbstractNamespace(
    const std::string& path,
    const std::string& fallback_path,
    StreamListenSocket::Delegate* del,
    const AuthCallback& auth_callback) {
  return
      CreateAndListenInternal(path, fallback_path, del, auth_callback, true);
}
#endif

UnixDomainListenSocket::UnixDomainListenSocket(
    SocketDescriptor s,
    StreamListenSocket::Delegate* del,
    const AuthCallback& auth_callback)
    : StreamListenSocket(s, del),
      auth_callback_(auth_callback) {}

UnixDomainListenSocket::~UnixDomainListenSocket() {}

void UnixDomainListenSocket::Accept() {
  SocketDescriptor conn = StreamListenSocket::AcceptSocket();
  if (conn == kInvalidSocket)
    return;
  UnixDomainServerSocket::Credentials credentials;
  if (!UnixDomainServerSocket::GetPeerCredentials(conn, &credentials) ||
      !auth_callback_.Run(credentials)) {
    if (IGNORE_EINTR(close(conn)) < 0)
      LOG(ERROR) << "close() error";
    return;
  }
  scoped_ptr<UnixDomainListenSocket> sock(
      new UnixDomainListenSocket(conn, socket_delegate_, auth_callback_));
  // It's up to the delegate to AddRef if it wants to keep it around.
  sock->WatchSocket(WAITING_READ);
  socket_delegate_->DidAccept(this, sock.PassAs<StreamListenSocket>());
}

UnixDomainListenSocketFactory::UnixDomainListenSocketFactory(
    const std::string& path,
    const UnixDomainListenSocket::AuthCallback& auth_callback)
    : path_(path),
      auth_callback_(auth_callback) {}

UnixDomainListenSocketFactory::~UnixDomainListenSocketFactory() {}

scoped_ptr<StreamListenSocket> UnixDomainListenSocketFactory::CreateAndListen(
    StreamListenSocket::Delegate* delegate) const {
  return UnixDomainListenSocket::CreateAndListen(
      path_, delegate, auth_callback_).PassAs<StreamListenSocket>();
}

#if defined(SOCKET_ABSTRACT_NAMESPACE_SUPPORTED)

UnixDomainListenSocketWithAbstractNamespaceFactory::
UnixDomainListenSocketWithAbstractNamespaceFactory(
    const std::string& path,
    const std::string& fallback_path,
    const UnixDomainListenSocket::AuthCallback& auth_callback)
    : UnixDomainListenSocketFactory(path, auth_callback),
      fallback_path_(fallback_path) {}

UnixDomainListenSocketWithAbstractNamespaceFactory::
~UnixDomainListenSocketWithAbstractNamespaceFactory() {}

scoped_ptr<StreamListenSocket>
UnixDomainListenSocketWithAbstractNamespaceFactory::CreateAndListen(
    StreamListenSocket::Delegate* delegate) const {
  return UnixDomainListenSocket::CreateAndListenWithAbstractNamespace(
             path_, fallback_path_, delegate, auth_callback_)
         .PassAs<StreamListenSocket>();
}

#endif

}  // namespace deprecated
}  // namespace net
