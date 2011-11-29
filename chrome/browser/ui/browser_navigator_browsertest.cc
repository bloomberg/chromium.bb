// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_browsertest.h"

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace {

GURL GetGoogleURL() {
  return GURL("http://www.google.com/");
}

GURL GetSettingsURL() {
  return GURL(chrome::kChromeUISettingsURL);
}

GURL GetSettingsAdvancedURL() {
  return GURL(chrome::kChromeUISettingsURL).Resolve(
              chrome::kAdvancedOptionsSubPage);
}

GURL GetSettingsBrowserURL() {
  return GURL(chrome::kChromeUISettingsURL).Resolve(
              chrome::kBrowserOptionsSubPage);
}

GURL GetSettingsPersonalURL() {
  return GURL(chrome::kChromeUISettingsURL).Resolve(
              chrome::kPersonalOptionsSubPage);
}

} // namespace


browser::NavigateParams BrowserNavigatorTest::MakeNavigateParams() const {
  return MakeNavigateParams(browser());
}

browser::NavigateParams BrowserNavigatorTest::MakeNavigateParams(
    Browser* browser) const {
  browser::NavigateParams params(browser, GetGoogleURL(),
                                 content::PAGE_TRANSITION_LINK);
  params.window_action = browser::NavigateParams::SHOW_WINDOW;
  return params;
}

Browser* BrowserNavigatorTest::CreateEmptyBrowserForType(Browser::Type type,
                                                         Profile* profile) {
  Browser* browser = Browser::CreateForType(type, profile);
  browser->AddBlankTab(true);
  return browser;
}

Browser* BrowserNavigatorTest::CreateEmptyBrowserForApp(Browser::Type type,
                                                        Profile* profile) {
  Browser* browser = Browser::CreateForApp(Browser::TYPE_POPUP, "Test",
                                           gfx::Rect(), profile);
  browser->AddBlankTab(true);
  return browser;
}

TabContentsWrapper* BrowserNavigatorTest::CreateTabContents() {
  return Browser::TabContentsFactory(
      browser()->profile(),
      NULL,
      MSG_ROUTING_NONE,
      browser()->GetSelectedTabContents(),
      NULL);
}

void BrowserNavigatorTest::RunSuppressTest(WindowOpenDisposition disposition) {
  GURL old_url = browser()->GetSelectedTabContents()->GetURL();
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = disposition;
  browser::Navigate(&p);

  // Nothing should have happened as a result of Navigate();
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(old_url, browser()->GetSelectedTabContents()->GetURL());
}

void BrowserNavigatorTest::RunUseNonIncognitoWindowTest(const GURL& url) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, incognito_browser->tab_count());

  // Navigate to the page.
  browser::NavigateParams p(MakeNavigateParams(incognito_browser));
  p.disposition = SINGLETON_TAB;
  p.url = url;
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  browser::Navigate(&p);

  // This page should be opened in browser() window.
  EXPECT_NE(incognito_browser, p.browser);
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(url, browser()->GetSelectedTabContents()->GetURL());
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
  browser::NavigateParams p(MakeNavigateParams(browser));
  p.disposition = OFF_THE_RECORD;
  p.url = url;
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  browser::Navigate(&p);

  // The page should not be opened.
  EXPECT_EQ(browser, p.browser);
  EXPECT_EQ(1, browser->tab_count());
  EXPECT_EQ(GURL(chrome::kAboutBlankURL),
            browser->GetSelectedTabContents()->GetURL());
}

