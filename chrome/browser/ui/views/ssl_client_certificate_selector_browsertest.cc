// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_client_auth_requestor_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/ssl_client_certificate_selector.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/cert_test_util.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/test_data_directory.h"
#include "net/base/x509_certificate.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::StrictMock;
using content::BrowserThread;

// We don't have a way to do end-to-end SSL client auth testing, so this test
// creates a certificate selector_ manually with a mocked
// SSLClientAuthHandler.

class SSLClientCertificateSelectorTest : public InProcessBrowserTest {
 public:
  SSLClientCertificateSelectorTest() : io_loop_finished_event_(false, false) {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    FilePath certs_dir = net::GetTestCertsDirectory();

    mit_davidben_cert_ = net::ImportCertFromFile(certs_dir, "mit.davidben.der");
    ASSERT_NE(static_cast<net::X509Certificate*>(NULL), mit_davidben_cert_);

    foaf_me_chromium_test_cert_ = net::ImportCertFromFile(
        certs_dir, "foaf.me.chromium-test-cert.der");
    ASSERT_NE(static_cast<net::X509Certificate*>(NULL),
              foaf_me_chromium_test_cert_);

    cert_request_info_ = new net::SSLCertRequestInfo;
    cert_request_info_->host_and_port = "foo:123";
    cert_request_info_->client_certs.push_back(mit_davidben_cert_);
    cert_request_info_->client_certs.push_back(foaf_me_chromium_test_cert_);
  }

  virtual void SetUpOnMainThread() {
    url_request_context_getter_ = browser()->profile()->GetRequestContext();

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SSLClientCertificateSelectorTest::SetUpOnIOThread, this));

    io_loop_finished_event_.Wait();

    content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents());
    selector_ = new SSLClientCertificateSelector(
        browser()->tab_strip_model()->GetActiveWebContents(),
        auth_requestor_->http_network_session_,
        auth_requestor_->cert_request_info_,
        base::Bind(&SSLClientAuthRequestorMock::CertificateSelected,
                   auth_requestor_));
    selector_->Init();

    EXPECT_EQ(mit_davidben_cert_.get(), selector_->GetSelectedCert());
  }

  virtual void SetUpOnIOThread() {
    url_request_ = MakeURLRequest(url_request_context_getter_);

    auth_requestor_ = new StrictMock<SSLClientAuthRequestorMock>(
        url_request_,
        cert_request_info_);

    io_loop_finished_event_.Signal();
  }

  // Have to release our reference to the auth handler during the test to allow
  // it to be destroyed while the Browser and its IO thread still exist.
  virtual void CleanUpOnMainThread() {
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
  net::URLRequest* MakeURLRequest(
      net::URLRequestContextGetter* context_getter) {
    net::URLRequest* request =
        context_getter->GetURLRequestContext()->CreateRequest(
            GURL("https://example"), NULL);
    return request;
  }

  base::WaitableEvent io_loop_finished_event_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  net::URLRequest* url_request_;

  scoped_refptr<net::X509Certificate> mit_davidben_cert_;
  scoped_refptr<net::X509Certificate> foaf_me_chromium_test_cert_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  scoped_refptr<StrictMock<SSLClientAuthRequestorMock> > auth_requestor_;
  // The selector will be deleted when a cert is selected or the tab is closed.
  SSLClientCertificateSelector* selector_;
};

