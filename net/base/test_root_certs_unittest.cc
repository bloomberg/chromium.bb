// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "build/build_config.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_test_util.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/test_root_certs.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// The local test root certificate.
const char kRootCertificateFile[] = "root_ca_cert.pem";
// A certificate issued by the local test root for 127.0.0.1.
const char kGoodCertificateFile[] = "ok_cert.pem";

}  // namespace

// Test basic functionality when adding from an existing X509Certificate.
TEST(TestRootCertsTest, AddFromPointer) {
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kRootCertificateFile);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), root_cert);

  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(NULL), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  EXPECT_TRUE(test_roots->Add(root_cert));
  EXPECT_FALSE(test_roots->IsEmpty());

  test_roots->Clear();
  EXPECT_TRUE(test_roots->IsEmpty());
}

// Test basic functionality when adding directly from a file, which should
// behave the same as when adding from an existing certificate.
TEST(TestRootCertsTest, AddFromFile) {
  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(NULL), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  FilePath cert_path =
      GetTestCertsDirectory().AppendASCII(kRootCertificateFile);
  EXPECT_TRUE(test_roots->AddFromFile(cert_path));
  EXPECT_FALSE(test_roots->IsEmpty());

  test_roots->Clear();
  EXPECT_TRUE(test_roots->IsEmpty());
}

// Test that TestRootCerts actually adds the appropriate trust status flags
// when requested, and that the trusted status is cleared once the root is
// removed the TestRootCerts. This test acts as a canary/sanity check for
// the results of the rest of net_unittests, ensuring that the trust status
// is properly being set and cleared.
TEST(TestRootCertsTest, OverrideTrust) {
  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(NULL), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  scoped_refptr<X509Certificate> test_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kGoodCertificateFile);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), test_cert);

  // Test that the good certificate fails verification, because the root
  // certificate should not yet be trusted.
  int flags = 0;
  CertVerifyResult bad_verify_result;
  int bad_status = test_cert->Verify("127.0.0.1", flags, &bad_verify_result);
  EXPECT_NE(OK, bad_status);
  EXPECT_NE(0, bad_verify_result.cert_status &
      CERT_STATUS_AUTHORITY_INVALID);

  // Add the root certificate and mark it as trusted.
  EXPECT_TRUE(test_roots->AddFromFile(
      GetTestCertsDirectory().AppendASCII(kRootCertificateFile)));
  EXPECT_FALSE(test_roots->IsEmpty());

  // Test that the certificate verification now succeeds, because the
  // TestRootCerts is successfully imbuing trust.
  CertVerifyResult good_verify_result;
  int good_status = test_cert->Verify("127.0.0.1", flags, &good_verify_result);
  EXPECT_EQ(OK, good_status);
  EXPECT_EQ(0, good_verify_result.cert_status);

  test_roots->Clear();
  EXPECT_TRUE(test_roots->IsEmpty());

  // Ensure that when the TestRootCerts is cleared, the trust settings
  // revert to their original state, and don't linger. If trust status
  // lingers, it will likely break other tests in net_unittests.
  CertVerifyResult restored_verify_result;
  int restored_status = test_cert->Verify("127.0.0.1", flags,
                                          &restored_verify_result);
  EXPECT_NE(OK, restored_status);
  EXPECT_NE(0, restored_verify_result.cert_status &
      CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_EQ(bad_status, restored_status);
  EXPECT_EQ(bad_verify_result.cert_status, restored_verify_result.cert_status);
}

// TODO(rsleevi): Add tests for revocation checking via CRLs, ensuring that
// TestRootCerts properly injects itself into the validation process. See
// http://crbug.com/63958

}  // namespace net
