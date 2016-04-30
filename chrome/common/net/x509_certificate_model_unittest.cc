// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "net/base/test_data_directory.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS_CERTS)
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#endif

TEST(X509CertificateModelTest, GetCertNameOrNicknameAndGetTitle) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());
  EXPECT_EQ(
      "Test Root CA",
      x509_certificate_model::GetCertNameOrNickname(cert->os_cert_handle()));

  scoped_refptr<net::X509Certificate> punycode_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "punycodetest.pem"));
  ASSERT_TRUE(punycode_cert.get());
  EXPECT_EQ("xn--wgv71a119e.com (日本語.com)",
            x509_certificate_model::GetCertNameOrNickname(
                punycode_cert->os_cert_handle()));

  scoped_refptr<net::X509Certificate> no_cn_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "no_subject_common_name_cert.pem"));
  ASSERT_TRUE(no_cn_cert.get());
  // Temp cert has no nickname.
  EXPECT_EQ("",
            x509_certificate_model::GetCertNameOrNickname(
                no_cn_cert->os_cert_handle()));

  EXPECT_EQ("xn--wgv71a119e.com",
            x509_certificate_model::GetTitle(
                punycode_cert->os_cert_handle()));

  EXPECT_EQ("E=wtc@google.com",
            x509_certificate_model::GetTitle(
                no_cn_cert->os_cert_handle()));

  scoped_refptr<net::X509Certificate> no_cn_cert2(net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "ct-test-embedded-cert.pem"));
  ASSERT_TRUE(no_cn_cert2.get());
  EXPECT_EQ("L=Erw Wen,ST=Wales,O=Certificate Transparency,C=GB",
            x509_certificate_model::GetTitle(no_cn_cert2->os_cert_handle()));
}

