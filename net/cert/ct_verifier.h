// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_VERIFIER_H_
#define NET_CERT_CT_VERIFIER_H_

#include "net/base/net_export.h"

namespace net {

namespace ct {
struct CTVerifyResult;
}  // namespace ct

class BoundNetLog;
class X509Certificate;

// Interface for verifying Signed Certificate Timestamps over a certificate.
class NET_EXPORT CTVerifier {
 public:
  virtual ~CTVerifier() {}

  // Verifies either embedded SCTs or SCTs obtained via the
  // signed_certificate_timestamp TLS extension or OCSP on  the given |cert|
  // |result| will be filled with these SCTs, divided into categories based on
  // the verification result.
  virtual int Verify(X509Certificate* cert,
                     const std::string& sct_list_from_ocsp,
                     const std::string& sct_list_from_tls_extension,
                     ct::CTVerifyResult* result,
                     const BoundNetLog& net_log) = 0;
};

}  // namespace net

#endif  // NET_CERT_CT_VERIFIER_H_
