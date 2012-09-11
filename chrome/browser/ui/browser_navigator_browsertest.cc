// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_browsertest.h"

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ipc/ipc_message.h"

using content::WebContents;

namespace {

GURL GetGoogleURL() {
  return GURL("http://www.google.com/");
}

GURL GetSettingsURL() {
  return GURL(chrome::kChromeUIUberURL).Resolve(
      chrome::kChromeUISettingsHost + std::string("/"));
}

GURL GetContentSettingsURL() {
  return GetSettingsURL().Resolve(chrome::kContentSettingsExceptionsSubPage);
}

GURL GetClearBrowsingDataURL() {
  return GetSettingsURL().Resolve(chrome::kClearBrowserDataSubPage);
}

} // namespace

chrome::NavigateParams BrowserNavigatorTest::MakeNavigateParams() const {
  return MakeNavigateParams(browser());
}

chrome::NavigateParams BrowserNavigatorTest::MakeNavigateParams(
    Browser* browser) const {
  chrome::NavigateParams params(browser, GetGoogleURL(),
                                content::PAGE_TRANSITION_LINK);
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  return params;
}

Browser* BrowserNavigatorTest::CreateEmptyBrowserForType(Browser::Type type,
                                                         Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(type, profile));
  chrome::AddBlankTab(browser, true);
  return browser;
}

Browser* BrowserNavigatorTest::CreateEmptyBrowserForApp(Browser::Type type,
                                                        Profile* profile) {
  Browser* browser = new Browser(
      Browser::CreateParams::CreateForApp(
          Browser::TYPE_POPUP, "Test", gfx::Rect(), profile));
  chrome::AddBlankTab(browser, true);
  return browser;
}

TabContents* BrowserNavigatorTest::CreateTabContents() {
  return chrome::TabContentsFactory(
      browser()->profile(),
      NULL,
      MSG_ROUTING_NONE,
      chrome::GetActiveWebContents(browser()));
}

void BrowserNavigatorTest::RunSuppressTest(WindowOpenDisposition disposition) {
  GURL old_url = chrome::GetActiveWebContents(browser())->GetURL();
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = disposition;
  chrome::Navigate(&p);

  // Nothing should have happened as a result of Navigate();
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(old_url, chrome::GetActiveWebContents(browser())->GetURL());
}

void BrowserNavigatorTest::RunUseNonIncognitoWindowTest(const GURL& url) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, incognito_browser->tab_count());

  // Navigate to the page.
  chrome::NavigateParams p(MakeNavigateParams(incognito_browser));
  p.disposition = SINGLETON_TAB;
  p.url = url;
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&p);

  // This page should be opened in browser() window.
  EXPECT_NE(incognito_browser, p.browser);
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(url, chrome::GetActiveWebContents(browser())->GetURL());
}

void BrowserNavigatorTest::RunDoNothingIfIncognitoIsForcedTest(
    const GURL& url) {
  Browser* browser = CreateIncognitoBrowser();

  // Set kIncognitoModeAvailability to FORCED.
  PrefService* prefs1 = browser->profile()->GetPrefs();
  prefs1->SetInteger(prefs::kIncognitoModeAvailability,
                     IncognitoModePrefs::FORCED);
  PrefService* prefs2 = browser->profile()->GetOriginalProfile()->GetPrefs();
  prefs2->SetInteger(prefs::kIncognitoModeAvailability,
                     IncognitoModePrefs::FORCED);

  // Navigate to the page.
  chrome::NavigateParams p(MakeNavigateParams(browser));
  p.disposition = OFF_THE_RECORD;
  p.url = url;
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&p);

  // The page should not be opened.
  EXPECT_EQ(browser, p.browser);
  EXPECT_EQ(1, browser->tab_count());
  EXPECT_EQ(GURL(chrome::kAboutBlankURL),
            chrome::GetActiveWebContents(browser)->GetURL());
}

void BrowserNavigatorTest::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED: {
      ++this->created_tab_contents_count_;
      break;
    }
    default:
      break;
  }
}


