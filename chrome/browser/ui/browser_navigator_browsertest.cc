// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_browsertest.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;

namespace {

const char kExpectedTitle[] = "PASSED!";
const char kEchoTitleCommand[] = "/echotitle";

GURL GetGoogleURL() {
  return GURL("http://www.google.com/");
}

GURL GetSettingsURL() {
  return GURL(chrome::kChromeUISettingsURL);
}

GURL GetContentSettingsURL() {
  return GetSettingsURL().Resolve(chrome::kContentSettingsSubPage);
}

GURL GetClearBrowsingDataURL() {
  return GetSettingsURL().Resolve(chrome::kClearBrowserDataSubPage);
}

// Converts long uber URLs ("chrome://chrome/foo/") to short (virtual) URLs
// ("chrome://foo/"). This should be used to convert the return value of
// WebContentsImpl::GetURL before comparison because it can return either the
// real URL or the virtual URL.
GURL ShortenUberURL(const GURL& url) {
  std::string url_string = url.spec();
  const std::string long_prefix = "chrome://chrome/";
  const std::string short_prefix = "chrome://";
  if (!base::StartsWith(url_string, long_prefix, base::CompareCase::SENSITIVE))
    return url;
  url_string.replace(0, long_prefix.length(), short_prefix);
  return GURL(url_string);
}

}  // namespace

chrome::NavigateParams BrowserNavigatorTest::MakeNavigateParams() const {
  return MakeNavigateParams(browser());
}

chrome::NavigateParams BrowserNavigatorTest::MakeNavigateParams(
    Browser* browser) const {
  chrome::NavigateParams params(browser, GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  return params;
}

bool BrowserNavigatorTest::OpenPOSTURLInNewForegroundTabAndGetTitle(
    const GURL& url, const std::string& post_data, bool is_browser_initiated,
    base::string16* title) {
  chrome::NavigateParams param(MakeNavigateParams());
  param.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  param.url = url;
  param.is_renderer_initiated = !is_browser_initiated;
  param.uses_post = true;
  param.post_data = content::ResourceRequestBody::CreateFromBytes(
      post_data.data(), post_data.size());

  ui_test_utils::NavigateToURL(&param);
  if (!param.target_contents)
    return false;

  // Navigate() should have opened the contents in new foreground tab in the
  // current Browser.
  EXPECT_EQ(browser(), param.browser);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            param.target_contents);
  // We should have one window, with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  *title = param.target_contents->GetTitle();
  return true;
}

Browser* BrowserNavigatorTest::CreateEmptyBrowserForType(Browser::Type type,
                                                         Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(type, profile, true));
  chrome::AddTabAt(browser, GURL(), -1, true);
  return browser;
}

Browser* BrowserNavigatorTest::CreateEmptyBrowserForApp(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams::CreateForApp(
      "Test", false /* trusted_source */, gfx::Rect(), profile, true));
  chrome::AddTabAt(browser, GURL(), -1, true);
  return browser;
}

WebContents* BrowserNavigatorTest::CreateWebContents(bool initialize_renderer) {
  content::WebContents::CreateParams create_params(browser()->profile());
  create_params.initialize_renderer = initialize_renderer;
  content::WebContents* base_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (base_web_contents) {
    create_params.initial_size =
        base_web_contents->GetContainerBounds().size();
  }
  return WebContents::Create(create_params);
}

