// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/origin_bound_cert_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/ec_private_key.h"
#include "crypto/rsa_private_key.h"
#include "net/base/asn1_util.h"
#include "net/base/default_origin_bound_cert_store.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void FailTest(int /* result */) {
  FAIL();
}

// See http://crbug.com/91512 - implement OpenSSL version of CreateSelfSigned.
#if !defined(USE_OPENSSL)

TEST(OriginBoundCertServiceTest, CacheHit) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");

  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_RSA_SIGN);
  TestCompletionCallback callback;
  OriginBoundCertService::RequestHandle request_handle;

  // Asynchronous completion.
  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  EXPECT_EQ(0, service->cert_count());
  error = service->GetOriginBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, type1);
  EXPECT_FALSE(private_key_info1.empty());
  EXPECT_FALSE(der_cert1.empty());

  // Synchronous completion.
  SSLClientCertType type2;
  // If we request EC and RSA, should still retrieve the RSA cert.
  types.insert(types.begin(), CLIENT_CERT_ECDSA_SIGN);
  std::string private_key_info2, der_cert2;
  error = service->GetOriginBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_TRUE(request_handle == NULL);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, type2);
  EXPECT_EQ(private_key_info1, private_key_info2);
  EXPECT_EQ(der_cert1, der_cert2);

  // Request only EC.  Should generate a new EC cert and discard the old RSA
  // cert.
  SSLClientCertType type3;
  types.pop_back();  // Remove CLIENT_CERT_RSA_SIGN from requested types.
  std::string private_key_info3, der_cert3;
  EXPECT_EQ(1, service->cert_count());
  error = service->GetOriginBoundCert(
      origin, types, &type3, &private_key_info3, &der_cert3,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type3);
  EXPECT_FALSE(private_key_info1.empty());
  EXPECT_FALSE(der_cert1.empty());
  EXPECT_NE(private_key_info1, private_key_info3);
  EXPECT_NE(der_cert1, der_cert3);

  // Synchronous completion.
  // If we request RSA and EC, should now retrieve the EC cert.
  SSLClientCertType type4;
  types.insert(types.begin(), CLIENT_CERT_RSA_SIGN);
  std::string private_key_info4, der_cert4;
  error = service->GetOriginBoundCert(
      origin, types, &type4, &private_key_info4, &der_cert4,
      callback.callback(), &request_handle);
  EXPECT_TRUE(request_handle == NULL);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type4);
  EXPECT_EQ(private_key_info3, private_key_info4);
  EXPECT_EQ(der_cert3, der_cert4);

  EXPECT_EQ(4u, service->requests());
  EXPECT_EQ(2u, service->cert_store_hits());
  EXPECT_EQ(0u, service->inflight_joins());
}

TEST(OriginBoundCertServiceTest, UnsupportedTypes) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");

  int error;
  std::vector<uint8> types;
  TestCompletionCallback callback;
  OriginBoundCertService::RequestHandle request_handle;

  // Empty requested_types.
  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  error = service->GetOriginBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_INVALID_ARGUMENT, error);
  EXPECT_TRUE(request_handle == NULL);

  // No supported types in requested_types.
  types.push_back(2);
  types.push_back(3);
  error = service->GetOriginBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_CLIENT_AUTH_CERT_TYPE_UNSUPPORTED, error);
  EXPECT_TRUE(request_handle == NULL);

  // Supported types after unsupported ones in requested_types.
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  types.push_back(CLIENT_CERT_RSA_SIGN);
  // Asynchronous completion.
  EXPECT_EQ(0, service->cert_count());
  error = service->GetOriginBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type1);
  EXPECT_FALSE(private_key_info1.empty());
  EXPECT_FALSE(der_cert1.empty());

  // Now that the cert is created, doing requests for unsupported types
  // shouldn't affect the created cert.
  // Empty requested_types.
  types.clear();
  SSLClientCertType type2;
  std::string private_key_info2, der_cert2;
  error = service->GetOriginBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_INVALID_ARGUMENT, error);
  EXPECT_TRUE(request_handle == NULL);

  // No supported types in requested_types.
  types.push_back(2);
  types.push_back(3);
  error = service->GetOriginBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_CLIENT_AUTH_CERT_TYPE_UNSUPPORTED, error);
  EXPECT_TRUE(request_handle == NULL);

  // If we request EC, the cert we created before should still be there.
  types.push_back(CLIENT_CERT_RSA_SIGN);
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  error = service->GetOriginBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_TRUE(request_handle == NULL);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type2);
  EXPECT_EQ(private_key_info1, private_key_info2);
  EXPECT_EQ(der_cert1, der_cert2);
}

TEST(OriginBoundCertServiceTest, StoreCerts) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_RSA_SIGN);
  TestCompletionCallback callback;
  OriginBoundCertService::RequestHandle request_handle;

  std::string origin1("https://encrypted.google.com:443");
  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  EXPECT_EQ(0, service->cert_count());
  error = service->GetOriginBoundCert(
      origin1, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());

  std::string origin2("https://www.verisign.com:443");
  SSLClientCertType type2;
  std::string private_key_info2, der_cert2;
  error = service->GetOriginBoundCert(
      origin2, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(2, service->cert_count());

  std::string origin3("https://www.twitter.com:443");
  SSLClientCertType type3;
  std::string private_key_info3, der_cert3;
  types[0] = CLIENT_CERT_ECDSA_SIGN;
  error = service->GetOriginBoundCert(
      origin3, types, &type3, &private_key_info3, &der_cert3,
      callback.callback(), &request_handle);
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
  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, type1);
  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, type2);
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type3);
}

