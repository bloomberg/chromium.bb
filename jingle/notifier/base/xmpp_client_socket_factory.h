// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_BASE_XMPP_CLIENT_SOCKET_FACTORY_H_
#define JINGLE_NOTIFIER_BASE_XMPP_CLIENT_SOCKET_FACTORY_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "jingle/notifier/base/resolving_client_socket_factory.h"
#include "net/base/ssl_config_service.h"

namespace net {
class ClientSocketFactory;
class ClientSocketHandle;
class HostPortPair;
class SSLClientSocket;
class StreamSocket;
class URLRequestContextGetter;
}

namespace notifier {

class XmppClientSocketFactory : public ResolvingClientSocketFactory {
 public:
  // Does not take ownership of |client_socket_factory|.
  XmppClientSocketFactory(
      net::ClientSocketFactory* client_socket_factory,
      const net::SSLConfig& ssl_config,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      bool use_fake_ssl_client_socket);

  virtual ~XmppClientSocketFactory();

  // ResolvingClientSocketFactory implementation.
  virtual net::StreamSocket* CreateTransportClientSocket(
      const net::HostPortPair& host_and_port) OVERRIDE;

  virtual net::SSLClientSocket* CreateSSLClientSocket(
      net::ClientSocketHandle* transport_socket,
      const net::HostPortPair& host_and_port) OVERRIDE;

 private:
  net::ClientSocketFactory* const client_socket_factory_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  const net::SSLConfig ssl_config_;
  const bool use_fake_ssl_client_socket_;

  DISALLOW_COPY_AND_ASSIGN(XmppClientSocketFactory);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_XMPP_CLIENT_SOCKET_FACTORY_H_
