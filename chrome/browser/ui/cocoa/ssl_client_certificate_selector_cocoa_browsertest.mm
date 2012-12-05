// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/ssl_client_certificate_selector_cocoa.h"

#import <SecurityInterface/SFChooseIdentityPanel.h>

#include "base/bind.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector_test.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

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
  NSWindow* parent_window = web_contents->GetView()->GetTopLevelNativeWindow();
  EXPECT_FALSE([parent_window attachedSheet]);

  net::X509Certificate* cert = NULL;
  int count = 0;
  SSLClientCertificateSelectorCocoa* selector =
      [[[SSLClientCertificateSelectorCocoa alloc]
          initWithNetworkSession:auth_requestor_->http_network_session_
                 certRequestInfo:auth_requestor_->cert_request_info_
                        callback:base::Bind(&OnCertificateSelected,
                                            &cert,
                                            &count)]
      autorelease];
  [selector displayForWebContents:web_contents];
  EXPECT_TRUE([selector panel]);
  EXPECT_TRUE([parent_window attachedSheet]);

  [selector onNotification];
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_FALSE([parent_window attachedSheet]);

  EXPECT_EQ(NULL, cert);
  EXPECT_EQ(1, count);
}
