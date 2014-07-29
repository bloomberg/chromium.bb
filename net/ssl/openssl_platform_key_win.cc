// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_platform_key.h"

#include "base/logging.h"

namespace net {

crypto::ScopedEVP_PKEY FetchClientCertPrivateKey(
    const X509Certificate* certificate) {
  // TODO(davidben): Implement on Windows.
  NOTIMPLEMENTED();
  return crypto::ScopedEVP_PKEY();
}

}  // namespace net
