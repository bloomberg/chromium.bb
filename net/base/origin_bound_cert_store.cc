// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/origin_bound_cert_store.h"

namespace net {

OriginBoundCertStore::OriginBoundCert::OriginBoundCert() {}

OriginBoundCertStore::OriginBoundCert::OriginBoundCert(
    const std::string& origin,
    SSLClientCertType type,
    const std::string& private_key,
    const std::string& cert)
    : origin_(origin),
      type_(type),
      private_key_(private_key),
      cert_(cert) {}

OriginBoundCertStore::OriginBoundCert::~OriginBoundCert() {}

}  // namespace net
