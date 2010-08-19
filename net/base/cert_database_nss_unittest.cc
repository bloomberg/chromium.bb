// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cert.h>
#include <pk11pub.h>

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

}  // namespace


class CertDatabaseNSSTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(temp_db_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::OpenTestNSSDB(temp_db_dir_.path(), "CertDatabaseNSSTest db"));
  }
  virtual void TearDown() {
    base::CloseTestNSSDB();
  }
 private:
  ScopedTempDir temp_db_dir_;
};

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12WrongPassword) {
  PK11SlotInfo* slot = base::GetDefaultNSSKeySlot();
  CertDatabase cert_db;

  CERTCertList* cert_list = PK11_ListCertsInSlot(slot);
  // Test db should be empty at start of test.
  EXPECT_TRUE(CERT_LIST_END(CERT_LIST_HEAD(cert_list), cert_list));
  CERT_DestroyCertList(cert_list);

  FilePath cert_path = GetTestCertsDirectory().AppendASCII("client.p12");
  std::string cert_data;
  ASSERT_TRUE(file_util::ReadFileToString(cert_path, &cert_data));
  EXPECT_EQ(ERR_PKCS12_IMPORT_BAD_PASSWORD,
            cert_db.ImportFromPKCS12(cert_data, ASCIIToUTF16("")));


  cert_list = PK11_ListCertsInSlot(slot);
  // Test db should still be empty.
  EXPECT_TRUE(CERT_LIST_END(CERT_LIST_HEAD(cert_list), cert_list));
  CERT_DestroyCertList(cert_list);

  PK11_FreeSlot(slot);
}

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12AndExportAgain) {
  PK11SlotInfo* slot = base::GetDefaultNSSKeySlot();
  CertDatabase cert_db;

  CERTCertList* cert_list = PK11_ListCertsInSlot(slot);
  // Test db should be empty at start of test.
  EXPECT_TRUE(CERT_LIST_END(CERT_LIST_HEAD(cert_list), cert_list));
  CERT_DestroyCertList(cert_list);

  FilePath cert_path = GetTestCertsDirectory().AppendASCII("client.p12");
  std::string cert_data;
  ASSERT_TRUE(file_util::ReadFileToString(cert_path, &cert_data));
  EXPECT_EQ(OK, cert_db.ImportFromPKCS12(cert_data, ASCIIToUTF16("12345")));

  cert_list = PK11_ListCertsInSlot(slot);
  // Test db should be empty at start of test.
  ASSERT_FALSE(CERT_LIST_END(CERT_LIST_HEAD(cert_list), cert_list));
  scoped_refptr<X509Certificate> cert(
      X509Certificate::CreateFromHandle(
          CERT_LIST_HEAD(cert_list)->cert,
          X509Certificate::SOURCE_LONE_CERT_IMPORT,
          X509Certificate::OSCertHandles()));
  CERT_DestroyCertList(cert_list);

  EXPECT_EQ("testusercert",
            cert->subject().common_name);

  // TODO(mattm): move export test to seperate test case?
  CertificateList certs;
  certs.push_back(cert);
  std::string exported_data;
  EXPECT_EQ(1, cert_db.ExportToPKCS12(certs, ASCIIToUTF16("exportpw"),
                                      &exported_data));
  ASSERT_LT(0U, exported_data.size());
  // TODO(mattm): further verification of exported data?

  PK11_FreeSlot(slot);
}

}  // namespace net