namespace {

// This test verifies that when a navigation occurs within a tab, the tab count
// of the Browser remains the same and the current tab bears the loaded URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_CurrentTab) {
  ui_test_utils::NavigateToURL(browser(), GetGoogleURL());
  EXPECT_EQ(GetGoogleURL(), chrome::GetActiveWebContents(browser())->GetURL());
  // We should have one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
}

// This test verifies that a singleton tab is refocused if one is already opened
// in another or an existing window, or added if it is not.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SingletonTabExisting) {
  GURL singleton_url1("http://maps.google.com/");

  // Register for a notification if an additional tab_contents was instantiated.
  // Opening a Singleton tab that is already opened should not be opening a new
  // tab nor be creating a new TabContents object.
  content::NotificationRegistrar registrar;

  // As the registrar object goes out of scope, this will get unregistered
  registrar.Add(this,
                content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                content::NotificationService::AllSources());

  chrome::AddSelectedTabWithURL(browser(), singleton_url1,
                                content::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                content::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->active_index());

  unsigned int previous_tab_contents_count =
      created_tab_contents_count_ = 0;

  // Navigate to singleton_url1.
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url1;
  chrome::Navigate(&p);

  // The middle tab should now be selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(1, browser()->active_index());

  // No tab contents should have been created
  EXPECT_EQ(previous_tab_contents_count,
            created_tab_contents_count_);
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabRespectingRef) {
  GURL singleton_ref_url1("http://maps.google.com/#a");
  GURL singleton_ref_url2("http://maps.google.com/#b");
  GURL singleton_ref_url3("http://maps.google.com/");

  chrome::AddSelectedTabWithURL(browser(), singleton_ref_url1,
                                content::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Navigate to singleton_url2.
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_ref_url2;
  chrome::Navigate(&p);

  // We should now have 2 tabs, the 2nd one selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Navigate to singleton_url2, but with respect ref set.
  p = MakeNavigateParams();
  p.disposition = SINGLETON_TAB;
  p.url = singleton_ref_url2;
  p.ref_behavior = chrome::NavigateParams::RESPECT_REF;
  chrome::Navigate(&p);

  // We should now have 3 tabs, the 3th one selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());

  // Navigate to singleton_url3.
  p = MakeNavigateParams();
  p.disposition = SINGLETON_TAB;
  p.url = singleton_ref_url3;
  p.ref_behavior = chrome::NavigateParams::RESPECT_REF;
  chrome::Navigate(&p);

  // We should now have 4 tabs, the 4th one selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(4, browser()->tab_count());
  EXPECT_EQ(3, browser()->active_index());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabNoneExisting) {
  GURL singleton_url1("http://maps.google.com/");

  // We should have one browser with 1 tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->active_index());

  // Navigate to singleton_url1.
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url1;
  chrome::Navigate(&p);

  // We should now have 2 tabs, the 2nd one selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
}

// This test verifies that when a navigation results in a foreground tab, the
// tab count of the Browser increases and the selected tab shifts to the new
// foreground tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewForegroundTab) {
  WebContents* old_contents = chrome::GetActiveWebContents(browser());
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&p);
  EXPECT_NE(old_contents, chrome::GetActiveWebContents(browser()));
  EXPECT_EQ(chrome::GetActiveTabContents(browser()), p.target_contents);
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that when a navigation results in a background tab, the
// tab count of the Browser increases but the selected tab remains the same.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewBackgroundTab) {
  WebContents* old_contents = chrome::GetActiveWebContents(browser());
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_BACKGROUND_TAB;
  chrome::Navigate(&p);
  WebContents* new_contents = chrome::GetActiveWebContents(browser());
  // The selected tab should have remained unchanged, since the new tab was
  // opened in the background.
  EXPECT_EQ(old_contents, new_contents);
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that when a navigation requiring a new foreground tab
// occurs in a Browser that cannot host multiple tabs, the new foreground tab
// is created in an existing compatible Browser.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_IncompatibleWindow_Existing) {
  // Open a foreground tab in a window that cannot open popups when there is an
  // existing compatible window somewhere else that they can be opened within.
  Browser* popup = CreateEmptyBrowserForType(Browser::TYPE_POPUP,
                                             browser()->profile());
  chrome::NavigateParams p(MakeNavigateParams(popup));
  p.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&p);

  // Navigate() should have opened the tab in a different browser since the
  // one we supplied didn't support additional tabs.
  EXPECT_NE(popup, p.browser);

  // Since browser() is an existing compatible tabbed browser, it should have
  // opened the tab there.
  EXPECT_EQ(browser(), p.browser);

  // We should be left with 2 windows, the popup with one tab and the browser()
  // provided by the framework with two.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, popup->tab_count());
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that when a navigation requiring a new foreground tab
// occurs in a Browser that cannot host multiple tabs and no compatible Browser
// that can is open, a compatible Browser is created.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_IncompatibleWindow_NoExisting) {
  // We want to simulate not being able to find an existing window compatible
  // with our non-tabbed browser window so Navigate() is forced to create a
  // new compatible window. Because browser() supplied by the in-process
  // browser testing framework is compatible with browser()->profile(), we
  // need a different profile, and creating a popup window with an incognito
  // profile is a quick and dirty way of achieving this.
  Browser* popup = CreateEmptyBrowserForType(
      Browser::TYPE_POPUP,
      browser()->profile()->GetOffTheRecordProfile());
  chrome::NavigateParams p(MakeNavigateParams(popup));
  p.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&p);

  // Navigate() should have opened the tab in a different browser since the
  // one we supplied didn't support additional tabs.
  EXPECT_NE(popup, p.browser);

  // This time, browser() is _not_ compatible with popup since it is not an
  // incognito window.
  EXPECT_NE(browser(), p.browser);

  // We should have three windows, each with one tab:
  // 1. the browser() provided by the framework (unchanged in this test)
  // 2. the incognito popup we created originally
  // 3. the new incognito tabbed browser that was created by Navigate().
  EXPECT_EQ(3u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, popup->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
  EXPECT_TRUE(p.browser->is_type_tabbed());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal Browser results in a new Browser with TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopup) {
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_POPUP;
  p.window_bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&p);

  // Navigate() should have opened a new, focused popup window.
  EXPECT_NE(browser(), p.browser);
