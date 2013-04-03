// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "crypto/nss_util.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_trust_anchor_provider.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::ReturnRef;

namespace policy {

namespace {

class MockCertTrustAnchorProvider : public net::CertTrustAnchorProvider {
 public:
  MockCertTrustAnchorProvider() {}
  virtual ~MockCertTrustAnchorProvider() {}

  MOCK_METHOD0(GetAdditionalTrustAnchors, const net::CertificateList&());
};

}  // namespace

// This is actually a unit test, but is linked with browser_tests because
// importing a certificate into the NSS test database persists for the duration
// of a process; since each browser_test runs in a separate process then this
// won't affect subsequent tests.
// This can be moved to the unittests target once the TODO in ~ScopedTestNSSDB
// is fixed.
class PolicyCertVerifierTest : public testing::Test {
 public:
  PolicyCertVerifierTest()
      : cert_db_(NULL),
        ui_thread_(content::BrowserThread::UI, &loop_),
        io_thread_(content::BrowserThread::IO, &loop_),
        profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(NULL) {}

  virtual ~PolicyCertVerifierTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(test_nssdb_.is_open());
    cert_db_ = net::NSSCertDatabase::GetInstance();

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("profile");

    cert_verifier_.reset(new PolicyCertVerifier(profile_, &trust_provider_));
  }

  scoped_refptr<net::X509Certificate> LoadCertificate(const std::string& name,
                                                      net::CertType type) {
    scoped_refptr<net::X509Certificate> cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), name);

    // No certificate is trusted right after it's loaded.
    net::NSSCertDatabase::TrustBits trust =
        cert_db_->GetCertTrust(cert.get(), type);
    EXPECT_EQ(net::NSSCertDatabase::TRUST_DEFAULT, trust);

    return cert;
  }

 protected:
  crypto::ScopedTestNSSDB test_nssdb_;
  net::NSSCertDatabase* cert_db_;
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  MockCertTrustAnchorProvider trust_provider_;
  scoped_ptr<PolicyCertVerifier> cert_verifier_;
  const net::CertificateList empty_cert_list_;
};

TEST_F(PolicyCertVerifierTest, VerifyUntrustedCert) {
  scoped_refptr<net::X509Certificate> cert =
      LoadCertificate("ok_cert.pem", net::SERVER_CERT);
  ASSERT_TRUE(cert);

  // |cert| is untrusted, so Verify() fails.
  net::CertVerifyResult verify_result;
  net::TestCompletionCallback callback;
  net::CertVerifier::RequestHandle request_handle;
  EXPECT_CALL(trust_provider_, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(empty_cert_list_));
  int error = cert_verifier_->Verify(cert, "127.0.0.1", 0, NULL,
                                     &verify_result, callback.callback(),
                                     &request_handle, net::BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider_);
  ASSERT_EQ(net::ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle);
  error = callback.WaitForResult();
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);

  // Issuing the same request again hits the cache. This tests the synchronous
  // path.
  EXPECT_CALL(trust_provider_, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(empty_cert_list_));
  error = cert_verifier_->Verify(cert, "127.0.0.1", 0, NULL,
                                 &verify_result, callback.callback(),
                                 &request_handle, net::BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider_);
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);

  // The profile is not tainted.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kUsedPolicyCertificatesOnce));
}

TEST_F(PolicyCertVerifierTest, VerifyTrustedCert) {
  // |ca_cert| is the issuer of |cert|.
  scoped_refptr<net::X509Certificate> ca_cert =
      LoadCertificate("root_ca_cert.crt", net::CA_CERT);
  ASSERT_TRUE(ca_cert);
  scoped_refptr<net::X509Certificate> cert =
      LoadCertificate("ok_cert.pem", net::SERVER_CERT);
  ASSERT_TRUE(cert);

  // Make the database trust |ca_cert|.
  net::CertificateList import_list;
  import_list.push_back(ca_cert);
  net::NSSCertDatabase::ImportCertFailureList failure_list;
  ASSERT_TRUE(cert_db_->ImportCACerts(
      import_list, net::NSSCertDatabase::TRUSTED_SSL, &failure_list));
  ASSERT_TRUE(failure_list.empty());

  // Verify that it is now trusted.
  net::NSSCertDatabase::TrustBits trust =
      cert_db_->GetCertTrust(ca_cert.get(), net::CA_CERT);
  EXPECT_EQ(net::NSSCertDatabase::TRUSTED_SSL, trust);

  // Verify() successfully verifies |cert| after it was imported.
  net::CertVerifyResult verify_result;
  net::TestCompletionCallback callback;
  net::CertVerifier::RequestHandle request_handle;
  EXPECT_CALL(trust_provider_, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(empty_cert_list_));
  int error = cert_verifier_->Verify(cert, "127.0.0.1", 0, NULL,
                                     &verify_result, callback.callback(),
                                     &request_handle, net::BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider_);
  ASSERT_EQ(net::ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle);
  error = callback.WaitForResult();
  EXPECT_EQ(net::OK, error);

  // The profile is not tainted, since the certificate is trusted from the
  // database.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kUsedPolicyCertificatesOnce));
}

