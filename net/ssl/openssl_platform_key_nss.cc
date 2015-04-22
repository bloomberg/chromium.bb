// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/ssl/openssl_platform_key.h"

namespace net {

crypto::ScopedEVP_PKEY FetchClientCertPrivateKey(
    const X509Certificate* certificate) {
  // TODO(davidben): Implement client auth for NSS. https://crbug.com/479036
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace net
