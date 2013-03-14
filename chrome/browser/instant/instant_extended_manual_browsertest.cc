// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/instant/instant_overlay.h"
#include "chrome/browser/instant/instant_test_utils.h"
#include "chrome/browser/instant/search.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mock_host_resolver.h"

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
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::search::EnableInstantExtendedAPIForTesting();
  }

  void FocusOmniboxAndWaitForInstantSupport() {
    content::WindowedNotificationObserver ntp_observer(
        chrome::NOTIFICATION_INSTANT_NTP_SUPPORT_DETERMINED,
        content::NotificationService::AllSources());
    content::WindowedNotificationObserver overlay_observer(
        chrome::NOTIFICATION_INSTANT_OVERLAY_SUPPORT_DETERMINED,
        content::NotificationService::AllSources());
    FocusOmnibox();
    ntp_observer.Wait();
    overlay_observer.Wait();
  }

 private:
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_proc_;
  scoped_ptr<net::ScopedDefaultHostResolverProc> scoped_host_resolver_proc_;
};

IN_PROC_BROWSER_TEST_F(InstantExtendedManualTest,
                       MANUAL_OmniboxFocusLoadsInstant) {
  set_browser(browser());

  // Explicitly unfocus the omnibox.
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_FALSE(omnibox()->model()->has_focus());

  // Delete any existing overlay.
  instant()->overlay_.reset();
  EXPECT_FALSE(instant()->GetOverlayContents());

  // Refocus the omnibox. The InstantController should've preloaded Instant.
  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(omnibox()->model()->has_focus());

  content::WebContents* overlay_tab = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay_tab);

  // Check that the page supports Instant, but it isn't showing.
  EXPECT_TRUE(instant()->overlay_->supports_instant());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}
