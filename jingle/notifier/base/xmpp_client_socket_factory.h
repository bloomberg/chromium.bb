// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_BASE_XMPP_CLIENT_SOCKET_FACTORY_H_
#define JINGLE_NOTIFIER_BASE_XMPP_CLIENT_SOCKET_FACTORY_H_

#include <string>

#include "net/socket/client_socket_factory.h"

namespace notifier {

class XmppClientSocketFactory : public net::ClientSocketFactory {
 public:
  // Does not take ownership of |client_socket_factory|.
  XmppClientSocketFactory(
      net::ClientSocketFactory* client_socket_factory,
      bool use_fake_ssl_client_socket);

  virtual ~XmppClientSocketFactory();

  // net::ClientSocketFactory implementation.
  virtual net::ClientSocket* CreateTCPClientSocket(
      const net::AddressList& addresses, net::NetLog* net_log,
      const net::NetLog::Source& source);
  virtual net::SSLClientSocket* CreateSSLClientSocket(
      net::ClientSocketHandle* transport_socket, const std::string& hostname,
      const net::SSLConfig& ssl_config);

 private:
  net::ClientSocketFactory* const client_socket_factory_;
  const bool use_fake_ssl_client_socket_;

  DISALLOW_COPY_AND_ASSIGN(XmppClientSocketFactory);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_XMPP_CLIENT_SOCKET_FACTORY_H_
