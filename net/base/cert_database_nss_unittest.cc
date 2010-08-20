// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cert.h>
#include <pk11pub.h>

#include "base/crypto/scoped_nss_types.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/nss_util.h"
#include "base/nss_util_internal.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/cert_database.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Returns a FilePath object representing the src/net/data/ssl/certificates
// directory in the source tree.
FilePath GetTestCertsDirectory() {
  FilePath certs_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &certs_dir);
  certs_dir = certs_dir.AppendASCII("net");
  certs_dir = certs_dir.AppendASCII("data");
  certs_dir = certs_dir.AppendASCII("ssl");
  certs_dir = certs_dir.AppendASCII("certificates");
  return certs_dir;
}

CertificateList ListCertsInSlot(PK11SlotInfo* slot) {
  CertificateList result;
  CERTCertList* cert_list = PK11_ListCertsInSlot(slot);
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    result.push_back(
        X509Certificate::CreateFromHandle(
            node->cert,
            X509Certificate::SOURCE_LONE_CERT_IMPORT,
            X509Certificate::OSCertHandles()));
  }
  CERT_DestroyCertList(cert_list);
  return result;
}

std::string ReadTestFile(const std::string& name) {
  std::string result;
  FilePath cert_path = GetTestCertsDirectory().AppendASCII(name);
  EXPECT_TRUE(file_util::ReadFileToString(cert_path, &result));
  return result;
}

}  // namespace

class CertDatabaseNSSTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(temp_db_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::OpenTestNSSDB(temp_db_dir_.path(), "CertDatabaseNSSTest db"));
    slot_.reset(base::GetDefaultNSSKeySlot());

    // Test db should be empty at start of test.
    EXPECT_EQ(0U, ListCertsInSlot(slot_.get()).size());
  }
  virtual void TearDown() {
    base::CloseTestNSSDB();
  }

 protected:
  base::ScopedPK11Slot slot_;
  CertDatabase cert_db_;

 private:
  ScopedTempDir temp_db_dir_;
};

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12WrongPassword) {
  std::string pkcs12_data = ReadTestFile("client.p12");

  EXPECT_EQ(ERR_PKCS12_IMPORT_BAD_PASSWORD,
            cert_db_.ImportFromPKCS12(pkcs12_data, ASCIIToUTF16("")));

  // Test db should still be empty.
  EXPECT_EQ(0U, ListCertsInSlot(slot_.get()).size());
}

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12AndExportAgain) {
  std::string pkcs12_data = ReadTestFile("client.p12");

  EXPECT_EQ(OK, cert_db_.ImportFromPKCS12(pkcs12_data, ASCIIToUTF16("12345")));

  CertificateList cert_list = ListCertsInSlot(slot_.get());
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  EXPECT_EQ("testusercert",
            cert->subject().common_name);

  // TODO(mattm): move export test to seperate test case?
  std::string exported_data;
  EXPECT_EQ(1, cert_db_.ExportToPKCS12(cert_list, ASCIIToUTF16("exportpw"),
                                       &exported_data));
  ASSERT_LT(0U, exported_data.size());
  // TODO(mattm): further verification of exported data?
}

}  // namespace net
