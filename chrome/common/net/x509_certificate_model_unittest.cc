// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "net/base/test_data_directory.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS)
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
                              "punycodetest.der"));
  ASSERT_TRUE(punycode_cert.get());
  EXPECT_EQ("xn--wgv71a119e.com (日本語.com)",
            x509_certificate_model::GetCertNameOrNickname(
                punycode_cert->os_cert_handle()));

  scoped_refptr<net::X509Certificate> no_cn_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(),
                              "no_subject_common_name_cert.pem"));
  ASSERT_TRUE(no_cn_cert.get());
#if defined(USE_OPENSSL)
  EXPECT_EQ("emailAddress=wtc@google.com",
            x509_certificate_model::GetCertNameOrNickname(
                no_cn_cert->os_cert_handle()));
#else
  // Temp cert has no nickname.
  EXPECT_EQ("",
            x509_certificate_model::GetCertNameOrNickname(
                no_cn_cert->os_cert_handle()));
#endif

  EXPECT_EQ("xn--wgv71a119e.com",
            x509_certificate_model::GetTitle(
                punycode_cert->os_cert_handle()));

#if defined(USE_OPENSSL)
  EXPECT_EQ("emailAddress=wtc@google.com",
            x509_certificate_model::GetTitle(
                no_cn_cert->os_cert_handle()));
#else
  EXPECT_EQ("E=wtc@google.com",
            x509_certificate_model::GetTitle(
                no_cn_cert->os_cert_handle()));
#endif

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
        "notcrit\nKey ID: 2B 88 93 E1 D2 54 50 F4 B8 A4 20 BD B1 79 E6 0B\nAA "
        "EB EC 1A",
        extensions[1].value);

    EXPECT_EQ("Certificate Key Usage", extensions[2].name);
    EXPECT_EQ("critical\nCertificate Signer\nCRL Signer", extensions[2].value);
  }

  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "subjectAltName_sanity_check.pem"));
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

#if defined(USE_OPENSSL)
  // Remove this when OpenSSL build implements the necessary functions.
  EXPECT_EQ(net::OTHER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#else
  EXPECT_EQ(net::CA_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  // Test that explicitly distrusted CA certs are still returned as CA_CERT
  // type. See http://crbug.com/96654.
  EXPECT_TRUE(net::NSSCertDatabase::GetInstance()->SetCertTrust(
      cert.get(), net::CA_CERT, net::NSSCertDatabase::DISTRUSTED_SSL));

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
  EXPECT_EQ(net::OTHER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));
#else
  // Test mozilla_security_manager::GetCertType with server certs and default
  // trust.  Currently this doesn't work.
  // TODO(mattm): make mozilla_security_manager::GetCertType smarter so we can
  // tell server certs even if they have no trust bits set.
  EXPECT_EQ(net::OTHER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  net::NSSCertDatabase* cert_db = net::NSSCertDatabase::GetInstance();
  // Test GetCertType with server certs and explicit trust.
  EXPECT_TRUE(cert_db->SetCertTrust(
      cert.get(), net::SERVER_CERT, net::NSSCertDatabase::TRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT,
            x509_certificate_model::GetType(cert->os_cert_handle()));

  // Test GetCertType with server certs and explicit distrust.
  EXPECT_TRUE(cert_db->SetCertTrust(
      cert.get(), net::SERVER_CERT, net::NSSCertDatabase::DISTRUSTED_SSL));

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
#if defined(USE_OPENSSL)
    for (size_t i = 0; i < certs.size(); ++i)
      EXPECT_TRUE(certs[i]->Equals(decoded_certs[i]));
#else
    // NSS sorts the certs before writing the file.
    EXPECT_TRUE(certs[0]->Equals(decoded_certs.back()));
    for (size_t i = 1; i < certs.size(); ++i)
      EXPECT_TRUE(certs[i]->Equals(decoded_certs[i-1]));
#endif
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
    EXPECT_TRUE(certs[0]->Equals(decoded_certs[0]));
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
        "  AB A3 84 16 05 AE F4 80 85 81 A7 A8 59 FA BB 0E\n"
        "5E 7B 04 DC C4 44 7A 41 05 37 9D 45 A1 6B DE E8\n"
        "FE 0F 89 D3 39 78 EB 68 01 4F 15 C0 4B 13 A4 4C\n"
        "25 95 ED A4 BB D9 AD F7 54 0C F1 33 4E D7 25 88\n"
        "B0 28 5E 64 01 F0 33 7C 4D 3B D8 5C 48 04 AF 77\n"
        "52 6F EA 99 B0 07 E6 6D BB 63 9E 33 AD 18 94 30\n"
        "96 46 F4 41 D6 69 E3 EE 55 DE FA C3 D4 36 D3 D1\n"
        "71 87 28 3B B8 FC 4B 2D BF 3C E2 FB 8C E8 FA 99\n"
        "44 0C BD 5D CB E3 A9 F6 0D 3D 1C EB B6 80 1E BE\n"
        "A5 51 B5 60 04 77 72 47 96 17 0D 8E 44 EE FA C4\n"
        "5F AB 31 16 DC 68 9A 9F 9A 79 94 04 B9 0F 14 DF\n"
        "C1 9A FA 37 AB 7F 70 B8 80 DD 48 25 ED BD 43 67\n"
        "01 C1 32 9D 76 A1 FE C1 64 D8 00 77 73 D1 3F 21\n"
        "86 92 72 E8 91 36 45 84 8B B7 14 5E B0 32 5C A3\n"
        "ED 30 DA 36 45 DB DF 55 41 18 CF FE 36 37 ED BB\n"
        "D3 09 1F D6 D6 91 D2 D8 5F 73 02 52 D3 AA 0D 23\n"
        "\n"
#if defined(USE_OPENSSL)
        "  Public Exponent (17 bits):\n"
#else
        "  Public Exponent (24 bits):\n"
#endif
        "  01 00 01",
        x509_certificate_model::ProcessSubjectPublicKeyInfo(
            cert->os_cert_handle()));
  }
  {
    scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "prime256v1-ecdsa-intermediate.pem"));
    ASSERT_TRUE(cert.get());
    EXPECT_EQ(
        "04 D1 35 14 53 74 2F E1 E4 9B 41 9E 42 9D 10 6B\n"
        "0B F4 16 8F BC A7 C7 A4 39 09 73 34 CB 87 DF 2F\n"
        "7E 4A 5F B1 B5 E4 DC 49 41 4E A8 81 34 B5 DA 7D\n"
        "27 7D 05 C1 BD 0A 29 6D AD A3 5D 37 7B 56 B7 1B\n"
        "60",
        x509_certificate_model::ProcessSubjectPublicKeyInfo(
            cert->os_cert_handle()));
  }
}

