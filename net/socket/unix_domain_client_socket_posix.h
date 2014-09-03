// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UNIX_DOMAIN_CLIENT_SOCKET_POSIX_H_
#define NET_SOCKET_UNIX_DOMAIN_CLIENT_SOCKET_POSIX_H_

#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/stream_socket.h"

namespace net {

class SocketLibevent;
struct SockaddrStorage;

// A client socket that uses unix domain socket as the transport layer.
class NET_EXPORT UnixDomainClientSocket : public StreamSocket {
 public:
  // Builds a client socket with |socket_path|. The caller should call Connect()
  // to connect to a server socket.
  UnixDomainClientSocket(const std::string& socket_path,
                         bool use_abstract_namespace);
  // Builds a client socket with socket libevent which is already connected.
  // UnixDomainServerSocket uses this after it accepts a connection.
  explicit UnixDomainClientSocket(scoped_ptr<SocketLibevent> socket);

  virtual ~UnixDomainClientSocket();

  // Fills |address| with |socket_path| and its length. For Android or Linux
  // platform, this supports abstract namespaces.
  static bool FillAddress(const std::string& socket_path,
                          bool use_abstract_namespace,
                          SockaddrStorage* address);

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual bool WasNpnNegotiated() const OVERRIDE;
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE;
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE;

  // Socket implementation.
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) OVERRIDE;
  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback) OVERRIDE;
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual int SetSendBufferSize(int32 size) OVERRIDE;

  // Releases ownership of underlying SocketDescriptor to caller.
  // Internal state is reset so that this object can be used again.
  // Socket must be connected in order to release it.
  SocketDescriptor ReleaseConnectedSocket();

 private:
  const std::string socket_path_;
  const bool use_abstract_namespace_;
  scoped_ptr<SocketLibevent> socket_;
  // This net log is just to comply StreamSocket::NetLog(). It throws away
  // everything.
  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_UNIX_DOMAIN_CLIENT_SOCKET_POSIX_H_
