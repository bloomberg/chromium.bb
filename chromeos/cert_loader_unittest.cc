// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cert_loader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

bool IsCertInCertificateList(const net::X509Certificate* cert,
                             const net::CertificateList& cert_list) {
  for (net::CertificateList::const_iterator it = cert_list.begin();
       it != cert_list.end();
       ++it) {
    if (net::X509Certificate::IsSameOSCert((*it)->os_cert_handle(),
                                            cert->os_cert_handle())) {
      return true;
    }
  }
  return false;
}

void FailOnPrivateSlotCallback(crypto::ScopedPK11Slot slot) {
  EXPECT_FALSE(true) << "GetPrivateSlotForChromeOSUser callback called even "
                     << "though the private slot had been initialized.";
}

class CertLoaderTest : public testing::Test,
                       public CertLoader::Observer {
 public:
  CertLoaderTest() : cert_loader_(NULL),
                     primary_user_("primary"),
                     certificates_loaded_events_count_(0U) {
  }

  virtual ~CertLoaderTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(primary_user_.constructed_successfully());
    ASSERT_TRUE(
        crypto::GetPublicSlotForChromeOSUser(primary_user_.username_hash()));

    CertLoader::Initialize();
    cert_loader_ = CertLoader::Get();
    cert_loader_->AddObserver(this);
  }

  virtual void TearDown() {
    cert_loader_->RemoveObserver(this);
    CertLoader::Shutdown();
  }

 protected:
  void StartCertLoaderWithPrimaryUser() {
    FinishUserInitAndGetDatabase(&primary_user_, &primary_db_);
    cert_loader_->StartWithNSSDB(primary_db_.get());

    base::RunLoop().RunUntilIdle();
    GetAndResetCertificatesLoadedEventsCount();
  }

  // CertLoader::Observer:
  // The test keeps count of times the observer method was called.
  virtual void OnCertificatesLoaded(const net::CertificateList& cert_list,
                                    bool initial_load) OVERRIDE {
    EXPECT_TRUE(certificates_loaded_events_count_ == 0 || !initial_load);
    certificates_loaded_events_count_++;
  }

  // Returns the number of |OnCertificatesLoaded| calls observed since the
  // last call to this method equals |value|.
  size_t GetAndResetCertificatesLoadedEventsCount() {
    size_t result = certificates_loaded_events_count_;
    certificates_loaded_events_count_ = 0;
    return result;
  }

  // Finishes initialization for the |user| and returns a user's NSS database
  // instance.
  void FinishUserInitAndGetDatabase(
      crypto::ScopedTestNSSChromeOSUser* user,
      scoped_ptr<net::NSSCertDatabaseChromeOS>* database) {
    ASSERT_TRUE(user->constructed_successfully());

    user->FinishInit();

    crypto::ScopedPK11Slot private_slot(
        crypto::GetPrivateSlotForChromeOSUser(
            user->username_hash(),
            base::Bind(&FailOnPrivateSlotCallback)));
    ASSERT_TRUE(private_slot);

    database->reset(new net::NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(user->username_hash()),
        private_slot.Pass()));
    (*database)->SetSlowTaskRunnerForTest(message_loop_.message_loop_proxy());
  }

  int GetDbPrivateSlotId(net::NSSCertDatabase* db) {
    return static_cast<int>(PK11_GetSlotID(db->GetPrivateSlot().get()));
  }

  void ImportCACert(const std::string& cert_file,
                    net::NSSCertDatabase* database,
                    net::CertificateList* imported_certs) {
    ASSERT_TRUE(database);
    ASSERT_TRUE(imported_certs);

    // Add a certificate to the user's db.
    *imported_certs = net::CreateCertificateListFromFile(
        net::GetTestCertsDirectory(),
        cert_file,
        net::X509Certificate::FORMAT_AUTO);
    ASSERT_EQ(1U, imported_certs->size());

    net::NSSCertDatabase::ImportCertFailureList failed;
    ASSERT_TRUE(database->ImportCACerts(*imported_certs,
                                        net::NSSCertDatabase::TRUST_DEFAULT,
                                        &failed));
    ASSERT_TRUE(failed.empty());
  }

  void ImportClientCertAndKey(const std::string& pkcs12_file,
                              net::NSSCertDatabase* database,
                              net::CertificateList* imported_certs) {
    ASSERT_TRUE(database);
    ASSERT_TRUE(imported_certs);

    std::string pkcs12_data;
    base::FilePath pkcs12_file_path =
        net::GetTestCertsDirectory().Append(pkcs12_file);
    ASSERT_TRUE(base::ReadFileToString(pkcs12_file_path, &pkcs12_data));

    net::CertificateList client_cert_list;
    scoped_refptr<net::CryptoModule> module(net::CryptoModule::CreateFromHandle(
        database->GetPrivateSlot().get()));
    ASSERT_EQ(
        net::OK,
        database->ImportFromPKCS12(module, pkcs12_data, base::string16(), false,
                                   imported_certs));
    ASSERT_EQ(1U, imported_certs->size());
  }

  CertLoader* cert_loader_;

  // The user is primary as the one whose certificates CertLoader handles, it
  // has nothing to do with crypto::InitializeNSSForChromeOSUser is_primary_user
  // parameter (which is irrelevant for these tests).
  crypto::ScopedTestNSSChromeOSUser primary_user_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> primary_db_;

  base::MessageLoop message_loop_;

 private:
  size_t certificates_loaded_events_count_;
};