TEST(X509CertificateModelTest, GetExtensions) {
  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "root_ca_cert.pem"));
    ASSERT_TRUE(cert.get());

    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(3U, extensions.size());

    EXPECT_EQ("Certificate Basic Constraints", extensions[0].name);
    EXPECT_EQ(
        "critical\nIs a Certification Authority\n"
        "Maximum number of intermediate CAs: unlimited",
        extensions[0].value);

    EXPECT_EQ("Certificate Subject Key ID", extensions[1].name);
    EXPECT_EQ(
        "notcrit\nKey ID: BC F7 30 D1 3C C0 F2 79 FA EF 9F C9 6C 5C 93 F3\n8A "
        "68 AB 83",
        extensions[1].value);

    EXPECT_EQ("Certificate Key Usage", extensions[2].name);
    EXPECT_EQ("critical\nCertificate Signer\nCRL Signer", extensions[2].value);
  }

  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "subjectAltName_sanity_check.pem"));
    ASSERT_TRUE(cert.get());

    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(2U, extensions.size());
    EXPECT_EQ("Certificate Subject Alternative Name", extensions[1].name);
    EXPECT_EQ(
        "notcrit\nIP Address: 127.0.0.2\nIP Address: fe80::1\nDNS Name: "
        "test.example\nEmail Address: test@test.example\nOID.1.2.3.4: 0C 09 69 "
        "67 6E 6F 72 65 20 6D 65\nX.500 Name: CN = 127.0.0.3\n\n",
        extensions[1].value);
  }

  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "foaf.me.chromium-test-cert.der"));
    ASSERT_TRUE(cert.get());

    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(5U, extensions.size());
    EXPECT_EQ("Netscape Certificate Comment", extensions[1].name);
    EXPECT_EQ("notcrit\nOpenSSL Generated Certificate", extensions[1].value);
  }

  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "2029_globalsign_com_cert.pem"));
    ASSERT_TRUE(cert.get());

    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(9U, extensions.size());

    EXPECT_EQ("Certificate Subject Key ID", extensions[0].name);
    EXPECT_EQ(
        "notcrit\nKey ID: 59 BC D9 69 F7 B0 65 BB C8 34 C5 D2 C2 EF 17 78\nA6 "
        "47 1E 8B",
        extensions[0].value);

    EXPECT_EQ("Certification Authority Key ID", extensions[1].name);
    EXPECT_EQ(
        "notcrit\nKey ID: 8A FC 14 1B 3D A3 59 67 A5 3B E1 73 92 A6 62 91\n7F "
        "E4 78 30\n",
        extensions[1].value);

    EXPECT_EQ("Authority Information Access", extensions[2].name);
    EXPECT_EQ(
        "notcrit\nCA Issuers: "
        "URI: http://secure.globalsign.net/cacert/SHA256extendval1.crt\n",
        extensions[2].value);

    EXPECT_EQ("CRL Distribution Points", extensions[3].name);
    EXPECT_EQ("notcrit\nURI: http://crl.globalsign.net/SHA256ExtendVal1.crl\n",
              extensions[3].value);

    EXPECT_EQ("Certificate Basic Constraints", extensions[4].name);
    EXPECT_EQ("notcrit\nIs not a Certification Authority\n",
              extensions[4].value);

    EXPECT_EQ("Certificate Key Usage", extensions[5].name);
    EXPECT_EQ(
        "critical\nSigning\nNon-repudiation\nKey Encipherment\n"
        "Data Encipherment",
        extensions[5].value);

    EXPECT_EQ("Extended Key Usage", extensions[6].name);
    EXPECT_EQ(
        "notcrit\nTLS WWW Server Authentication (OID.1.3.6.1.5.5.7.3.1)\n"
        "TLS WWW Client Authentication (OID.1.3.6.1.5.5.7.3.2)\n",
        extensions[6].value);

    EXPECT_EQ("Certificate Policies", extensions[7].name);
    EXPECT_EQ(
        "notcrit\nOID.1.3.6.1.4.1.4146.1.1:\n"
        "  Certification Practice Statement Pointer:"
        "    http://www.globalsign.net/repository/\n",
        extensions[7].value);

    EXPECT_EQ("Netscape Certificate Type", extensions[8].name);
    EXPECT_EQ("notcrit\nSSL Client Certificate\nSSL Server Certificate",
              extensions[8].value);
  }

  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "diginotar_public_ca_2025.pem"));
    ASSERT_TRUE(cert.get());

    x509_certificate_model::Extensions extensions;
    x509_certificate_model::GetExtensions(
        "critical", "notcrit", cert->os_cert_handle(), &extensions);
    ASSERT_EQ(7U, extensions.size());

    EXPECT_EQ("Authority Information Access", extensions[0].name);
    EXPECT_EQ(
        "notcrit\nOCSP Responder: "
        "URI: http://validation.diginotar.nl\n",
        extensions[0].value);

    EXPECT_EQ("Certificate Basic Constraints", extensions[2].name);
    EXPECT_EQ(
        "critical\nIs a Certification Authority\n"
        "Maximum number of intermediate CAs: 0",
        extensions[2].value);
    EXPECT_EQ("Certificate Policies", extensions[3].name);
    EXPECT_EQ(
        "notcrit\nOID.2.16.528.1.1001.1.1.1.1.5.2.6.4:\n"
        "  Certification Practice Statement Pointer:"
        "    http://www.diginotar.nl/cps\n"
        "  User Notice:\n"
        "    Conditions, as mentioned on our website (www.diginotar.nl), are "
        "applicable to all our products and services.\n",
        extensions[3].value);
  }
}

