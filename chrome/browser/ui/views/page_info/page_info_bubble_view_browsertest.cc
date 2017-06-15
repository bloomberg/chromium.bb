// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/accessibility/ax_action_data.h"

namespace {

typedef InProcessBrowserTest PageInfoBubbleViewBrowserTest;

void PerformMouseClickOnView(views::View* view) {
  ui::AXActionData data;
  data.action = ui::AX_ACTION_DO_DEFAULT;
  view->HandleAccessibleAction(data);
}

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  PerformMouseClickOnView(location_icon_view);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubble();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Opens the Page Info bubble and retrieves the "Site settings" button.
views::View* GetSiteSettingsButton(Browser* browser) {
  OpenPageInfoBubble(browser);
  views::Widget* page_info_bubble =
      PageInfoBubbleView::GetPageInfoBubble()->GetWidget();
  EXPECT_TRUE(page_info_bubble);

  // Retrieve the "Site settings" button.
  views::View* site_settings_button =
      page_info_bubble->GetRootView()->GetViewByID(
          VIEW_ID_PAGE_INFO_LINK_SITE_SETTINGS);
  EXPECT_TRUE(site_settings_button);
  return site_settings_button;
}

// Clicks the "Site settings" button from Page Info and waits for a "Settings"
// tab to open.
void ClickAndWaitForSettingsPageToOpen(views::View* site_settings_button) {
  content::WebContentsAddedObserver new_tab_observer;
  PerformMouseClickOnView(site_settings_button);

  base::string16 expected_title(base::ASCIIToUTF16("Settings"));
  content::TitleWatcher title_watcher(new_tab_observer.GetWebContents(),
                                      expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Returns the URL of the new tab that's opened on clicking the "Site settings"
// button from Page Info.
const GURL OpenSiteSettingsForUrl(Browser* browser,
                                  const GURL& url,
                                  bool enable_site_details) {
  ui_test_utils::NavigateToURL(browser, url);
  views::View* site_settings_button = GetSiteSettingsButton(browser);
  base::test::ScopedFeatureList feature_list;
  if (enable_site_details)
    feature_list.InitAndEnableFeature(features::kSiteDetails);
  else
    feature_list.InitAndDisableFeature(features::kSiteDetails);
  ClickAndWaitForSettingsPageToOpen(site_settings_button);

  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetLastCommittedURL();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ShowBubble) {
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ChromeURL) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings"));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ChromeExtensionURL) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-extension://extension-id/options.html"));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

// Times out due to isolation, see crbug.com/733767
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       DISABLED_ChromeDevtoolsURL) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-devtools://devtools/bundled/inspector.html"));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ViewSourceURL) {
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  chrome::ViewSelectedSource(browser());
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

// Test opening "Content Settings" via Page Info from an ASCII origin works.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, SiteSettingsLink) {
  GURL url = GURL("https://www.google.com/");
  EXPECT_EQ(GURL(chrome::kChromeUIContentSettingsURL),
            OpenSiteSettingsForUrl(browser(), url, false));
}

// Test opening "Site Details" via Page Info from an ASCII origin does the
// correct URL canonicalization.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       SiteSettingsLinkWithSiteDetailsEnabled) {
  GURL url = GURL("https://www.google.com/");
  std::string expected_origin = "https%3A%2F%2Fwww.google.com";
  EXPECT_EQ(GURL(chrome::kChromeUISiteDetailsPrefixURL + expected_origin),
            OpenSiteSettingsForUrl(browser(), url, true));
}

// Test opening "Site Details" via Page Info from a non-ASCII URL converts it to
// an origin and does punycode conversion as well as URL canonicalization.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       SiteSettingsLinkWithSiteDetailsEnabledAndNonAsciiUrl) {
  GURL url = GURL("http://🥄.ws/other/stuff.htm");
  std::string expected_origin = "http%3A%2F%2Fxn--9q9h.ws";
  EXPECT_EQ(GURL(chrome::kChromeUISiteDetailsPrefixURL + expected_origin),
            OpenSiteSettingsForUrl(browser(), url, true));
}

// Test opening "Site Details" via Page Info from an origin with a non-default
// (scheme, port) pair will specify port # in the origin passed to query params.
IN_PROC_BROWSER_TEST_F(
    PageInfoBubbleViewBrowserTest,
    SiteSettingsLinkWithSiteDetailsEnabledAndNonDefaultPort) {
  GURL url = GURL("https://www.example.com:8372");
  std::string expected_origin = "https%3A%2F%2Fwww.example.com%3A8372";
  EXPECT_EQ(GURL(chrome::kChromeUISiteDetailsPrefixURL + expected_origin),
            OpenSiteSettingsForUrl(browser(), url, true));
}

// Test opening "Site Details" via Page Info from about:blank goes to "Content
// Settings" (the alternative is a blank origin being sent to "Site Details").
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       SiteSettingsLinkAboutBlankWithSiteDetailsEnabled) {
  EXPECT_EQ(GURL(chrome::kChromeUIContentSettingsURL),
            OpenSiteSettingsForUrl(browser(), GURL(url::kAboutBlankURL), true));
}

}  // namespace
