// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "services/network/public/cpp/network_switches.h"

namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

}  // namespace

class PwaInstallViewBrowserTest : public InProcessBrowserTest {
 public:
  PwaInstallViewBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~PwaInstallViewBrowserTest() override {}

  void SetUp() override {
    DCHECK(base::FeatureList::IsEnabled(features::kDesktopPWAWindowing));
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsOmniboxInstall);

    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
    ASSERT_TRUE(https_server_.Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        GetInstallableAppURL().GetOrigin().spec());
  }

  content::WebContents* GetCurrentTab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* OpenNewTab(const GURL& url,
                                   bool expected_installability) {
    chrome::NewTab(browser());
    content::WebContents* web_contents = GetCurrentTab();
    auto* app_banner_manager =
        banners::TestAppBannerManagerDesktop::CreateForWebContents(
            web_contents);
    DCHECK(!app_banner_manager->WaitForInstallableCheck());

    ui_test_utils::NavigateToURL(browser(), url);
    DCHECK_EQ(app_banner_manager->WaitForInstallableCheck(),
              expected_installability);

    return web_contents;
  }

  GURL GetInstallableAppURL() {
    return https_server_.GetURL("/banners/manifest_test_page.html");
  }

  GURL GetNonInstallableAppURL() {
    return https_server_.GetURL("app.com", "/simple.html");
  }

  PageActionIconView* GetPwaInstallView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconContainerView()
        ->GetPageActionIconView(PageActionIconType::kPwaInstall);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(PwaInstallViewBrowserTest);
};

// Tests that the plus icon updates its visibiliy when switching between
// installable/non-installable tabs.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       IconVisibilityAfterTabSwitching) {
  PageActionIconView* pwa_install_view = GetPwaInstallView();
  EXPECT_FALSE(pwa_install_view->visible());

  content::WebContents* installable_web_contents =
      OpenNewTab(GetInstallableAppURL(), true);
  content::WebContents* non_installable_web_contents =
      OpenNewTab(GetNonInstallableAppURL(), false);

  chrome::SelectPreviousTab(browser());
  ASSERT_EQ(installable_web_contents, GetCurrentTab());
  EXPECT_TRUE(pwa_install_view->visible());

  chrome::SelectNextTab(browser());
  ASSERT_EQ(non_installable_web_contents, GetCurrentTab());
  EXPECT_FALSE(pwa_install_view->visible());
}

// Tests that the plus icon updates its visibiliy once the installability check
// completes.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       IconVisibilityAfterInstallabilityCheck) {
  PageActionIconView* pwa_install_view = GetPwaInstallView();
  EXPECT_FALSE(pwa_install_view->visible());

  content::WebContents* web_contents = GetCurrentTab();
  auto* app_banner_manager =
      banners::TestAppBannerManagerDesktop::CreateForWebContents(web_contents);

  ui_test_utils::NavigateToURL(browser(), GetInstallableAppURL());
  EXPECT_FALSE(pwa_install_view->visible());
  ASSERT_TRUE(app_banner_manager->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view->visible());

  ui_test_utils::NavigateToURL(browser(), GetNonInstallableAppURL());
  EXPECT_FALSE(pwa_install_view->visible());
  ASSERT_FALSE(app_banner_manager->WaitForInstallableCheck());
  EXPECT_FALSE(pwa_install_view->visible());
}

// Tests that the plus icon animates its label when the installability check
// passes but doesn't animate more than once for the same installability check.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, LabelAnimation) {
  PageActionIconView* pwa_install_view = GetPwaInstallView();
  EXPECT_FALSE(pwa_install_view->visible());

  content::WebContents* web_contents = GetCurrentTab();
  auto* app_banner_manager =
      banners::TestAppBannerManagerDesktop::CreateForWebContents(web_contents);

  ui_test_utils::NavigateToURL(browser(), GetInstallableAppURL());
  EXPECT_FALSE(pwa_install_view->visible());
  ASSERT_TRUE(app_banner_manager->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view->visible());
  EXPECT_TRUE(pwa_install_view->is_animating_label());

  chrome::NewTab(browser());
  EXPECT_FALSE(pwa_install_view->visible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(pwa_install_view->visible());
  EXPECT_FALSE(pwa_install_view->is_animating_label());
}