TEST(X509CertificateModelTest, GetTypeCA) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());

  EXPECT_EQ(net::CA_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  crypto::ScopedTestNSSDB test_nssdb;
  net::NSSCertDatabase db(crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* public slot */,
                          crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* private slot */);

  // Test that explicitly distrusted CA certs are still returned as CA_CERT
  // type. See http://crbug.com/96654.
  EXPECT_TRUE(db.SetCertTrust(
      cert.get(), net::CA_CERT, net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::CA_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
}

TEST(X509CertificateModelTest, GetTypeServer) {
  scoped_refptr<net::X509Certificate> cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "google.single.der"));
  ASSERT_TRUE(cert.get());

  // Test mozilla_security_manager::GetCertType with server certs and default
  // trust.  Currently this doesn't work.
  // TODO(mattm): make mozilla_security_manager::GetCertType smarter so we can
  // tell server certs even if they have no trust bits set.
  EXPECT_EQ(net::OTHER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  crypto::ScopedTestNSSDB test_nssdb;
  net::NSSCertDatabase db(crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* public slot */,
                          crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* private slot */);

  // Test GetCertType with server certs and explicit trust.
  EXPECT_TRUE(db.SetCertTrust(
      cert.get(), net::SERVER_CERT, net::NSSCertDatabase::TRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  // Test GetCertType with server certs and explicit distrust.
  EXPECT_TRUE(db.SetCertTrust(
      cert.get(), net::SERVER_CERT, net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
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

TEST(X509CertificateModelTest, GetCMSString) {
  net::CertificateList certs =
      CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                    "multi-root-chain1.pem",
                                    net::X509Certificate::FORMAT_AUTO);

  net::X509Certificate::OSCertHandles cert_handles;
  for (net::CertificateList::iterator i = certs.begin(); i != certs.end(); ++i)
    cert_handles.push_back((*i)->os_cert_handle());
  ASSERT_EQ(4U, cert_handles.size());

  {
    // Write the full chain.
    std::string pkcs7_string = x509_certificate_model::GetCMSString(
        cert_handles, 0, cert_handles.size());

    ASSERT_FALSE(pkcs7_string.empty());

    net::CertificateList decoded_certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            pkcs7_string.data(),
            pkcs7_string.size(),
            net::X509Certificate::FORMAT_PKCS7);

    ASSERT_EQ(certs.size(), decoded_certs.size());

    // NSS sorts the certs before writing the file.
    EXPECT_TRUE(certs[0]->Equals(decoded_certs.back().get()));
    for (size_t i = 1; i < certs.size(); ++i)
      EXPECT_TRUE(certs[i]->Equals(decoded_certs[i - 1].get()));
  }

  {
    // Write only the first cert.
    std::string pkcs7_string =
        x509_certificate_model::GetCMSString(cert_handles, 0, 1);

    net::CertificateList decoded_certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            pkcs7_string.data(),
            pkcs7_string.size(),
            net::X509Certificate::FORMAT_PKCS7);

    ASSERT_EQ(1U, decoded_certs.size());
    EXPECT_TRUE(certs[0]->Equals(decoded_certs[0].get()));
  }
}

TEST(X509CertificateModelTest, ProcessSecAlgorithms) {
  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "root_ca_cert.pem"));
    ASSERT_TRUE(cert.get());

    EXPECT_EQ("PKCS #1 SHA-1 With RSA Encryption",
              x509_certificate_model::ProcessSecAlgorithmSignature(
                  cert->os_cert_handle()));
    EXPECT_EQ("PKCS #1 SHA-1 With RSA Encryption",
              x509_certificate_model::ProcessSecAlgorithmSignatureWrap(
                  cert->os_cert_handle()));
    EXPECT_EQ("PKCS #1 RSA Encryption",
              x509_certificate_model::ProcessSecAlgorithmSubjectPublicKey(
                  cert->os_cert_handle()));
  }
  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "weak_digest_md5_root.pem"));
    ASSERT_TRUE(cert.get());

    EXPECT_EQ("PKCS #1 MD5 With RSA Encryption",
              x509_certificate_model::ProcessSecAlgorithmSignature(
                  cert->os_cert_handle()));
    EXPECT_EQ("PKCS #1 MD5 With RSA Encryption",
              x509_certificate_model::ProcessSecAlgorithmSignatureWrap(
                  cert->os_cert_handle()));
    EXPECT_EQ("PKCS #1 RSA Encryption",
              x509_certificate_model::ProcessSecAlgorithmSubjectPublicKey(
                  cert->os_cert_handle()));
  }
}

