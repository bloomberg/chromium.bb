// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_settings/onc_certificate_importer.h"

#include <cert.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <string>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/onc_constants.h"
#include "chrome/browser/chromeos/network_settings/onc_test_utils.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "crypto/nss_util.h"
#include "net/base/cert_type.h"
#include "net/base/crypto_module.h"
#include "net/base/nss_cert_database.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

net::CertType GetCertType(const net::X509Certificate* cert) {
  DCHECK(cert);
  return x509_certificate_model::GetType(cert->os_cert_handle());
}

}  // namespace

namespace chromeos {
namespace onc {

class ONCCertificateImporterTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(test_nssdb_.is_open());

    slot_ = net::NSSCertDatabase::GetInstance()->GetPublicModule();

    // Don't run the test if the setup failed.
    ASSERT_TRUE(slot_->os_module_handle());

    // Test db should be empty at start of test.
    EXPECT_EQ(0ul, ListCertsInSlot(slot_->os_module_handle()).size());
  }

  virtual void TearDown() {
    EXPECT_TRUE(CleanupSlotContents(slot_->os_module_handle()));
    EXPECT_EQ(0ul, ListCertsInSlot(slot_->os_module_handle()).size());
  }

  virtual ~ONCCertificateImporterTest() {}

 protected:
  void AddCertificateFromFile(std::string filename,
                              net::CertType expected_type,
                              std::string* guid) {
    scoped_ptr<base::DictionaryValue> onc =
        test_utils::ReadTestDictionary(filename);
    base::ListValue* certificates;
    onc->GetListWithoutPathExpansion(kCertificates, &certificates);

    base::DictionaryValue* certificate;
    certificates->GetDictionary(0, &certificate);
    certificate->GetStringWithoutPathExpansion(kGUID, guid);

    CertificateImporter importer(NetworkUIData::ONC_SOURCE_USER_IMPORT,
                                 false /* don't allow webtrust */);
    std::string error;
    EXPECT_TRUE(importer.ParseAndStoreCertificates(*certificates, &error));
    EXPECT_TRUE(error.empty());

    net::CertificateList result_list;
    CertificateImporter::ListCertsWithNickname(*guid, &result_list);
    ASSERT_EQ(1ul, result_list.size());
    EXPECT_EQ(expected_type, GetCertType(result_list[0].get()));
  }

  scoped_refptr<net::CryptoModule> slot_;

 private:
  net::CertificateList ListCertsInSlot(PK11SlotInfo* slot) {
    net::CertificateList result;
    CERTCertList* cert_list = PK11_ListCertsInSlot(slot);
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

  bool CleanupSlotContents(PK11SlotInfo* slot) {
    bool ok = true;
    net::CertificateList certs = ListCertsInSlot(slot);
    for (size_t i = 0; i < certs.size(); ++i) {
      if (!net::NSSCertDatabase::GetInstance()->DeleteCertAndKey(certs[i]))
        ok = false;
    }
    return ok;
  }

  crypto::ScopedTestNSSDB test_nssdb_;
};

TEST_F(ONCCertificateImporterTest, AddClientCertificate) {
  std::string guid;
  AddCertificateFromFile("certificate-client.onc", net::USER_CERT, &guid);

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
}

class ONCCertificateImporterTestWithParam :
      public ONCCertificateImporterTest,
      public testing::WithParamInterface<
          std::pair<net::CertType, std::pair<const char*, const char*> > > {
 protected:
  net::CertType GetCertTypeParam() {
    return GetParam().first;
  }

  std::string GetOriginalFilename() {
    return GetParam().second.first;
  }

  std::string GetUpdatedFilename() {
    return GetParam().second.second;
  }
};

TEST_P(ONCCertificateImporterTestWithParam, UpdateCertificate) {
  // First we import a certificate.
  {
    SCOPED_TRACE("Import original certificate");
    std::string guid_original;
    AddCertificateFromFile(GetOriginalFilename(), GetCertTypeParam(),
                           &guid_original);
  }

  // Now we import the same certificate with a different GUID. The cert should
  // be retrievable via the new GUID.
  {
    SCOPED_TRACE("Import updated certificate");
    std::string guid_updated;
    AddCertificateFromFile(GetUpdatedFilename(), GetCertTypeParam(),
                           &guid_updated);
  }
}

TEST_P(ONCCertificateImporterTestWithParam, ReimportCertificate) {
  // Verify that reimporting a client certificate works.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE("Import certificate, iteration " + base::IntToString(i));

    std::string guid_original;
    AddCertificateFromFile(GetOriginalFilename(), GetCertTypeParam(),
                           &guid_original);
  }
}

INSTANTIATE_TEST_CASE_P(
    ONCCertificateImporterTestWithParam,
    ONCCertificateImporterTestWithParam,
    ::testing::Values(
        std::make_pair(net::USER_CERT,
                       std::make_pair("certificate-client.onc",
                                      "certificate-client-update.onc")),
        std::make_pair(net::SERVER_CERT,
                       std::make_pair("certificate-server.onc",
                                      "certificate-server-update.onc")),
        std::make_pair(
            net::CA_CERT,
            std::make_pair("certificate-web-authority.onc",
                           "certificate-web-authority-update.onc"))));

}  // namespace onc
}  // namespace chromeos
