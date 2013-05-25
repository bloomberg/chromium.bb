// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_certificate_importer.h"

#include <cert.h>
#include <certdb.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <string>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "crypto/nss_util.h"
#include "net/base/crypto_module.h"
#include "net/cert/cert_type.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

#if defined(USE_NSS)
// In NSS 3.13, CERTDB_VALID_PEER was renamed CERTDB_TERMINAL_RECORD. So we use
// the new name of the macro.
#if !defined(CERTDB_TERMINAL_RECORD)
#define CERTDB_TERMINAL_RECORD CERTDB_VALID_PEER
#endif

net::CertType GetCertType(net::X509Certificate::OSCertHandle cert) {
  CERTCertTrust trust = {0};
  CERT_GetCertTrust(cert, &trust);

  unsigned all_flags = trust.sslFlags | trust.emailFlags |
      trust.objectSigningFlags;

  if (cert->nickname && (all_flags & CERTDB_USER))
    return net::USER_CERT;
  if ((all_flags & CERTDB_VALID_CA) || CERT_IsCACert(cert, NULL))
    return net::CA_CERT;
  // TODO(mattm): http://crbug.com/128633.
  if (trust.sslFlags & CERTDB_TERMINAL_RECORD)
    return net::SERVER_CERT;
  return net::UNKNOWN_CERT;
}
#else
net::CertType GetCertType(net::X509Certificate::OSCertHandle cert) {
  NOTIMPLEMENTED();
  return net::UNKNOWN_CERT;
}
#endif  // USE_NSS

class ONCCertificateImporterTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(test_nssdb_.is_open());

    slot_ = net::NSSCertDatabase::GetInstance()->GetPublicModule();

    // Don't run the test if the setup failed.
    ASSERT_TRUE(slot_->os_module_handle());

    // Test db should be empty at start of test.
    EXPECT_EQ(0ul, ListCertsInSlot().size());
  }

  virtual void TearDown() {
    EXPECT_TRUE(CleanupSlotContents());
    EXPECT_EQ(0ul, ListCertsInSlot().size());
  }

  virtual ~ONCCertificateImporterTest() {}

 protected:
  void AddCertificatesFromFile(
      std::string filename,
      CertificateImporter::ParseResult expected_parse_result) {
    scoped_ptr<base::DictionaryValue> onc =
        test_utils::ReadTestDictionary(filename);
    base::Value* certificates_value = NULL;
    base::ListValue* certificates = NULL;
    onc->RemoveWithoutPathExpansion(toplevel_config::kCertificates,
                                    &certificates_value);
    certificates_value->GetAsList(&certificates);
    onc_certificates_.reset(certificates);

    web_trust_certificates_.clear();
    CertificateImporter importer(true /* allow web trust */);
    EXPECT_EQ(expected_parse_result,
              importer.ParseAndStoreCertificates(*certificates,
                                                 &web_trust_certificates_));

    result_list_.clear();
    result_list_ = ListCertsInSlot();
  }

  void AddCertificateFromFile(std::string filename,
                              net::CertType expected_type,
                              std::string* guid) {
    AddCertificatesFromFile(filename, CertificateImporter::IMPORT_OK);
    EXPECT_EQ(1ul, result_list_.size());

    base::DictionaryValue* certificate = NULL;
    onc_certificates_->GetDictionary(0, &certificate);
    certificate->GetStringWithoutPathExpansion(certificate::kGUID, guid);

    CertificateImporter::ListCertsWithNickname(*guid, &result_list_);
    ASSERT_EQ(1ul, result_list_.size());
    EXPECT_EQ(expected_type, GetCertType(result_list_[0]->os_cert_handle()));
  }

  scoped_ptr<base::ListValue> onc_certificates_;
  scoped_refptr<net::CryptoModule> slot_;
  net::CertificateList result_list_;
  net::CertificateList web_trust_certificates_;

 private:
  net::CertificateList ListCertsInSlot() {
    net::CertificateList result;
    CERTCertList* cert_list = PK11_ListCertsInSlot(slot_->os_module_handle());
    for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
         !CERT_LIST_END(node, cert_list);
         node = CERT_LIST_NEXT(node)) {
      result.push_back(net::X509Certificate::CreateFromHandle(
          node->cert, net::X509Certificate::OSCertHandles()));
    }
    CERT_DestroyCertList(cert_list);

    // Sort the result so that test comparisons can be deterministic.
    std::sort(result.begin(), result.end(), net::X509Certificate::LessThan());
    return result;
  }

  bool CleanupSlotContents() {
    bool ok = true;
    net::CertificateList certs = ListCertsInSlot();
    for (size_t i = 0; i < certs.size(); ++i) {
      if (!net::NSSCertDatabase::GetInstance()->DeleteCertAndKey(certs[i]))
        ok = false;
    }
    return ok;
  }

  crypto::ScopedTestNSSDB test_nssdb_;
};

TEST_F(ONCCertificateImporterTest, MultipleCertificates) {
  AddCertificatesFromFile("managed_toplevel2.onc",
                          CertificateImporter::IMPORT_OK);
  EXPECT_EQ(onc_certificates_->GetSize(), result_list_.size());
}