#if 0
  // TODO(stevenjb): Enable this test. See: crbug.com/79493
  EXPECT_TRUE(p.browser->window()->IsActive());
#endif
  EXPECT_TRUE(p.browser->is_type_popup());
  EXPECT_FALSE(p.browser->is_app());

  // We should have two windows, the browser() provided by the framework and the
  // new popup window.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal Browser results in a new Browser with is_app() true.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopup_ExtensionId) {
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_POPUP;
  p.extension_app_id = "extensionappid";
  p.window_bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&p);

  // Navigate() should have opened a new, focused popup window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_TRUE(p.browser->is_type_popup());
  EXPECT_TRUE(p.browser->is_app());

  // We should have two windows, the browser() provided by the framework and the
  // new popup window.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal popup results in a new Browser with TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupFromPopup) {
  // Open a popup.
  chrome::NavigateParams p1(MakeNavigateParams());
  p1.disposition = NEW_POPUP;
  p1.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&p1);
  // Open another popup.
  chrome::NavigateParams p2(MakeNavigateParams(p1.browser));
  p2.disposition = NEW_POPUP;
  p2.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&p2);

  // Navigate() should have opened a new normal popup window.
  EXPECT_NE(p1.browser, p2.browser);
  EXPECT_TRUE(p2.browser->is_type_popup());
  EXPECT_FALSE(p2.browser->is_app());

  // We should have three windows, the browser() provided by the framework,
  // the first popup window, and the second popup window.
  EXPECT_EQ(3u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p1.browser->tab_count());
  EXPECT_EQ(1, p2.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an app frame results in a new Browser with TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromAppWindow) {
  Browser* app_browser = CreateEmptyBrowserForApp(Browser::TYPE_TABBED,
                                                  browser()->profile());
  chrome::NavigateParams p(MakeNavigateParams(app_browser));
  p.disposition = NEW_POPUP;
  p.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&p);

  // Navigate() should have opened a new popup app window.
  EXPECT_NE(app_browser, p.browser);
  EXPECT_NE(browser(), p.browser);
  EXPECT_TRUE(p.browser->is_type_popup());
  EXPECT_TRUE(p.browser->is_app());

  // We should now have three windows, the app window, the app popup it created,
  // and the original browser() provided by the framework.
  EXPECT_EQ(3u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, app_browser->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an app popup results in a new Browser also of TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromAppPopup) {
  Browser* app_browser = CreateEmptyBrowserForApp(Browser::TYPE_TABBED,
                                                  browser()->profile());
  // Open an app popup.
  chrome::NavigateParams p1(MakeNavigateParams(app_browser));
  p1.disposition = NEW_POPUP;
  p1.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&p1);
  // Now open another app popup.
  chrome::NavigateParams p2(MakeNavigateParams(p1.browser));
  p2.disposition = NEW_POPUP;
  p2.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&p2);

  // Navigate() should have opened a new popup app window.
  EXPECT_NE(browser(), p1.browser);
  EXPECT_NE(p1.browser, p2.browser);
  EXPECT_TRUE(p2.browser->is_type_popup());
  EXPECT_TRUE(p2.browser->is_app());

  // We should now have four windows, the app window, the first app popup,
  // the second app popup, and the original browser() provided by the framework.
  EXPECT_EQ(4u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, app_browser->tab_count());
  EXPECT_EQ(1, p1.browser->tab_count());
  EXPECT_EQ(1, p2.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an extension app tab results in a new Browser with TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromExtensionApp) {
  // TODO(beng): TBD.
}

