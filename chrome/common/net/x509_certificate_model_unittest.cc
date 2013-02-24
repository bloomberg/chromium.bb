// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "net/base/cert_test_util.h"
#include "net/base/nss_cert_database.h"
#include "net/base/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(X509CertificateModelTest, GetTypeCA) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "root_ca_cert.crt"));
  ASSERT_TRUE(cert.get());

#if defined(USE_OPENSSL)
  // Remove this when OpenSSL build implements the necessary functions.
  EXPECT_EQ(net::UNKNOWN_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#else
  EXPECT_EQ(net::CA_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  // Test that explicitly distrusted CA certs are still returned as CA_CERT
  // type. See http://crbug.com/96654.
  EXPECT_TRUE(net::NSSCertDatabase::GetInstance()->SetCertTrust(
      cert, net::CA_CERT, net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::CA_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#endif
}

TEST(X509CertificateModelTest, GetTypeServer) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "google.single.der"));
  ASSERT_TRUE(cert.get());

#if defined(USE_OPENSSL)
  // Remove this when OpenSSL build implements the necessary functions.
  EXPECT_EQ(net::UNKNOWN_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#else
  // Test mozilla_security_manager::GetCertType with server certs and default
  // trust.  Currently this doesn't work.
  // TODO(mattm): make mozilla_security_manager::GetCertType smarter so we can
  // tell server certs even if they have no trust bits set.
  EXPECT_EQ(net::UNKNOWN_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  net::NSSCertDatabase* cert_db = net::NSSCertDatabase::GetInstance();
  // Test GetCertType with server certs and explicit trust.
  EXPECT_TRUE(cert_db->SetCertTrust(
      cert, net::SERVER_CERT, net::NSSCertDatabase::TRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  // Test GetCertType with server certs and explicit distrust.
  EXPECT_TRUE(cert_db->SetCertTrust(
      cert, net::SERVER_CERT, net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#endif
}

// An X.509 v1 certificate with the version field omitted should get
// the default value v1.
TEST(X509CertificateModelTest, GetVersionOmitted) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "ndn.ca.crt"));
  ASSERT_TRUE(cert.get());

  EXPECT_EQ("1", x509_certificate_model::GetVersion(cert->os_cert_handle()));
}
