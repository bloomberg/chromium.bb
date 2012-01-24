// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ssl_client_certificate_selector_webui_browsertest.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/ssl_client_certificate_selector_webui.h"
#include "chrome/test/base/test_html_dialog_observer.h"
#include "content/browser/ssl/ssl_client_auth_handler_mock.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

SSLClientCertificateSelectorUITest::SSLClientCertificateSelectorUITest() :
    io_loop_finished_event_(false, false), url_request_(NULL) {}

SSLClientCertificateSelectorUITest::~SSLClientCertificateSelectorUITest() {}

void SSLClientCertificateSelectorUITest::ShowSSLClientCertificateSelector() {
  cert_request_info_ = new net::SSLCertRequestInfo;

  url_request_context_getter_ = browser()->profile()->GetRequestContext();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SSLClientCertificateSelectorUITest::SetUpOnIOThread, this));
  io_loop_finished_event_.Wait();

  // The TestHtmlDialogObserver will catch our dialog when it gets created.
  TestHtmlDialogObserver dialog_observer(this);

  // Show the SSL client certificate selector.
  SSLClientCertificateSelectorWebUI::ShowDialog(
    browser()->GetSelectedTabContentsWrapper(),
    cert_request_info_,
    auth_handler_
  );

  EXPECT_CALL(*auth_handler_, CertificateSelectedNoNotify(::testing::_));

  // Tell the test which WebUI instance we are dealing with and complete
  // initialization of this test.
  content::WebUI* webui = dialog_observer.GetWebUI();
  SetWebUIInstance(webui);
}

void SSLClientCertificateSelectorUITest::CleanUpOnMainThread() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SSLClientCertificateSelectorUITest::CleanUpOnIOThread,
                 this));
  io_loop_finished_event_.Wait();
  auth_handler_ = NULL;
  WebUIBrowserTest::CleanUpOnMainThread();
}

void SSLClientCertificateSelectorUITest::SetUpOnIOThread() {
  url_request_ = new net::URLRequest(GURL("https://example"), NULL);
  url_request_->set_context(url_request_context_getter_->
      GetURLRequestContext());
  auth_handler_ = new testing::StrictMock<SSLClientAuthHandlerMock>(
      url_request_,
      cert_request_info_);
  io_loop_finished_event_.Signal();
}

void SSLClientCertificateSelectorUITest::CleanUpOnIOThread() {
  delete url_request_;
  url_request_ = NULL;
  io_loop_finished_event_.Signal();
}