void BrowserNavigatorTest::RunSuppressTest(WindowOpenDisposition disposition) {
  GURL old_url = browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = disposition;
  chrome::Navigate(&params);

  // Nothing should have happened as a result of Navigate();
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(old_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

void BrowserNavigatorTest::RunUseNonIncognitoWindowTest(const GURL& url) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, incognito_browser->tab_strip_model()->count());

  // Navigate to the page.
  chrome::NavigateParams params(MakeNavigateParams(incognito_browser));
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = url;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);

  // This page should be opened in browser() window.
  EXPECT_NE(incognito_browser, params.browser);
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
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
  chrome::NavigateParams params(MakeNavigateParams(browser));
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;
  params.url = url;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);

  // The page should not be opened.
  EXPECT_EQ(browser, params.browser);
  EXPECT_EQ(1, browser->tab_strip_model()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

void BrowserNavigatorTest::SetUpCommandLine(base::CommandLine* command_line) {
  // Disable settings-in-a-window so that we can use the settings page and
  // sub-pages to test browser navigation.
  command_line->AppendSwitch(::switches::kDisableSettingsWindow);
}

void BrowserNavigatorTest::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED, type);
  ++created_tab_contents_count_;
}


namespace {

// This test verifies that when a navigation occurs within a tab, the tab count
// of the Browser remains the same and the current tab bears the loaded URL.
// Note that network URLs are not actually loaded in tests, so this also tests
// that error pages leave the intended URL in the address bar.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_CurrentTab) {
  ui_test_utils::NavigateToURL(browser(), GetGoogleURL());
  EXPECT_EQ(GetGoogleURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  // We should have one window with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

// This test verifies that a singleton tab is refocused if one is already opened
// in another or an existing window, or added if it is not.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SingletonTabExisting) {
  GURL singleton_url1("http://maps.google.com/");

  // Register for a notification if an additional WebContents was instantiated.
  // Opening a Singleton tab that is already opened should not be opening a new
  // tab nor be creating a new WebContents object.
  content::NotificationRegistrar registrar;

  // As the registrar object goes out of scope, this will get unregistered
  registrar.Add(this,
                content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                content::NotificationService::AllSources());

  chrome::AddSelectedTabWithURL(browser(), singleton_url1,
                                ui::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());

  unsigned int previous_tab_contents_count =
      created_tab_contents_count_ = 0;

  // Navigate to singleton_url1.
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url1;
  chrome::Navigate(&params);

  // The middle tab should now be selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

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
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, 2nd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Navigate to singleton_url2.
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_ref_url2;
  chrome::Navigate(&params);

  // We should now have 2 tabs, the 2nd one selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Navigate to singleton_url2, but with respect ref set.
  params = MakeNavigateParams();
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_ref_url2;
  params.ref_behavior = chrome::NavigateParams::RESPECT_REF;
  chrome::Navigate(&params);

  // We should now have 3 tabs, the 3th one selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());

  // Navigate to singleton_url3.
  params = MakeNavigateParams();
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_ref_url3;
  params.ref_behavior = chrome::NavigateParams::RESPECT_REF;
  chrome::Navigate(&params);

  // We should now have 4 tabs, the 4th one selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(4, browser()->tab_strip_model()->count());
  EXPECT_EQ(3, browser()->tab_strip_model()->active_index());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabNoneExisting) {
  GURL singleton_url1("http://maps.google.com/");

  // We should have one browser with 1 tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Navigate to singleton_url1.
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url1;
  chrome::Navigate(&params);

  // We should now have 2 tabs, the 2nd one selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
}

// This test verifies that when a navigation results in a foreground tab, the
// tab count of the Browser increases and the selected tab shifts to the new
// foreground tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewForegroundTab) {
  WebContents* old_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
  EXPECT_NE(old_contents,
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            params.target_contents);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// This test verifies that when a navigation results in a background tab, the
// tab count of the Browser increases but the selected tab remains the same.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewBackgroundTab) {
  WebContents* old_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  chrome::Navigate(&params);
  WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // The selected tab should have remained unchanged, since the new tab was
  // opened in the background.
  EXPECT_EQ(old_contents, new_contents);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
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
  chrome::NavigateParams params(MakeNavigateParams(popup));
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);

  // Navigate() should have opened the tab in a different browser since the
  // one we supplied didn't support additional tabs.
  EXPECT_NE(popup, params.browser);

  // Since browser() is an existing compatible tabbed browser, it should have
  // opened the tab there.
  EXPECT_EQ(browser(), params.browser);

  // We should be left with 2 windows, the popup with one tab and the browser()
  // provided by the framework with two.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, popup->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
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
  chrome::NavigateParams params(MakeNavigateParams(popup));
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);

  // Navigate() should have opened the tab in a different browser since the
  // one we supplied didn't support additional tabs.
  EXPECT_NE(popup, params.browser);

  // This time, browser() is _not_ compatible with popup since it is not an
  // incognito window.
  EXPECT_NE(browser(), params.browser);

  // We should have three windows, each with one tab:
  // 1. the browser() provided by the framework (unchanged in this test)
  // 2. the incognito popup we created originally
  // 3. the new incognito tabbed browser that was created by Navigate().
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, popup->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
  EXPECT_TRUE(params.browser->is_type_tabbed());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal Browser results in a new Browser with TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopup) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&params);

  // Navigate() should have opened a new, focused popup window.
  EXPECT_NE(browser(), params.browser);
