// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cert_loader.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
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

class TestNSSCertDatabase : public net::NSSCertDatabaseChromeOS {
 public:
  TestNSSCertDatabase(crypto::ScopedPK11Slot public_slot,
                      crypto::ScopedPK11Slot private_slot)
      : NSSCertDatabaseChromeOS(std::move(public_slot),
                                std::move(private_slot)) {}
  ~TestNSSCertDatabase() override {}

  void NotifyOfCertAdded(const net::X509Certificate* cert) {
    NSSCertDatabaseChromeOS::NotifyObserversCertDBChanged();
  }
};

class CertLoaderTest : public testing::Test,
                       public CertLoader::Observer {
 public:
  CertLoaderTest()
      : cert_loader_(nullptr), certificates_loaded_events_count_(0U) {}

  ~CertLoaderTest() override {}

  void SetUp() override {
    ASSERT_TRUE(primary_db_.is_open());

    CertLoader::Initialize();
    cert_loader_ = CertLoader::Get();
    cert_loader_->AddObserver(this);
  }

  void TearDown() override {
    cert_loader_->RemoveObserver(this);
    CertLoader::Shutdown();
  }

 protected:
  void StartCertLoaderWithPrimaryDB() {
    CreateCertDatabase(&primary_db_, &primary_certdb_);
    cert_loader_->StartWithNSSDB(primary_certdb_.get());

    base::RunLoop().RunUntilIdle();
    GetAndResetCertificatesLoadedEventsCount();
  }

  // CertLoader::Observer:
  // The test keeps count of times the observer method was called.
  void OnCertificatesLoaded(const net::CertificateList& cert_list,
                            bool initial_load) override {
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

  void CreateCertDatabase(crypto::ScopedTestNSSDB* db,
                          std::unique_ptr<TestNSSCertDatabase>* certdb) {
    ASSERT_TRUE(db->is_open());

    certdb->reset(new TestNSSCertDatabase(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(db->slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(db->slot()))));
    (*certdb)->SetSlowTaskRunnerForTest(message_loop_.task_runner());
  }

  void ImportCACert(const std::string& cert_file,
                    net::NSSCertDatabase* database,
                    net::CertificateList* imported_certs) {
    ASSERT_TRUE(database);
    ASSERT_TRUE(imported_certs);

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

  scoped_refptr<net::X509Certificate> ImportClientCertAndKey(
      TestNSSCertDatabase* database) {
    // Import a client cert signed by that CA.
    scoped_refptr<net::X509Certificate> client_cert(
        net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                            "client_1.pem", "client_1.pk8",
                                            database->GetPrivateSlot().get()));
    database->NotifyOfCertAdded(client_cert.get());
    return client_cert;
  }

  CertLoader* cert_loader_;

  // The user is primary as the one whose certificates CertLoader handles, it
  // has nothing to do with crypto::InitializeNSSForChromeOSUser is_primary_user
  // parameter (which is irrelevant for these tests).
  crypto::ScopedTestNSSDB primary_db_;
  std::unique_ptr<TestNSSCertDatabase> primary_certdb_;

  base::MessageLoop message_loop_;

 private:
  size_t certificates_loaded_events_count_;
};

}  // namespace

TEST_F(CertLoaderTest, Basic) {
  EXPECT_FALSE(cert_loader_->CertificatesLoading());
  EXPECT_FALSE(cert_loader_->certificates_loaded());

  CreateCertDatabase(&primary_db_, &primary_certdb_);
  cert_loader_->StartWithNSSDB(primary_certdb_.get());

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
  StartCertLoaderWithPrimaryDB();

  net::CertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_certdb_.get(), &certs);

  // Certs are loaded asynchronously, so the new cert should not yet be in the
  // cert list.
  EXPECT_FALSE(
      IsCertInCertificateList(certs[0].get(), cert_loader_->cert_list()));

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  // The certificate list should be updated now, as the message loop's been run.
  EXPECT_TRUE(
      IsCertInCertificateList(certs[0].get(), cert_loader_->cert_list()));

  EXPECT_FALSE(cert_loader_->IsCertificateHardwareBacked(certs[0].get()));
}

TEST_F(CertLoaderTest, CertLoaderNoUpdateOnSecondaryDbChanges) {
  crypto::ScopedTestNSSDB secondary_db;
  std::unique_ptr<TestNSSCertDatabase> secondary_certdb;

  StartCertLoaderWithPrimaryDB();
  CreateCertDatabase(&secondary_db, &secondary_certdb);

  net::CertificateList certs;
  ImportCACert("root_ca_cert.pem", secondary_certdb.get(), &certs);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      IsCertInCertificateList(certs[0].get(), cert_loader_->cert_list()));
}

TEST_F(CertLoaderTest, ClientLoaderUpdateOnNewClientCert) {
  StartCertLoaderWithPrimaryDB();

  scoped_refptr<net::X509Certificate> cert(
      ImportClientCertAndKey(primary_certdb_.get()));

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  EXPECT_TRUE(IsCertInCertificateList(cert.get(), cert_loader_->cert_list()));
}

TEST_F(CertLoaderTest, CertLoaderNoUpdateOnNewClientCertInSecondaryDb) {
  crypto::ScopedTestNSSDB secondary_db;
  std::unique_ptr<TestNSSCertDatabase> secondary_certdb;

  StartCertLoaderWithPrimaryDB();
  CreateCertDatabase(&secondary_db, &secondary_certdb);

  scoped_refptr<net::X509Certificate> cert(
      ImportClientCertAndKey(secondary_certdb.get()));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsCertInCertificateList(cert.get(), cert_loader_->cert_list()));
}

TEST_F(CertLoaderTest, UpdatedOnCertRemoval) {
  StartCertLoaderWithPrimaryDB();

  scoped_refptr<net::X509Certificate> cert(
      ImportClientCertAndKey(primary_certdb_.get()));

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  ASSERT_TRUE(IsCertInCertificateList(cert.get(), cert_loader_->cert_list()));

  primary_certdb_->DeleteCertAndKey(cert.get());

  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());

  ASSERT_FALSE(IsCertInCertificateList(cert.get(), cert_loader_->cert_list()));
}

TEST_F(CertLoaderTest, UpdatedOnCACertTrustChange) {
  StartCertLoaderWithPrimaryDB();

  net::CertificateList certs;
  ImportCACert("root_ca_cert.pem", primary_certdb_.get(), &certs);

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
  ASSERT_TRUE(
      IsCertInCertificateList(certs[0].get(), cert_loader_->cert_list()));

  // The value that should have been set by |ImportCACert|.
  ASSERT_EQ(net::NSSCertDatabase::TRUST_DEFAULT,
            primary_certdb_->GetCertTrust(certs[0].get(), net::CA_CERT));
  ASSERT_TRUE(primary_certdb_->SetCertTrust(certs[0].get(), net::CA_CERT,
                                            net::NSSCertDatabase::TRUSTED_SSL));

  // Cert trust change should trigger certificate reload in cert_loader_.
  ASSERT_EQ(0U, GetAndResetCertificatesLoadedEventsCount());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetCertificatesLoadedEventsCount());
}

}  // namespace chromeos
