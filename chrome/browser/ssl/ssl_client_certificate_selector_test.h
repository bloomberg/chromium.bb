// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_CERTIFICATE_SELECTOR_TEST_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_CERTIFICATE_SELECTOR_TEST_H_

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/ssl/ssl_client_auth_requestor_mock.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
class URLRequestContextGetter;
}

class SSLClientCertificateSelectorTestBase : public InProcessBrowserTest {
 public:
  SSLClientCertificateSelectorTestBase();
  virtual ~SSLClientCertificateSelectorTestBase();

  // InProcessBrowserTest:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

  virtual void SetUpOnIOThread();
  virtual void CleanUpOnIOThread();

 protected:
  scoped_ptr<net::URLRequest> MakeURLRequest(
      net::URLRequestContextGetter* context_getter);

  base::WaitableEvent io_loop_finished_event_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  net::URLRequest* url_request_;

  scoped_refptr<net::X509Certificate> mit_davidben_cert_;
  scoped_refptr<net::X509Certificate> foaf_me_chromium_test_cert_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  scoped_refptr<testing::StrictMock<SSLClientAuthRequestorMock> >
      auth_requestor_;
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_CERTIFICATE_SELECTOR_TEST_H_