#if 0
  // TODO(stevenjb): Enable this test. See: crbug.com/79493
  EXPECT_TRUE(browser->window()->IsActive());
#endif
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_FALSE(params.browser->is_app());

  // We should have two windows, the browser() provided by the framework and the
  // new popup window.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal Browser results in a new Browser with is_app() true.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopup_ExtensionId) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.extension_app_id = "extensionappid";
  params.window_bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&params);

  // Navigate() should have opened a new, focused popup window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_TRUE(params.browser->is_app());

  // We should have two windows, the browser() provided by the framework and the
  // new popup window.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal popup results in a new Browser with TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupFromPopup) {
  // Open a popup.
  chrome::NavigateParams params1(MakeNavigateParams());
  params1.disposition = WindowOpenDisposition::NEW_POPUP;
  params1.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&params1);
  // Open another popup.
  chrome::NavigateParams params2(MakeNavigateParams(params1.browser));
  params2.disposition = WindowOpenDisposition::NEW_POPUP;
  params2.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&params2);

  // Navigate() should have opened a new normal popup window.
  EXPECT_NE(params1.browser, params2.browser);
  EXPECT_TRUE(params2.browser->is_type_popup());
  EXPECT_FALSE(params2.browser->is_app());

  // We should have three windows, the browser() provided by the framework,
  // the first popup window, and the second popup window.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params1.browser->tab_strip_model()->count());
  EXPECT_EQ(1, params2.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an app frame results in a new Browser with TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromAppWindow) {
  Browser* app_browser = CreateEmptyBrowserForApp(browser()->profile());
  chrome::NavigateParams params(MakeNavigateParams(app_browser));
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&params);

  // Navigate() should have opened a new popup app window.
  EXPECT_NE(app_browser, params.browser);
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_TRUE(params.browser->is_app());

  // We should now have three windows, the app window, the app popup it created,
  // and the original browser() provided by the framework.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an app popup results in a new Browser also of TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromAppPopup) {
  Browser* app_browser = CreateEmptyBrowserForApp(browser()->profile());
  // Open an app popup.
  chrome::NavigateParams params1(MakeNavigateParams(app_browser));
  params1.disposition = WindowOpenDisposition::NEW_POPUP;
  params1.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&params1);
  // Now open another app popup.
  chrome::NavigateParams params2(MakeNavigateParams(params1.browser));
  params2.disposition = WindowOpenDisposition::NEW_POPUP;
  params2.window_bounds = gfx::Rect(0, 0, 200, 200);
  chrome::Navigate(&params2);

  // Navigate() should have opened a new popup app window.
  EXPECT_NE(browser(), params1.browser);
  EXPECT_NE(params1.browser, params2.browser);
  EXPECT_TRUE(params2.browser->is_type_popup());
  EXPECT_TRUE(params2.browser->is_app());

  // We should now have four windows, the app window, the first app popup,
  // the second app popup, and the original browser() provided by the framework.
  EXPECT_EQ(4u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(1, params1.browser->tab_strip_model()->count());
  EXPECT_EQ(1, params2.browser->tab_strip_model()->count());
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
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_bounds = gfx::Rect(0, 0, 200, 200);
  params.window_action = chrome::NavigateParams::SHOW_WINDOW_INACTIVE;
  // Wait for new popup to load (and gain focus if the test fails).
  ui_test_utils::NavigateToURL(&params);

  // Navigate() should have opened a new, unfocused, popup window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_EQ(Browser::TYPE_POPUP, params.browser->type());
#if 0
// TODO(stevenjb): Enable this test. See: crbug.com/79493
  EXPECT_FALSE(p.browser->window()->IsActive());
#endif
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// and trusted_source = true results in a new Browser where is_trusted_source()
// is true.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupTrusted) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.trusted_source = true;
  params.window_bounds = gfx::Rect(0, 0, 200, 200);
  // Wait for new popup to to load and gain focus.
  ui_test_utils::NavigateToURL(&params);

  // Navigate() should have opened a new popup window of TYPE_TRUSTED_POPUP.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_TRUE(params.browser->is_trusted_source());
}


