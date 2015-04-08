// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SCOPED_OPENSSL_TYPES_H_
#define NET_SSL_SCOPED_OPENSSL_TYPES_H_

#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "crypto/scoped_openssl_types.h"

namespace net {

using ScopedPKCS8_PRIV_KEY_INFO =
    crypto::ScopedOpenSSL<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free>;
using ScopedSSL = crypto::ScopedOpenSSL<SSL, SSL_free>;
using ScopedSSL_CTX = crypto::ScopedOpenSSL<SSL_CTX, SSL_CTX_free>;
using ScopedSSL_SESSION = crypto::ScopedOpenSSL<SSL_SESSION, SSL_SESSION_free>;
using ScopedX509 = crypto::ScopedOpenSSL<X509, X509_free>;

}  // namespace net

#endif  // NET_SSL_SCOPED_OPENSSL_TYPES_H_