// This test verifies that navigating with window_action = SHOW_WINDOW_INACTIVE
// does not focus a new new popup window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupUnfocused) {
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_POPUP;
  p.window_bounds = gfx::Rect(0, 0, 200, 200);
  p.window_action = chrome::NavigateParams::SHOW_WINDOW_INACTIVE;
  // Wait for new popup to load (and gain focus if the test fails).
  ui_test_utils::NavigateToURL(&p);

  // Navigate() should have opened a new, unfocused, popup window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(Browser::TYPE_POPUP, p.browser->type());
#if 0
// TODO(stevenjb): Enable this test. See: crbug.com/79493
  EXPECT_FALSE(p.browser->window()->IsActive());
#endif
}

// This test verifies that navigating with WindowOpenDisposition = NEW_WINDOW
// always opens a new window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewWindow) {
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_WINDOW;
  chrome::Navigate(&p);

  // Navigate() should have opened a new toplevel window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_TRUE(p.browser->is_type_tabbed());

  // We should now have two windows, the browser() provided by the framework and
  // the new normal window.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = INCOGNITO
// opens a new incognito window if no existing incognito window is present.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_Incognito) {
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = OFF_THE_RECORD;
  chrome::Navigate(&p);

  // Navigate() should have opened a new toplevel incognito window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(browser()->profile()->GetOffTheRecordProfile(),
            p.browser->profile());

  // |source_contents| should be set to NULL because the profile for the new
  // page is different from the originating page.
  EXPECT_EQ(NULL, p.source_contents);

  // We should now have two windows, the browser() provided by the framework and
  // the new incognito window.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = INCOGNITO
// reuses an existing incognito window when possible.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_IncognitoRefocus) {
  Browser* incognito_browser =
      CreateEmptyBrowserForType(Browser::TYPE_TABBED,
                                browser()->profile()->GetOffTheRecordProfile());
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = OFF_THE_RECORD;
  chrome::Navigate(&p);

  // Navigate() should have opened a new tab in the existing incognito window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(p.browser, incognito_browser);

  // We should now have two windows, the browser() provided by the framework and
  // the incognito window we opened earlier.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(2, incognito_browser->tab_count());
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = SUPPRESS_OPEN.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SuppressOpen) {
  RunSuppressTest(SUPPRESS_OPEN);
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = SAVE_TO_DISK.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SaveToDisk) {
  RunSuppressTest(SAVE_TO_DISK);
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = IGNORE_ACTION.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_IgnoreAction) {
  RunSuppressTest(IGNORE_ACTION);
}

