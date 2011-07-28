// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/origin_bound_cert_service.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "crypto/rsa_private_key.h"
#include "net/base/default_origin_bound_cert_store.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(OriginBoundCertServiceTest, DuplicateCertTest) {
  scoped_refptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin1("https://encrypted.google.com/");
  std::string origin2("https://www.verisign.com/");

  // The store should start out empty and should increment appropriately.
  std::string private_key_info_1a, der_cert_1a;
  EXPECT_EQ(0, service->GetCertCount());
  EXPECT_TRUE(service->GetOriginBoundCert(
      origin1, &private_key_info_1a, &der_cert_1a));
  EXPECT_EQ(1, service->GetCertCount());

  // We should get the same cert and key for the same origin.
  std::string private_key_info_1b, der_cert_1b;
  EXPECT_TRUE(service->GetOriginBoundCert(
      origin1, &private_key_info_1b, &der_cert_1b));
  EXPECT_EQ(1, service->GetCertCount());
  EXPECT_EQ(private_key_info_1a, private_key_info_1b);
  EXPECT_EQ(der_cert_1a, der_cert_1b);

  // We should get a different cert and key for different origins.
  std::string private_key_info_2, der_cert_2;
  EXPECT_TRUE(service->GetOriginBoundCert(
      origin2, &private_key_info_2, &der_cert_2));
  EXPECT_EQ(2, service->GetCertCount());
  EXPECT_NE(private_key_info_1a, private_key_info_2);
  EXPECT_NE(der_cert_1a, der_cert_2);
}

TEST(OriginBoundCertServiceTest, ExtractValuesFromBytes) {
  scoped_refptr<OriginBoundCertService> service(
      new OriginBoundCertService(new DefaultOriginBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com/");
  std::string private_key_info, der_cert;
  EXPECT_TRUE(service->GetOriginBoundCert(
      origin, &private_key_info, &der_cert));
  std::vector<uint8> key_vec(private_key_info.begin(), private_key_info.end());

  // Check that we can retrieve the key pair from the bytes.
  scoped_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vec));
  EXPECT_TRUE(private_key != NULL);

  // Check that we can retrieve the cert from the bytes.
  scoped_refptr<X509Certificate> x509cert(
      X509Certificate::CreateFromBytes(der_cert.data(), der_cert.size()));
  EXPECT_TRUE(x509cert != NULL);
}

}  // namespace net
