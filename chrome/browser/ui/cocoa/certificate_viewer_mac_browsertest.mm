// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/certificate_viewer.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/cocoa/certificate_viewer_mac.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "ui/base/cocoa/window_size_constants.h"

using web_modal::WebContentsModalDialogManager;

typedef InProcessBrowserTest SSLCertificateViewerCocoaTest;

namespace {
scoped_refptr<net::X509Certificate> GetSampleCertificate() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 "mit.davidben.der");
}
} // namespace

IN_PROC_BROWSER_TEST_F(SSLCertificateViewerCocoaTest, Basic) {
  scoped_refptr<net::X509Certificate> cert = GetSampleCertificate();
  ASSERT_TRUE(cert.get());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  gfx::NativeWindow window = web_contents->GetTopLevelNativeWindow();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  ShowCertificateViewer(web_contents, window, cert.get());

  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  WebContentsModalDialogManager::TestApi test_api(
      web_contents_modal_dialog_manager);
  test_api.CloseAllDialogs();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());
}

// Test that switching to another tab correctly hides the sheet.
IN_PROC_BROWSER_TEST_F(SSLCertificateViewerCocoaTest, HideShow) {
  scoped_refptr<net::X509Certificate> cert = GetSampleCertificate();
  ASSERT_TRUE(cert.get());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  SSLCertificateViewerCocoa* viewer =
      [[SSLCertificateViewerCocoa alloc] initWithCertificate:cert.get()];
  [viewer displayForWebContents:web_contents];

  content::RunAllPendingInMessageLoop();

  NSWindow* sheetWindow = [[viewer overlayWindow] attachedSheet];
  NSRect sheetFrame = [sheetWindow frame];
  EXPECT_EQ(1.0, [sheetWindow alphaValue]);

  // Switch to another tab and verify that the sheet is hidden.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(0.0, [sheetWindow alphaValue]);
  EXPECT_TRUE(
      NSEqualRects(ui::kWindowSizeDeterminedLater, [sheetWindow frame]));

  // Switch back and verify that the sheet is shown.
  chrome::SelectNumberedTab(browser(), 0);
  EXPECT_EQ(1.0, [sheetWindow alphaValue]);
  EXPECT_TRUE(NSEqualRects(sheetFrame, [sheetWindow frame]));
}
