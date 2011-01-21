// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verifier.h"

#include "base/callback.h"
#include "base/file_path.h"
#include "base/stringprintf.h"
#include "net/base/cert_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class TestTimeService : public CertVerifier::TimeService {
 public:
  // CertVerifier::TimeService methods:
  virtual base::Time Now() { return current_time_; }

  void set_current_time(base::Time now) { current_time_ = now; }

 private:
  base::Time current_time_;
};

class CertVerifierTest : public testing::Test {
};

class ExplodingCallback : public CallbackRunner<Tuple1<int> > {
 public:
  virtual void RunWithParams(const Tuple1<int>& params) {
    FAIL();
  }
};

// Tests a cache hit, which should results in synchronous completion.
TEST_F(CertVerifierTest, CacheHit) {
  TestTimeService* time_service = new TestTimeService;
  base::Time current_time = base::Time::Now();
  time_service->set_current_time(current_time);
  CertVerifier verifier(time_service);

  FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> google_cert(
      ImportCertFromFile(certs_dir, "google.single.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;

  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result,
                          &callback, &request_handle);
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier.requests());
  ASSERT_EQ(0u, verifier.cache_hits());
  ASSERT_EQ(0u, verifier.inflight_joins());

  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result,
                          &callback, &request_handle);
  // Synchronous completion.
  ASSERT_NE(ERR_IO_PENDING, error);
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_TRUE(request_handle == NULL);
  ASSERT_EQ(2u, verifier.requests());
  ASSERT_EQ(1u, verifier.cache_hits());
  ASSERT_EQ(0u, verifier.inflight_joins());
}

// Tests an inflight join.
TEST_F(CertVerifierTest, InflightJoin) {
  TestTimeService* time_service = new TestTimeService;
  base::Time current_time = base::Time::Now();
  time_service->set_current_time(current_time);
  CertVerifier verifier(time_service);

  FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> google_cert(
      ImportCertFromFile(certs_dir, "google.single.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;
  CertVerifyResult verify_result2;
  TestCompletionCallback callback2;
  CertVerifier::RequestHandle request_handle2;

  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result,
                          &callback, &request_handle);
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle != NULL);
  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result2,
                          &callback2, &request_handle2);
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle2 != NULL);
  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  error = callback2.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(2u, verifier.requests());
  ASSERT_EQ(0u, verifier.cache_hits());
  ASSERT_EQ(1u, verifier.inflight_joins());
}

// Tests cache entry expiration.
TEST_F(CertVerifierTest, ExpiredCacheEntry) {
  TestTimeService* time_service = new TestTimeService;
  base::Time current_time = base::Time::Now();
  time_service->set_current_time(current_time);
  CertVerifier verifier(time_service);

  FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> google_cert(
      ImportCertFromFile(certs_dir, "google.single.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;

  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result,
                          &callback, &request_handle);
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier.requests());
  ASSERT_EQ(0u, verifier.cache_hits());
  ASSERT_EQ(0u, verifier.inflight_joins());

  // Before expiration, should have a cache hit.
  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result,
                          &callback, &request_handle);
  // Synchronous completion.
  ASSERT_NE(ERR_IO_PENDING, error);
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_TRUE(request_handle == NULL);
  ASSERT_EQ(2u, verifier.requests());
  ASSERT_EQ(1u, verifier.cache_hits());
  ASSERT_EQ(0u, verifier.inflight_joins());

  // After expiration, should not have a cache hit.
  ASSERT_EQ(1u, verifier.GetCacheSize());
  current_time += base::TimeDelta::FromMinutes(60);
  time_service->set_current_time(current_time);
  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result,
                          &callback, &request_handle);
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle != NULL);
  ASSERT_EQ(0u, verifier.GetCacheSize());
  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(3u, verifier.requests());
  ASSERT_EQ(1u, verifier.cache_hits());
  ASSERT_EQ(0u, verifier.inflight_joins());
}

// Tests a full cache.
TEST_F(CertVerifierTest, FullCache) {
  TestTimeService* time_service = new TestTimeService;
  base::Time current_time = base::Time::Now();
  time_service->set_current_time(current_time);
  CertVerifier verifier(time_service);

  FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> google_cert(
      ImportCertFromFile(certs_dir, "google.single.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;

  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result,
                          &callback, &request_handle);
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier.requests());
  ASSERT_EQ(0u, verifier.cache_hits());
  ASSERT_EQ(0u, verifier.inflight_joins());

  const unsigned kCacheSize = 256;

  for (unsigned i = 0; i < kCacheSize; i++) {
    std::string hostname = base::StringPrintf("www%d.example.com", i + 1);
    error = verifier.Verify(google_cert, hostname, 0, &verify_result,
                            &callback, &request_handle);
    ASSERT_EQ(ERR_IO_PENDING, error);
    ASSERT_TRUE(request_handle != NULL);
    error = callback.WaitForResult();
    ASSERT_TRUE(IsCertificateError(error));
  }
  ASSERT_EQ(kCacheSize + 1, verifier.requests());
  ASSERT_EQ(0u, verifier.cache_hits());
  ASSERT_EQ(0u, verifier.inflight_joins());

  ASSERT_EQ(kCacheSize, verifier.GetCacheSize());
  current_time += base::TimeDelta::FromMinutes(60);
  time_service->set_current_time(current_time);
  error = verifier.Verify(google_cert, "www999.example.com", 0, &verify_result,
                          &callback, &request_handle);
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle != NULL);
  ASSERT_EQ(kCacheSize, verifier.GetCacheSize());
  error = callback.WaitForResult();
  ASSERT_EQ(1u, verifier.GetCacheSize());
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(kCacheSize + 2, verifier.requests());
  ASSERT_EQ(0u, verifier.cache_hits());
  ASSERT_EQ(0u, verifier.inflight_joins());
}

// Tests that the callback of a canceled request is never made.
TEST_F(CertVerifierTest, CancelRequest) {
  CertVerifier verifier;

  FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> google_cert(
      ImportCertFromFile(certs_dir, "google.single.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  int error;
  CertVerifyResult verify_result;
  ExplodingCallback exploding_callback;
  CertVerifier::RequestHandle request_handle;

  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result,
                          &exploding_callback, &request_handle);
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle != NULL);
  verifier.CancelRequest(request_handle);

  // Issue a few more requests to the worker pool and wait for their
  // completion, so that the task of the canceled request (which runs on a
  // worker thread) is likely to complete by the end of this test.
  TestCompletionCallback callback;
  for (int i = 0; i < 5; ++i) {
    error = verifier.Verify(google_cert, "www2.example.com", 0, &verify_result,
                            &callback, &request_handle);
    ASSERT_EQ(ERR_IO_PENDING, error);
    ASSERT_TRUE(request_handle != NULL);
    error = callback.WaitForResult();
    verifier.ClearCache();
  }
}

// Tests that a canceled request is not leaked.
TEST_F(CertVerifierTest, CancelRequestThenQuit) {
  CertVerifier verifier;

  FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> google_cert(
      ImportCertFromFile(certs_dir, "google.single.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  CertVerifier::RequestHandle request_handle;

  error = verifier.Verify(google_cert, "www.example.com", 0, &verify_result,
                          &callback, &request_handle);
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request_handle != NULL);
  verifier.CancelRequest(request_handle);
  // Destroy |verifier| by going out of scope.
}

}  // namespace net