// Tests an inflight join.
TEST(OriginBoundCertServiceTest, InflightJoin) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_RSA_SIGN);

  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  TestCompletionCallback callback1;
  OriginBoundCertService::RequestHandle request_handle1;

  SSLClientCertType type2;
  std::string private_key_info2, der_cert2;
  TestCompletionCallback callback2;
  OriginBoundCertService::RequestHandle request_handle2;

  error = service->GetOriginBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback1.callback(), &request_handle1);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle1 != NULL);
  // If we request EC and RSA in the 2nd request, should still join with the
  // original request.
  types.insert(types.begin(), CLIENT_CERT_ECDSA_SIGN);
  error = service->GetOriginBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback2.callback(), &request_handle2);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle2 != NULL);

  error = callback1.WaitForResult();
  EXPECT_EQ(OK, error);
  error = callback2.WaitForResult();
  EXPECT_EQ(OK, error);

  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, type1);
  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, type2);
  EXPECT_EQ(2u, service->requests());
  EXPECT_EQ(0u, service->cert_store_hits());
  EXPECT_EQ(1u, service->inflight_joins());
}

// Tests an inflight join with mismatching request types.
TEST(OriginBoundCertServiceTest, InflightJoinTypeMismatch) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  int error;
  std::vector<uint8> types1;
  types1.push_back(CLIENT_CERT_RSA_SIGN);
  std::vector<uint8> types2;
  types2.push_back(CLIENT_CERT_ECDSA_SIGN);

  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  TestCompletionCallback callback1;
  OriginBoundCertService::RequestHandle request_handle1;

  SSLClientCertType type2;
  std::string private_key_info2, der_cert2;
  TestCompletionCallback callback2;
  OriginBoundCertService::RequestHandle request_handle2;

  error = service->GetOriginBoundCert(
      origin, types1, &type1, &private_key_info1, &der_cert1,
      callback1.callback(), &request_handle1);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle1 != NULL);
  // If we request only EC in the 2nd request, it should return an error.
  error = service->GetOriginBoundCert(
      origin, types2, &type2, &private_key_info2, &der_cert2,
      callback2.callback(), &request_handle2);
  EXPECT_EQ(ERR_ORIGIN_BOUND_CERT_GENERATION_TYPE_MISMATCH, error);
  EXPECT_TRUE(request_handle2 == NULL);

  error = callback1.WaitForResult();
  EXPECT_EQ(OK, error);

  EXPECT_FALSE(private_key_info1.empty());
  EXPECT_FALSE(der_cert1.empty());
  EXPECT_TRUE(private_key_info2.empty());
  EXPECT_TRUE(der_cert2.empty());
  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, type1);
  EXPECT_EQ(2u, service->requests());
  EXPECT_EQ(0u, service->cert_store_hits());
  EXPECT_EQ(0u, service->inflight_joins());
}

TEST(OriginBoundCertServiceTest, ExtractValuesFromBytesRSA) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  SSLClientCertType type;
  std::string private_key_info, der_cert;
  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_RSA_SIGN);
  TestCompletionCallback callback;
  OriginBoundCertService::RequestHandle request_handle;

  error = service->GetOriginBoundCert(
      origin, types, &type, &private_key_info, &der_cert, callback.callback(),
      &request_handle);
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

TEST(OriginBoundCertServiceTest, ExtractValuesFromBytesEC) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  SSLClientCertType type;
  std::string private_key_info, der_cert;
  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  TestCompletionCallback callback;
  OriginBoundCertService::RequestHandle request_handle;

  error = service->GetOriginBoundCert(
      origin, types, &type, &private_key_info, &der_cert, callback.callback(),
      &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);

  base::StringPiece spki_piece;
  ASSERT_TRUE(asn1::ExtractSPKIFromDERCert(der_cert, &spki_piece));
  std::vector<uint8> spki(
      spki_piece.data(),
      spki_piece.data() + spki_piece.size());

  // Check that we can retrieve the key from the bytes.
  std::vector<uint8> key_vec(private_key_info.begin(), private_key_info.end());
  scoped_ptr<crypto::ECPrivateKey> private_key(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          OriginBoundCertService::kEPKIPassword, key_vec, spki));
  EXPECT_TRUE(private_key != NULL);

  // Check that we can retrieve the cert from the bytes.
  scoped_refptr<X509Certificate> x509cert(
      X509Certificate::CreateFromBytes(der_cert.data(), der_cert.size()));
  EXPECT_TRUE(x509cert != NULL);
}

// Tests that the callback of a canceled request is never made.
TEST(OriginBoundCertServiceTest, CancelRequest) {
  scoped_ptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  SSLClientCertType type;
  std::string private_key_info, der_cert;
  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_RSA_SIGN);
  OriginBoundCertService::RequestHandle request_handle;

  error = service->GetOriginBoundCert(origin,
                                      types,
                                      &type,
                                      &private_key_info,
                                      &der_cert,
                                      base::Bind(&FailTest),
                                      &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  service->CancelRequest(request_handle);

  // Issue a few more requests to the worker pool and wait for their
  // completion, so that the task of the canceled request (which runs on a
  // worker thread) is likely to complete by the end of this test.
  TestCompletionCallback callback;
  for (int i = 0; i < 5; ++i) {
    error = service->GetOriginBoundCert(
        "https://encrypted.google.com:" + std::string(1, (char) ('1' + i)),
        types,
        &type,
        &private_key_info,
        &der_cert,
        callback.callback(),
        &request_handle);
    EXPECT_EQ(ERR_IO_PENDING, error);
    EXPECT_TRUE(request_handle != NULL);
    error = callback.WaitForResult();
  }

  // Even though the original request was cancelled, the service will still
  // store the result, it just doesn't call the callback.
  EXPECT_EQ(6, service->cert_count());
}

#endif  // !defined(USE_OPENSSL)

}  // namespace

}  // namespace net