// This tests adding a foreground tab with a predefined TabContents.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, TargetContents_ForegroundTab) {
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_FOREGROUND_TAB;
  p.target_contents = CreateTabContents();
  chrome::Navigate(&p);

  // Navigate() should have opened the contents in a new foreground in the
  // current Browser.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(chrome::GetActiveTabContents(browser()), p.target_contents);

  // We should have one window, with two tabs.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
}

#if defined(OS_WIN)
// This tests adding a popup with a predefined TabContents.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, DISABLED_TargetContents_Popup) {
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_POPUP;
  p.target_contents = CreateTabContents();
  p.window_bounds = gfx::Rect(10, 10, 500, 500);
  chrome::Navigate(&p);

  // Navigate() should have opened a new popup window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_TRUE(p.browser->is_type_popup());
  EXPECT_FALSE(p.browser->is_app());

  // The web platform is weird. The window bounds specified in
  // |p.window_bounds| are used as follows:
  // - the origin is used to position the window
  // - the size is used to size the TabContents of the window.
  // As such the position of the resulting window will always match
  // p.window_bounds.origin(), but its size will not. We need to match
  // the size against the selected tab's view's container size.
  // Only Windows positions the window according to |p.window_bounds.origin()| -
  // on Mac the window is offset from the opener and on Linux it always opens
  // at 0,0.
  EXPECT_EQ(p.window_bounds.origin(),
            p.browser->window()->GetRestoredBounds().origin());
  // All platforms should respect size however provided width > 400 (Mac has a
  // minimum window width of 400).
  EXPECT_EQ(p.window_bounds.size(),
            p.target_contents->web_contents()->GetView()->GetContainerSize());

  // We should have two windows, the new popup and the browser() provided by the
  // framework.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}
#endif

// This tests adding a tab at a specific index.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Tabstrip_InsertAtIndex) {
  // This is not meant to be a comprehensive test of whether or not the tab
  // implementation of the browser observes the insertion index. That is
  // covered by the unit tests for TabStripModel. This merely verifies that
  // insertion index preference is reflected in common cases.
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_FOREGROUND_TAB;
  p.tabstrip_index = 0;
  p.tabstrip_add_types = TabStripModel::ADD_FORCE_INDEX;
  chrome::Navigate(&p);

  // Navigate() should have inserted a new tab at slot 0 in the tabstrip.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(0, browser()->tab_strip_model()->GetIndexOfTabContents(
      static_cast<const TabContents*>(p.target_contents)));

  // We should have one window - the browser() provided by the framework.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens a new tab navigated to the specified URL if
// no previous tab with that URL (minus the path) exists.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabNew_IgnorePath) {
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                content::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Navigate to a new singleton tab with a sub-page.
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GetContentSettingsURL();
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&p);

  // The last tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());
  EXPECT_EQ(GetContentSettingsURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExisting_IgnorePath) {
  GURL singleton_url1(GetSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url1,
                                content::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                content::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());

  // Navigate to singleton_url1.
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GetContentSettingsURL();
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&p);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ(GetContentSettingsURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExistingSubPath_IgnorePath) {
  GURL singleton_url1(GetContentSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url1,
                                content::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                content::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());

  // Navigate to singleton_url1.
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GetClearBrowsingDataURL();
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&p);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ(GetClearBrowsingDataURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_STAY_PUT opens an existing tab with the matching URL (minus
// the path).
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExistingSubPath_IgnorePath2) {
  GURL singleton_url1(GetContentSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url1,
                                content::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                content::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());

  // Navigate to singleton_url1.
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GetClearBrowsingDataURL();
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.path_behavior = chrome::NavigateParams::IGNORE_AND_STAY_PUT;
  chrome::Navigate(&p);

  // The middle tab should now be selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ(singleton_url1,
            chrome::GetActiveWebContents(browser())->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE will update the current tab's URL if the currently
// selected tab is a match but has a different path.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabFocused_IgnorePath) {
  GURL singleton_url_current(GetContentSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url_current,
                                content::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Navigate to a different settings path.
  GURL singleton_url_target(GetClearBrowsingDataURL());
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url_target;
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&p);

  // The second tab should still be selected, but navigated to the new path.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ(singleton_url_target,
            chrome::GetActiveWebContents(browser())->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE will open an existing matching tab with a different
// query.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExisting_IgnoreQuery) {
  int initial_tab_count = browser()->tab_count();
  GURL singleton_url_current("chrome://settings/internet");
  chrome::AddSelectedTabWithURL(browser(), singleton_url_current,
                                content::PAGE_TRANSITION_LINK);

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_count());
  EXPECT_EQ(initial_tab_count, browser()->active_index());

  // Navigate to a different settings path.
  GURL singleton_url_target(
      "chrome://settings/internet?"
      "servicePath=/profile/ethernet_00aa00aa00aa&networkType=1");
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url_target;
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&p);

  // Last tab should still be selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(initial_tab_count + 1, browser()->tab_count());
  EXPECT_EQ(initial_tab_count, browser()->active_index());
}