void BrowserNavigatorTest::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
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
  EXPECT_EQ(GetGoogleURL(), browser()->GetSelectedTabContents()->GetURL());
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
  // tab nor be creating a new TabContents object
  content::NotificationRegistrar registrar;

  // As the registrar object goes out of scope, this will get unregistered
  registrar.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
                content::NotificationService::AllSources());

  browser()->AddSelectedTabWithURL(
      singleton_url1, content::PAGE_TRANSITION_LINK);
  browser()->AddSelectedTabWithURL(
      GetGoogleURL(), content::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->active_index());

  unsigned int previous_tab_contents_count =
      created_tab_contents_count_ = 0;

  // Navigate to singleton_url1.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url1;
  browser::Navigate(&p);

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

  browser()->AddSelectedTabWithURL(
      singleton_ref_url1, content::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Navigate to singleton_url2.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_ref_url2;
  browser::Navigate(&p);

  // We should now have 2 tabs, the 2nd one selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Navigate to singleton_url2, but with respect ref set.
  p = MakeNavigateParams();
  p.disposition = SINGLETON_TAB;
  p.url = singleton_ref_url2;
  p.ref_behavior = browser::NavigateParams::RESPECT_REF;
  browser::Navigate(&p);

  // We should now have 3 tabs, the 3th one selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());

  // Navigate to singleton_url3.
  p = MakeNavigateParams();
  p.disposition = SINGLETON_TAB;
  p.url = singleton_ref_url3;
  p.ref_behavior = browser::NavigateParams::RESPECT_REF;
  browser::Navigate(&p);

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
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url1;
  browser::Navigate(&p);

  // We should now have 2 tabs, the 2nd one selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
}

// This test verifies that when a navigation results in a foreground tab, the
// tab count of the Browser increases and the selected tab shifts to the new
// foreground tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewForegroundTab) {
  TabContents* old_contents = browser()->GetSelectedTabContents();
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&p);
  EXPECT_NE(old_contents, browser()->GetSelectedTabContents());
  EXPECT_EQ(browser()->GetSelectedTabContentsWrapper(), p.target_contents);
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that when a navigation results in a background tab, the
// tab count of the Browser increases but the selected tab remains the same.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewBackgroundTab) {
  TabContents* old_contents = browser()->GetSelectedTabContents();
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_BACKGROUND_TAB;
  browser::Navigate(&p);
  TabContents* new_contents = browser()->GetSelectedTabContents();
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
  browser::NavigateParams p(MakeNavigateParams(popup));
  p.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&p);

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
  browser::NavigateParams p(MakeNavigateParams(popup));
  p.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&p);

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
  browser::NavigateParams p(MakeNavigateParams());
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
  browser::NavigateParams p(MakeNavigateParams());
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
  browser::NavigateParams p1(MakeNavigateParams());
  p1.disposition = NEW_POPUP;
  p1.window_bounds = gfx::Rect(0, 0, 200, 200);
  browser::Navigate(&p1);
  // Open another popup.
  browser::NavigateParams p2(MakeNavigateParams(p1.browser));
  p2.disposition = NEW_POPUP;
  p2.window_bounds = gfx::Rect(0, 0, 200, 200);
  browser::Navigate(&p2);

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
  browser::NavigateParams p(MakeNavigateParams(app_browser));
  p.disposition = NEW_POPUP;
  p.window_bounds = gfx::Rect(0, 0, 200, 200);
  browser::Navigate(&p);

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
  browser::NavigateParams p1(MakeNavigateParams(app_browser));
  p1.disposition = NEW_POPUP;
  p1.window_bounds = gfx::Rect(0, 0, 200, 200);
  browser::Navigate(&p1);
  // Now open another app popup.
  browser::NavigateParams p2(MakeNavigateParams(p1.browser));
  p2.disposition = NEW_POPUP;
  p2.window_bounds = gfx::Rect(0, 0, 200, 200);
  browser::Navigate(&p2);

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
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_POPUP;
  p.window_bounds = gfx::Rect(0, 0, 200, 200);
  p.window_action = browser::NavigateParams::SHOW_WINDOW_INACTIVE;
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
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_WINDOW;
  browser::Navigate(&p);

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
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = OFF_THE_RECORD;
  browser::Navigate(&p);

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
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = OFF_THE_RECORD;
  browser::Navigate(&p);

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
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_FOREGROUND_TAB;
  p.target_contents = CreateTabContents();
  browser::Navigate(&p);

  // Navigate() should have opened the contents in a new foreground in the
  // current Browser.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(browser()->GetSelectedTabContentsWrapper(), p.target_contents);

  // We should have one window, with two tabs.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
}

