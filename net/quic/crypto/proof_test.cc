// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/proof_source.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using std::vector;

namespace net {
namespace test {

TEST(Proof, Verify) {
  // TODO(rtenneti): Enable testing of ProofVerifier.
#if 0
  scoped_ptr<ProofSource> source(CryptoTestUtils::ProofSourceForTesting());
  scoped_ptr<ProofVerifier> verifier(
      CryptoTestUtils::ProofVerifierForTesting());

  const string server_config = "server config bytes";
  const string hostname = "test.example.com";
  const vector<string>* certs;
  const vector<string>* first_certs;
  string error_details, signature, first_signature;

  ASSERT_TRUE(source->GetProof(hostname, server_config, &first_certs,
                               &first_signature));
  ASSERT_TRUE(source->GetProof(hostname, server_config, &certs, &signature));

  // Check that the proof source is caching correctly:
  ASSERT_EQ(first_certs, certs);
  ASSERT_EQ(signature, first_signature);

  ASSERT_TRUE(verifier->VerifyProof(hostname, server_config, *certs, signature,
                                    &error_details));
  ASSERT_FALSE(verifier->VerifyProof("foo.com", server_config, *certs,
                                     signature, &error_details));
  ASSERT_FALSE(
      verifier->VerifyProof(hostname, server_config.substr(1, string::npos),
                            *certs, signature, &error_details));
  const string corrupt_signature = "1" + signature;
  ASSERT_FALSE(verifier->VerifyProof(hostname, server_config, *certs,
                                     corrupt_signature, &error_details));

  vector<string> wrong_certs;
  for (size_t i = 1; i < certs->size(); i++) {
    wrong_certs.push_back((*certs)[i]);
  }
  ASSERT_FALSE(verifier->VerifyProof("foo.com", server_config, wrong_certs,
                                     signature, &error_details));
#endif  // 0
}

}  // namespace test
}  // namespace net
