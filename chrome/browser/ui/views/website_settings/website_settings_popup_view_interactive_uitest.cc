// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"

namespace {

typedef InProcessBrowserTest WebSiteSettingsPopupViewBrowserTest;

// Clicks the location icon to open the page info bubble.
void ClickAndWait(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  ui_test_utils::MoveMouseToCenterAndPress(
      location_icon_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, runner->QuitClosure());
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(WebSiteSettingsPopupViewBrowserTest, ShowPopup) {
  ClickAndWait(browser());
  EXPECT_EQ(WebsiteSettingsPopupView::POPUP_WEBSITE_SETTINGS,
            WebsiteSettingsPopupView::GetShownPopupType());
}

IN_PROC_BROWSER_TEST_F(WebSiteSettingsPopupViewBrowserTest, ChromeURL) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings"));
  ClickAndWait(browser());
  EXPECT_EQ(WebsiteSettingsPopupView::POPUP_INTERNAL_PAGE,
            WebsiteSettingsPopupView::GetShownPopupType());
}

IN_PROC_BROWSER_TEST_F(WebSiteSettingsPopupViewBrowserTest,
                       ChromeExtensionURL) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-extension://extension-id/options.html"));
  ClickAndWait(browser());
  EXPECT_EQ(WebsiteSettingsPopupView::POPUP_INTERNAL_PAGE,
            WebsiteSettingsPopupView::GetShownPopupType());
}

IN_PROC_BROWSER_TEST_F(WebSiteSettingsPopupViewBrowserTest, ChromeDevtoolsURL) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-devtools://devtools/bundled/inspector.html"));
  ClickAndWait(browser());
  EXPECT_EQ(WebsiteSettingsPopupView::POPUP_INTERNAL_PAGE,
            WebsiteSettingsPopupView::GetShownPopupType());
}

IN_PROC_BROWSER_TEST_F(WebSiteSettingsPopupViewBrowserTest, ViewSourceURL) {
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  chrome::ViewSelectedSource(browser());
  ClickAndWait(browser());
  EXPECT_EQ(WebsiteSettingsPopupView::POPUP_INTERNAL_PAGE,
            WebsiteSettingsPopupView::GetShownPopupType());
}

}  // namespace
