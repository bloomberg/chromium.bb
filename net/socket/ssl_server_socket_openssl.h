// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SERVER_SOCKET_OPENSSL_H_
#define NET_SOCKET_SSL_SERVER_SOCKET_OPENSSL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/log/net_log.h"
#include "net/socket/ssl_server_socket.h"
#include "net/ssl/scoped_openssl_types.h"
#include "net/ssl/ssl_server_config.h"

// Avoid including misc OpenSSL headers, i.e.:
// <openssl/bio.h>
typedef struct bio_st BIO;
// <openssl/ssl.h>
typedef struct ssl_st SSL;
typedef struct x509_store_ctx_st X509_STORE_CTX;

namespace net {

class SSLInfo;

class SSLServerContextOpenSSL : public SSLServerContext {
 public:
  SSLServerContextOpenSSL(X509Certificate* certificate,
                          const crypto::RSAPrivateKey& key,
                          const SSLServerConfig& ssl_server_config);
  ~SSLServerContextOpenSSL() override;

  scoped_ptr<SSLServerSocket> CreateSSLServerSocket(
      scoped_ptr<StreamSocket> socket) override;

 private:
  ScopedSSL_CTX ssl_ctx_;

  // Options for the SSL socket.
  SSLServerConfig ssl_server_config_;

  // Certificate for the server.
  scoped_refptr<X509Certificate> cert_;

  // Private key used by the server.
  scoped_ptr<crypto::RSAPrivateKey> key_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SERVER_SOCKET_OPENSSL_H_
