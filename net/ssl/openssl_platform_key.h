// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_OPENSSL_PLATFORM_KEY_H_
#define NET_SSL_OPENSSL_PLATFORM_KEY_H_

#include "crypto/scoped_openssl_types.h"

namespace net {

class X509Certificate;

// Looks up the private key from the platform key store corresponding
// to |certificate|'s public key. Then wraps it in an OpenSSL EVP_PKEY
// structure to be used for SSL client auth.
//
// TODO(davidben): This combines looking up a private key with
// wrapping it in an OpenSSL structure. This will be separated with
// https://crbug.com/394131
crypto::ScopedEVP_PKEY FetchClientCertPrivateKey(
    const X509Certificate* certificate);

}  // namespace net

#endif  // NET_SSL_OPENSSL_PLATFORM_KEY_H_