// This test verifies that navigating with WindowOpenDisposition = NEW_WINDOW
// always opens a new window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewWindow) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  chrome::Navigate(&params);

  // Navigate() should have opened a new toplevel window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_tabbed());

  // We should now have two windows, the browser() provided by the framework and
  // the new normal window.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

#if defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
// Flaky on ASAN on Mac. See https://crbug.com/674497.
#define MAYBE_Disposition_Incognito DISABLED_Disposition_Incognito
#else
#define MAYBE_Disposition_Incognito Disposition_Incognito
#endif
// This test verifies that navigating with WindowOpenDisposition = INCOGNITO
// opens a new incognito window if no existing incognito window is present.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, MAYBE_Disposition_Incognito) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;
  chrome::Navigate(&params);

  // Navigate() should have opened a new toplevel incognito window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_EQ(browser()->profile()->GetOffTheRecordProfile(),
            params.browser->profile());

  // |source_contents| should be set to NULL because the profile for the new
  // page is different from the originating page.
  EXPECT_EQ(NULL, params.source_contents);

  // We should now have two windows, the browser() provided by the framework and
  // the new incognito window.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}

// This test verifies that navigating with WindowOpenDisposition = INCOGNITO
// reuses an existing incognito window when possible.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_IncognitoRefocus) {
  Browser* incognito_browser =
      CreateEmptyBrowserForType(Browser::TYPE_TABBED,
                                browser()->profile()->GetOffTheRecordProfile());
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;
  chrome::Navigate(&params);

  // Navigate() should have opened a new tab in the existing incognito window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_EQ(params.browser, incognito_browser);

  // We should now have two windows, the browser() provided by the framework and
  // the incognito window we opened earlier.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, incognito_browser->tab_strip_model()->count());
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = SAVE_TO_DISK.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SaveToDisk) {
  RunSuppressTest(WindowOpenDisposition::SAVE_TO_DISK);
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = IGNORE_ACTION.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_IgnoreAction) {
  RunSuppressTest(WindowOpenDisposition::IGNORE_ACTION);
}

// This tests adding a foreground tab with a predefined WebContents.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, TargetContents_ForegroundTab) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.target_contents = CreateWebContents(false);
  chrome::Navigate(&params);

  // Navigate() should have opened the contents in a new foreground tab in the
  // current Browser.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            params.target_contents);

  // We should have one window, with two tabs.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

