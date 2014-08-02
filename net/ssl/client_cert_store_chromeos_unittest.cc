// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/run_loop.h"
#include "crypto/nss_util_internal.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_store_unittest-inl.h"
#include "net/test/cert_test_util.h"

namespace net {

namespace {

enum ReadFromSlot {
  READ_FROM_SLOT_USER,
  READ_FROM_SLOT_SYSTEM
};

enum SystemSlotAvailability {
  SYSTEM_SLOT_AVAILABILITY_ENABLED,
  SYSTEM_SLOT_AVAILABILITY_DISABLED
};

}  // namespace

// Define a delegate to be used for instantiating the parameterized test set
// ClientCertStoreTest.
template <ReadFromSlot read_from,
          SystemSlotAvailability system_slot_availability>
class ClientCertStoreChromeOSTestDelegate {
 public:
  ClientCertStoreChromeOSTestDelegate()
      : user_("scopeduser"),
        store_(system_slot_availability == SYSTEM_SLOT_AVAILABILITY_ENABLED,
               user_.username_hash(),
               ClientCertStoreChromeOS::PasswordDelegateFactory()) {
    // Defer futher initialization and checks to SelectClientCerts, because the
    // constructor doesn't allow us to return an initialization result. Could be
    // cleaned up by adding an Init() function.
  }

  // Called by the ClientCertStoreTest tests.
  // |inpurt_certs| contains certificates to select from. Because
  // ClientCertStoreChromeOS filters also for the right slot, we have to import
  // the certs at first.
  // Since the certs are imported, the store can be tested by using its public
  // interface (GetClientCerts), which will read the certs from NSS.
  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         CertificateList* selected_certs) {
    if (!user_.constructed_successfully()) {
      LOG(ERROR) << "Scoped test user DB could not be constructed.";
      return false;
    }
    user_.FinishInit();

    crypto::ScopedPK11Slot slot;
    switch (read_from) {
      case READ_FROM_SLOT_USER:
        slot = crypto::GetPublicSlotForChromeOSUser(user_.username_hash());
        break;
      case READ_FROM_SLOT_SYSTEM:
        slot.reset(PK11_ReferenceSlot(system_db_.slot()));
        break;
      default:
        CHECK(false);
    }
    if (!slot) {
      LOG(ERROR) << "Could not get the NSS key slot";
      return false;
    }

    // Only user certs are considered for the cert request, which means that the
    // private key must be known to NSS. Import all private keys for certs that
    // are used througout the test.
    if (!ImportSensitiveKeyFromFile(
            GetTestCertsDirectory(), "client_1.pk8", slot.get()) ||
        !ImportSensitiveKeyFromFile(
            GetTestCertsDirectory(), "client_2.pk8", slot.get())) {
      return false;
    }

    for (CertificateList::const_iterator it = input_certs.begin();
         it != input_certs.end();
         ++it) {
      if (!ImportClientCertToSlot(*it, slot.get()))
        return false;
    }
    base::RunLoop run_loop;
    store_.GetClientCerts(
        cert_request_info, selected_certs, run_loop.QuitClosure());
    run_loop.Run();
    return true;
  }

 private:
  crypto::ScopedTestNSSChromeOSUser user_;
  crypto::ScopedTestSystemNSSKeySlot system_db_;
  ClientCertStoreChromeOS store_;
};

// ClientCertStoreChromeOS derives from ClientCertStoreNSS and delegates the
// filtering by issuer to that base class.
// To verify that this delegation is functional, run the same filtering tests as
// for the other implementations. These tests are defined in
// client_cert_store_unittest-inl.h and are instantiated for each platform.

// In this case, all requested certs are read from the user's slot and the
// system slot is not enabled in the store.
typedef ClientCertStoreChromeOSTestDelegate<READ_FROM_SLOT_USER,
                                            SYSTEM_SLOT_AVAILABILITY_DISABLED>
    DelegateReadUserDisableSystem;
INSTANTIATE_TYPED_TEST_CASE_P(ChromeOS_ReadUserDisableSystem,
                              ClientCertStoreTest,
                              DelegateReadUserDisableSystem);

