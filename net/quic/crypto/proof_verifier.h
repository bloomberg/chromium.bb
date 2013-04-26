// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_PROOF_VERIFIER_H_
#define NET_QUIC_CRYPTO_PROOF_VERIFIER_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace net {

// A ProofVerifier checks the signature on a server config, and the certificate
// chain that backs the public key.
class NET_EXPORT_PRIVATE ProofVerifier {
 public:
  virtual ~ProofVerifier() {}

  // VerifyProof checks that |signature| is a valid signature of
  // |server_config| by the public key in the leaf certificate of |certs|, and
  // that |certs| is a valid chain for |hostname|. On success, it returns true.
  // On failure, it returns false and sets |*error_details| to a description of
  // the problem.
  //
  // The signature uses SHA-256 as the hash function and PSS padding in the
  // case of RSA.
  //
  // Note: this is just for testing. The CN of the certificate is ignored and
  // wildcards in the SANs are not supported.
  virtual bool VerifyProof(const std::string& hostname,
                           const std::string& server_config,
                           const std::vector<std::string>& certs,
                           const std::string& signature,
                           std::string* error_details) const = 0;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_PROOF_VERIFIER_H_
