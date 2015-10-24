// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"

using WebUIBrowserTest = InProcessBrowserTest;

// Tests that navigating between WebUIs of different types results in
// SiteInstance swap when running in process-per-tab process model.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, ForceSwapOnDifferenteWebUITypes) {
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
          web_contents->GetRenderProcessHost()->GetID()));

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
          web_contents->GetRenderProcessHost()->GetID()));
}
