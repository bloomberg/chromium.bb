// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/cert_verify_proc_chromeos.h"

#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class CertVerifyProcChromeOSTest : public testing::Test {
 public:
  CertVerifyProcChromeOSTest() : user_1_("user1"), user_2_("user2") {}

  virtual void SetUp() OVERRIDE {
    // Initialize nss_util slots.
    ASSERT_TRUE(user_1_.constructed_successfully());
    ASSERT_TRUE(user_2_.constructed_successfully());
    user_1_.FinishInit();
    user_2_.FinishInit();

    // Create NSSCertDatabaseChromeOS for each user.
    db_1_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(user_1_.username_hash()),
        crypto::GetPrivateSlotForChromeOSUser(
            user_1_.username_hash(),
            base::Callback<void(crypto::ScopedPK11Slot)>())));
    db_2_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(user_2_.username_hash()),
        crypto::GetPrivateSlotForChromeOSUser(
            user_2_.username_hash(),
            base::Callback<void(crypto::ScopedPK11Slot)>())));

    // Create default verifier and for each user.
    verify_proc_default_ = new CertVerifyProcChromeOS();
    verify_proc_1_ = new CertVerifyProcChromeOS(db_1_->GetPublicSlot());
    verify_proc_2_ = new CertVerifyProcChromeOS(db_2_->GetPublicSlot());

    // Load test cert chains from disk.
    certs_1_ =
        net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                           "multi-root-chain1.pem",
                                           net::X509Certificate::FORMAT_AUTO);
    ASSERT_EQ(4U, certs_1_.size());

    certs_2_ =
        net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                           "multi-root-chain2.pem",
                                           net::X509Certificate::FORMAT_AUTO);
    ASSERT_EQ(4U, certs_2_.size());

    // The chains:
    //   1. A (end-entity) -> B -> C -> D (self-signed root)
    //   2. A (end-entity) -> B -> C2 -> E (self-signed root)
    ASSERT_TRUE(certs_1_[0]->Equals(certs_2_[0].get()));
    ASSERT_TRUE(certs_1_[1]->Equals(certs_2_[1].get()));
    ASSERT_FALSE(certs_1_[2]->Equals(certs_2_[2].get()));
    ASSERT_EQ("C CA", certs_1_[2]->subject().common_name);
    ASSERT_EQ("C CA", certs_2_[2]->subject().common_name);

    root_1_.push_back(certs_1_.back());
    root_2_.push_back(certs_2_.back());

    ASSERT_EQ("D Root CA", root_1_[0]->subject().common_name);
    ASSERT_EQ("E Root CA", root_2_[0]->subject().common_name);
  }

  int VerifyWithAdditionalTrustAnchors(
      net::CertVerifyProc* verify_proc,
      const net::CertificateList& additional_trust_anchors,
      net::X509Certificate* cert,
      std::string* root_subject_name) {
    int flags = 0;
    net::CertVerifyResult verify_result;
    int error = verify_proc->Verify(cert,
                                    "127.0.0.1",
                                    flags,
                                    NULL,
                                    additional_trust_anchors,
                                    &verify_result);
    if (verify_result.verified_cert.get() &&
        !verify_result.verified_cert->GetIntermediateCertificates().empty()) {
      net::X509Certificate::OSCertHandle root =
          verify_result.verified_cert->GetIntermediateCertificates().back();
      root_subject_name->assign(root->subjectName);
    } else {
      root_subject_name->clear();
    }
    return error;
  }

  int Verify(net::CertVerifyProc* verify_proc,
             net::X509Certificate* cert,
             std::string* root_subject_name) {
    net::CertificateList additional_trust_anchors;
    return VerifyWithAdditionalTrustAnchors(
        verify_proc, additional_trust_anchors, cert, root_subject_name);
  }

 protected:
  crypto::ScopedTestNSSChromeOSUser user_1_;
  crypto::ScopedTestNSSChromeOSUser user_2_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> db_1_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> db_2_;
  scoped_refptr<net::CertVerifyProc> verify_proc_default_;
  scoped_refptr<net::CertVerifyProc> verify_proc_1_;
  scoped_refptr<net::CertVerifyProc> verify_proc_2_;
  net::CertificateList certs_1_;
  net::CertificateList certs_2_;
  net::CertificateList root_1_;
  net::CertificateList root_2_;
};

