// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/ssl_client_certificate_selector_cocoa.h"

#import <SecurityInterface/SFChooseIdentityPanel.h>

#include "base/bind.h"
#import "base/mac/mac_util.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "ui/base/cocoa/window_size_constants.h"

using web_modal::WebContentsModalDialogManager;

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

// Flaky on 10.7; crbug.com/313243
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorCocoaTest, DISABLED_Basic) {
  // TODO(kbr): re-enable: http://crbug.com/222296
  if (base::mac::IsOSMountainLionOrLater())
    return;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

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
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  WebContentsModalDialogManager::TestApi test_api(
      web_contents_modal_dialog_manager);
  test_api.CloseAllDialogs();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  EXPECT_EQ(NULL, cert);
  EXPECT_EQ(1, count);
}

// Test that switching to another tab correctly hides the sheet.
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorCocoaTest, HideShow) {
  SSLClientCertificateSelectorCocoa* selector =
      [[SSLClientCertificateSelectorCocoa alloc]
          initWithNetworkSession:auth_requestor_->http_network_session_
                 certRequestInfo:auth_requestor_->cert_request_info_
                        callback:chrome::SelectCertificateCallback()];
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  [selector displayForWebContents:web_contents];
  content::RunAllPendingInMessageLoop();

  NSWindow* sheetWindow = [[selector overlayWindow] attachedSheet];
  NSRect sheetFrame = [sheetWindow frame];
  EXPECT_EQ(1.0, [sheetWindow alphaValue]);

  // Switch to another tab and verify that the sheet is hidden.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(0.0, [sheetWindow alphaValue]);
  EXPECT_TRUE(NSEqualRects(ui::kWindowSizeDeterminedLater,
                           [sheetWindow frame]));

  // Switch back and verify that the sheet is shown.
  chrome::SelectNumberedTab(browser(), 0);
  EXPECT_EQ(1.0, [sheetWindow alphaValue]);
  EXPECT_TRUE(NSEqualRects(sheetFrame, [sheetWindow frame]));
}
