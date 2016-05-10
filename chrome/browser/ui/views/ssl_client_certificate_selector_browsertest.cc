// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_client_auth_requestor_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/ssl_client_certificate_selector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/request_priority.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/test/cert_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS_CERTS)
#include "crypto/scoped_test_nss_db.h"
#endif

using ::testing::Mock;
using ::testing::StrictMock;
using content::BrowserThread;

// We don't have a way to do end-to-end SSL client auth testing, so this test
// creates a certificate selector_ manually with a mocked
// SSLClientAuthHandler.

class SSLClientCertificateSelectorTest : public InProcessBrowserTest {
 public:
  SSLClientCertificateSelectorTest()
      : io_loop_finished_event_(false, false),
        url_request_(NULL),
        selector_(NULL) {
  }

  void SetUpInProcessBrowserTestFixture() override {
    base::FilePath certs_dir = net::GetTestCertsDirectory();

#if defined(USE_NSS_CERTS)
    // If USE_NSS_CERTS, the selector tries to unlock the slot where the
    // private key of each certificate is stored. If no private key is found,
    // the slot would be null and the unlock will crash.
    ASSERT_TRUE(test_nssdb_.is_open());
    client_cert_1_ = net::ImportClientCertAndKeyFromFile(
        certs_dir, "client_1.pem", "client_1.pk8", test_nssdb_.slot());
    client_cert_2_ = net::ImportClientCertAndKeyFromFile(
        certs_dir, "client_2.pem", "client_2.pk8", test_nssdb_.slot());
#else
    // No unlock is attempted if !USE_NSS_CERTS. Thus, there is no need to
    // import a private key.
    client_cert_1_ = net::ImportCertFromFile(certs_dir, "client_1.pem");
    client_cert_2_ = net::ImportCertFromFile(certs_dir, "client_2.pem");
#endif
    ASSERT_NE(nullptr, client_cert_1_.get());
    ASSERT_NE(nullptr, client_cert_2_.get());

    cert_request_info_ = new net::SSLCertRequestInfo;
    cert_request_info_->host_and_port = net::HostPortPair("foo", 123);
    cert_request_info_->client_certs.push_back(client_cert_1_);
    cert_request_info_->client_certs.push_back(client_cert_2_);
  }

  void SetUpOnMainThread() override {
    url_request_context_getter_ = browser()->profile()->GetRequestContext();

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SSLClientCertificateSelectorTest::SetUpOnIOThread, this));

    io_loop_finished_event_.Wait();

    content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents());
    selector_ = new SSLClientCertificateSelector(
        browser()->tab_strip_model()->GetActiveWebContents(),
        auth_requestor_->cert_request_info_, auth_requestor_->CreateDelegate());
    selector_->Init();
    selector_->Show();

    EXPECT_EQ(client_cert_1_.get(), selector_->GetSelectedCert());
  }

  virtual void SetUpOnIOThread() {
    url_request_ = MakeURLRequest(url_request_context_getter_.get()).release();

    auth_requestor_ = new StrictMock<SSLClientAuthRequestorMock>(
        url_request_,
        cert_request_info_);

    io_loop_finished_event_.Signal();
  }

  // Have to release our reference to the auth handler during the test to allow
  // it to be destroyed while the Browser and its IO thread still exist.
  void TearDownOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SSLClientCertificateSelectorTest::CleanUpOnIOThread, this));

    io_loop_finished_event_.Wait();

    auth_requestor_ = NULL;
  }

  virtual void CleanUpOnIOThread() {
    delete url_request_;

    io_loop_finished_event_.Signal();
  }

 protected:
  std::unique_ptr<net::URLRequest> MakeURLRequest(
      net::URLRequestContextGetter* context_getter) {
    return context_getter->GetURLRequestContext()->CreateRequest(
        GURL("https://example"), net::DEFAULT_PRIORITY, NULL);
  }

  base::WaitableEvent io_loop_finished_event_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  net::URLRequest* url_request_;

  scoped_refptr<net::X509Certificate> client_cert_1_;
  scoped_refptr<net::X509Certificate> client_cert_2_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  scoped_refptr<StrictMock<SSLClientAuthRequestorMock> > auth_requestor_;
  // The selector will be deleted when a cert is selected or the tab is closed.
  SSLClientCertificateSelector* selector_;