#if defined(OS_WIN)
// This tests adding a popup with a predefined TabContents.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, DISABLED_TargetContents_Popup) {
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_POPUP;
  p.target_contents = CreateTabContents();
  p.window_bounds = gfx::Rect(10, 10, 500, 500);
  browser::Navigate(&p);

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
            p.target_contents->tab_contents()->view()->GetContainerSize());

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
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_FOREGROUND_TAB;
  p.tabstrip_index = 0;
  p.tabstrip_add_types = TabStripModel::ADD_FORCE_INDEX;
  browser::Navigate(&p);

  // Navigate() should have inserted a new tab at slot 0 in the tabstrip.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(0, browser()->tabstrip_model()->GetIndexOfTabContents(
      static_cast<const TabContentsWrapper*>(p.target_contents)));

  // We should have one window - the browser() provided by the framework.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that constructing params with a NULL browser has
// the same result as navigating to a new foreground tab in the (only)
// active browser. Tests are the same as for Disposition_NewForegroundTab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, NullBrowser_NewForegroundTab) {
  TabContents* old_contents = browser()->GetSelectedTabContents();
  // Navigate with a NULL browser.
  browser::NavigateParams p(MakeNavigateParams(NULL));
  p.disposition = NEW_FOREGROUND_TAB;
  p.profile = browser()->profile();
  browser::Navigate(&p);

  // Navigate() should have found browser() and create a new tab.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_NE(old_contents, browser()->GetSelectedTabContents());
  EXPECT_EQ(browser()->GetSelectedTabContentsWrapper(), p.target_contents);
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that constructing params with a NULL browser and
// a specific profile matches the specified profile.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, NullBrowser_MatchProfile) {
  // Create a new browser with using the incognito profile.
  Browser* incognito =
      Browser::Create(browser()->profile()->GetOffTheRecordProfile());

  // Navigate with a NULL browser and the incognito profile.
  browser::NavigateParams p(MakeNavigateParams(NULL));
  p.disposition = NEW_FOREGROUND_TAB;
  p.profile = incognito->profile();
  browser::Navigate(&p);

  // Navigate() should have found incognito, not browser().
  EXPECT_EQ(incognito, p.browser);
  EXPECT_EQ(incognito->GetSelectedTabContentsWrapper(), p.target_contents);
  EXPECT_EQ(1, incognito->tab_count());
}

// This test verifies that constructing params with a NULL browser and
// disposition = NEW_WINDOW always opens exactly one new window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, NullBrowser_NewWindow) {
  browser::NavigateParams p(MakeNavigateParams(NULL));
  p.disposition = NEW_WINDOW;
  p.profile = browser()->profile();
  browser::Navigate(&p);

  // Navigate() should have created a new browser.
  EXPECT_NE(browser(), p.browser);
  EXPECT_TRUE( p.browser->is_type_tabbed());

  // We should now have two windows, the browser() provided by the framework and
  // the new normal window.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens a new tab navigated to the specified URL if
