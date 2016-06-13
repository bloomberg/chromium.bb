// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/caching_cert_verifier.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_trust_anchor_provider.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log.h"
#include "net/test/cert_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::ReturnRef;

namespace net {

namespace {

class MockCertTrustAnchorProvider : public CertTrustAnchorProvider {
 public:
  MockCertTrustAnchorProvider() {}
  virtual ~MockCertTrustAnchorProvider() {}

  MOCK_METHOD0(GetAdditionalTrustAnchors, const CertificateList&());
};

}  // namespace

class CachingCertVerifierTest : public ::testing::Test {
 public:
  CachingCertVerifierTest() : verifier_(base::MakeUnique<MockCertVerifier>()) {}
  ~CachingCertVerifierTest() override {}

 protected:
  CachingCertVerifier verifier_;
};

TEST_F(CachingCertVerifierTest, CacheHit) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  error = callback.GetResult(verifier_.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  std::string(), CertificateList()),
      nullptr, &verify_result, callback.callback(), &request, BoundNetLog()));
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(1u, verifier_.GetCacheSize());

  error = verifier_.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  std::string(), CertificateList()),
      nullptr, &verify_result, callback.callback(), &request, BoundNetLog());
  // Synchronous completion.
  ASSERT_NE(ERR_IO_PENDING, error);
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_FALSE(request);
  ASSERT_EQ(2u, verifier_.requests());
  ASSERT_EQ(1u, verifier_.cache_hits());
  ASSERT_EQ(1u, verifier_.GetCacheSize());
}

// Tests the same server certificate with different intermediate CA
// certificates.  These should be treated as different certificate chains even
// though the two X509Certificate objects contain the same server certificate.
TEST_F(CachingCertVerifierTest, DifferentCACerts) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "salesforce_com_test.pem");
  ASSERT_TRUE(server_cert);

  scoped_refptr<X509Certificate> intermediate_cert1 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2011.pem");
  ASSERT_TRUE(intermediate_cert1);

  scoped_refptr<X509Certificate> intermediate_cert2 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2016.pem");
  ASSERT_TRUE(intermediate_cert2);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert1->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain1 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);
  ASSERT_TRUE(cert_chain1);

  intermediates.clear();
  intermediates.push_back(intermediate_cert2->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain2 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);
  ASSERT_TRUE(cert_chain2);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  error = callback.GetResult(verifier_.Verify(
      CertVerifier::RequestParams(cert_chain1, "www.example.com", 0,
                                  std::string(), CertificateList()),
      nullptr, &verify_result, callback.callback(), &request, BoundNetLog()));
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(1u, verifier_.GetCacheSize());

  error = callback.GetResult(verifier_.Verify(
      CertVerifier::RequestParams(cert_chain2, "www.example.com", 0,
                                  std::string(), CertificateList()),
      nullptr, &verify_result, callback.callback(), &request, BoundNetLog()));
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(2u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(2u, verifier_.GetCacheSize());
}

TEST_F(CachingCertVerifierTest, CertTrustAnchorProvider) {
  MockCertTrustAnchorProvider trust_provider;
  verifier_.SetCertTrustAnchorProvider(&trust_provider);

  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  const CertificateList empty_cert_list;
  CertificateList cert_list;
  cert_list.push_back(test_cert);

  // Check that Verify() asks the |trust_provider| for the current list of
  // additional trust anchors.
  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  EXPECT_CALL(trust_provider, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(empty_cert_list));
  error = callback.GetResult(verifier_.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  std::string(), CertificateList()),
      nullptr, &verify_result, callback.callback(), &request, BoundNetLog()));
  Mock::VerifyAndClearExpectations(&trust_provider);
  EXPECT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());

  // The next Verify() uses the cached result.
  EXPECT_CALL(trust_provider, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(empty_cert_list));
  error = callback.GetResult(verifier_.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  std::string(), CertificateList()),
      nullptr, &verify_result, callback.callback(), &request, BoundNetLog()));
  Mock::VerifyAndClearExpectations(&trust_provider);
  EXPECT_TRUE(IsCertificateError(error));
  ASSERT_EQ(2u, verifier_.requests());
  ASSERT_EQ(1u, verifier_.cache_hits());

  // Another Verify() for the same certificate but with a different list of
  // trust anchors will not reuse the cache.
  EXPECT_CALL(trust_provider, GetAdditionalTrustAnchors())
      .WillOnce(ReturnRef(cert_list));
  error = callback.GetResult(verifier_.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  std::string(), CertificateList()),
      nullptr, &verify_result, callback.callback(), &request, BoundNetLog()));
  Mock::VerifyAndClearExpectations(&trust_provider);
  EXPECT_TRUE(IsCertificateError(error));
  ASSERT_EQ(3u, verifier_.requests());
  ASSERT_EQ(1u, verifier_.cache_hits());
}

}  // namespace net
