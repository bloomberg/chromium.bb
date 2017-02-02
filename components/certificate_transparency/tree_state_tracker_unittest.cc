// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/tree_state_tracker.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/merkle_tree_leaf.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_tree_head.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::ct::SignedCertificateTimestamp;
using net::ct::SignedTreeHead;
using net::ct::GetSampleSignedTreeHead;
using net::ct::GetTestPublicKeyId;
using net::ct::GetTestPublicKey;
using net::ct::kSthRootHashLength;
using net::ct::GetX509CertSCT;

const base::Feature kCTLogAuditing = {"CertificateTransparencyLogAuditing",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

namespace certificate_transparency {

class TreeStateTrackerTest : public ::testing::Test {
  void SetUp() override {
    log_ = net::CTLogVerifier::Create(GetTestPublicKey(), "testlog",
                                      "https://ct.example.com",
                                      "unresolvable.invalid");

    ASSERT_TRUE(log_);
    ASSERT_EQ(log_->key_id(), GetTestPublicKeyId());

    const std::string der_test_cert(net::ct::GetDerEncodedX509Cert());
    chain_ = net::X509Certificate::CreateFromBytes(der_test_cert.data(),
                                                   der_test_cert.length());
    ASSERT_TRUE(chain_.get());
    GetX509CertSCT(&cert_sct_);
    cert_sct_->origin = SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE;
  }

 protected:
  base::MessageLoopForIO message_loop_;
  scoped_refptr<const net::CTLogVerifier> log_;
  std::unique_ptr<TreeStateTracker> tree_tracker_;
  scoped_refptr<net::X509Certificate> chain_;
  scoped_refptr<SignedCertificateTimestamp> cert_sct_;
  net::TestNetLog net_log_;
};

// Test that a new STH & SCT are delegated correctly to a
// SingleTreeTracker instance created by the TreeStateTracker.
// This is verified by looking for a single event on the net_log_
// passed into the TreeStateTracker c'tor.
TEST_F(TreeStateTrackerTest, TestDelegatesCorrectly) {
  std::vector<scoped_refptr<const net::CTLogVerifier>> verifiers;
  verifiers.push_back(log_);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCTLogAuditing);

  tree_tracker_ = base::MakeUnique<TreeStateTracker>(verifiers, &net_log_);

  SignedTreeHead sth;
  GetSampleSignedTreeHead(&sth);
  ASSERT_EQ(log_->key_id(), sth.log_id);
  tree_tracker_->NewSTHObserved(sth);

  ASSERT_EQ(log_->key_id(), cert_sct_->log_id);
  tree_tracker_->OnSCTVerified(chain_.get(), cert_sct_.get());
  base::RunLoop().RunUntilIdle();

  net::ct::MerkleTreeLeaf leaf;
  ASSERT_TRUE(GetMerkleTreeLeaf(chain_.get(), cert_sct_.get(), &leaf));

  std::string leaf_hash;
  ASSERT_TRUE(HashMerkleTreeLeaf(leaf, &leaf_hash));
  // There should be one NetLog event.
  EXPECT_EQ(1u, net_log_.GetSize());
}

}  // namespace certificate_transparency