class SSLClientCertificateSelectorMultiTabTest
    : public SSLClientCertificateSelectorTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() {
    SSLClientCertificateSelectorTest::SetUpInProcessBrowserTestFixture();

    cert_request_info_1_ = new net::SSLCertRequestInfo;
    cert_request_info_1_->host_and_port = "bar:123";
    cert_request_info_1_->client_certs.push_back(mit_davidben_cert_);
    cert_request_info_1_->client_certs.push_back(foaf_me_chromium_test_cert_);

    cert_request_info_2_ = new net::SSLCertRequestInfo;
    cert_request_info_2_->host_and_port = "bar:123";
    cert_request_info_2_->client_certs.push_back(mit_davidben_cert_);
    cert_request_info_2_->client_certs.push_back(foaf_me_chromium_test_cert_);
  }

  virtual void SetUpOnMainThread() {
    // Also calls SetUpOnIOThread.
    SSLClientCertificateSelectorTest::SetUpOnMainThread();

    AddTabAtIndex(1, GURL("about:blank"), content::PAGE_TRANSITION_LINK);
    AddTabAtIndex(2, GURL("about:blank"), content::PAGE_TRANSITION_LINK);
    ASSERT_TRUE(NULL != browser()->tab_strip_model()->GetWebContentsAt(0));
    ASSERT_TRUE(NULL != browser()->tab_strip_model()->GetWebContentsAt(1));
    ASSERT_TRUE(NULL != browser()->tab_strip_model()->GetWebContentsAt(2));
    content::WaitForLoadStop(browser()->tab_strip_model()->GetWebContentsAt(1));
    content::WaitForLoadStop(browser()->tab_strip_model()->GetWebContentsAt(2));

    selector_1_ = new SSLClientCertificateSelector(
        browser()->tab_strip_model()->GetWebContentsAt(1),
        auth_requestor_1_->http_network_session_,
        auth_requestor_1_->cert_request_info_,
        base::Bind(&SSLClientAuthRequestorMock::CertificateSelected,
                   auth_requestor_1_));
    selector_1_->Init();
    selector_2_ = new SSLClientCertificateSelector(
        browser()->tab_strip_model()->GetWebContentsAt(2),
        auth_requestor_2_->http_network_session_,
        auth_requestor_2_->cert_request_info_,
        base::Bind(&SSLClientAuthRequestorMock::CertificateSelected,
                   auth_requestor_2_));
    selector_2_->Init();

    EXPECT_EQ(2, browser()->tab_strip_model()->active_index());
    EXPECT_EQ(mit_davidben_cert_.get(), selector_1_->GetSelectedCert());
    EXPECT_EQ(mit_davidben_cert_.get(), selector_2_->GetSelectedCert());
  }

  virtual void SetUpOnIOThread() {
    url_request_1_ = MakeURLRequest(url_request_context_getter_);
    url_request_2_ = MakeURLRequest(url_request_context_getter_);

    auth_requestor_1_ = new StrictMock<SSLClientAuthRequestorMock>(
        url_request_1_,
        cert_request_info_1_);
    auth_requestor_2_ = new StrictMock<SSLClientAuthRequestorMock>(
        url_request_2_,
        cert_request_info_2_);

    SSLClientCertificateSelectorTest::SetUpOnIOThread();
  }

  virtual void CleanUpOnMainThread() {
    auth_requestor_2_ = NULL;
    auth_requestor_1_ = NULL;
    SSLClientCertificateSelectorTest::CleanUpOnMainThread();
  }

  virtual void CleanUpOnIOThread() {
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
  virtual void SetUpInProcessBrowserTestFixture() {
    SSLClientCertificateSelectorTest::SetUpInProcessBrowserTestFixture();

    cert_request_info_1_ = new net::SSLCertRequestInfo;
    cert_request_info_1_->host_and_port = "foo:123";
    cert_request_info_1_->client_certs.push_back(mit_davidben_cert_);
    cert_request_info_1_->client_certs.push_back(foaf_me_chromium_test_cert_);
  }

  virtual void SetUpOnMainThread() {
    browser_1_ = CreateIncognitoBrowser();
    url_request_context_getter_1_ = browser_1_->profile()->GetRequestContext();

    // Also calls SetUpOnIOThread.
    SSLClientCertificateSelectorTest::SetUpOnMainThread();

    selector_1_ = new SSLClientCertificateSelector(
        browser_1_->tab_strip_model()->GetActiveWebContents(),
        auth_requestor_1_->http_network_session_,
        auth_requestor_1_->cert_request_info_,
        base::Bind(&SSLClientAuthRequestorMock::CertificateSelected,
                   auth_requestor_1_));
    selector_1_->Init();

    EXPECT_EQ(mit_davidben_cert_.get(), selector_1_->GetSelectedCert());
  }

  virtual void SetUpOnIOThread() {
    url_request_1_ = MakeURLRequest(url_request_context_getter_1_);

    auth_requestor_1_ = new StrictMock<SSLClientAuthRequestorMock>(
        url_request_1_,
        cert_request_info_1_);

    SSLClientCertificateSelectorTest::SetUpOnIOThread();
  }

  virtual void CleanUpOnMainThread() {
    auth_requestor_1_ = NULL;
    SSLClientCertificateSelectorTest::CleanUpOnMainThread();
  }

  virtual void CleanUpOnIOThread() {
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
  EXPECT_CALL(*auth_requestor_, CertificateSelected(NULL));

  // Let the mock get checked on destruction.
}

// http://crbug.com/121007
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorTest, DISABLED_Escape) {
  EXPECT_CALL(*auth_requestor_, CertificateSelected(NULL));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
}

// Flaky, http://crbug.com/103534 .
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorTest,
                       DISABLED_SelectDefault) {
  EXPECT_CALL(*auth_requestor_, CertificateSelected(mit_davidben_cert_.get()));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
}

// http://crbug.com/121007
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiTabTest,
                       DISABLED_Escape) {
  // auth_requestor_1_ should get selected automatically by the
  // SSLClientAuthObserver when selector_2_ is accepted, since both 1 & 2 have
  // the same host:port.
  EXPECT_CALL(*auth_requestor_1_, CertificateSelected(NULL));
  EXPECT_CALL(*auth_requestor_2_, CertificateSelected(NULL));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());
  Mock::VerifyAndClear(auth_requestor_2_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_, CertificateSelected(NULL));
}

// http://crbug.com/121007
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiTabTest,
                       DISABLED_SelectSecond) {
  // auth_requestor_1_ should get selected automatically by the
  // SSLClientAuthObserver when selector_2_ is accepted, since both 1 & 2 have
  // the same host:port.
  EXPECT_CALL(*auth_requestor_1_,
              CertificateSelected(foaf_me_chromium_test_cert_.get()));
  EXPECT_CALL(*auth_requestor_2_,
              CertificateSelected(foaf_me_chromium_test_cert_.get()));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_DOWN, false, false, false, false));

  EXPECT_EQ(mit_davidben_cert_.get(), selector_->GetSelectedCert());
  EXPECT_EQ(mit_davidben_cert_.get(), selector_1_->GetSelectedCert());
  EXPECT_EQ(foaf_me_chromium_test_cert_.get(), selector_2_->GetSelectedCert());

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());
  Mock::VerifyAndClear(auth_requestor_2_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_, CertificateSelected(NULL));
}

// http://crbug.com/103529
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiProfileTest,
                       DISABLED_Escape) {
  EXPECT_CALL(*auth_requestor_1_, CertificateSelected(NULL));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser_1_, ui::VKEY_ESCAPE, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_, CertificateSelected(NULL));
}

// http://crbug.com/103534
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiProfileTest,
                       DISABLED_SelectDefault) {
  EXPECT_CALL(*auth_requestor_1_,
              CertificateSelected(mit_davidben_cert_.get()));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser_1_, ui::VKEY_RETURN, false, false, false, false));

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_, CertificateSelected(NULL));
}