// This test verifies that the settings page isn't opened in the incognito
// window.
// Disabled until fixed for uber settings: http://crbug.com/111243
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       DISABLED_Disposition_Settings_UseNonIncognitoWindow) {
  RunUseNonIncognitoWindowTest(GetSettingsURL());
}

// This test verifies that the settings page isn't opened in the incognito
// window from a non-incognito window (bookmark open-in-incognito trigger).
// Disabled until fixed for uber settings: http://crbug.com/111243
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
    DISABLED_Disposition_Settings_UseNonIncognitoWindowForBookmark) {
  chrome::NavigateParams params(browser(), GetSettingsURL(),
                                content::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = OFF_THE_RECORD;
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::Navigate(&params);
    observer.Wait();
  }

  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(GetSettingsURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

// Settings page is expected to always open in normal mode regardless
// of whether the user is trying to open it in incognito mode or not.
// This test verifies that if incognito mode is forced (by policy), settings
// page doesn't open at all.
// Disabled until fixed for uber settings: http://crbug.com/111243
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
    DISABLED_Disposition_Settings_DoNothingIfIncognitoIsForced) {
  RunDoNothingIfIncognitoIsForcedTest(GetSettingsURL());
}

// This test verifies that the bookmarks page isn't opened in the incognito
// window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Bookmarks_UseNonIncognitoWindow) {
  RunUseNonIncognitoWindowTest(GURL(chrome::kChromeUIBookmarksURL));
}

// Bookmark manager is expected to always open in normal mode regardless
// of whether the user is trying to open it in incognito mode or not.
// This test verifies that if incognito mode is forced (by policy), bookmark
// manager doesn't open at all.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Bookmarks_DoNothingIfIncognitoIsForced) {
  RunDoNothingIfIncognitoIsForcedTest(GURL(chrome::kChromeUIBookmarksURL));
}

// This test verifies that the sync promo page isn't opened in the incognito
// window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SyncPromo_UseNonIncognitoWindow) {
  RunUseNonIncognitoWindowTest(GURL(chrome::kChromeUISyncPromoURL));
}

// The Sync promo page is expected to always open in normal mode regardless of
// whether the user is trying to open it in incognito mode or not.  This test
// verifies that if incognito mode is forced (by policy), the sync promo page
// doesn't open at all.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SyncPromo_DoNothingIfIncognitoIsForced) {
  RunDoNothingIfIncognitoIsForcedTest(GURL(chrome::kChromeUISyncPromoURL));
}

