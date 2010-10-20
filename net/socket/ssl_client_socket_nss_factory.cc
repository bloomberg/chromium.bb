// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_factory.h"

#include "build/build_config.h"
#include "net/socket/ssl_client_socket_nss.h"
#include "net/socket/ssl_host_info.h"
#if defined(OS_WIN)
#include "net/socket/ssl_client_socket_win.h"
#endif

// This file is only used on platforms where NSS is not the system SSL
// library.  When compiled, this file is the only object module that pulls
// in the dependency on NSPR and NSS.  This allows us to control which
// projects depend on NSPR and NSS on those platforms.

namespace net {

SSLClientSocket* SSLClientSocketNSSFactory(
    ClientSocketHandle* transport_socket,
    const std::string& hostname,
    const SSLConfig& ssl_config,
    SSLHostInfo* ssl_host_info) {
  scoped_ptr<SSLHostInfo> shi(ssl_host_info);
  // TODO(wtc): SSLClientSocketNSS can't do SSL client authentication using
  // CryptoAPI yet (http://crbug.com/37560), so we fall back on
  // SSLClientSocketWin.
#if defined(OS_WIN)
  if (ssl_config.send_client_cert)
    return new SSLClientSocketWin(transport_socket, hostname, ssl_config);
#endif

  return new SSLClientSocketNSS(transport_socket, hostname, ssl_config,
                                shi.release());
}

}  // namespace net