TEST_F(ONCCertificateImporterTest, MultipleCertificatesWithFailures) {
  AddCertificatesFromFile("toplevel_partially_invalid.onc",
                          CertificateImporter::IMPORT_INCOMPLETE);
  EXPECT_EQ(2ul, onc_certificates_->GetSize());
  EXPECT_EQ(1ul, result_list_.size());
}

TEST_F(ONCCertificateImporterTest, AddClientCertificate) {
  std::string guid;
  AddCertificateFromFile("certificate-client.onc", net::USER_CERT, &guid);
  EXPECT_TRUE(web_trust_certificates_.empty());

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(slot_->os_module_handle(), NULL, NULL);
  EXPECT_TRUE(privkey_list);
  if (privkey_list) {
    SECKEYPrivateKeyListNode* node = PRIVKEY_LIST_HEAD(privkey_list);
    int count = 0;
    while (!PRIVKEY_LIST_END(node, privkey_list)) {
      char* name = PK11_GetPrivateKeyNickname(node->key);
      EXPECT_STREQ(guid.c_str(), name);
      PORT_Free(name);
      count++;
      node = PRIVKEY_LIST_NEXT(node);
    }
    EXPECT_EQ(1, count);
    SECKEY_DestroyPrivateKeyList(privkey_list);
  }

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(slot_->os_module_handle(), NULL);
  EXPECT_TRUE(pubkey_list);
  if (pubkey_list) {
    SECKEYPublicKeyListNode* node = PUBKEY_LIST_HEAD(pubkey_list);
    int count = 0;
    while (!PUBKEY_LIST_END(node, pubkey_list)) {
      count++;
      node = PUBKEY_LIST_NEXT(node);
    }
    EXPECT_EQ(1, count);
    SECKEY_DestroyPublicKeyList(pubkey_list);
  }
}

TEST_F(ONCCertificateImporterTest, AddServerCertificate) {
  std::string guid;
  AddCertificateFromFile("certificate-server.onc", net::SERVER_CERT, &guid);

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(slot_->os_module_handle(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(slot_->os_module_handle(), NULL);
  EXPECT_FALSE(pubkey_list);

  ASSERT_EQ(1u, web_trust_certificates_.size());
  ASSERT_EQ(1u, result_list_.size());
  EXPECT_TRUE(CERT_CompareCerts(result_list_[0]->os_cert_handle(),
                                web_trust_certificates_[0]->os_cert_handle()));
}

TEST_F(ONCCertificateImporterTest, AddWebAuthorityCertificate) {
  std::string guid;
  AddCertificateFromFile("certificate-web-authority.onc", net::CA_CERT, &guid);

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(slot_->os_module_handle(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(slot_->os_module_handle(), NULL);
  EXPECT_FALSE(pubkey_list);

  ASSERT_EQ(1u, web_trust_certificates_.size());
  ASSERT_EQ(1u, result_list_.size());
  EXPECT_TRUE(CERT_CompareCerts(result_list_[0]->os_cert_handle(),
                                web_trust_certificates_[0]->os_cert_handle()));
}

TEST_F(ONCCertificateImporterTest, AddAuthorityCertificateWithoutWebTrust) {
  std::string guid;
  AddCertificateFromFile("certificate-authority.onc", net::CA_CERT, &guid);
  EXPECT_TRUE(web_trust_certificates_.empty());

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(slot_->os_module_handle(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(slot_->os_module_handle(), NULL);
  EXPECT_FALSE(pubkey_list);
}

struct CertParam {
  CertParam(net::CertType certificate_type,
            const char* original_filename,
            const char* update_filename)
      : cert_type(certificate_type),
        original_file(original_filename),
        update_file(update_filename) {}

  net::CertType cert_type;
  const char* original_file;
  const char* update_file;
};

class ONCCertificateImporterTestWithParam :
      public ONCCertificateImporterTest,
      public testing::WithParamInterface<CertParam> {
};

TEST_P(ONCCertificateImporterTestWithParam, UpdateCertificate) {
  // First we import a certificate.
  {
    SCOPED_TRACE("Import original certificate");
    std::string guid_original;
    AddCertificateFromFile(GetParam().original_file, GetParam().cert_type,
                           &guid_original);
  }

  // Now we import the same certificate with a different GUID. The cert should
  // be retrievable via the new GUID.
  {
    SCOPED_TRACE("Import updated certificate");
    std::string guid_updated;
    AddCertificateFromFile(GetParam().update_file, GetParam().cert_type,
                           &guid_updated);
  }
}

TEST_P(ONCCertificateImporterTestWithParam, ReimportCertificate) {
  // Verify that reimporting a client certificate works.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE("Import certificate, iteration " + base::IntToString(i));

    std::string guid_original;
    AddCertificateFromFile(GetParam().original_file, GetParam().cert_type,
                           &guid_original);
  }
}

INSTANTIATE_TEST_CASE_P(
    ONCCertificateImporterTestWithParam,
    ONCCertificateImporterTestWithParam,
    ::testing::Values(
        CertParam(net::USER_CERT,
                  "certificate-client.onc",
                  "certificate-client-update.onc"),
        CertParam(net::SERVER_CERT,
                  "certificate-server.onc",
                  "certificate-server-update.onc"),
        CertParam(net::CA_CERT,
                  "certificate-web-authority.onc",
                  "certificate-web-authority-update.onc")));

}  // namespace onc
}  // namespace chromeos