TEST(X509CertificateModelTest, ProcessRawBitsSignatureWrap) {
  scoped_refptr<net::X509Certificate> cert(net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());
  EXPECT_EQ(
      "A8 58 42 E4 7C B1 46 11 EE 56 B7 09 08 FB 06 44\n"
      "F0 A9 60 03 F0 05 23 09 3C 36 D6 28 1B E5 D6 61\n"
      "15 A0 6F DE 69 AC 28 58 05 F1 CE 9B 61 C2 58 B0\n"
      "5D ED 6C 75 44 E2 68 01 91 59 B1 4F F3 51 F2 23\n"
      "F6 47 42 41 57 26 4F 87 1E D2 9F 94 3A E2 D0 4E\n"
      "6F 02 D2 92 76 2C 0A DD 58 93 E1 47 B9 02 A3 3D\n"
      "75 B4 BA 24 70 87 32 87 CF 76 4E A0 41 8B 86 42\n"
      "18 55 ED A5 AE 5D 6A 3A 8C 28 70 4C F1 C5 36 6C\n"
      "EC 01 A9 D6 51 39 32 31 30 24 82 9F 88 D9 F5 C1\n"
      "09 6B 5A 6B F1 95 D3 9D 3F E0 42 63 FC B7 32 90\n"
      "55 56 F2 76 1B 71 38 BD BD FB 3B 23 50 46 4C 2C\n"
      "4E 49 48 52 EA 05 5F 16 F2 98 51 AF 2F 79 36 2A\n"
      "A0 BA 36 68 1B 29 8B 7B E8 8C EA 73 31 E5 86 D7\n"
      "2C D8 56 06 43 D7 72 D2 F0 27 4E 64 0A 2B 27 38\n"
      "36 CD BE C1 33 DB 74 4B 4E 74 BE 21 BD F6 81 66\n"
      "D2 FD 2B 7F F4 55 36 C0 ED A7 44 CA B1 78 1D 0F",
      x509_certificate_model::ProcessRawBitsSignatureWrap(
          cert->os_cert_handle()));
}
