// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace {

const GURL kExampleUrl = GURL("https://www.example.com/");

}  // namespace

class SiteEngagementUiBrowserTest : public InProcessBrowserTest {
 protected:
  // Returns the SiteEngagementService for the test browser profile.
  SiteEngagementService* engagement_service() {
    return SiteEngagementService::Get(browser()->profile());
  }

  // (Re)sets the base score for a URL's site engagement.
  void ResetBaseScore(const GURL& url, double score) {
    engagement_service()->ResetBaseScoreForURL(url, score);
  }
  void ResetBaseScoreToMax(const GURL& url) {
    ResetBaseScore(url, engagement_service()->GetMaxPoints());
  }

  // Navigates the tab to the site engagement WebUI.
  void NavigateToWebUi() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    const GURL web_ui_url(base::JoinString(
        {content::kChromeUIScheme, "://", chrome::kChromeUISiteEngagementHost},
        ""));
    EXPECT_TRUE(ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
        web_contents->GetBrowserContext(), web_ui_url));
    ui_test_utils::NavigateToURL(browser(), web_ui_url);
    EXPECT_TRUE(
        content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
            web_contents->GetRenderProcessHost()->GetID()));

    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  }

  // Waits for the tab to be populated with site engagement data.
  void WaitUntilPagePopulated() {
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.whenPageIsPopulatedForTest().then(() => {"
        "  window.domAutomationController.send(true);"
        "});",
        &page_is_populated_));

    ASSERT_TRUE(page_is_populated_);
  }

  // Verifies that a row exists for the specified site URL.
  void ExpectPageContainsUrl(const GURL& url) {
    ASSERT_TRUE(page_is_populated_);

    bool found_url = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::JoinString(
            {"var origin_cells = "
             "    Array.from(document.getElementsByClassName('origin-cell'));"
             "window.domAutomationController.send(origin_cells.reduce("
             "    (found, element) => {"
             "      return found || (element.innerHTML == '",
             url.spec(),
             "');"
             "    }, false));"},
            ""),
        &found_url));
    EXPECT_TRUE(found_url);
  }

 private:
  // True if the page contains site engagement data.
  bool page_is_populated_ = false;
};

IN_PROC_BROWSER_TEST_F(SiteEngagementUiBrowserTest, Basic) {
  ResetBaseScoreToMax(kExampleUrl);

  NavigateToWebUi();
  WaitUntilPagePopulated();

  ExpectPageContainsUrl(kExampleUrl);
}
