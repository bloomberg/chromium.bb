// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UNIX_DOMAIN_LISTEN_SOCKET_POSIX_H_
#define NET_SOCKET_UNIX_DOMAIN_LISTEN_SOCKET_POSIX_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/socket/stream_listen_socket.h"
#include "net/socket/unix_domain_server_socket_posix.h"

#if defined(OS_ANDROID) || defined(OS_LINUX)
// Feature only supported on Linux currently. This lets the Unix Domain Socket
// not be backed by the file system.
#define SOCKET_ABSTRACT_NAMESPACE_SUPPORTED
#endif

namespace net {
namespace deprecated {

// Unix Domain Socket Implementation. Supports abstract namespaces on Linux.
class NET_EXPORT UnixDomainListenSocket : public StreamListenSocket {
 public:
  typedef UnixDomainServerSocket::AuthCallback AuthCallback;

  virtual ~UnixDomainListenSocket();

  // Note that the returned UnixDomainListenSocket instance does not take
  // ownership of |del|.
  static scoped_ptr<UnixDomainListenSocket> CreateAndListen(
      const std::string& path,
      StreamListenSocket::Delegate* del,
      const AuthCallback& auth_callback);

#if defined(SOCKET_ABSTRACT_NAMESPACE_SUPPORTED)
  // Same as above except that the created socket uses the abstract namespace
  // which is a Linux-only feature. If |fallback_path| is not empty,
  // make the second attempt with the provided fallback name.
  static scoped_ptr<UnixDomainListenSocket>
  CreateAndListenWithAbstractNamespace(
      const std::string& path,
      const std::string& fallback_path,
      StreamListenSocket::Delegate* del,
      const AuthCallback& auth_callback);
#endif

 private:
  UnixDomainListenSocket(SocketDescriptor s,
                         StreamListenSocket::Delegate* del,
                         const AuthCallback& auth_callback);

  static scoped_ptr<UnixDomainListenSocket> CreateAndListenInternal(
      const std::string& path,
      const std::string& fallback_path,
      StreamListenSocket::Delegate* del,
      const AuthCallback& auth_callback,
      bool use_abstract_namespace);

  // StreamListenSocket:
  virtual void Accept() OVERRIDE;

  AuthCallback auth_callback_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainListenSocket);
};

// Factory that can be used to instantiate UnixDomainListenSocket.
class NET_EXPORT UnixDomainListenSocketFactory
    : public StreamListenSocketFactory {
 public:
  // Note that this class does not take ownership of the provided delegate.
  UnixDomainListenSocketFactory(
      const std::string& path,
      const UnixDomainListenSocket::AuthCallback& auth_callback);
  virtual ~UnixDomainListenSocketFactory();

  // StreamListenSocketFactory:
  virtual scoped_ptr<StreamListenSocket> CreateAndListen(
      StreamListenSocket::Delegate* delegate) const OVERRIDE;

 protected:
  const std::string path_;
  const UnixDomainListenSocket::AuthCallback auth_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnixDomainListenSocketFactory);
};

#if defined(SOCKET_ABSTRACT_NAMESPACE_SUPPORTED)
// Use this factory to instantiate UnixDomainListenSocket using the abstract
// namespace feature (only supported on Linux).
class NET_EXPORT UnixDomainListenSocketWithAbstractNamespaceFactory
    : public UnixDomainListenSocketFactory {
 public:
  UnixDomainListenSocketWithAbstractNamespaceFactory(
      const std::string& path,
      const std::string& fallback_path,
      const UnixDomainListenSocket::AuthCallback& auth_callback);
  virtual ~UnixDomainListenSocketWithAbstractNamespaceFactory();

  // UnixDomainListenSocketFactory:
  virtual scoped_ptr<StreamListenSocket> CreateAndListen(
      StreamListenSocket::Delegate* delegate) const OVERRIDE;

 private:
  std::string fallback_path_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainListenSocketWithAbstractNamespaceFactory);
};
#endif

}  // namespace deprecated
}  // namespace net

#endif  // NET_SOCKET_UNIX_DOMAIN_LISTEN_SOCKET_POSIX_H_
