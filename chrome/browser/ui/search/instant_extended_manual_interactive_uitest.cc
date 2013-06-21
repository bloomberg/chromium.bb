// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_overlay.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/search_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/mock_host_resolver.h"

// !!! IMPORTANT !!!
// These tests are run against a mock GWS using the web-page-replay system.
// If you change a test, you MUST re-record the mock GWS session.
// See: src/chrome/test/data/search/tools/instant_extended_manual_tests.py
// for details.

// Instant extended tests that need to be run manually because they need to
// talk to the external network. All tests in this file should be marked as
// "MANUAL_" unless they are disabled.
class InstantExtendedManualTest : public InProcessBrowserTest,
                                  public InstantTestBase {
 public:
  InstantExtendedManualTest() {
    host_resolver_proc_ = new net::RuleBasedHostResolverProc(NULL);
    host_resolver_proc_->AllowDirectLookup("*");
    scoped_host_resolver_proc_.reset(
        new net::ScopedDefaultHostResolverProc(host_resolver_proc_.get()));
  }

  virtual ~InstantExtendedManualTest() {
    scoped_host_resolver_proc_.reset();
    host_resolver_proc_ = NULL;
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    ASSERT_TRUE(StartsWithASCII(test_info->name(), "MANUAL_", true) ||
                StartsWithASCII(test_info->name(), "DISABLED_", true));
    // Make IsOffline() return false so we don't try to use the local overlay.
    disable_network_change_notifier_.reset(
        new net::NetworkChangeNotifier::DisableForTest());
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    disable_network_change_notifier_.reset();
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::EnableInstantExtendedAPIForTesting();
  }

  content::WebContents* active_tab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool PressBackspace() {
    return ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_BACK,
                                           false, false, false, false);
  }

  bool PressBackspaceAndWaitForSuggestions() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_INSTANT_SET_SUGGESTION,
        content::NotificationService::AllSources());
    bool result = PressBackspace();
    observer.Wait();
    return result;
  }

  bool PressBackspaceAndWaitForOverlayToShow() {
    InstantTestModelObserver observer(
        instant()->model(), SearchMode::MODE_SEARCH_SUGGESTIONS);
    return PressBackspace() && observer.WaitForExpectedOverlayState() ==
        SearchMode::MODE_SEARCH_SUGGESTIONS;
  }

  bool PressEnterAndWaitForNavigationWithTitle(content::WebContents* contents,
                                               const string16& title) {
    content::TitleWatcher title_watcher(contents, title);
    content::WindowedNotificationObserver nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
    browser()->window()->GetLocationBar()->AcceptInput();
    nav_observer.Wait();
    return title_watcher.WaitAndGetTitle() == title;
  }

  GURL GetActiveTabURL() {
    return active_tab()->GetController().GetActiveEntry()->GetURL();
  }

  bool GetSelectionState(bool* selected) {
    return GetBoolFromJS(instant()->GetOverlayContents(),
                         "google.ac.gs().api.i()", selected);
  }

  bool IsGooglePage(content::WebContents* contents) {
    bool is_google = false;
    if (!GetBoolFromJS(contents, "!!window.google", &is_google))
      return false;
    return is_google;
  }

 private:
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_proc_;
  scoped_ptr<net::ScopedDefaultHostResolverProc> scoped_host_resolver_proc_;
  scoped_ptr<net::NetworkChangeNotifier::DisableForTest>
      disable_network_change_notifier_;
};

IN_PROC_BROWSER_TEST_F(InstantExtendedManualTest, MANUAL_ShowsGoogleNTP) {
  set_browser(browser());
  instant()->SetInstantEnabled(false, true);
  instant()->SetInstantEnabled(true, false);
  FocusOmniboxAndWaitForInstantNTPSupport();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsGooglePage(active_tab));
}