// no previous tab with that URL (minus the path) exists.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabNew_IgnorePath) {
  browser()->AddSelectedTabWithURL(
      GetGoogleURL(), content::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Navigate to a new singleton tab with a sub-page.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GetSettingsAdvancedURL();
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  browser::Navigate(&p);

  // The last tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());
  EXPECT_EQ(GetSettingsAdvancedURL(),
            browser()->GetSelectedTabContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExisting_IgnorePath) {
  GURL singleton_url1(GetSettingsURL());
  browser()->AddSelectedTabWithURL(
      singleton_url1, content::PAGE_TRANSITION_LINK);
  browser()->AddSelectedTabWithURL(
      GetGoogleURL(), content::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());

  // Navigate to singleton_url1.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GetSettingsAdvancedURL();
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  browser::Navigate(&p);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ(GetSettingsAdvancedURL(),
            browser()->GetSelectedTabContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExistingSubPath_IgnorePath) {
  GURL singleton_url1(GetSettingsAdvancedURL());
  browser()->AddSelectedTabWithURL(
      singleton_url1, content::PAGE_TRANSITION_LINK);
  browser()->AddSelectedTabWithURL(
      GetGoogleURL(), content::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());

  // Navigate to singleton_url1.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GetSettingsPersonalURL();
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  browser::Navigate(&p);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ(GetSettingsPersonalURL(),
            browser()->GetSelectedTabContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_STAY_PUT opens an existing tab with the matching URL (minus
// the path).
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExistingSubPath_IgnorePath2) {
  GURL singleton_url1(GetSettingsAdvancedURL());
  browser()->AddSelectedTabWithURL(
      singleton_url1, content::PAGE_TRANSITION_LINK);
  browser()->AddSelectedTabWithURL(
      GetGoogleURL(), content::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->active_index());

  // Navigate to singleton_url1.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GetSettingsPersonalURL();
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.path_behavior = browser::NavigateParams::IGNORE_AND_STAY_PUT;
  browser::Navigate(&p);

  // The middle tab should now be selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ(singleton_url1,
            browser()->GetSelectedTabContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE will update the current tab's URL if the currently
// selected tab is a match but has a different path.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabFocused_IgnorePath) {
  GURL singleton_url_current(GetSettingsAdvancedURL());
  browser()->AddSelectedTabWithURL(
      singleton_url_current, content::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Navigate to a different settings path.
  GURL singleton_url_target(GetSettingsPersonalURL());
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url_target;
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  browser::Navigate(&p);

  // The second tab should still be selected, but navigated to the new path.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ(singleton_url_target,
            browser()->GetSelectedTabContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE will open an existing matching tab with a different
// query.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExisting_IgnoreQuery) {
  int initial_tab_count = browser()->tab_count();
  GURL singleton_url_current("chrome://settings/internet");
  browser()->AddSelectedTabWithURL(
      singleton_url_current, content::PAGE_TRANSITION_LINK);

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_count());
  EXPECT_EQ(initial_tab_count, browser()->active_index());

  // Navigate to a different settings path.
  GURL singleton_url_target(
      "chrome://settings/internet?"
      "servicePath=/profile/ethernet_00aa00aa00aa&networkType=1");
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url_target;
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  browser::Navigate(&p);

  // Last tab should still be selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(initial_tab_count + 1, browser()->tab_count());
  EXPECT_EQ(initial_tab_count, browser()->active_index());
}

// This test verifies that the settings page isn't opened in the incognito
// window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Settings_UseNonIncognitoWindow) {
  RunUseNonIncognitoWindowTest(GetSettingsURL());
}

// This test verifies that the settings page isn't opened in the incognito
// window from a non-incognito window (bookmark open-in-incognito trigger).
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Settings_UseNonIncognitoWindowForBookmark) {
  browser::NavigateParams params(browser(), GURL("chrome://settings"),
                                 content::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = OFF_THE_RECORD;
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser::Navigate(&params);
    observer.Wait();
  }

  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(GURL("chrome://settings"),
            browser()->GetSelectedTabContents()->GetURL().GetOrigin());
}

// Settings page is expected to always open in normal mode regardless
// of whether the user is trying to open it in incognito mode or not.
// This test verifies that if incognito mode is forced (by policy), settings
// page doesn't open at all.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_Settings_DoNothingIfIncognitoIsForced) {
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
  GURL singleton_url(GetSettingsAdvancedURL());
  TabContentsWrapper* wrapper = browser()->AddSelectedTabWithURL(
      singleton_url, content::PAGE_TRANSITION_LINK);
  TabContents* tab_contents = wrapper->tab_contents();

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Kill the singleton tab.
  tab_contents->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_TRUE(tab_contents->is_crashed());

  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url;
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ui_test_utils::NavigateToURL(&p);

  // The tab should not be sad anymore.
  EXPECT_FALSE(tab_contents->is_crashed());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToOptionsInSameTab) {
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->OpenOptionsDialog();
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->GetSelectedTabContents()->GetURL().GetOrigin());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromBlankToOptionsInSameTab) {
  browser::NavigateParams p(MakeNavigateParams());
  p.url = GURL(chrome::kAboutBlankURL);
  ui_test_utils::NavigateToURL(&p);

  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->OpenOptionsDialog();
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->GetSelectedTabContents()->GetURL().GetOrigin());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromNTPToOptionsInSameTab) {
  browser::NavigateParams p(MakeNavigateParams());
  p.url = GURL(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(&p);
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            browser()->GetSelectedTabContents()->GetURL());

  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->OpenOptionsDialog();
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->GetSelectedTabContents()->GetURL().GetOrigin());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromPageToOptionsInNewTab) {
  browser::NavigateParams p(MakeNavigateParams());
  ui_test_utils::NavigateToURL(&p);
  EXPECT_EQ(GetGoogleURL(), browser()->GetSelectedTabContents()->GetURL());
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());

  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->OpenOptionsDialog();
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->GetSelectedTabContents()->GetURL().GetOrigin());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromNTPToOptionsSingleton) {
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->OpenOptionsDialog();
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());

  browser()->NewTab();
  EXPECT_EQ(2, browser()->tab_count());

  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->OpenOptionsDialog();
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->GetSelectedTabContents()->GetURL().GetOrigin());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromNTPToOptionsPageInSameTab) {
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->ShowOptionsTab(chrome::kPersonalOptionsSubPage);
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GetSettingsPersonalURL(),
            browser()->GetSelectedTabContents()->GetURL());

  browser()->NewTab();
  EXPECT_EQ(2, browser()->tab_count());

  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->ShowOptionsTab(chrome::kPersonalOptionsSubPage);
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GetSettingsPersonalURL(),
            browser()->GetSelectedTabContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromOtherTabToSingletonOptions) {
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->OpenOptionsDialog();
    observer.Wait();
  }
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->AddSelectedTabWithURL(
        GetGoogleURL(), content::PAGE_TRANSITION_LINK);
    observer.Wait();
  }

  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->OpenOptionsDialog();
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(GetSettingsURL(),
            browser()->GetSelectedTabContents()->GetURL().GetOrigin());
}

