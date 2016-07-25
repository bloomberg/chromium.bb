// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CERTIFICATE_CAST_CRL_H_
#define COMPONENTS_CAST_CERTIFICATE_CAST_CRL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/cert/internal/parsed_certificate.h"

namespace cast_certificate {

// This class represents the CRL information parsed from the binary proto.
class CastCRL {
 public:
  virtual ~CastCRL(){};

  // Verifies the revocation status of a cast device certificate given a chain
  // of X.509 certificates.
  //
  // Inputs:
  // * |certs| is the verified chain of X.509 certificates:
  //   * |certs[0]| is the target certificate (i.e. the device certificate).
  //   * |certs[i]| is the certificate that issued certs[i-1].
  //   * |certs.back()| is assumed to be a trusted root.
  //
  // * |time| is the unix timestamp to use for determining if the certificate
  //   is revoked.
  //
  // Output:
  // Returns true if no certificate in the chain was revoked.
  virtual bool CheckRevocation(const net::ParsedCertificateList& certs,
                               const base::Time& time) const = 0;
};

// Parses and verifies the CRL used to verify the revocation status of
// Cast device certificates.
//
// Inputs:
// * |crl_proto| is a serialized cast_certificate.CrlBundle proto.
// * |time| is the unix timestamp to use for determining if the CRL is valid.
//
// Output:
// Returns the CRL object if success, nullptr otherwise.
std::unique_ptr<CastCRL> ParseAndVerifyCRL(const std::string& crl_proto,
                                           const base::Time& time);

// Exposed only for testing, not for use in production code.
//
// Replaces trusted root certificates into the CastCRLTrustStore.
//
// Output:
// Returns true if successful, false if nothing is changed.
bool SetCRLTrustAnchorForTest(const std::string& cert) WARN_UNUSED_RESULT;

}  // namespace cast_certificate

#endif  // COMPONENTS_CAST_CERTIFICATE_CAST_CRL_H_
