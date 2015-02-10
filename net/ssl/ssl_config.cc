// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config.h"

#include "net/socket/ssl_client_socket.h"

namespace net {

const uint16 kDefaultSSLVersionMin = SSL_PROTOCOL_VERSION_TLS1;

const uint16 kDefaultSSLVersionFallbackMin = SSL_PROTOCOL_VERSION_TLS1;

SSLConfig::CertAndStatus::CertAndStatus() : cert_status(0) {}

SSLConfig::CertAndStatus::~CertAndStatus() {}

SSLConfig::SSLConfig()
    : rev_checking_enabled(false),
      rev_checking_required_local_anchors(false),
      version_min(kDefaultSSLVersionMin),
      version_max(SSLClientSocket::GetMaxSupportedSSLVersion()),
      version_fallback_min(kDefaultSSLVersionFallbackMin),
      channel_id_enabled(true),
      false_start_enabled(true),
      signed_cert_timestamps_enabled(true),
      require_forward_secrecy(false),
      send_client_cert(false),
      verify_ev_cert(false),
      version_fallback(false),
      cert_io_enabled(true),
      fastradio_padding_enabled(false),
      fastradio_padding_eligible(false) {
}

SSLConfig::~SSLConfig() {}

bool SSLConfig::IsAllowedBadCert(X509Certificate* cert,
                                 CertStatus* cert_status) const {
  std::string der_cert;
  if (!X509Certificate::GetDEREncoded(cert->os_cert_handle(), &der_cert))
    return false;
  return IsAllowedBadCert(der_cert, cert_status);
}

bool SSLConfig::IsAllowedBadCert(const base::StringPiece& der_cert,
                                 CertStatus* cert_status) const {
  for (size_t i = 0; i < allowed_bad_certs.size(); ++i) {
    if (der_cert == allowed_bad_certs[i].der_cert) {
      if (cert_status)
        *cert_status = allowed_bad_certs[i].cert_status;
      return true;
    }
  }
  return false;
}

}  // namespace net
