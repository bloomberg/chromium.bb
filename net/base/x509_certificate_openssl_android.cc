// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include "base/logging.h"
#include "net/android/network_library.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"

namespace net {

int X509Certificate::VerifyInternal(const std::string& hostname,
                                    int flags,
                                    CertVerifyResult* verify_result) const {
  if (!VerifyNameMatch(hostname))
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;

  std::vector<std::string> cert_bytes;
  GetChainDEREncodedBytes(&cert_bytes);

  // TODO(joth): Fetch the authentication type from SSL rather than hardcode.
  android::VerifyResult result =
      android::VerifyX509CertChain(cert_bytes, hostname, "RSA");
  switch (result) {
    case android::VERIFY_OK:
      return OK;
    case android::VERIFY_BAD_HOSTNAME:
      verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;
      break;
    case android::VERIFY_NO_TRUSTED_ROOT:
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
      break;
    case android::VERIFY_INVOCATION_ERROR:
    default:
      verify_result->cert_status |= ERR_CERT_INVALID;
      break;
  }
  return MapCertStatusToNetError(verify_result->cert_status);
}

void X509Certificate::GetChainDEREncodedBytes(
    std::vector<std::string>* chain_bytes) const {
  OSCertHandles cert_handles(intermediate_ca_certs_);
  // Make sure the peer's own cert is the first in the chain, if it's not
  // already there.
  if (cert_handles.empty())
    cert_handles.insert(cert_handles.begin(), cert_handle_);

  chain_bytes->reserve(cert_handles.size());
  for (OSCertHandles::const_iterator it = cert_handles.begin();
       it != cert_handles.end(); ++it) {
    DERCache der_cache = {0};
    GetDERAndCacheIfNeeded(*it, &der_cache);
    std::string cert_bytes (
        reinterpret_cast<const char*>(der_cache.data), der_cache.data_length);
    chain_bytes->push_back(cert_bytes);
  }
}

}  // namespace net
