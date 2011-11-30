// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/hung_renderer_dialog.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_html_dialog_observer.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"

// Test framework for chrome/test/data/webui/hung_renderer_dialog_test.js.
class HungRendererDialogUITest : public WebUIBrowserTest {
 public:
  HungRendererDialogUITest();
  virtual ~HungRendererDialogUITest();

 protected:
  void ShowHungRendererDialogInternal();
};

void HungRendererDialogUITest::ShowHungRendererDialogInternal() {
  // Force the flag so that we will use the WebUI version of the Dialog.
  ChromeWebUI::OverrideMoreWebUI(true);

  // Choose which tab contents to report as hung.  In this case, the default
  // tab contents will be about:blank.
  ASSERT_TRUE(browser());
  TabContents* tab_contents = browser()->GetSelectedTabContents();

  // The TestHtmlDialogObserver will catch our dialog when it gets created.
  TestHtmlDialogObserver dialog_observer(this);

  // Show a disabled Hung Renderer Dialog that won't kill processes or restart
  // hang timers.
  HungRendererDialog::ShowHungRendererDialogInternal(tab_contents, false);

  // Now we can get the WebUI object from the observer, and make some details
  // about our test available to the JavaScript.
  WebUI* webui = dialog_observer.GetWebUI();
  webui->tab_contents()->render_view_host()->SetWebUIProperty(
      "expectedUrl", chrome::kChromeUIHungRendererDialogURL);
  webui->tab_contents()->render_view_host()->SetWebUIProperty(
      "expectedTitle", "about:blank");

  // Tell the test which WebUI instance we are dealing with and complete
  // initialization of this test.
  SetWebUIInstance(webui);
}