// In this case, all requested certs are read from the user's slot and the
// system slot is enabled in the store.
typedef ClientCertStoreChromeOSTestDelegate<READ_FROM_SLOT_USER,
                                            SYSTEM_SLOT_AVAILABILITY_ENABLED>
    DelegateReadUserEnableSystem;
INSTANTIATE_TYPED_TEST_CASE_P(ChromeOS_ReadUserEnableSystem,
                              ClientCertStoreTest,
                              DelegateReadUserEnableSystem);

// In this case, all requested certs are read from the system slot, therefore
// the system slot is enabled in the store.
typedef ClientCertStoreChromeOSTestDelegate<READ_FROM_SLOT_SYSTEM,
                                            SYSTEM_SLOT_AVAILABILITY_ENABLED>
    DelegateReadSystem;
INSTANTIATE_TYPED_TEST_CASE_P(ChromeOS_ReadSystem,
                              ClientCertStoreTest,
                              DelegateReadSystem);

class ClientCertStoreChromeOSTest : public ::testing::Test {
 public:
  scoped_refptr<X509Certificate> ImportCertForUser(
      const std::string& username_hash,
      const std::string& cert_filename,
      const std::string& key_filename) {
    crypto::ScopedPK11Slot slot(
        crypto::GetPublicSlotForChromeOSUser(username_hash));
    if (!slot) {
      LOG(ERROR) << "No slot for user " << username_hash;
      return NULL;
    }

    return ImportClientCertAndKeyFromFile(
        GetTestCertsDirectory(), cert_filename, key_filename, slot.get());
  }

};

// Ensure that cert requests, that are started before the user's NSS DB is
// initialized, will wait for the initialization and succeed afterwards.
TEST_F(ClientCertStoreChromeOSTest, RequestWaitsForNSSInitAndSucceeds) {
  crypto::ScopedTestNSSChromeOSUser user("scopeduser");
  ASSERT_TRUE(user.constructed_successfully());

  crypto::ScopedTestSystemNSSKeySlot system_slot;

  ClientCertStoreChromeOS store(
      true /* use system slot */,
      user.username_hash(),
      ClientCertStoreChromeOS::PasswordDelegateFactory());
  scoped_refptr<X509Certificate> cert_1(
      ImportCertForUser(user.username_hash(), "client_1.pem", "client_1.pk8"));
  ASSERT_TRUE(cert_1);

  // Request any client certificate, which is expected to match client_1.
  scoped_refptr<SSLCertRequestInfo> request_all(new SSLCertRequestInfo());

  base::RunLoop run_loop;
  store.GetClientCerts(
      *request_all, &request_all->client_certs, run_loop.QuitClosure());

  {
    base::RunLoop run_loop_inner;
    run_loop_inner.RunUntilIdle();
    // GetClientCerts should wait for the initialization of the user's DB to
    // finish.
    ASSERT_EQ(0u, request_all->client_certs.size());
  }
  // This should trigger the GetClientCerts operation to finish and to call
  // back.
  user.FinishInit();

  run_loop.Run();

  ASSERT_EQ(1u, request_all->client_certs.size());
}

