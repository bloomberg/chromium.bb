// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/cert_host_pair.h"

#include "net/cert/x509_certificate.h"

namespace web {

CertHostPair::CertHostPair(scoped_refptr<net::X509Certificate> cert,
                           std::string host)
    : cert_(std::move(cert)),
      host_(std::move(host)),
      cert_hash_(net::X509Certificate::CalculateChainFingerprint256(
          cert_->os_cert_handle(),
          cert_->GetIntermediateCertificates())) {}

CertHostPair::CertHostPair(const CertHostPair& other) = default;

CertHostPair::~CertHostPair() {}

bool CertHostPair::operator<(const CertHostPair& other) const {
  if (host_ != other.host_)
    return host_ < other.host_;
  return net::SHA256HashValueLessThan()(cert_hash_, other.cert_hash_);
}

}  // web
