// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/test_html_dialog_observer.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"
#include "net/base/test_certificate_data.h"
#include "net/base/x509_certificate.h"

// Test framework for chrome/test/data/webui/certificate_viewer_dialog_test.js.
class CertificateViewerUITest : public WebUIBrowserTest {
 public:
  CertificateViewerUITest();
  virtual ~CertificateViewerUITest();

 protected:
  void ShowCertificateViewer();
};

void CertificateViewerUITest::ShowCertificateViewer() {
  // Enable more WebUI to use WebUI certificate viewer.
  ChromeWebUI::OverrideMoreWebUI(true);

  scoped_refptr<net::X509Certificate> google_cert(
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  ASSERT_TRUE(browser());
  ASSERT_TRUE(browser()->window());

  TestHtmlDialogObserver dialog_observer(this);
  ::ShowCertificateViewer(browser()->window()->GetNativeHandle(), google_cert);
  WebUI* webui = dialog_observer.GetWebUI();
  webui->tab_contents()->render_view_host()->SetWebUIProperty(
      "expectedUrl", chrome::kChromeUICertificateViewerURL);
  SetWebUIInstance(webui);
  WebUIBrowserTest::SetUpOnMainThread();
}
