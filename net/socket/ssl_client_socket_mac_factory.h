// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_MAC_FACTORY_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_MAC_FACTORY_H_
#pragma once

#include "net/socket/client_socket_factory.h"

namespace net {

class DnsCertProvenanceChecker;
class SSLHostInfo;

// Creates SSLClientSocketMac objects.
SSLClientSocket* SSLClientSocketMacFactory(
    ClientSocketHandle* transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    SSLHostInfo* ssl_host_info,
    CertVerifier* cert_verifier,
    DnsCertProvenanceChecker* dns_cert_checker);

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_MAC_FACTORY_H_
