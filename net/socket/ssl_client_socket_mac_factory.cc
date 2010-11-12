// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_factory.h"

#include "net/socket/ssl_client_socket_mac.h"
#include "net/socket/ssl_host_info.h"

namespace net {

SSLClientSocket* SSLClientSocketMacFactory(
    ClientSocketHandle* transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    SSLHostInfo* ssl_host_info,
    DnsRRResolver* dnsrr_resolver) {
  delete ssl_host_info;
  return new SSLClientSocketMac(transport_socket, host_and_port, ssl_config);
}

}  // namespace net
