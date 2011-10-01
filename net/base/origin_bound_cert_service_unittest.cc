// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/origin_bound_cert_service.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "crypto/rsa_private_key.h"
#include "net/base/default_origin_bound_cert_store.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class OriginBoundCertServiceTest : public testing::Test {
};

class ExplodingCallback : public CallbackRunner<Tuple1<int> > {
 public:
  virtual void RunWithParams(const Tuple1<int>& params) {
    FAIL();
  }
};

// See http://crbug.com/91512 - implement OpenSSL version of CreateSelfSigned.
#if !defined(USE_OPENSSL)

TEST_F(OriginBoundCertServiceTest, CacheHit) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");

  int error;
  TestOldCompletionCallback callback;
  OriginBoundCertService::RequestHandle request_handle;

  // Asynchronous completion.
  std::string private_key_info1, der_cert1;
  EXPECT_EQ(0, service->cert_count());
  error = service->GetOriginBoundCert(
      origin, &private_key_info1, &der_cert1, &callback, &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_FALSE(private_key_info1.empty());
  EXPECT_FALSE(der_cert1.empty());

  // Synchronous completion.
  std::string private_key_info2, der_cert2;
  error = service->GetOriginBoundCert(
      origin, &private_key_info2, &der_cert2, &callback, &request_handle);
  EXPECT_TRUE(request_handle == NULL);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());

  EXPECT_EQ(private_key_info1, private_key_info2);
  EXPECT_EQ(der_cert1, der_cert2);

  EXPECT_EQ(2u, service->requests());
  EXPECT_EQ(1u, service->cert_store_hits());
  EXPECT_EQ(0u, service->inflight_joins());
}

TEST_F(OriginBoundCertServiceTest, StoreCerts) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  int error;
  TestOldCompletionCallback callback;
  OriginBoundCertService::RequestHandle request_handle;

  std::string origin1("https://encrypted.google.com:443");
  std::string private_key_info1, der_cert1;
  EXPECT_EQ(0, service->cert_count());
  error = service->GetOriginBoundCert(
      origin1, &private_key_info1, &der_cert1, &callback, &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());

  std::string origin2("https://www.verisign.com:443");
  std::string private_key_info2, der_cert2;
  error = service->GetOriginBoundCert(
      origin2, &private_key_info2, &der_cert2, &callback, &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(2, service->cert_count());

  std::string origin3("https://www.twitter.com:443");
  std::string private_key_info3, der_cert3;
  error = service->GetOriginBoundCert(
      origin3, &private_key_info3, &der_cert3, &callback, &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(3, service->cert_count());

  EXPECT_NE(private_key_info1, private_key_info2);
  EXPECT_NE(der_cert1, der_cert2);
  EXPECT_NE(private_key_info1, private_key_info3);
  EXPECT_NE(der_cert1, der_cert3);
  EXPECT_NE(private_key_info2, private_key_info3);
  EXPECT_NE(der_cert2, der_cert3);
}

// Tests an inflight join.
TEST_F(OriginBoundCertServiceTest, InflightJoin) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  int error;

  std::string private_key_info1, der_cert1;
  TestOldCompletionCallback callback1;
  OriginBoundCertService::RequestHandle request_handle1;

  std::string private_key_info2, der_cert2;
  TestOldCompletionCallback callback2;
  OriginBoundCertService::RequestHandle request_handle2;

  error = service->GetOriginBoundCert(
      origin, &private_key_info1, &der_cert1, &callback1, &request_handle1);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle1 != NULL);
  error = service->GetOriginBoundCert(
      origin, &private_key_info2, &der_cert2, &callback2, &request_handle2);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle2 != NULL);

  error = callback1.WaitForResult();
  EXPECT_EQ(OK, error);
  error = callback2.WaitForResult();
  EXPECT_EQ(OK, error);

  EXPECT_EQ(2u, service->requests());
  EXPECT_EQ(0u, service->cert_store_hits());
  EXPECT_EQ(1u, service->inflight_joins());
}

TEST_F(OriginBoundCertServiceTest, ExtractValuesFromBytes) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  std::string private_key_info, der_cert;
  int error;
  TestOldCompletionCallback callback;
  OriginBoundCertService::RequestHandle request_handle;

  error = service->GetOriginBoundCert(
      origin, &private_key_info, &der_cert, &callback, &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);

  // Check that we can retrieve the key from the bytes.
  std::vector<uint8> key_vec(private_key_info.begin(), private_key_info.end());
  scoped_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vec));
  EXPECT_TRUE(private_key != NULL);

  // Check that we can retrieve the cert from the bytes.
  scoped_refptr<X509Certificate> x509cert(
      X509Certificate::CreateFromBytes(der_cert.data(), der_cert.size()));
  EXPECT_TRUE(x509cert != NULL);
}

// Tests that the callback of a canceled request is never made.
TEST_F(OriginBoundCertServiceTest, CancelRequest) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  std::string private_key_info, der_cert;
  int error;
  ExplodingCallback exploding_callback;
  OriginBoundCertService::RequestHandle request_handle;

  error = service->GetOriginBoundCert(origin,
                                      &private_key_info,
                                      &der_cert,
                                      &exploding_callback,
                                      &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  service->CancelRequest(request_handle);

  // Issue a few more requests to the worker pool and wait for their
  // completion, so that the task of the canceled request (which runs on a
  // worker thread) is likely to complete by the end of this test.
  TestOldCompletionCallback callback;
  for (int i = 0; i < 5; ++i) {
    error = service->GetOriginBoundCert(
        "https://encrypted.google.com:" + std::string(1, (char) ('1' + i)),
        &private_key_info,
        &der_cert,
        &callback,
        &request_handle);
    EXPECT_EQ(ERR_IO_PENDING, error);
    EXPECT_TRUE(request_handle != NULL);
    error = callback.WaitForResult();
  }
}

#endif  // !defined(USE_OPENSSL)

}  // namespace net
