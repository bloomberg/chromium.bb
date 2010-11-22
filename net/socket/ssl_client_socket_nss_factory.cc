// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_factory.h"

#include "net/socket/ssl_client_socket_nss.h"
#include "net/socket/ssl_host_info.h"

// This file is only used on platforms where NSS is not the system SSL
// library.  When compiled, this file is the only object module that pulls
// in the dependency on NSPR and NSS.  This allows us to control which
// projects depend on NSPR and NSS on those platforms.

namespace net {

SSLClientSocket* SSLClientSocketNSSFactory(
    ClientSocketHandle* transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    SSLHostInfo* ssl_host_info,
    DnsCertProvenanceChecker* dns_cert_checker) {
  scoped_ptr<SSLHostInfo> shi(ssl_host_info);
  return new SSLClientSocketNSS(transport_socket, host_and_port, ssl_config,
                                shi.release(), dns_cert_checker);
}

}  // namespace net
