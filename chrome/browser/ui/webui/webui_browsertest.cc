// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class TestWebUIMessageHandler : public content::WebUIMessageHandler {
 public:
  void RegisterMessages() override {}
};

}  // namespace

using WebUIImplBrowserTest = InProcessBrowserTest;

// Tests that navigating between WebUIs of different types results in
// SiteInstance swap when running in process-per-tab process model.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ForceSwapOnDifferenteWebUITypes) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerTab);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const GURL web_ui_url(std::string(content::kChromeUIScheme) + "://" +
                        std::string(chrome::kChromeUIFlagsHost));
  EXPECT_TRUE(ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));
  ui_test_utils::NavigateToURL(browser(), web_ui_url);
  EXPECT_TRUE(
      content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
          web_contents->GetMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating for later comparison.
  scoped_refptr<content::SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());

  // Navigate to a different WebUI type and ensure that the SiteInstance
  // has changed and the new process also has WebUI bindings.
  const GURL web_ui_url2(std::string(content::kChromeUIScheme) + "://" +
                         std::string(chrome::kChromeUIVersionHost));
  EXPECT_TRUE(ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url2));
  ui_test_utils::NavigateToURL(browser(), web_ui_url2);
  EXPECT_NE(orig_site_instance, web_contents->GetSiteInstance());
  EXPECT_TRUE(
      content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
          web_contents->GetMainFrame()->GetProcess()->GetID()));
}

IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, SameDocumentNavigationsAndReload) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUITermsURL));

  content::WebUIMessageHandler* test_handler = new TestWebUIMessageHandler;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetWebUI()
      ->AddMessageHandler(base::WrapUnique(test_handler));
  test_handler->AllowJavascriptForTesting();

  // Push onto window.history. Back should now be an in-page navigation.
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.history.pushState({}, '', 'foo.html')"));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Test handler should still have JavaScript allowed after in-page navigation.
  EXPECT_TRUE(test_handler->IsJavascriptAllowed());

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Verify that after a reload, the test handler has been disallowed.
  EXPECT_FALSE(test_handler->IsJavascriptAllowed());
}

// Tests that navigating to chrome://connection-help displays the proper help
// page.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ConnectionHelpUI) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kBundledConnectionHelpFeature);
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUIConnectionHelpURL));
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  base::string16 tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &tab_title);
  EXPECT_EQ(base::UTF16ToUTF8(tab_title),
            l10n_util::GetStringUTF8(IDS_CONNECTION_HELP_TITLE));
}