#if defined(USE_NSS_CERTS)
  crypto::ScopedTestNSSDB test_nssdb_;
#endif
};

class SSLClientCertificateSelectorMultiTabTest
    : public SSLClientCertificateSelectorTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    SSLClientCertificateSelectorTest::SetUpInProcessBrowserTestFixture();

    cert_request_info_1_ = new net::SSLCertRequestInfo;
    cert_request_info_1_->host_and_port = net::HostPortPair("bar", 123);
    cert_request_info_1_->client_certs.push_back(client_cert_1_);
    cert_request_info_1_->client_certs.push_back(client_cert_2_);

    cert_request_info_2_ = new net::SSLCertRequestInfo;
    cert_request_info_2_->host_and_port = net::HostPortPair("bar", 123);
    cert_request_info_2_->client_certs.push_back(client_cert_1_);
    cert_request_info_2_->client_certs.push_back(client_cert_2_);
  }

  void SetUpOnMainThread() override {
    // Also calls SetUpOnIOThread.
    SSLClientCertificateSelectorTest::SetUpOnMainThread();

    AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
    AddTabAtIndex(2, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
    ASSERT_TRUE(NULL != browser()->tab_strip_model()->GetWebContentsAt(0));
    ASSERT_TRUE(NULL != browser()->tab_strip_model()->GetWebContentsAt(1));
    ASSERT_TRUE(NULL != browser()->tab_strip_model()->GetWebContentsAt(2));
    content::WaitForLoadStop(browser()->tab_strip_model()->GetWebContentsAt(1));
    content::WaitForLoadStop(browser()->tab_strip_model()->GetWebContentsAt(2));

    selector_1_ = new SSLClientCertificateSelector(
        browser()->tab_strip_model()->GetWebContentsAt(1),
        auth_requestor_1_->cert_request_info_,
        auth_requestor_1_->CreateDelegate());
    selector_1_->Init();
    selector_1_->Show();
    selector_2_ = new SSLClientCertificateSelector(
        browser()->tab_strip_model()->GetWebContentsAt(2),
        auth_requestor_2_->cert_request_info_,
        auth_requestor_2_->CreateDelegate());
    selector_2_->Init();
    selector_2_->Show();

    EXPECT_EQ(2, browser()->tab_strip_model()->active_index());
    EXPECT_EQ(client_cert_1_.get(), selector_1_->GetSelectedCert());
    EXPECT_EQ(client_cert_1_.get(), selector_2_->GetSelectedCert());
  }

  void SetUpOnIOThread() override {
    url_request_1_ =
        MakeURLRequest(url_request_context_getter_.get()).release();
    url_request_2_ =
        MakeURLRequest(url_request_context_getter_.get()).release();

    auth_requestor_1_ = new StrictMock<SSLClientAuthRequestorMock>(
        url_request_1_,
        cert_request_info_1_);
    auth_requestor_2_ = new StrictMock<SSLClientAuthRequestorMock>(
        url_request_2_,
        cert_request_info_2_);

    SSLClientCertificateSelectorTest::SetUpOnIOThread();
  }

  void TearDownOnMainThread() override {
    auth_requestor_2_ = NULL;
    auth_requestor_1_ = NULL;
    SSLClientCertificateSelectorTest::TearDownOnMainThread();
  }

  void CleanUpOnIOThread() override {
    delete url_request_1_;
    delete url_request_2_;
    SSLClientCertificateSelectorTest::CleanUpOnIOThread();
  }

 protected:
  net::URLRequest* url_request_1_;
  net::URLRequest* url_request_2_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_1_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_2_;
  scoped_refptr<StrictMock<SSLClientAuthRequestorMock> > auth_requestor_1_;
  scoped_refptr<StrictMock<SSLClientAuthRequestorMock> > auth_requestor_2_;
  SSLClientCertificateSelector* selector_1_;
  SSLClientCertificateSelector* selector_2_;
};

