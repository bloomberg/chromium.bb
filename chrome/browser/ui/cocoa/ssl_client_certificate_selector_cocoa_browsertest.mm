// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/ssl_client_certificate_selector_cocoa.h"

#import <SecurityInterface/SFChooseIdentityPanel.h>

#include "base/bind.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector_test.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/test_utils.h"

namespace {

void OnCertificateSelected(net::X509Certificate** out_cert,
                           int* out_count,
                           net::X509Certificate* cert) {
  *out_cert = cert;
  ++(*out_count);
}

}  // namespace

typedef SSLClientCertificateSelectorTestBase
    SSLClientCertificateSelectorCocoaTest;

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorCocoaTest, Basic) {
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  ConstrainedWindowTabHelper* constrained_window_tab_helper =
      ConstrainedWindowTabHelper::FromWebContents(web_contents);
  EXPECT_EQ(0u, constrained_window_tab_helper->constrained_window_count());

  net::X509Certificate* cert = NULL;
  int count = 0;
  SSLClientCertificateSelectorCocoa* selector =
      [[SSLClientCertificateSelectorCocoa alloc]
          initWithNetworkSession:auth_requestor_->http_network_session_
                 certRequestInfo:auth_requestor_->cert_request_info_
                        callback:base::Bind(&OnCertificateSelected,
                                            &cert,
                                            &count)];
  [selector displayForWebContents:web_contents];
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE([selector panel]);
  EXPECT_EQ(1u, constrained_window_tab_helper->constrained_window_count());

  constrained_window_tab_helper->CloseConstrainedWindows();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, constrained_window_tab_helper->constrained_window_count());

  EXPECT_EQ(NULL, cert);
  EXPECT_EQ(1, count);
}