TEST(X509CertificateModelTest, ProcessSubjectPublicKeyInfo) {
  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "root_ca_cert.pem"));
    ASSERT_TRUE(cert.get());

    EXPECT_EQ(
        "Modulus (2048 bits):\n"
        "  B6 49 41 E3 42 01 51 A8 7F 3C 7A 71 D3 FB CD 91\n"
        "35 17 84 1A 8E F6 36 C7 D1 70 1D FA 86 F3 6E BB\n"
        "76 6F E8 32 2E 37 FD 38 92 3D 68 E4 8A 7D 42 33\n"
        "14 46 1B DC 04 F6 91 6E 54 40 C4 0A 09 FD EC 2D\n"
        "62 E2 5E E1 BA 2C 9C C1 B1 60 4C DA C7 F8 22 5C\n"
        "82 20 65 42 1E 56 77 75 4F EB 90 2C 4A EA 57 0E\n"
        "22 8D 6C 95 AC 11 EA CC D7 EE F6 70 0D 09 DD A6\n"
        "35 61 5D C9 76 6D B0 F2 1E BF 30 86 D8 77 52 36\n"
        "95 97 0E D1 46 C5 ED 81 3D 1B B0 F2 61 95 3C C1\n"
        "40 38 EF 5F 5D BA 61 9F EF 2B 9C 9F 85 89 74 70\n"
        "63 D5 76 E8 35 7E CE 01 E1 F3 11 11 90 1C 0D F5\n"
        "FD 8D CE 10 6C AD 7C 55 1A 21 6F D7 2D F4 78 15\n"
        "EA 2F 38 BD 91 9E 3C 1D 07 46 F5 43 C1 82 8B AF\n"
        "12 53 65 19 8A 69 69 66 06 B2 DA 0B FA 2A 00 A1\n"
        "2A 15 84 49 F1 01 BF 9B 30 06 D0 15 A0 1F 9D 51\n"
        "91 47 E1 53 5F EF 5E EC C2 61 79 C2 14 9F C4 E3\n"
        "\n"
        "  Public Exponent (24 bits):\n"
        "  01 00 01",
        x509_certificate_model::ProcessSubjectPublicKeyInfo(
            cert->os_cert_handle()));
  }
  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "prime256v1-ecdsa-intermediate.pem"));
    ASSERT_TRUE(cert.get());

    EXPECT_EQ(
        "04 D5 C1 4A 32 95 95 C5 88 FA 01 FA C5 9E DC E2\n"
        "99 62 EB 13 E5 35 42 B3 7A FC 46 C0 FA 29 12 C8\n"
        "2D EA 30 0F D2 9A 47 97 2C 7E 89 E6 EF 49 55 06\n"
        "C9 37 C7 99 56 16 B2 2B C9 7C 69 8E 10 7A DD 1F\n"
        "42",
        x509_certificate_model::ProcessSubjectPublicKeyInfo(
            cert->os_cert_handle()));
  }
}

TEST(X509CertificateModelTest, ProcessRawBitsSignatureWrap) {
  scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());

  EXPECT_EQ(
      "57 07 29 FB 7F E8 FF B0 E6 D8 58 6A C3 90 A1 38\n"
      "1C B4 F3 68 B1 EC E8 89 23 24 D7 A8 F2 21 C3 60\n"
      "E4 A4 49 5C 00 BF DF C7 82 78 80 2B 18 F7 AD DD\n"
      "D0 62 5E A7 B0 CC F0 AA B4 CE 70 12 59 65 67 76\n"
      "05 00 18 9A FF C4 2A 17 E3 F1 55 D8 BE 5C 5E EB\n"
      "CA CB 53 87 10 D5 09 32 36 A7 5E 41 F4 53 DA 7E\n"
      "56 60 D2 7E 4E 9A A5 08 5F 5D 75 E9 E7 30 CB 22\n"
      "E9 EF 19 49 83 A5 23 A1 F8 60 4C E5 36 D5 39 78\n"
      "18 F1 5E BF CE AA 0B 53 81 2C 78 A9 0A 6B DB 13\n"
      "10 21 14 7F 1B 70 3D 89 1A 40 8A 06 2C 5D 50 19\n"
      "62 F9 C7 45 89 F2 3D 66 05 3D 7D 75 5B 55 1E 80\n"
      "42 72 A1 9A 7C 6D 0A 74 F6 EE A6 21 6C 3A 98 FB\n"
      "77 82 5F F2 6B 56 E6 DD 9B 8E 50 F0 C6 AE FD EA\n"
      "A6 05 07 A9 26 06 56 B3 B2 D9 B2 37 A0 21 3E 79\n"
      "06 1F B9 51 BE F4 B1 49 4D 90 B5 33 E5 0E C7 5E\n"
      "5B 40 C5 6A 04 D1 43 7A 94 6A A4 4F 61 FC 82 E0",
      x509_certificate_model::ProcessRawBitsSignatureWrap(
          cert->os_cert_handle()));
}
