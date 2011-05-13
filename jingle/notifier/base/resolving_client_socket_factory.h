// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_BASE_RESOLVING_CLIENT_SOCKET_FACTORY_H_
#define JINGLE_NOTIFIER_BASE_RESOLVING_CLIENT_SOCKET_FACTORY_H_


namespace net {
class ClientSocketHandle;
class HostPortPair;
class NetLog;
class SSLClientSocket;
class StreamSocket;
}

// TODO(sanjeevr): Move this to net/

namespace notifier {

// Interface for a ClientSocketFactory that creates ClientSockets that can
// resolve host names and tunnel through proxies.
class ResolvingClientSocketFactory {
 public:
  virtual ~ResolvingClientSocketFactory() { }
  // Method to create a transport socket using a HostPortPair.
  virtual net::StreamSocket* CreateTransportClientSocket(
      const net::HostPortPair& host_and_port) = 0;

  virtual net::SSLClientSocket* CreateSSLClientSocket(
      net::ClientSocketHandle* transport_socket,
      const net::HostPortPair& host_and_port) = 0;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_RESOLVING_CLIENT_SOCKET_FACTORY_H_
