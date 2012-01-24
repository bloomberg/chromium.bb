// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SSL_CLIENT_CERTIFICATE_SELECTOR_WEBUI_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_SSL_CLIENT_CERTIFICATE_SELECTOR_WEBUI_BROWSERTEST_H_
#pragma once

#include "base/synchronization/waitable_event.h"
#include "chrome/browser/ui/webui/ssl_client_certificate_selector_webui.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"

namespace net {
class URLRequest;
class URLRequestContextGetter;
class SSLCertRequestInfo;
}

class SSLClientAuthHandlerMock;

namespace testing {
template<class T> class StrictMock;
}

// Test framework for the WebUI based edit search engine dialog.
class SSLClientCertificateSelectorUITest : public WebUIBrowserTest {
 protected:
  SSLClientCertificateSelectorUITest();
  virtual ~SSLClientCertificateSelectorUITest();

  // Displays the SSL client certificate selector for testing.
  void ShowSSLClientCertificateSelector();

  // Releases resources used by the test harness for the SSL client
  // certificate.
  virtual void CleanUpOnMainThread() OVERRIDE;

  // Initializes the URL request on the IO thread.
  void SetUpOnIOThread();

  // Releases the URL request on the IO thread.
  void CleanUpOnIOThread();

 private:
  base::WaitableEvent io_loop_finished_event_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  net::URLRequest* url_request_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  scoped_refptr<testing::StrictMock<SSLClientAuthHandlerMock> >
      auth_handler_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientCertificateSelectorUITest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SSL_CLIENT_CERTIFICATE_SELECTOR_WEBUI_BROWSERTEST_H_
