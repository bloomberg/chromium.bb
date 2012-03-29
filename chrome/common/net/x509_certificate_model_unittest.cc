// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "net/base/cert_database.h"
#include "testing/gtest/include/gtest/gtest.h"

class X509CertificateModelTest : public testing::Test {
 protected:
  static std::string ReadTestFile(const std::string& name) {
    std::string result;
    FilePath cert_path = GetTestCertsDirectory().AppendASCII(name);
    EXPECT_TRUE(file_util::ReadFileToString(cert_path, &result));
    return result;
  }

 private:
  // Returns a FilePath object representing the src/net/data/ssl/certificates
  // directory in the source tree.
  static FilePath GetTestCertsDirectory() {
    FilePath certs_dir;
    PathService::Get(base::DIR_SOURCE_ROOT, &certs_dir);
    certs_dir = certs_dir.AppendASCII("net");
    certs_dir = certs_dir.AppendASCII("data");
    certs_dir = certs_dir.AppendASCII("ssl");
    certs_dir = certs_dir.AppendASCII("certificates");
    return certs_dir;
  }
};

TEST_F(X509CertificateModelTest, GetTypeCA) {
  std::string cert_data = ReadTestFile("root_ca_cert.crt");

  net::CertificateList certs =
      net::X509Certificate::CreateCertificateListFromBytes(
          cert_data.data(), cert_data.size(),
          net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());

#if defined(USE_OPENSSL)
  // Remove this when OpenSSL build implements the necessary functions.
  EXPECT_EQ(net::UNKNOWN_CERT,
            x509_certificate_model::GetType(certs[0]->os_cert_handle()));
#else
  EXPECT_EQ(net::CA_CERT,
            x509_certificate_model::GetType(certs[0]->os_cert_handle()));

  // Test that explicitly distrusted CA certs are still returned as CA_CERT
  // type. See http://crbug.com/96654.
  net::CertDatabase cert_db;
  // TODO(mattm): This depends on the implementation details of SetCertTrust
  // where calling with SERVER_CERT and UNTRUSTED causes a cert to be explicitly
  // distrusted (trust set to CERTDB_TERMINAL_RECORD). See
  // http://crbug.com/116411.  When I fix that bug I'll also add a way to set
  // this directly.
  EXPECT_TRUE(cert_db.SetCertTrust(certs[0], net::SERVER_CERT,
                                   net::CertDatabase::UNTRUSTED));

  EXPECT_EQ(net::CA_CERT,
            x509_certificate_model::GetType(certs[0]->os_cert_handle()));
#endif
}

TEST_F(X509CertificateModelTest, GetTypeServer) {
  std::string cert_data = ReadTestFile("google.single.der");

  net::CertificateList certs =
      net::X509Certificate::CreateCertificateListFromBytes(
          cert_data.data(), cert_data.size(),
          net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());

#if defined(USE_OPENSSL)
  // Remove this when OpenSSL build implements the necessary functions.
  EXPECT_EQ(net::UNKNOWN_CERT,
            x509_certificate_model::GetType(certs[0]->os_cert_handle()));
#else
  // TODO(mattm): make GetCertType smarter so we can tell server certs even if
  // they have no trust bits set.
  EXPECT_EQ(net::UNKNOWN_CERT,
            x509_certificate_model::GetType(certs[0]->os_cert_handle()));

  net::CertDatabase cert_db;
  EXPECT_TRUE(cert_db.SetCertTrust(certs[0], net::SERVER_CERT,
                                   net::CertDatabase::TRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT,
            x509_certificate_model::GetType(certs[0]->os_cert_handle()));

  EXPECT_TRUE(cert_db.SetCertTrust(certs[0], net::SERVER_CERT,
                                   net::CertDatabase::UNTRUSTED));

  EXPECT_EQ(net::SERVER_CERT,
            x509_certificate_model::GetType(certs[0]->os_cert_handle()));
#endif
}
