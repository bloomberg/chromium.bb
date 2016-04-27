// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/merkle_tree_leaf.h"

#include <string>

#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace ct {

namespace {

class MerkleTreeLeafTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::string der_test_cert(ct::GetDerEncodedX509Cert());
    test_cert_ = X509Certificate::CreateFromBytes(der_test_cert.data(),
                                                  der_test_cert.length());
    ASSERT_TRUE(test_cert_);

    GetX509CertSCT(&x509_sct_);
    x509_sct_->origin = SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE;

    test_precert_ = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), "ct-test-embedded-cert.pem",
        X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(test_precert_);
    ASSERT_EQ(1u, test_precert_->GetIntermediateCertificates().size());
    GetPrecertSCT(&precert_sct_);
    precert_sct_->origin = SignedCertificateTimestamp::SCT_EMBEDDED;
  }

 protected:
  scoped_refptr<SignedCertificateTimestamp> x509_sct_;
  scoped_refptr<SignedCertificateTimestamp> precert_sct_;
  scoped_refptr<X509Certificate> test_cert_;
  scoped_refptr<X509Certificate> test_precert_;
};

TEST_F(MerkleTreeLeafTest, CreatesForX509Cert) {
  MerkleTreeLeaf leaf;
  ASSERT_TRUE(GetMerkleTreeLeaf(test_cert_.get(), x509_sct_.get(), &leaf));

  EXPECT_EQ(x509_sct_->log_id, leaf.log_id);
  EXPECT_EQ(LogEntry::LOG_ENTRY_TYPE_X509, leaf.log_entry.type);
  EXPECT_FALSE(leaf.log_entry.leaf_certificate.empty());
  EXPECT_TRUE(leaf.log_entry.tbs_certificate.empty());

  EXPECT_EQ(x509_sct_->timestamp, leaf.timestamp);
  EXPECT_EQ(x509_sct_->extensions, leaf.extensions);
}

TEST_F(MerkleTreeLeafTest, CreatesForPrecert) {
  MerkleTreeLeaf leaf;
  ASSERT_TRUE(
      GetMerkleTreeLeaf(test_precert_.get(), precert_sct_.get(), &leaf));

  EXPECT_EQ(precert_sct_->log_id, leaf.log_id);
  EXPECT_EQ(LogEntry::LOG_ENTRY_TYPE_PRECERT, leaf.log_entry.type);
  EXPECT_FALSE(leaf.log_entry.tbs_certificate.empty());
  EXPECT_TRUE(leaf.log_entry.leaf_certificate.empty());

  EXPECT_EQ(precert_sct_->timestamp, leaf.timestamp);
  EXPECT_EQ(precert_sct_->extensions, leaf.extensions);
}

TEST_F(MerkleTreeLeafTest, DoesNotCreateForEmbeddedSCTButNotPrecert) {
  MerkleTreeLeaf leaf;
  ASSERT_FALSE(GetMerkleTreeLeaf(test_cert_.get(), precert_sct_.get(), &leaf));
}

}  // namespace

}  // namespace ct

}  // namespace net
