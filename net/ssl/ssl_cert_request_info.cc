// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_cert_request_info.h"

#include "net/cert/x509_certificate.h"

namespace net {

SSLCertRequestInfo::SSLCertRequestInfo() : is_proxy(false) {
}

void SSLCertRequestInfo::Reset() {
  host_and_port = HostPortPair();
  is_proxy = false;
  cert_authorities.clear();
  cert_key_types.clear();
}

SSLCertRequestInfo::~SSLCertRequestInfo() = default;

}  // namespace net