class SSLClientCertificateSelectorMultiProfileTest
    : public SSLClientCertificateSelectorTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    SSLClientCertificateSelectorTest::SetUpInProcessBrowserTestFixture();

    cert_request_info_1_ = new net::SSLCertRequestInfo;
    cert_request_info_1_->host_and_port = net::HostPortPair("foo", 123);
    cert_request_info_1_->client_certs.push_back(client_cert_1_);
    cert_request_info_1_->client_certs.push_back(client_cert_2_);
  }

  void SetUpOnMainThread() override {
    browser_1_ = CreateIncognitoBrowser();
    url_request_context_getter_1_ = browser_1_->profile()->GetRequestContext();

    // Also calls SetUpOnIOThread.
    SSLClientCertificateSelectorTest::SetUpOnMainThread();

    selector_1_ = new SSLClientCertificateSelector(
        browser_1_->tab_strip_model()->GetActiveWebContents(),
        auth_requestor_1_->cert_request_info_,
        auth_requestor_1_->CreateDelegate());
    selector_1_->Init();
    selector_1_->Show();

    EXPECT_EQ(client_cert_1_.get(), selector_1_->GetSelectedCert());
  }

  void SetUpOnIOThread() override {
    url_request_1_ =
        MakeURLRequest(url_request_context_getter_1_.get()).release();

    auth_requestor_1_ = new StrictMock<SSLClientAuthRequestorMock>(
        url_request_1_,
        cert_request_info_1_);

    SSLClientCertificateSelectorTest::SetUpOnIOThread();
  }

  void TearDownOnMainThread() override {
    auth_requestor_1_ = NULL;
    SSLClientCertificateSelectorTest::TearDownOnMainThread();
  }

  void CleanUpOnIOThread() override {
    delete url_request_1_;
    SSLClientCertificateSelectorTest::CleanUpOnIOThread();
  }

 protected:
  Browser* browser_1_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_1_;
  net::URLRequest* url_request_1_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_1_;
  scoped_refptr<StrictMock<SSLClientAuthRequestorMock> > auth_requestor_1_;
  SSLClientCertificateSelector* selector_1_;
};

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorTest, SelectNone) {
  EXPECT_CALL(*auth_requestor_.get(), CancelCertificateSelection());

  // Let the mock get checked on destruction.
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorTest, Escape) {
  EXPECT_CALL(*auth_requestor_.get(), CertificateSelected(NULL));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorTest, SelectDefault) {
  EXPECT_CALL(*auth_requestor_.get(),
              CertificateSelected(client_cert_1_.get()));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiTabTest, Escape) {
  // auth_requestor_1_ should get selected automatically by the
  // SSLClientAuthObserver when selector_2_ is accepted, since both 1 & 2 have
  // the same host:port.
  EXPECT_CALL(*auth_requestor_1_.get(), CertificateSelected(NULL));
  EXPECT_CALL(*auth_requestor_2_.get(), CertificateSelected(NULL));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());
  Mock::VerifyAndClear(auth_requestor_2_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_.get(), CancelCertificateSelection());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiTabTest, SelectSecond) {
  // auth_requestor_1_ should get selected automatically by the
  // SSLClientAuthObserver when selector_2_ is accepted, since both 1 & 2 have
  // the same host:port.
  EXPECT_CALL(*auth_requestor_1_.get(),
              CertificateSelected(client_cert_2_.get()));
  EXPECT_CALL(*auth_requestor_2_.get(),
              CertificateSelected(client_cert_2_.get()));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_DOWN, false, false, false, false));

  EXPECT_EQ(client_cert_1_.get(), selector_->GetSelectedCert());
  EXPECT_EQ(client_cert_1_.get(), selector_1_->GetSelectedCert());
  EXPECT_EQ(client_cert_2_.get(), selector_2_->GetSelectedCert());

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());
  Mock::VerifyAndClear(auth_requestor_2_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_.get(), CancelCertificateSelection());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiProfileTest, Escape) {
  EXPECT_CALL(*auth_requestor_1_.get(), CertificateSelected(NULL));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser_1_, ui::VKEY_ESCAPE, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_.get(), CancelCertificateSelection());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiProfileTest,
                       SelectDefault) {
  EXPECT_CALL(*auth_requestor_1_.get(),
              CertificateSelected(client_cert_1_.get()));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser_1_, ui::VKEY_RETURN, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_.get(), CancelCertificateSelection());
}