#if defined(OS_WIN)
// This tests adding a popup with a predefined WebContents.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, DISABLED_TargetContents_Popup) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.target_contents = CreateWebContents(false);
  params.window_bounds = gfx::Rect(10, 10, 500, 500);
  chrome::Navigate(&params);

  // Navigate() should have opened a new popup window.
  EXPECT_NE(browser(), params.browser);
  EXPECT_TRUE(params.browser->is_type_popup());
  EXPECT_FALSE(params.browser->is_app());

  // The web platform is weird. The window bounds specified in
  // |params.window_bounds| are used as follows:
  // - the origin is used to position the window
  // - the size is used to size the WebContents of the window.
  // As such the position of the resulting window will always match
  // params.window_bounds.origin(), but its size will not. We need to match
  // the size against the selected tab's view's container size.
  // Only Windows positions the window according to
  // |params.window_bounds.origin()| - on Mac the window is offset from the
  // opener and on Linux it always opens at 0,0.
  EXPECT_EQ(params.window_bounds.origin(),
            params.browser->window()->GetRestoredBounds().origin());
  // All platforms should respect size however provided width > 400 (Mac has a
  // minimum window width of 400).
  EXPECT_EQ(params.window_bounds.size(),
            params.target_contents->GetContainerBounds().size());

  // We should have two windows, the new popup and the browser() provided by the
  // framework.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, params.browser->tab_strip_model()->count());
}
#endif

// This test checks that we can create WebContents with renderer process and
// RenderFrame without navigating it.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       CreateWebContentsWithRendererProcess) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.target_contents = CreateWebContents(true);
  ASSERT_TRUE(params.target_contents);

  // There is no navigation (to about:blank or something like that).
  EXPECT_FALSE(params.target_contents->IsLoading());

  ASSERT_TRUE(params.target_contents->GetMainFrame());
  EXPECT_TRUE(params.target_contents->GetMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(
      params.target_contents->GetController().IsInitialBlankNavigation());
  int renderer_id = params.target_contents->GetRenderProcessHost()->GetID();

  // We should have one window, with one tab of WebContents differ from
  // params.target_contents.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(browser()->tab_strip_model()->GetActiveWebContents(),
            params.target_contents);

  chrome::Navigate(&params);

  // Navigate() should have opened the contents in a new foreground tab in the
  // current Browser, without changing the renderer process of target_contents.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            params.target_contents);
  EXPECT_EQ(renderer_id,
            params.target_contents->GetRenderProcessHost()->GetID());

  // We should have one window, with two tabs.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// This tests adding a tab at a specific index.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Tabstrip_InsertAtIndex) {
  // This is not meant to be a comprehensive test of whether or not the tab
  // implementation of the browser observes the insertion index. That is
  // covered by the unit tests for TabStripModel. This merely verifies that
  // insertion index preference is reflected in common cases.
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_index = 0;
  params.tabstrip_add_types = TabStripModel::ADD_FORCE_INDEX;
  chrome::Navigate(&params);

  // Navigate() should have inserted a new tab at slot 0 in the tabstrip.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(0, browser()->tab_strip_model()->GetIndexOfWebContents(
      static_cast<const WebContents*>(params.target_contents)));

  // We should have one window - the browser() provided by the framework.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens a new tab navigated to the specified URL if
