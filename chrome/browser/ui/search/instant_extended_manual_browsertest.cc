// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_overlay.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
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
#include "net/dns/mock_host_resolver.h"

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
    bool result = PressBackspace();
    observer.WaitForDesiredOverlayState();
    return result;
  }

  void PressEnterAndWaitForNavigation() {
    content::WindowedNotificationObserver nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
    browser()->window()->GetLocationBar()->AcceptInput();
    nav_observer.Wait();
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

  string16 GetBlueText() {
    size_t start = 0, end = 0;
    omnibox()->GetSelectionBounds(&start, &end);
    if (start > end)
      std::swap(start, end);
    return omnibox()->GetText().substr(start, end - start);
  }

  bool GetSelectionState(bool* selected) {
    return GetBoolFromJS(instant()->GetOverlayContents(),
                         "google.ac.gs().api.i()", selected);
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
  FocusOmniboxAndWaitForInstantExtendedSupport();

  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(omnibox()->model()->has_focus());

  content::WebContents* overlay_tab = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay_tab);

  // Check that the page supports Instant, but it isn't showing.
  EXPECT_TRUE(instant()->overlay_->supports_instant());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedManualTest,
                       MANUAL_BackspaceFromQueryToSameQueryAndSearch) {
  set_browser(browser());
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Type "face" and expect Google to set gray text for "book" to suggest
  // [facebook], the query.
  SetOmniboxTextAndWaitForOverlayToShow("face");
  EXPECT_EQ(ASCIIToUTF16("face"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("book"), omnibox()->GetInstantSuggestion());

  // Backspace to the text "fac".
  EXPECT_TRUE(PressBackspaceAndWaitForSuggestions());
  EXPECT_EQ(ASCIIToUTF16("fac"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("ebook"), omnibox()->GetInstantSuggestion());

  // Press Enter. The page should show search results for [fac].
  EXPECT_TRUE(PressEnterAndWaitForNavigationWithTitle(
      instant()->GetOverlayContents(),
      ASCIIToUTF16("fac - Google Search")));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedManualTest,
                       MANUAL_BackspaceFromQueryToOtherQueryAndSearch) {
  set_browser(browser());
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Type "fan" and expect Google to set gray text to "dango" to suggest
  // [fandango], the query.
  SetOmniboxTextAndWaitForOverlayToShow("fan");
  EXPECT_EQ(ASCIIToUTF16("fan"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("dango"), omnibox()->GetInstantSuggestion());

  // Backspace to the text "fa". Expect Google to set gray text for "cebook" to
  // suggest [facebook], the query.
  EXPECT_TRUE(PressBackspaceAndWaitForSuggestions());
  EXPECT_EQ(ASCIIToUTF16("fa"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("cebook"), omnibox()->GetInstantSuggestion());

  // Press Enter. Instant should clear gray text and submit the query [fa].
  EXPECT_TRUE(PressEnterAndWaitForNavigationWithTitle(
      instant()->GetOverlayContents(),
      ASCIIToUTF16("fa - Google Search")));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedManualTest,
                       MANUAL_BackspaceFromUrlToNonSelectedUrlAndSearch) {
  set_browser(browser());
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Type "facebook.c" and expect Google to set blue text to "om" to suggest
  // http://www.facebook.com/, the URL.
  SetOmniboxTextAndWaitForOverlayToShow("facebook.c");
  EXPECT_EQ(ASCIIToUTF16("facebook.com"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("om"), GetBlueText());
  bool selected = false;
  EXPECT_TRUE(GetSelectionState(&selected));
  EXPECT_TRUE(selected);

  // Backspace to the text "facebook.c". Expect no suggestion text and no
  // selected suggestion.
  // Page won't actually show because it's showing "press enter to search".
  EXPECT_TRUE(PressBackspaceAndWaitForSuggestions());
  EXPECT_EQ(ASCIIToUTF16("facebook.c"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetBlueText());
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
  EXPECT_TRUE(GetSelectionState(&selected));
  EXPECT_FALSE(selected);

  // Press Enter. Instant should submit the query "facebook.c".
  EXPECT_TRUE(PressEnterAndWaitForNavigationWithTitle(
      active_tab(), ASCIIToUTF16("facebook.c - Google Search")));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedManualTest,
                       MANUAL_BackspaceFromUrlToUrlAndNavigate) {
  set_browser(browser());
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Type "facebook.com/" and expect Google to set blue text to "login.php" to
  // suggest http://www.facebook.com/login.php, the URL.
  SetOmniboxTextAndWaitForOverlayToShow("facebook.com/");
  EXPECT_EQ(ASCIIToUTF16("facebook.com/login.php"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("login.php"), GetBlueText());
  bool selected = false;
  EXPECT_TRUE(GetSelectionState(&selected));
  EXPECT_TRUE(selected);

  // Backspace to the text "facebook.com/". There should be no suggestion text,
  // but the first suggestion should be selected.
  EXPECT_TRUE(PressBackspaceAndWaitForSuggestions());
  EXPECT_EQ(ASCIIToUTF16("facebook.com/"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetBlueText());
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
  selected = false;
  EXPECT_TRUE(GetSelectionState(&selected));
  EXPECT_TRUE(selected);

  // Press Enter. Instant should navigate to facebook.com.
  PressEnterAndWaitForNavigation();
  EXPECT_TRUE(GetActiveTabURL().DomainIs("facebook.com"));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedManualTest,
                       DISABLED_BackspaceFromQueryToSelectedUrlAndNavigate) {
  set_browser(browser());
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Type "a.cop" and expect Google to set gray text to "land" to suggest the
  // query [a.copland].
  SetOmniboxTextAndWaitForOverlayToShow("a.cop");
  EXPECT_EQ(ASCIIToUTF16("a.cop"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("land"), omnibox()->GetInstantSuggestion());

  // Backspace to the text "a.co". Expect no suggestion text, but the
  // first suggestion should be selected URL "a.co".
  EXPECT_TRUE(PressBackspaceAndWaitForSuggestions());
  EXPECT_EQ(ASCIIToUTF16("a.co"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetBlueText());
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
  bool selected = false;
  EXPECT_TRUE(GetSelectionState(&selected));
  EXPECT_TRUE(selected);

  // Press Enter. Instant should navigate to a.co/.
  PressEnterAndWaitForNavigation();
  EXPECT_TRUE(GetActiveTabURL().DomainIs("amazon.com"));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedManualTest,
                       DISABLED_BackspaceFromSelectedUrlToQueryAndSearch) {
  set_browser(browser());
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Type "e.co/" and expect the top suggestion to be the URL "e.co/".
  SetOmniboxTextAndWaitForOverlayToShow("e.co/");
  EXPECT_EQ(ASCIIToUTF16("e.co/"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
  EXPECT_EQ(ASCIIToUTF16(""), GetBlueText());
  bool selected = false;
  EXPECT_TRUE(GetSelectionState(&selected));
  EXPECT_TRUE(selected);

  // Backspace to the text "e.co". Expect Google to suggest the query [e.coli].
  EXPECT_TRUE(PressBackspaceAndWaitForOverlayToShow());
  EXPECT_EQ(ASCIIToUTF16("e.co"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("li"), omnibox()->GetInstantSuggestion());
  EXPECT_EQ(ASCIIToUTF16(""), GetBlueText());
  selected = true;
  EXPECT_TRUE(GetSelectionState(&selected));
  EXPECT_FALSE(selected);

  // Press Enter. Instant should search for "e.co".
  EXPECT_TRUE(PressEnterAndWaitForNavigationWithTitle(
      instant()->GetOverlayContents(),
      ASCIIToUTF16("e.co - Google Search")));
}