// Tests that when a new tab is opened from the omnibox, the focus is moved from
// the omnibox for the current tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromOmniboxIntoNewTab) {
  GURL url("http://www.google.com/");
  GURL url2("http://maps.google.com/");

  // Navigate to url.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = CURRENT_TAB;
  p.url = url;
  browser::Navigate(&p);

  // Focus the omnibox.
  browser()->FocusLocationBar();

  AutocompleteEditController* controller =
      browser()->window()->GetLocationBar()->location_entry()->model()->
          controller();

  // Simulate an alt-enter.
  controller->OnAutocompleteAccept(url2, NEW_FOREGROUND_TAB,
                                   content::PAGE_TRANSITION_TYPED, GURL());

  // Make sure the second tab is selected.
  EXPECT_EQ(1, browser()->active_index());

  // The tab contents should have the focus in the second tab.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

  // Go back to the first tab. The focus should not be in the omnibox.
  browser()->SelectPreviousTab();
  EXPECT_EQ(0, browser()->active_index());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(),
                                            VIEW_ID_LOCATION_BAR));
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToHistoryInSameTab) {
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->ShowHistoryTab();
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUIHistoryURL),
            browser()->GetSelectedTabContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToBookmarksInSameTab) {
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->OpenBookmarkManager();
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
            browser()->GetSelectedTabContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToDownloadsInSameTab) {
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->ShowDownloadsTab();
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUIDownloadsURL),
            browser()->GetSelectedTabContents()->GetURL());
}

} // namespace
