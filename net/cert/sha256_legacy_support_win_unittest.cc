// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/sha256_legacy_support_win.h"

#include "base/memory/ref_counted.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace sha256_interception {

namespace {

// Verifies that SHA-256 signatures can be validated through the interception.
// Although this is only needed on legacy platforms, the test is run on all
// Windows platforms to make sure that the CryptoAPI<->NSS integration does not
// regress.
TEST(Sha256Interception, HandlesSHA2) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "sha256.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert.get());

  CryptVerifyCertificateSignatureExFunc cert_verify_signature_ptr =
      reinterpret_cast<CryptVerifyCertificateSignatureExFunc>(
              ::GetProcAddress(::GetModuleHandle(L"crypt32.dll"),
                               "CryptVerifyCertificateSignatureEx"));
  ASSERT_TRUE(cert_verify_signature_ptr);

  BOOL rv = CryptVerifyCertificateSignatureExHook(
      cert_verify_signature_ptr, NULL, X509_ASN_ENCODING,
      CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT,
      const_cast<void*>(reinterpret_cast<const void*>(
          server_cert->os_cert_handle())),
      CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT,
      const_cast<void*>(reinterpret_cast<const void*>(
          server_cert->os_cert_handle())),
      0, NULL);
  EXPECT_EQ(TRUE, rv);
}

}  // namespace

}  // namespace sha256_interception

}  // namespace net