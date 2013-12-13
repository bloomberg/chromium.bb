// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_log_ct_verifier.h"

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/test_data_directory.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/ct_verify_result.h"
#include "net/cert/pem_tokenizer.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kLogDescription[] = "somelog";

class MultiLogCTVerifierTest : public ::testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    scoped_ptr<CTLogVerifier> log(
        CTLogVerifier::Create(ct::GetTestPublicKey(), kLogDescription));
    ASSERT_TRUE(log);

    verifier_.reset(new MultiLogCTVerifier());
    verifier_->AddLog(log.Pass());
    std::string der_test_cert(ct::GetDerEncodedX509Cert());
    chain_ = X509Certificate::CreateFromBytes(
        der_test_cert.data(),
        der_test_cert.length());
    ASSERT_TRUE(chain_);
  }

  bool CheckForSingleVerifiedSCTInResult(const ct::CTVerifyResult& result) {
    return (result.verified_scts.size() == 1U) &&
        result.invalid_scts.empty() &&
        result.unknown_logs_scts.empty() &&
        result.verified_scts[0]->log_description == kLogDescription;
  }

  bool CheckForSCTOrigin(
      const ct::CTVerifyResult& result,
      ct::SignedCertificateTimestamp::Origin origin) {
    return (result.verified_scts.size() > 0) &&
        (result.verified_scts[0]->origin == origin);
  }

  bool CheckForEmbeddedSCTInNetLog(CapturingNetLog& net_log) {
    CapturingNetLog::CapturedEntryList entries;
    net_log.GetEntries(&entries);
    if (entries.size() != 2)
      return false;

    const CapturingNetLog::CapturedEntry& received(entries[0]);
    std::string embedded_scts;
    if (!received.GetStringValue("embedded_scts", &embedded_scts))
      return false;
    if (embedded_scts.empty())
      return false;

    //XXX(eranm): entries[1] is the NetLog message with the checked SCTs.
    //When CapturedEntry has methods to get a dictionary, rather than just
    //a string, add more checks here.

    return true;
  }

  bool CheckPrecertificateVerification(scoped_refptr<X509Certificate> chain) {
    ct::CTVerifyResult result;
    CapturingNetLog net_log;
    BoundNetLog bound_net_log =
      BoundNetLog::Make(&net_log, NetLog::SOURCE_CONNECT_JOB);
    return (verifier_->Verify(chain, std::string(), std::string(), &result,
                              bound_net_log) == OK) &&
        CheckForSingleVerifiedSCTInResult(result) &&
        CheckForSCTOrigin(
            result, ct::SignedCertificateTimestamp::SCT_EMBEDDED) &&
        CheckForEmbeddedSCTInNetLog(net_log);
  }

 protected:
  scoped_ptr<MultiLogCTVerifier> verifier_;
  scoped_refptr<X509Certificate> chain_;
};

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCT) {
  scoped_refptr<X509Certificate> chain(
      CreateCertificateChainFromFile(GetTestCertsDirectory(),
                                     "ct-test-embedded-cert.pem",
                                     X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain);
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCTWithPreCA) {
  scoped_refptr<X509Certificate> chain(
      CreateCertificateChainFromFile(GetTestCertsDirectory(),
                                     "ct-test-embedded-with-preca-chain.pem",
                                     X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain);
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCTWithIntermediate) {
  scoped_refptr<X509Certificate> chain(CreateCertificateChainFromFile(
      GetTestCertsDirectory(),
      "ct-test-embedded-with-intermediate-chain.pem",
      X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain);
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest,
       VerifiesEmbeddedSCTWithIntermediateAndPreCA) {
  scoped_refptr<X509Certificate> chain(CreateCertificateChainFromFile(
      GetTestCertsDirectory(),
      "ct-test-embedded-with-intermediate-preca-chain.pem",
      X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain);
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest,
       VerifiesSCTOverX509Cert) {
  std::string sct(ct::GetTestSignedCertificateTimestamp());

  std::string sct_list;
  ASSERT_TRUE(ct::EncodeSCTListForTesting(sct, &sct_list));

  ct::CTVerifyResult result;
  EXPECT_EQ(OK,
            verifier_->Verify(chain_, std::string(), sct_list, &result,
                              BoundNetLog()));
  ASSERT_TRUE(CheckForSingleVerifiedSCTInResult(result));
  ASSERT_TRUE(CheckForSCTOrigin(
      result, ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION));
}

TEST_F(MultiLogCTVerifierTest,
       IdentifiesSCTFromUnknownLog) {
  std::string sct(ct::GetTestSignedCertificateTimestamp());

  // Change a byte inside the Log ID part of the SCT so it does
  // not match the log used in the tests
  sct[15] = 't';

  std::string sct_list;
  ASSERT_TRUE(ct::EncodeSCTListForTesting(sct, &sct_list));

  ct::CTVerifyResult result;
  EXPECT_NE(OK,
            verifier_->Verify(chain_, std::string(), sct_list, &result,
                              BoundNetLog()));
  EXPECT_EQ(1U, result.unknown_logs_scts.size());
  EXPECT_EQ("", result.unknown_logs_scts[0]->log_description);
}

}  // namespace

}  // namespace net