// no previous tab with that URL (minus the path) exists.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabNew_IgnorePath) {
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Navigate to a new singleton tab with a sub-page.
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GetContentSettingsURL();
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&params);

  // The last tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(GetContentSettingsURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExisting_IgnorePath) {
  GURL singleton_url1(GetSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url1,
                                ui::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());

  // Navigate to singleton_url1.
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GetContentSettingsURL();
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&params);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(GetContentSettingsURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExistingSubPath_IgnorePath) {
  GURL singleton_url1(GetContentSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url1,
                                ui::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());

  // Navigate to singleton_url1.
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GetClearBrowsingDataURL();
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&params);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(GetClearBrowsingDataURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_STAY_PUT opens an existing tab with the matching URL (minus
// the path).
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExistingSubPath_IgnorePath2) {
  GURL singleton_url1(GetContentSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url1,
                                ui::PAGE_TRANSITION_LINK);
  chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(2, browser()->tab_strip_model()->active_index());

  // Navigate to singleton_url1.
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = GetClearBrowsingDataURL();
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_STAY_PUT;
  chrome::Navigate(&params);

  // The middle tab should now be selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(singleton_url1,
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE will update the current tab's URL if the currently
// selected tab is a match but has a different path.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabFocused_IgnorePath) {
  GURL singleton_url_current(GetContentSettingsURL());
  chrome::AddSelectedTabWithURL(browser(), singleton_url_current,
                                ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Navigate to a different settings path.
  GURL singleton_url_target(GetClearBrowsingDataURL());
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url_target;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&params);

  // The second tab should still be selected, but navigated to the new path.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(singleton_url_target,
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and IGNORE_AND_NAVIGATE will open an existing matching tab with a different
// query.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExisting_IgnoreQuery) {
  int initial_tab_count = browser()->tab_strip_model()->count();
  GURL singleton_url_current("chrome://settings/internet");
  chrome::AddSelectedTabWithURL(browser(), singleton_url_current,
                                ui::PAGE_TRANSITION_LINK);

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(initial_tab_count, browser()->tab_strip_model()->active_index());

  // Navigate to a different settings path.
  GURL singleton_url_target(
      "chrome://settings/internet?"
      "guid=ethernet_00aa00aa00aa&networkType=1");
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url_target;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&params);

  // Last tab should still be selected.
  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(initial_tab_count, browser()->tab_strip_model()->active_index());
}

// This test verifies that the settings page isn't opened in the incognito
// window.
// Disabled until fixed for uber settings: http://crbug.com/111243
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       DISABLED_Disposition_Settings_UseNonIncognitoWindow) {
  RunUseNonIncognitoWindowTest(GetSettingsURL());
}

// This test verifies that the view-source settings page isn't opened in the
// incognito window.
IN_PROC_BROWSER_TEST_F(
    BrowserNavigatorTest,
    Disposition_ViewSource_Settings_DoNothingIfIncognitoForced) {
  std::string view_source(content::kViewSourceScheme);
  view_source.append(":");
  view_source.append(chrome::kChromeUISettingsURL);
  RunDoNothingIfIncognitoIsForcedTest(GURL(view_source));
}

// This test verifies that the view-source settings page isn't opened in the
// incognito window even if incognito mode is forced (does nothing in that
// case).
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_ViewSource_Settings_UseNonIncognitoWindow) {
  std::string view_source(content::kViewSourceScheme);
  view_source.append(":");
  view_source.append(chrome::kChromeUISettingsURL);
  RunUseNonIncognitoWindowTest(GURL(view_source));
}

