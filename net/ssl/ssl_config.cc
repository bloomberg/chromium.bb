// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config.h"

#include "net/cert/cert_verifier.h"

namespace net {

const uint16_t kDefaultSSLVersionMin = SSL_PROTOCOL_VERSION_TLS1;

const uint16_t kDefaultSSLVersionMax = SSL_PROTOCOL_VERSION_TLS1_2;

const TLS13Variant kDefaultTLS13Variant = kTLS13VariantDraft23;

SSLConfig::CertAndStatus::CertAndStatus() = default;
SSLConfig::CertAndStatus::CertAndStatus(scoped_refptr<X509Certificate> cert_arg,
                                        CertStatus status)
    : cert(std::move(cert_arg)), cert_status(status) {}
SSLConfig::CertAndStatus::CertAndStatus(const CertAndStatus& other) = default;
SSLConfig::CertAndStatus::~CertAndStatus() = default;

SSLConfig::SSLConfig()
    : version_min(kDefaultSSLVersionMin),
      version_max(kDefaultSSLVersionMax),
      tls13_variant(kDefaultTLS13Variant),
      early_data_enabled(false),
      version_interference_probe(false),
      channel_id_enabled(false),
      false_start_enabled(true),
      require_ecdhe(false),
      disable_cert_verification_network_fetches(false),
      send_client_cert(false),
      renego_allowed_default(false) {}

SSLConfig::SSLConfig(const SSLConfig& other) = default;

SSLConfig::~SSLConfig() = default;

bool SSLConfig::IsAllowedBadCert(X509Certificate* cert,
                                 CertStatus* cert_status) const {
  for (const auto& allowed_bad_cert : allowed_bad_certs) {
    if (cert->EqualsExcludingChain(allowed_bad_cert.cert.get())) {
      if (cert_status)
        *cert_status = allowed_bad_cert.cert_status;
      return true;
    }
  }
  return false;
}

int SSLConfig::GetCertVerifyFlags() const {
  int flags = 0;
  if (disable_cert_verification_network_fetches)
    flags |= CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES;

  return flags;
}

}  // namespace net
