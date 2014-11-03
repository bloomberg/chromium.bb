// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_chromeos.h"

#include <string>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_store_unittest-inl.h"
#include "net/test/cert_test_util.h"

namespace net {

namespace {

class TestCertFilter : public net::ClientCertStoreChromeOS::CertFilter {
 public:
  explicit TestCertFilter(bool init_finished)
      : init_finished_(init_finished), init_called_(false) {}

  ~TestCertFilter() override {}

  bool Init(const base::Closure& callback) override {
    init_called_ = true;
    if (init_finished_)
      return true;
    pending_callback_ = callback;
    return false;
  }

  bool IsCertAllowed(
      const scoped_refptr<net::X509Certificate>& cert) const override {
    if (not_allowed_cert_.get() && cert->Equals(not_allowed_cert_.get()))
      return false;
    return true;
  }

  bool init_called() { return init_called_; }

  void FinishInit() {
    init_finished_ = true;
    base::MessageLoop::current()->PostTask(FROM_HERE, pending_callback_);
    pending_callback_.Reset();
  }

  void SetNotAllowedCert(scoped_refptr<X509Certificate> cert) {
    not_allowed_cert_ = cert;
  }

 private:
  bool init_finished_;
  bool init_called_;
  base::Closure pending_callback_;
  scoped_refptr<X509Certificate> not_allowed_cert_;
};

}  // namespace

// Define a delegate to be used for instantiating the parameterized test set
// ClientCertStoreTest.
class ClientCertStoreChromeOSTestDelegate {
 public:
  ClientCertStoreChromeOSTestDelegate()
      : store_(
            make_scoped_ptr(new TestCertFilter(true /* init synchronously */)),
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
    if (!test_db_.is_open()) {
      LOG(ERROR) << "NSS DB could not be constructed.";
      return false;
    }

    // Only user certs are considered for the cert request, which means that the
    // private key must be known to NSS. Import all private keys for certs that
    // are used througout the test.
    if (!ImportSensitiveKeyFromFile(
            GetTestCertsDirectory(), "client_1.pk8", test_db_.slot()) ||
        !ImportSensitiveKeyFromFile(
            GetTestCertsDirectory(), "client_2.pk8", test_db_.slot())) {
      return false;
    }

    for (CertificateList::const_iterator it = input_certs.begin();
         it != input_certs.end();
         ++it) {
      if (!ImportClientCertToSlot(*it, test_db_.slot()))
        return false;
    }
    base::RunLoop run_loop;
    store_.GetClientCerts(
        cert_request_info, selected_certs, run_loop.QuitClosure());
    run_loop.Run();
    return true;
  }

 private:
  crypto::ScopedTestNSSDB test_db_;
  ClientCertStoreChromeOS store_;
};

// ClientCertStoreChromeOS derives from ClientCertStoreNSS and delegates the
// filtering by issuer to that base class.
// To verify that this delegation is functional, run the same filtering tests as
// for the other implementations. These tests are defined in
// client_cert_store_unittest-inl.h and are instantiated for each platform.
INSTANTIATE_TYPED_TEST_CASE_P(ClientCertStoreTestChromeOS,
                              ClientCertStoreTest,
                              ClientCertStoreChromeOSTestDelegate);

class ClientCertStoreChromeOSTest : public ::testing::Test {
 public:
  scoped_refptr<X509Certificate> ImportCertToSlot(
      const std::string& cert_filename,
      const std::string& key_filename,
      PK11SlotInfo* slot) {
    return ImportClientCertAndKeyFromFile(
        GetTestCertsDirectory(), cert_filename, key_filename, slot);
  }
};

// Ensure that cert requests, that are started before the filter is initialized,
// will wait for the initialization and succeed afterwards.
TEST_F(ClientCertStoreChromeOSTest, RequestWaitsForNSSInitAndSucceeds) {
  crypto::ScopedTestNSSDB test_db;
  ASSERT_TRUE(test_db.is_open());

  TestCertFilter* cert_filter =
      new TestCertFilter(false /* init asynchronously */);
  ClientCertStoreChromeOS store(
      make_scoped_ptr(cert_filter),
      ClientCertStoreChromeOS::PasswordDelegateFactory());

  scoped_refptr<X509Certificate> cert_1(
      ImportCertToSlot("client_1.pem", "client_1.pk8", test_db.slot()));
  ASSERT_TRUE(cert_1.get());

  // Request any client certificate, which is expected to match client_1.
  scoped_refptr<SSLCertRequestInfo> request_all(new SSLCertRequestInfo());

  base::RunLoop run_loop;
  store.GetClientCerts(
      *request_all, &request_all->client_certs, run_loop.QuitClosure());

  {
    base::RunLoop run_loop_inner;
    run_loop_inner.RunUntilIdle();
    // GetClientCerts should wait for the initialization of the filter to
    // finish.
    ASSERT_EQ(0u, request_all->client_certs.size());
    EXPECT_TRUE(cert_filter->init_called());
  }
  cert_filter->FinishInit();
  run_loop.Run();

  ASSERT_EQ(1u, request_all->client_certs.size());
}

// Ensure that cert requests, that are started after the filter was initialized,
// will succeed.
TEST_F(ClientCertStoreChromeOSTest, RequestsAfterNSSInitSucceed) {
  crypto::ScopedTestNSSDB test_db;
  ASSERT_TRUE(test_db.is_open());

  ClientCertStoreChromeOS store(
      make_scoped_ptr(new TestCertFilter(true /* init synchronously */)),
      ClientCertStoreChromeOS::PasswordDelegateFactory());

  scoped_refptr<X509Certificate> cert_1(
      ImportCertToSlot("client_1.pem", "client_1.pk8", test_db.slot()));
  ASSERT_TRUE(cert_1.get());

  scoped_refptr<SSLCertRequestInfo> request_all(new SSLCertRequestInfo());

  base::RunLoop run_loop;
  store.GetClientCerts(
      *request_all, &request_all->client_certs, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_EQ(1u, request_all->client_certs.size());
}

TEST_F(ClientCertStoreChromeOSTest, Filter) {
  crypto::ScopedTestNSSDB test_db;
  ASSERT_TRUE(test_db.is_open());

  TestCertFilter* cert_filter =
      new TestCertFilter(true /* init synchronously */);
  ClientCertStoreChromeOS store(
      make_scoped_ptr(cert_filter),
      ClientCertStoreChromeOS::PasswordDelegateFactory());

  scoped_refptr<X509Certificate> cert_1(
      ImportCertToSlot("client_1.pem", "client_1.pk8", test_db.slot()));
  ASSERT_TRUE(cert_1.get());
  scoped_refptr<X509Certificate> cert_2(
      ImportCertToSlot("client_2.pem", "client_2.pk8", test_db.slot()));
  ASSERT_TRUE(cert_2.get());

  scoped_refptr<SSLCertRequestInfo> request_all(new SSLCertRequestInfo());

  {
    base::RunLoop run_loop;
    cert_filter->SetNotAllowedCert(cert_2);
    CertificateList selected_certs;
    store.GetClientCerts(*request_all, &selected_certs, run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_EQ(1u, selected_certs.size());
    EXPECT_TRUE(cert_1->Equals(selected_certs[0].get()));
  }

  {
    base::RunLoop run_loop;
    cert_filter->SetNotAllowedCert(cert_1);
    CertificateList selected_certs;
    store.GetClientCerts(*request_all, &selected_certs, run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_EQ(1u, selected_certs.size());
    EXPECT_TRUE(cert_2->Equals(selected_certs[0].get()));
  }
}

}  // namespace net
