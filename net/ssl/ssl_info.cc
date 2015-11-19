// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_info.h"

#include "base/pickle.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"

namespace net {

SSLInfo::SSLInfo() {
  Reset();
}

SSLInfo::SSLInfo(const SSLInfo& info) {
  *this = info;
}

SSLInfo::~SSLInfo() {
}

SSLInfo& SSLInfo::operator=(const SSLInfo& info) {
  cert = info.cert;
  unverified_cert = info.unverified_cert;
  cert_status = info.cert_status;
  security_bits = info.security_bits;
  key_exchange_info = info.key_exchange_info;
  connection_status = info.connection_status;
  is_issued_by_known_root = info.is_issued_by_known_root;
  client_cert_sent = info.client_cert_sent;
  channel_id_sent = info.channel_id_sent;
  token_binding_negotiated = info.token_binding_negotiated;
  token_binding_key_param = info.token_binding_key_param;
  handshake_type = info.handshake_type;
  public_key_hashes = info.public_key_hashes;
  signed_certificate_timestamps = info.signed_certificate_timestamps;
  pinning_failure_log = info.pinning_failure_log;

  return *this;
}

void SSLInfo::Reset() {
  cert = NULL;
  unverified_cert = NULL;
  cert_status = 0;
  security_bits = -1;
  key_exchange_info = 0;
  connection_status = 0;
  is_issued_by_known_root = false;
  client_cert_sent = false;
  channel_id_sent = false;
  token_binding_negotiated = false;
  token_binding_key_param = TB_PARAM_ECDSAP256;
  handshake_type = HANDSHAKE_UNKNOWN;
  public_key_hashes.clear();
  signed_certificate_timestamps.clear();
  pinning_failure_log.clear();
}

void SSLInfo::SetCertError(int error) {
  cert_status |= MapNetErrorToCertStatus(error);
}

void SSLInfo::UpdateSignedCertificateTimestamps(
    const ct::CTVerifyResult& ct_verify_result) {
  for (const auto& sct : ct_verify_result.verified_scts) {
    signed_certificate_timestamps.push_back(
        SignedCertificateTimestampAndStatus(sct, ct::SCT_STATUS_OK));
  }
  for (const auto& sct : ct_verify_result.invalid_scts) {
    signed_certificate_timestamps.push_back(
        SignedCertificateTimestampAndStatus(sct, ct::SCT_STATUS_INVALID));
  }
  for (const auto& sct : ct_verify_result.unknown_logs_scts) {
    signed_certificate_timestamps.push_back(
        SignedCertificateTimestampAndStatus(sct, ct::SCT_STATUS_LOG_UNKNOWN));
  }
}

}  // namespace net