// Ensure that cert requests, that are started after the user's NSS DB was
// initialized, will succeed.
TEST_F(ClientCertStoreChromeOSTest, RequestsAfterNSSInitSucceed) {
  crypto::ScopedTestNSSChromeOSUser user("scopeduser");
  ASSERT_TRUE(user.constructed_successfully());
  user.FinishInit();

  crypto::ScopedTestSystemNSSKeySlot system_slot;

  ClientCertStoreChromeOS store(
      true /* use system slot */,
      user.username_hash(),
      ClientCertStoreChromeOS::PasswordDelegateFactory());
  scoped_refptr<X509Certificate> cert_1(
      ImportCertForUser(user.username_hash(), "client_1.pem", "client_1.pk8"));
  ASSERT_TRUE(cert_1);

  scoped_refptr<SSLCertRequestInfo> request_all(new SSLCertRequestInfo());

  base::RunLoop run_loop;
  store.GetClientCerts(
      *request_all, &request_all->client_certs, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_EQ(1u, request_all->client_certs.size());
}

// This verifies that a request in the context of User1 doesn't see certificates
// of User2, and the other way round. We check both directions, to ensure that
// the behavior doesn't depend on initialization order of the DBs, for example.
TEST_F(ClientCertStoreChromeOSTest, RequestDoesCrossReadOtherUserDB) {
  crypto::ScopedTestNSSChromeOSUser user1("scopeduser1");
  ASSERT_TRUE(user1.constructed_successfully());
  crypto::ScopedTestNSSChromeOSUser user2("scopeduser2");
  ASSERT_TRUE(user2.constructed_successfully());

  user1.FinishInit();
  user2.FinishInit();

  crypto::ScopedTestSystemNSSKeySlot system_slot;

  ClientCertStoreChromeOS store1(
      true /* use system slot */,
      user1.username_hash(),
      ClientCertStoreChromeOS::PasswordDelegateFactory());
  ClientCertStoreChromeOS store2(
      true /* use system slot */,
      user2.username_hash(),
      ClientCertStoreChromeOS::PasswordDelegateFactory());

  scoped_refptr<X509Certificate> cert_1(
      ImportCertForUser(user1.username_hash(), "client_1.pem", "client_1.pk8"));
  ASSERT_TRUE(cert_1);
  scoped_refptr<X509Certificate> cert_2(
      ImportCertForUser(user2.username_hash(), "client_2.pem", "client_2.pk8"));
  ASSERT_TRUE(cert_2);

  scoped_refptr<SSLCertRequestInfo> request_all(new SSLCertRequestInfo());

  base::RunLoop run_loop_1;
  base::RunLoop run_loop_2;

  CertificateList selected_certs1, selected_certs2;
  store1.GetClientCerts(
      *request_all, &selected_certs1, run_loop_1.QuitClosure());
  store2.GetClientCerts(
      *request_all, &selected_certs2, run_loop_2.QuitClosure());

  run_loop_1.Run();
  run_loop_2.Run();

  // store1 should only return certs of user1, namely cert_1.
  ASSERT_EQ(1u, selected_certs1.size());
  EXPECT_TRUE(cert_1->Equals(selected_certs1[0]));

  // store2 should only return certs of user2, namely cert_2.
  ASSERT_EQ(1u, selected_certs2.size());
  EXPECT_TRUE(cert_2->Equals(selected_certs2[0]));
}

// This verifies that a request in the context of User1 doesn't see certificates
// of the system store if the system store is disabled.
TEST_F(ClientCertStoreChromeOSTest, RequestDoesCrossReadSystemDB) {
  crypto::ScopedTestNSSChromeOSUser user1("scopeduser1");
  ASSERT_TRUE(user1.constructed_successfully());

  user1.FinishInit();

  crypto::ScopedTestSystemNSSKeySlot system_slot;

  ClientCertStoreChromeOS store(
      false /* do not use system slot */,
      user1.username_hash(),
      ClientCertStoreChromeOS::PasswordDelegateFactory());

  scoped_refptr<X509Certificate> cert_1(
      ImportCertForUser(user1.username_hash(), "client_1.pem", "client_1.pk8"));
  ASSERT_TRUE(cert_1);
  scoped_refptr<X509Certificate> cert_2(
      ImportClientCertAndKeyFromFile(GetTestCertsDirectory(),
                                     "client_2.pem",
                                     "client_2.pk8",
                                     system_slot.slot()));
  ASSERT_TRUE(cert_2);

  scoped_refptr<SSLCertRequestInfo> request_all(new SSLCertRequestInfo());

  base::RunLoop run_loop;

  CertificateList selected_certs;
  store.GetClientCerts(*request_all, &selected_certs, run_loop.QuitClosure());

  run_loop.Run();

  // store should only return certs of the user, namely cert_1.
  ASSERT_EQ(1u, selected_certs.size());
  EXPECT_TRUE(cert_1->Equals(selected_certs[0]));
}

}  // namespace net