TEST_F(PolicyCertVerifierTest, VerifyUsingAdditionalTrustAnchor) {
  // |ca_cert| is the issuer of |cert|.
  scoped_refptr<net::X509Certificate> ca_cert =
      LoadCertificate("root_ca_cert.crt", net::CA_CERT);
  ASSERT_TRUE(ca_cert);
  scoped_refptr<net::X509Certificate> cert =
      LoadCertificate("ok_cert.pem", net::SERVER_CERT);
  ASSERT_TRUE(cert);

  net::CertificateList additional_trust_anchors;
  additional_trust_anchors.push_back(ca_cert);

  // Verify() successfully verifies |cert|, using |ca_cert| from the list of
  // |additional_trust_anchors|.
  net::CertVerifyResult verify_result;
  net::TestCompletionCallback callback;
  net::CertVerifier::RequestHandle request_handle;
  EXPECT_CALL(trust_provider_, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(additional_trust_anchors));
  int error = cert_verifier_->Verify(cert, "127.0.0.1", 0, NULL,
                                     &verify_result, callback.callback(),
                                     &request_handle, net::BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider_);
  ASSERT_EQ(net::ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle);
  error = callback.WaitForResult();
  EXPECT_EQ(net::OK, error);

  // The profile becomes tainted after using the trust anchors that came from
  // the policy configuration.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kUsedPolicyCertificatesOnce));
}

TEST_F(PolicyCertVerifierTest, ProfileRemainsTainted) {
  // |ca_cert| is the issuer of |cert|.
  scoped_refptr<net::X509Certificate> ca_cert =
      LoadCertificate("root_ca_cert.crt", net::CA_CERT);
  ASSERT_TRUE(ca_cert);
  scoped_refptr<net::X509Certificate> cert =
      LoadCertificate("ok_cert.pem", net::SERVER_CERT);
  ASSERT_TRUE(cert);

  net::CertificateList additional_trust_anchors;
  additional_trust_anchors.push_back(ca_cert);

  // |cert| is untrusted, so Verify() fails.
  net::CertVerifyResult verify_result;
  net::TestCompletionCallback callback;
  net::CertVerifier::RequestHandle request_handle;
  EXPECT_CALL(trust_provider_, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(empty_cert_list_));
  int error = cert_verifier_->Verify(cert, "127.0.0.1", 0, NULL,
                                     &verify_result, callback.callback(),
                                     &request_handle, net::BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider_);
  ASSERT_EQ(net::ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle);
  error = callback.WaitForResult();
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);

  // The profile is not tainted.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kUsedPolicyCertificatesOnce));

  // Verify() again with the additional trust anchors.
  EXPECT_CALL(trust_provider_, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(additional_trust_anchors));
  error = cert_verifier_->Verify(cert, "127.0.0.1", 0, NULL,
                                 &verify_result, callback.callback(),
                                 &request_handle, net::BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider_);
  ASSERT_EQ(net::ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle);
  error = callback.WaitForResult();
  EXPECT_EQ(net::OK, error);

  // The profile becomes tainted after using the trust anchors that came from
  // the policy configuration.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kUsedPolicyCertificatesOnce));

  // Verifying after removing the trust anchors should now fail.
  EXPECT_CALL(trust_provider_, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(empty_cert_list_));
  error = cert_verifier_->Verify(cert, "127.0.0.1", 0, NULL,
                                 &verify_result, callback.callback(),
                                 &request_handle, net::BoundNetLog());
  Mock::VerifyAndClearExpectations(&trust_provider_);
  // Note: this hits the cached result from the first Verify() in this test.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);

  // The profile is still tainted.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kUsedPolicyCertificatesOnce));
}

}  // namespace policy