// This test verifies that the settings page isn't opened in the incognito
// window from a non-incognito window (bookmark open-in-incognito trigger).
// Disabled until fixed for uber settings: http://crbug.com/111243
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
    DISABLED_Disposition_Settings_UseNonIncognitoWindowForBookmark) {
  chrome::NavigateParams params(browser(), GetSettingsURL(),
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::OFF_THE_RECORD;
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::Navigate(&params);
    observer.Wait();
  }

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GetSettingsURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
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

// This test makes sure a crashed singleton tab reloads from a new navigation.
// http://crbug.com/396371
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       DISABLED_NavigateToCrashedSingletonTab) {
  GURL singleton_url(GetContentSettingsURL());
  WebContents* web_contents = chrome::AddSelectedTabWithURL(
      browser(), singleton_url, ui::PAGE_TRANSITION_LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Kill the singleton tab.
  web_contents->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_TRUE(web_contents->IsCrashed());

  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.url = singleton_url;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  ui_test_utils::NavigateToURL(&params);

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
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromBlankToOptionsInSameTab) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.url = GURL(url::kAboutBlankURL);
  ui_test_utils::NavigateToURL(&params);

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromNTPToOptionsInSameTab) {
  chrome::NavigateParams params(MakeNavigateParams());
  params.url = GURL(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(&params);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromPageToOptionsInNewTab) {
  chrome::NavigateParams params(MakeNavigateParams());
  ui_test_utils::NavigateToURL(&params);
  EXPECT_EQ(GetGoogleURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
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
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  chrome::NewTab(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
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
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetClearBrowsingDataURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  chrome::NewTab(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowClearBrowsingDataDialog(browser());
    observer.Wait();
  }
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetClearBrowsingDataURL(),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromOtherTabToSingletonOptions) {
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
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }

  // This load should simply cause a tab switch.
  chrome::ShowSettings(browser());

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetSettingsURL(),
            ShortenUberURL(browser()->tab_strip_model()->
                GetActiveWebContents()->GetURL()));
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, CloseSingletonTab) {
  for (int i = 0; i < 2; ++i) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), GetGoogleURL(),
                                  ui::PAGE_TRANSITION_TYPED);
    observer.Wait();
  }

  browser()->tab_strip_model()->ActivateTabAt(0, true);

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowSettings(browser());
    observer.Wait();
  }

  EXPECT_TRUE(browser()->tab_strip_model()->CloseWebContentsAt(
      2, TabStripModel::CLOSE_USER_GESTURE));
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateFromDefaultToHistoryInSameTab) {
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowHistory(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(chrome::kChromeUIHistoryURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
#define MAYBE_NavigateFromDefaultToBookmarksInSameTab \
    DISABLED_NavigateFromDefaultToBookmarksInSameTab
#else
#define MAYBE_NavigateFromDefaultToBookmarksInSameTab \
    NavigateFromDefaultToBookmarksInSameTab
#endif
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       MAYBE_NavigateFromDefaultToBookmarksInSameTab) {
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::ShowBookmarkManager(browser());
    observer.Wait();
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(base::StartsWith(
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec(),
      chrome::kChromeUIBookmarksURL, base::CompareCase::SENSITIVE));
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
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(chrome::kChromeUIDownloadsURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateWithoutBrowser) {
  // First navigate using the profile of the existing browser window, and
  // check that the window is reused.
  chrome::NavigateParams params(browser()->profile(), GetGoogleURL(),
                                ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Now navigate using the incognito profile and check that a new window
  // is created.
  chrome::NavigateParams params_incognito(
      browser()->profile()->GetOffTheRecordProfile(),
      GetGoogleURL(), ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params_incognito);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, ViewSourceIsntSingleton) {
  const std::string viewsource_ntp_url =
      std::string(content::kViewSourceScheme) + ":" +
      chrome::kChromeUIVersionURL;

  chrome::NavigateParams viewsource_params(browser(),
                                           GURL(viewsource_ntp_url),
                                           ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&viewsource_params);

  chrome::NavigateParams singleton_params(browser(),
                                          GURL(chrome::kChromeUIVersionURL),
                                          ui::PAGE_TRANSITION_LINK);
  singleton_params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  EXPECT_EQ(-1, chrome::GetIndexOfSingletonTab(&singleton_params));
}

// This test verifies that browser initiated navigations can send requests
// using POST.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       SendBrowserInitiatedRequestUsingPOST) {
  // Uses a test sever to verify POST request.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open a browser initiated POST request in new foreground tab.
  base::string16 expected_title(base::ASCIIToUTF16(kExpectedTitle));
  std::string post_data = kExpectedTitle;
  base::string16 title;
  ASSERT_TRUE(OpenPOSTURLInNewForegroundTabAndGetTitle(
      embedded_test_server()->GetURL(kEchoTitleCommand), post_data, true,
      &title));
  EXPECT_EQ(expected_title, title);
}

// This test verifies that renderer initiated navigations can also send requests
// using POST.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       SendRendererInitiatedRequestUsingPOST) {
  // Uses a test sever to verify POST request.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open a renderer initiated POST request in new foreground tab.
  base::string16 expected_title(base::ASCIIToUTF16(kExpectedTitle));
  std::string post_data = kExpectedTitle;
  base::string16 title;
  ASSERT_TRUE(OpenPOSTURLInNewForegroundTabAndGetTitle(
      embedded_test_server()->GetURL(kEchoTitleCommand), post_data, false,
      &title));
  EXPECT_EQ(expected_title, title);
}

// This test navigates to a data URL that contains BiDi control
// characters. For security reasons, BiDi control chars should always be
// escaped in the URL but they should be unescaped in the loaded HTML.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       NavigateToDataURLWithBiDiControlChars) {
  // Text in Arabic.
  std::string text = "\xD8\xA7\xD8\xAE\xD8\xAA\xD8\xA8\xD8\xA7\xD8\xB1";
  // Page title starts with RTL mark.
  std::string unescaped_title = "\xE2\x80\x8F" + text;
  std::string data_url = "data:text/html;charset=utf-8,<html><title>" +
                         unescaped_title + "</title></html>";
  // BiDi control chars in URLs are always escaped, so the expected URL should
  // have the title with the escaped RTL mark.
  std::string escaped_title = "%E2%80%8F" + text;
  std::string expected_url = "data:text/html;charset=utf-8,<html><title>" +
                             escaped_title + "</title></html>";

  // Navigate to the page.
  chrome::NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.url = GURL(data_url);
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  ui_test_utils::NavigateToURL(&params);

  base::string16 expected_title(base::UTF8ToUTF16(unescaped_title));
  EXPECT_TRUE(params.target_contents);
  EXPECT_EQ(expected_title, params.target_contents->GetTitle());
  // GURL always keeps non-ASCII characters escaped, but check them anyways.
  EXPECT_EQ(GURL(expected_url).spec(), params.target_contents->GetURL().spec());
  // Check the omnibox text. It should have escaped RTL with unescaped text.
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  EXPECT_EQ(base::UTF8ToUTF16(expected_url), omnibox_view->GetText());
}

// Test that there's no crash when a navigation to a WebUI page reuses an
// existing swapped out RenderViewHost.  Previously, this led to a browser
// process crash in WebUI pages that use MojoWebUIController, which tried to
// use the RenderViewHost's GetMainFrame() when it was invalid in
// RenderViewCreated(). See https://crbug.com/627027.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, ReuseRVHWithWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Visit a WebUI page with bindings.
  GURL webui_url(chrome::kChromeUIOmniboxURL);
  ui_test_utils::NavigateToURL(browser(), webui_url);

  // window.open a new tab.  This will keep the chrome://omnibox process alive
  // once we navigate away from it.
  content::WindowedNotificationObserver windowed_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('" + webui_url.spec() + "');"));
  windowed_observer.Wait();
  content::NavigationController* controller =
      content::Source<content::NavigationController>(windowed_observer.source())
          .ptr();
  content::WebContents* popup = controller->GetWebContents();
  ASSERT_TRUE(popup);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::RenderViewHost* webui_rvh = popup->GetRenderViewHost();
  content::RenderFrameHost* webui_rfh = popup->GetMainFrame();
  EXPECT_EQ(content::BINDINGS_POLICY_WEB_UI, webui_rfh->GetEnabledBindings());

  // Navigate to another page in the popup.
  GURL nonwebui_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), nonwebui_url);
  EXPECT_NE(webui_rvh, popup->GetRenderViewHost());

  // Go back in the popup.  This should finish without crashing and should
  // reuse the old RenderViewHost.
  content::TestNavigationObserver back_load_observer(popup);
  controller->GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(webui_rvh, popup->GetRenderViewHost());
  EXPECT_TRUE(webui_rvh->IsRenderViewLive());
  EXPECT_EQ(content::BINDINGS_POLICY_WEB_UI,
            webui_rvh->GetMainFrame()->GetEnabledBindings());
}

}  // namespace
