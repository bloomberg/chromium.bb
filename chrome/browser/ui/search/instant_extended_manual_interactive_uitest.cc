// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/search_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/search/search.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

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

  ~InstantExtendedManualTest() override {
    scoped_host_resolver_proc_.reset();
    host_resolver_proc_ = NULL;
  }

  void SetUpOnMainThread() override {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    ASSERT_TRUE(base::StartsWith(test_info->name(), "MANUAL_",
                                 base::CompareCase::SENSITIVE) ||
                base::StartsWith(test_info->name(), "DISABLED_",
                                 base::CompareCase::SENSITIVE));
    // Make IsOffline() return false so we don't try to use the local NTP.
    disable_network_change_notifier_.reset(
        new net::NetworkChangeNotifier::DisableForTest());
  }

  void TearDownOnMainThread() override {
    disable_network_change_notifier_.reset();
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    search::EnableQueryExtractionForTesting();
  }

  content::WebContents* active_tab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool IsGooglePage(content::WebContents* contents) {
    bool is_google = false;
    if (!GetBoolFromJS(contents, "!!window.google", &is_google))
      return false;
    return is_google;
  }

 private:
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_proc_;
  std::unique_ptr<net::ScopedDefaultHostResolverProc>
      scoped_host_resolver_proc_;
  std::unique_ptr<net::NetworkChangeNotifier::DisableForTest>
      disable_network_change_notifier_;
};

IN_PROC_BROWSER_TEST_F(InstantExtendedManualTest, MANUAL_SearchesFromFakebox) {
  set_browser(browser());

  FocusOmnibox();
  // Open a new tab page.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  observer.Wait();
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(IsGooglePage(active_tab));

  // Click in the fakebox and expect invisible focus.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  bool fakebox_is_present = false;
  content::WindowedNotificationObserver focus_observer(
      chrome::NOTIFICATION_OMNIBOX_FOCUS_CHANGED,
      content::NotificationService::AllSources());
  ASSERT_TRUE(GetBoolFromJS(active_tab, "!!document.querySelector('#fkbx')",
                            &fakebox_is_present));
  ASSERT_TRUE(fakebox_is_present);
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.querySelector('#fkbx').click()"));
  focus_observer.Wait();
  EXPECT_EQ(OMNIBOX_FOCUS_INVISIBLE, omnibox()->model()->focus_state());

  // Type "test".
  const ui::KeyboardCode query[] = {
    ui::VKEY_T, ui::VKEY_E, ui::VKEY_S, ui::VKEY_T,
    ui::VKEY_UNKNOWN
  };
  for (size_t i = 0; query[i] != ui::VKEY_UNKNOWN; i++) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), query[i],
                                                false, false, false, false));
  }

  // The omnibox should say "test" and have visible focus.
  EXPECT_EQ("test", GetOmniboxText());
  EXPECT_EQ(OMNIBOX_FOCUS_VISIBLE, omnibox()->model()->focus_state());

  // Pressing enter should search for [test].
  const base::string16& search_title =
      base::ASCIIToUTF16("test - Google Search");
  content::TitleWatcher title_watcher(active_tab, search_title);
  PressEnterAndWaitForNavigation();
  EXPECT_EQ(search_title, title_watcher.WaitAndGetTitle());
}
