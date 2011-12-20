// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/origin_bound_cert_store.h"

namespace net {

OriginBoundCertStore::OriginBoundCert::OriginBoundCert()
    : type_(CLIENT_CERT_INVALID_TYPE) {
}

OriginBoundCertStore::OriginBoundCert::OriginBoundCert(
    const std::string& origin,
    SSLClientCertType type,
    base::Time expiration_time,
    const std::string& private_key,
    const std::string& cert)
    : origin_(origin),
      type_(type),
      expiration_time_(expiration_time),
      private_key_(private_key),
      cert_(cert) {}

OriginBoundCertStore::OriginBoundCert::~OriginBoundCert() {}

}  // namespace net
