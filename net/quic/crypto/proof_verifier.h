// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_PROOF_VERIFIER_H_
#define NET_QUIC_CRYPTO_PROOF_VERIFIER_H_

#include <string>
#include <vector>

#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

namespace net {

class CertVerifyResult;

// A ProofVerifier checks the signature on a server config, and the certificate
// chain that backs the public key.
class NET_EXPORT_PRIVATE ProofVerifier {
 public:
  virtual ~ProofVerifier() {}

  // VerifyProof checks that |signature| is a valid signature of
  // |server_config| by the public key in the leaf certificate of |certs|, and
  // that |certs| is a valid chain for |hostname|. On success, it returns OK.
  // On failure, it returns ERR_FAILED and sets |*error_details| to a
  // description of the problem. This function may also return ERR_IO_PENDING,
  // in which case the |callback| will be run on the calling thread with the
  // final OK/ERR_FAILED result when the proof is verified.
  //
  // The signature uses SHA-256 as the hash function and PSS padding in the
  // case of RSA.
  //
  // Note: this is just for testing. The CN of the certificate is ignored and
  // wildcards in the SANs are not supported.
  virtual int VerifyProof(const std::string& hostname,
                          const std::string& server_config,
                          const std::vector<std::string>& certs,
                          const std::string& signature,
                          std::string* error_details,
                          CertVerifyResult* cert_verify_result,
                          const CompletionCallback& callback) = 0;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_PROOF_VERIFIER_H_