// This test makes sure a crashed singleton tab reloads from a new navigation.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateToCrashedSingletonTab) {
  GURL singleton_url(GetContentSettingsURL());
  TabContents* tab_contents = chrome::AddSelectedTabWithURL(
      browser(), singleton_url, content::PAGE_TRANSITION_LINK);
  WebContents* web_contents = tab_contents->web_contents();

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Kill the singleton tab.
  web_contents->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_TRUE(web_contents->IsCrashed());

  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url;
  p.window_action = chrome::NavigateParams::SHOW_WINDOW;
  p.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  ui_test_utils::NavigateToURL(&p);

  // The tab should not be sad anymore.
  EXPECT_FALSE(web_contents->IsCrashed());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToOptionsInSameTab) {
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromBlankToOptionsInSameTab) {
  chrome::NavigateParams p(MakeNavigateParams());
  p.url = GURL(chrome::kAboutBlankURL);
  ui_test_utils::NavigateToURL(&p);

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromNTPToOptionsInSameTab) {
  chrome::NavigateParams p(MakeNavigateParams());
  p.url = GURL(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(&p);
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            chrome::GetActiveWebContents(browser())->GetURL());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromPageToOptionsInNewTab) {
  chrome::NavigateParams p(MakeNavigateParams());
  ui_test_utils::NavigateToURL(&p);
  EXPECT_EQ(GetGoogleURL(), chrome::GetActiveWebContents(browser())->GetURL());
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromNTPToOptionsSingleton) {
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());

  chrome::NewTab(browser());
  EXPECT_EQ(2, browser()->tab_count());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromNTPToOptionsPageInSameTab) {
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowClearBrowsingDataDialog(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GetClearBrowsingDataURL(),
            chrome::GetActiveWebContents(browser())->GetURL());

  chrome::NewTab(browser());
  EXPECT_EQ(2, browser()->tab_count());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowClearBrowsingDataDialog(browser());
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GetClearBrowsingDataURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

// Times out on mac, fails on linux.
// http://crbug.com/119779
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_NavigateFromOtherTabToSingletonOptions DISABLED_NavigateFromOtherTabToSingletonOptions
#else
#define MAYBE_NavigateFromOtherTabToSingletonOptions NavigatorFrameOtherTabToSingletonOptions
#endif
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       MAYBE_NavigateFromOtherTabToSingletonOptions) {
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                  content::PAGE_TRANSITION_LINK);
    observer.Wait();
  }

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            chrome::GetActiveWebContents(browser())->GetURL());
}

// Tests that when a new tab is opened from the omnibox, the focus is moved from
// the omnibox for the current tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromOmniboxIntoNewTab) {
  GURL url("http://www.google.com/");
  GURL url2("http://maps.google.com/");

  // Navigate to url.
  chrome::NavigateParams p(MakeNavigateParams());
  p.disposition = CURRENT_TAB;
  p.url = url;
  chrome::Navigate(&p);

  // Focus the omnibox.
  chrome::FocusLocationBar(browser());

  OmniboxEditController* controller =
      browser()->window()->GetLocationBar()->GetLocationEntry()->model()->
          controller();

  // Simulate an alt-enter.
  controller->OnAutocompleteAccept(url2, NEW_FOREGROUND_TAB,
                                   content::PAGE_TRANSITION_TYPED, GURL());

  // Make sure the second tab is selected.
  EXPECT_EQ(1, browser()->active_index());

  // The tab contents should have the focus in the second tab.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Go back to the first tab. The focus should not be in the omnibox.
  chrome::SelectPreviousTab(browser());
  EXPECT_EQ(0, browser()->active_index());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(),
                                            VIEW_ID_LOCATION_BAR));
}

// TODO(csilv): Update this for uber page. http://crbug.com/111579.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       DISABLED_NavigateFromDefaultToHistoryInSameTab) {
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowHistory(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUIHistoryFrameURL),
            chrome::GetActiveWebContents(browser())->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToBookmarksInSameTab) {
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowBookmarkManager(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
            chrome::GetActiveWebContents(browser())->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToDownloadsInSameTab) {
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowDownloads(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUIDownloadsURL),
            chrome::GetActiveWebContents(browser())->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateWithoutBrowser) {
  // First navigate using the profile of the existing browser window, and
  // check that the window is reused.
  chrome::NavigateParams params(browser()->profile(), GetGoogleURL(),
                                content::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  EXPECT_EQ(1u, BrowserList::size());

  // Now navigate using the incognito profile and check that a new window
  // is created.
  chrome::NavigateParams params_incognito(
      browser()->profile()->GetOffTheRecordProfile(),
      GetGoogleURL(), content::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params_incognito);
  EXPECT_EQ(2u, BrowserList::size());
}

} // namespace