// Test that the CertVerifyProcChromeOS doesn't trusts roots that are in other
// user's slots or that have been deleted, and that verifying done by one user
// doesn't affect verifications done by others.
TEST_F(CertVerifyProcChromeOSTest, TestChainVerify) {
  scoped_refptr<net::X509Certificate> server = certs_1_[0];
  std::string verify_root;
  // Before either of the root certs have been trusted, all verifications should
  // fail with CERT_AUTHORITY_INVALID.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_default_.get(), server.get(), &verify_root));
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_1_.get(), server.get(), &verify_root));
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_2_.get(), server.get(), &verify_root));

  // Import and trust the D root for user 1.
  net::NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(db_1_->ImportCACerts(
      root_1_, net::NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  // Imported CA certs are not trusted by default verifier.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_default_.get(), server.get(), &verify_root));
  // User 1 should now verify successfully through the D root.
  EXPECT_EQ(net::OK, Verify(verify_proc_1_.get(), server.get(), &verify_root));
  EXPECT_EQ("CN=D Root CA", verify_root);
  // User 2 should still fail.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_2_.get(), server.get(), &verify_root));

  // Import and trust the E root for user 2.
  failed.clear();
  EXPECT_TRUE(db_2_->ImportCACerts(
      root_2_, net::NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  // Imported CA certs are not trusted by default verifier.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_default_.get(), server.get(), &verify_root));
  // User 1 should still verify successfully through the D root.
  EXPECT_EQ(net::OK, Verify(verify_proc_1_.get(), server.get(), &verify_root));
  EXPECT_EQ("CN=D Root CA", verify_root);
  // User 2 should now verify successfully through the E root.
  EXPECT_EQ(net::OK, Verify(verify_proc_2_.get(), server.get(), &verify_root));
  EXPECT_EQ("CN=E Root CA", verify_root);

  // Delete D root.
  EXPECT_TRUE(db_1_->DeleteCertAndKey(root_1_[0].get()));
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_default_.get(), server.get(), &verify_root));
  // User 1 should now fail to verify.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_1_.get(), server.get(), &verify_root));
  // User 2 should still verify successfully through the E root.
  EXPECT_EQ(net::OK, Verify(verify_proc_2_.get(), server.get(), &verify_root));
  EXPECT_EQ("CN=E Root CA", verify_root);

  // Delete E root.
  EXPECT_TRUE(db_2_->DeleteCertAndKey(root_2_[0].get()));
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_default_.get(), server.get(), &verify_root));
  // User 1 should still fail to verify.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_1_.get(), server.get(), &verify_root));
  // User 2 should now fail to verify.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            Verify(verify_proc_2_.get(), server.get(), &verify_root));
}

