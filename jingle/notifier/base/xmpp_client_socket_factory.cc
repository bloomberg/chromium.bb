// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/xmpp_client_socket_factory.h"

#include "base/logging.h"
#include "jingle/notifier/base/fake_ssl_client_socket.h"

namespace notifier {

XmppClientSocketFactory::XmppClientSocketFactory(
    net::ClientSocketFactory* client_socket_factory,
    bool use_fake_ssl_client_socket)
    : client_socket_factory_(client_socket_factory),
      use_fake_ssl_client_socket_(use_fake_ssl_client_socket) {
  CHECK(client_socket_factory_);
}

XmppClientSocketFactory::~XmppClientSocketFactory() {}

net::ClientSocket* XmppClientSocketFactory::CreateTCPClientSocket(
    const net::AddressList& addresses,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  net::ClientSocket* transport_socket =
      client_socket_factory_->CreateTCPClientSocket(
          addresses, net_log, source);
  return (use_fake_ssl_client_socket_ ?
          new FakeSSLClientSocket(transport_socket) : transport_socket);
}

net::SSLClientSocket* XmppClientSocketFactory::CreateSSLClientSocket(
    net::ClientSocketHandle* transport_socket,
    const net::HostPortPair& host_and_port,
    const net::SSLConfig& ssl_config,
    net::SSLHostInfo* ssl_host_info,
    net::CertVerifier* cert_verifier,
    net::DnsCertProvenanceChecker* dns_cert_checker) {
  return client_socket_factory_->CreateSSLClientSocket(
      transport_socket, host_and_port, ssl_config, ssl_host_info,
      cert_verifier, dns_cert_checker);
}

void XmppClientSocketFactory::ClearSSLSessionCache() {
  client_socket_factory_->ClearSSLSessionCache();
}

}  // namespace
