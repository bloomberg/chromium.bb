// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/cert_host_pair.h"

#include "net/cert/x509_certificate.h"

namespace web {

CertHostPair::CertHostPair(const scoped_refptr<net::X509Certificate>& cert,
                           const std::string& host)
    : cert(cert), host(host) {}

CertHostPair::~CertHostPair() {}

bool CertHostPair::operator<(const CertHostPair& other) const {
  if (host != other.host)
    return host < other.host;
  return net::X509Certificate::LessThan()(cert, other.cert);
}

}  // web