// Test that roots specified through additional_trust_anchors are trusted for
// that verification, and that there is not any caching that affects later
// verifications.
TEST_F(CertVerifyProcChromeOSTest, TestAdditionalTrustAnchors) {
  EXPECT_TRUE(verify_proc_default_->SupportsAdditionalTrustAnchors());
  EXPECT_TRUE(verify_proc_1_->SupportsAdditionalTrustAnchors());

  scoped_refptr<net::X509Certificate> server = certs_1_[0];
  std::string verify_root;
  net::CertificateList additional_trust_anchors;

  // Before either of the root certs have been trusted, all verifications should
  // fail with CERT_AUTHORITY_INVALID.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyWithAdditionalTrustAnchors(verify_proc_default_.get(),
                                             additional_trust_anchors,
                                             server.get(),
                                             &verify_root));
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyWithAdditionalTrustAnchors(verify_proc_1_.get(),
                                             additional_trust_anchors,
                                             server.get(),
                                             &verify_root));

  // Use D Root CA as additional trust anchor. Verifications should succeed now.
  additional_trust_anchors.push_back(root_1_[0]);
  EXPECT_EQ(net::OK,
            VerifyWithAdditionalTrustAnchors(verify_proc_default_.get(),
                                             additional_trust_anchors,
                                             server.get(),
                                             &verify_root));
  EXPECT_EQ("CN=D Root CA", verify_root);
  EXPECT_EQ(net::OK,
            VerifyWithAdditionalTrustAnchors(verify_proc_1_.get(),
                                             additional_trust_anchors,
                                             server.get(),
                                             &verify_root));
  EXPECT_EQ("CN=D Root CA", verify_root);
  // User 2 should still fail.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyWithAdditionalTrustAnchors(verify_proc_2_.get(),
                                             net::CertificateList(),
                                             server.get(),
                                             &verify_root));

  // Without additional trust anchors, verification should fail again.
  additional_trust_anchors.clear();
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyWithAdditionalTrustAnchors(verify_proc_default_.get(),
                                             additional_trust_anchors,
                                             server.get(),
                                             &verify_root));
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyWithAdditionalTrustAnchors(verify_proc_1_.get(),
                                             additional_trust_anchors,
                                             server.get(),
                                             &verify_root));

  // Import and trust the D Root CA for user 2.
  net::CertificateList roots;
  roots.push_back(root_1_[0]);
  net::NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(
      db_2_->ImportCACerts(roots, net::NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  // Use D Root CA as additional trust anchor. Verifications should still
  // succeed even if the cert is trusted by a different profile.
  additional_trust_anchors.push_back(root_1_[0]);
  EXPECT_EQ(net::OK,
            VerifyWithAdditionalTrustAnchors(verify_proc_default_.get(),
                                             additional_trust_anchors,
                                             server.get(),
                                             &verify_root));
  EXPECT_EQ("CN=D Root CA", verify_root);
  EXPECT_EQ(net::OK,
            VerifyWithAdditionalTrustAnchors(verify_proc_1_.get(),
                                             additional_trust_anchors,
                                             server.get(),
                                             &verify_root));
  EXPECT_EQ("CN=D Root CA", verify_root);
  EXPECT_EQ(net::OK,
            VerifyWithAdditionalTrustAnchors(verify_proc_2_.get(),
                                             additional_trust_anchors,
                                             server.get(),
                                             &verify_root));
  EXPECT_EQ("CN=D Root CA", verify_root);
}

class CertVerifyProcChromeOSOrderingTest
    : public CertVerifyProcChromeOSTest,
      public ::testing::WithParamInterface<
          std::tr1::tuple<bool, int, std::string> > {};

// Test a variety of different combinations of (maybe) verifying / (maybe)
// importing / verifying again, to try to find any cases where caching might
// affect the results.
// http://crbug.com/396501
TEST_P(CertVerifyProcChromeOSOrderingTest, DISABLED_TrustThenVerify) {
  const ParamType& param = GetParam();
  const bool verify_first = std::tr1::get<0>(param);
  const int trust_bitmask = std::tr1::get<1>(param);
  const std::string test_order = std::tr1::get<2>(param);
  DVLOG(1) << "verify_first: " << verify_first
           << " trust_bitmask: " << trust_bitmask
           << " test_order: " << test_order;

  scoped_refptr<net::X509Certificate> server = certs_1_[0];
  std::string verify_root;

  if (verify_first) {
    // Before either of the root certs have been trusted, all verifications
    // should fail with CERT_AUTHORITY_INVALID.
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
              Verify(verify_proc_default_.get(), server.get(), &verify_root));
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
              Verify(verify_proc_1_.get(), server.get(), &verify_root));
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
              Verify(verify_proc_2_.get(), server.get(), &verify_root));
  }

  int expected_user1_result = net::ERR_CERT_AUTHORITY_INVALID;
  int expected_user2_result = net::ERR_CERT_AUTHORITY_INVALID;

  if (trust_bitmask & 1) {
    expected_user1_result = net::OK;
    // Import and trust the D root for user 1.
    net::NSSCertDatabase::ImportCertFailureList failed;
    EXPECT_TRUE(db_1_->ImportCACerts(
        root_1_, net::NSSCertDatabase::TRUSTED_SSL, &failed));
    EXPECT_EQ(0U, failed.size());
    for (size_t i = 0; i < failed.size(); ++i) {
      LOG(ERROR) << "import fail " << failed[i].net_error << " for "
                 << failed[i].certificate->subject().GetDisplayName();
    }
  }

  if (trust_bitmask & 2) {
    expected_user2_result = net::OK;
    // Import and trust the E root for user 2.
    net::NSSCertDatabase::ImportCertFailureList failed;
    EXPECT_TRUE(db_2_->ImportCACerts(
        root_2_, net::NSSCertDatabase::TRUSTED_SSL, &failed));
    EXPECT_EQ(0U, failed.size());
    for (size_t i = 0; i < failed.size(); ++i) {
      LOG(ERROR) << "import fail " << failed[i].net_error << " for "
                 << failed[i].certificate->subject().GetDisplayName();
    }
  }

  // Repeat the tests twice, they should return the same each time.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);
    for (std::string::const_iterator j = test_order.begin();
         j != test_order.end();
         ++j) {
      switch (*j) {
        case 'd':
          // Default verifier should always fail.
          EXPECT_EQ(
              net::ERR_CERT_AUTHORITY_INVALID,
              Verify(verify_proc_default_.get(), server.get(), &verify_root));
          break;
        case '1':
          EXPECT_EQ(expected_user1_result,
                    Verify(verify_proc_1_.get(), server.get(), &verify_root));
          if (expected_user1_result == net::OK)
            EXPECT_EQ("CN=D Root CA", verify_root);
          break;
        case '2':
          EXPECT_EQ(expected_user2_result,
                    Verify(verify_proc_2_.get(), server.get(), &verify_root));
          if (expected_user2_result == net::OK)
            EXPECT_EQ("CN=E Root CA", verify_root);
          break;
        default:
          FAIL();
      }
    }
  }
}

INSTANTIATE_TEST_CASE_P(
    Variations,
    CertVerifyProcChromeOSOrderingTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Range(0, 1 << 2),
        ::testing::Values("d12", "d21", "1d2", "12d", "2d1", "21d")));

}  // namespace chromeos
