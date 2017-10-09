// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/crw_session_certificate_policy_cache_storage.h"

#import "base/mac/scoped_nsobject.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Checks for equality between |cert_storage1| and |cert_storage2|.
bool CertStoragesAreEqual(CRWSessionCertificateStorage* cert_storage1,
                          CRWSessionCertificateStorage* cert_storage2) {
  std::string cert_string1;
  bool success1 = net::X509Certificate::GetDEREncoded(
      cert_storage1.certificate->os_cert_handle(), &cert_string1);
  std::string cert_string2;
  bool success2 = net::X509Certificate::GetDEREncoded(
      cert_storage2.certificate->os_cert_handle(), &cert_string2);
  return success1 && success2 && cert_string1 == cert_string2 &&
         cert_storage1.host == cert_storage2.host &&
         cert_storage1.status == cert_storage2.status;
}
// Checks for equality between |cache_storage1| and |cache_storage2|.
bool CacheStoragesAreEqual(
    CRWSessionCertificatePolicyCacheStorage* cache_storage1,
    CRWSessionCertificatePolicyCacheStorage* cache_storage2) {
  NSArray* certs1 = [cache_storage1.certificateStorages allObjects];
  NSArray* certs2 = [cache_storage2.certificateStorages allObjects];
  if (certs1.count != certs2.count)
    return false;
  for (NSUInteger i = 0; i < certs1.count; ++i) {
    if (!CertStoragesAreEqual(certs1[i], certs2[i]))
      return false;
  }
  return true;
}
}  // namespace

class CRWSessionCertificatePolicyCacheStorageTest : public PlatformTest {
 protected:
  CRWSessionCertificatePolicyCacheStorageTest()
      : cache_storage_([[CRWSessionCertificatePolicyCacheStorage alloc] init]) {
    // Set up |cache_storage_|.
    scoped_refptr<net::X509Certificate> cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    base::scoped_nsobject<NSMutableSet> certs([[NSMutableSet alloc] init]);
    [certs addObject:[[CRWSessionCertificateStorage alloc]
                         initWithCertificate:cert
                                        host:"test1.com"
                                      status:net::CERT_STATUS_REVOKED]];
    [cache_storage_ setCertificateStorages:certs];
  }

 protected:
  base::scoped_nsobject<CRWSessionCertificatePolicyCacheStorage> cache_storage_;
};

// Tests that unarchiving CRWSessionCertificatePolicyCacheStorage data results
// in an equivalent storage.
TEST_F(CRWSessionCertificatePolicyCacheStorageTest, EncodeDecode) {
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:cache_storage_];
  id decoded = [NSKeyedUnarchiver unarchiveObjectWithData:data];
  EXPECT_TRUE(CacheStoragesAreEqual(cache_storage_.get(), decoded));
}

using CRWSessionCertificateStorageTest = PlatformTest;

// Tests that unarchiving a CRWSessionCertificateStorage returns nil if the
// certificate data does not correctly decode to a certificate.
TEST_F(CRWSessionCertificateStorageTest, InvalidCertData) {
  NSMutableData* data = [[NSMutableData alloc] init];
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initForWritingWithMutableData:data];
  [archiver encodeObject:[@"not a  cert" dataUsingEncoding:NSUTF8StringEncoding]
                  forKey:web::kCertificateSerializationKey];
  [archiver encodeObject:@"host" forKey:web::kHostSerializationKey];
  [archiver encodeObject:@(net::CERT_STATUS_INVALID)
                  forKey:web::kStatusSerializationKey];
  [archiver finishEncoding];
  id decoded = [NSKeyedUnarchiver unarchiveObjectWithData:data];
  EXPECT_FALSE(decoded);
}
