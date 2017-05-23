// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_IGNORE_ERRORS_CERT_VERIFIER_H_
#define CHROME_BROWSER_SSL_IGNORE_ERRORS_CERT_VERIFIER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "net/cert/cert_verifier.h"

namespace net {
struct SHA256HashValue;
class SHA256HashValueLessThan;
}  // namespace net

// IgnoreErrorsCertVerifier wraps another CertVerifier in order to ignore
// verification errors from certificate chains that match a whitelist of SPKI
// fingerprints.
class IgnoreErrorsCertVerifier : public net::CertVerifier {
 public:
  // SPKIHashSet is a set of SHA-256 SPKI fingerprints (RFC 7469, Section 2.4).
  using SPKIHashSet =
      base::flat_set<net::SHA256HashValue, net::SHA256HashValueLessThan>;

  // MaybeWrapCertVerifier returns an IgnoreErrorsCertVerifier wrapping the
  // supplied verifier using the whitelist from the
  // --ignore-certificate-errors-spki-list flag of the command line if the
  // --user-data-dir flag is also present. If either of these flags are missing,
  // it returns the supplied verifier.
  static std::unique_ptr<net::CertVerifier> MaybeWrapCertVerifier(
      const base::CommandLine& command_line,
      std::unique_ptr<net::CertVerifier> verifier);

  // MakeWhitelist converts a vector of Base64-encoded SHA-256 SPKI fingerprints
  // into an SPKIHashSet. Invalid fingerprints are logged and skipped.
  static SPKIHashSet MakeWhitelist(
      const std::vector<std::string>& fingerprints);

  IgnoreErrorsCertVerifier(std::unique_ptr<net::CertVerifier> verifier,
                           SPKIHashSet whitelist);

  ~IgnoreErrorsCertVerifier() override;

  // Verify skips certificate verification and returns OK if any of the
  // certificates from the chain in |params| match one of the SPKI fingerprints
  // from the whitelist. Otherwise, it invokes Verify on the wrapped verifier
  // and returns the result.
  int Verify(const RequestParams& params,
             net::CRLSet* crl_set,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback& callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override;

 private:
  friend class IgnoreErrorsCertVerifierTest;
  void set_whitelist(const SPKIHashSet& whitelist);  // Testing only.

  std::unique_ptr<net::CertVerifier> verifier_;
  SPKIHashSet whitelist_;
};

#endif  // CHROME_BROWSER_SSL_IGNORE_ERRORS_CERT_VERIFIER_H_