TEST_F(CertLoaderTest, Basic) {
  EXPECT_FALSE(cert_loader_->CertificatesLoading());
  EXPECT_FALSE(cert_loader_->certificates_loaded());
  EXPECT_FALSE(cert_loader_->IsHardwareBacked());

  FinishUserInitAndGetDatabase(&primary_user_, &primary_db_);

  cert_loader_->StartWithNSSDB(primary_db_.get());

  EXPECT_FALSE(cert_loader_->certificates_loaded());
  EXPECT_TRUE(cert_loader_->CertificatesLoading());
  EXPECT_TRUE(cert_loader_->cert_list().empty());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(cert_loader_->certificates_loaded());
  EXPECT_FALSE(cert_loader_->CertificatesLoading());

  // Default CA cert roots should get loaded.
  EXPECT_FALSE(cert_loader_->cert_list().empty());
}

TEST_F(CertLoaderTest, CertLoaderUpdatesCertListOnNewCert) {
  StartCertLoaderWithPrimaryUser();

  net::CertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_db_.get(), &certs);

  // Certs are loaded asynchronously, so the new cert should not yet be in the
  // cert list.
  EXPECT_FALSE(IsCertInCertificateList(certs[0], cert_loader_->cert_list()));

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  // The certificate list should be updated now, as the message loop's been run.
  EXPECT_TRUE(IsCertInCertificateList(certs[0], cert_loader_->cert_list()));
}

TEST_F(CertLoaderTest, CertLoaderNoUpdateOnSecondaryDbChanges) {
  crypto::ScopedTestNSSChromeOSUser secondary_user("secondary");
  scoped_ptr<net::NSSCertDatabaseChromeOS> secondary_db;

  StartCertLoaderWithPrimaryUser();
  FinishUserInitAndGetDatabase(&secondary_user, &secondary_db);

  net::CertificateList certs;
  ImportCACert("root_ca_cert.pem", secondary_db.get(), &certs);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsCertInCertificateList(certs[0], cert_loader_->cert_list()));
}

TEST_F(CertLoaderTest, ClientLoaderUpdateOnNewClientCert) {
  StartCertLoaderWithPrimaryUser();

  net::CertificateList certs;
  ImportClientCertAndKey("websocket_client_cert.p12",
                         primary_db_.get(),
                         &certs);

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(IsCertInCertificateList(certs[0], cert_loader_->cert_list()));
}

TEST_F(CertLoaderTest, CertLoaderNoUpdateOnNewClientCertInSecondaryDb) {
  crypto::ScopedTestNSSChromeOSUser secondary_user("secondary");
  scoped_ptr<net::NSSCertDatabaseChromeOS> secondary_db;

  StartCertLoaderWithPrimaryUser();
  FinishUserInitAndGetDatabase(&secondary_user, &secondary_db);

  net::CertificateList certs;
  ImportClientCertAndKey("websocket_client_cert.p12",
                         secondary_db.get(),
                         &certs);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsCertInCertificateList(certs[0], cert_loader_->cert_list()));
}

TEST_F(CertLoaderTest, UpdatedOnCertRemoval) {
  StartCertLoaderWithPrimaryUser();

  net::CertificateList certs;
  ImportClientCertAndKey("websocket_client_cert.p12",
                         primary_db_.get(),
                         &certs);

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  ASSERT_TRUE(IsCertInCertificateList(certs[0], cert_loader_->cert_list()));

  primary_db_->DeleteCertAndKey(certs[0]);

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  ASSERT_FALSE(IsCertInCertificateList(certs[0], cert_loader_->cert_list()));
}

TEST_F(CertLoaderTest, UpdatedOnCACertTrustChange) {
  StartCertLoaderWithPrimaryUser();

  net::CertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_db_.get(), &certs);

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  ASSERT_TRUE(IsCertInCertificateList(certs[0], cert_loader_->cert_list()));

  // The value that should have been set by |ImportCACert|.
  ASSERT_EQ(net::NSSCertDatabase::TRUST_DEFAULT,
            primary_db_->GetCertTrust(certs[0], net::CA_CERT));
  ASSERT_TRUE(primary_db_->SetCertTrust(
      certs[0], net::CA_CERT, net::NSSCertDatabase::TRUSTED_SSL));

  // Cert trust change should trigger certificate reload in cert_loader_.
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
}

}  // namespace
}  // namespace chromeos
