// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SERVER_SOCKET_NSS_H_
#define NET_SOCKET_SSL_SERVER_SOCKET_NSS_H_

#include <certt.h>
#include <keyt.h>
#include <nspr.h>
#include <nss.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/nss_memio.h"
#include "net/log/net_log.h"
#include "net/socket/ssl_server_socket.h"
#include "net/ssl/ssl_server_config.h"

namespace net {

class SSLServerContextNSS : public SSLServerContext {
 public:
  SSLServerContextNSS(X509Certificate* certificate,
                      const crypto::RSAPrivateKey& key,
                      const SSLServerConfig& ssl_server_config);
  ~SSLServerContextNSS() override;

  std::unique_ptr<SSLServerSocket> CreateSSLServerSocket(
      std::unique_ptr<StreamSocket> socket) override;

 private:
  // Options for the SSL socket.
  SSLServerConfig ssl_server_config_;

  // Certificate for the server.
  scoped_refptr<X509Certificate> cert_;

  // Private key used by the server.
  std::unique_ptr<crypto::RSAPrivateKey> key_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SERVER_SOCKET_NSS_H_
