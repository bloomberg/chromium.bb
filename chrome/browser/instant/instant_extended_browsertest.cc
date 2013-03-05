// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/prefs/pref_service.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/instant/instant_ntp.h"
#include "chrome/browser/instant/instant_overlay.h"
#include "chrome/browser/instant/instant_service.h"
#include "chrome/browser/instant/instant_service_factory.h"
#include "chrome/browser/instant/instant_tab.h"
#include "chrome/browser/instant/instant_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/browser_test_utils.h"

class InstantExtendedTest : public InstantTestBase {
 public:
  InstantExtendedTest()
      : on_most_visited_change_calls_(0),
        most_visited_items_count_(0),
        first_most_visited_item_id_(0) {
  }
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::search::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server_.Start());
    instant_url_ = https_test_server_.
        GetURL("files/instant_extended.html?strk=1&");
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

  std::string GetOmniboxText() {
    return UTF16ToUTF8(omnibox()->GetText());
  }

  void SendDownArrow() {
    omnibox()->model()->OnUpOrDownKeyPressed(1);
    // Wait for JavaScript to run the key handler by executing a blank script.
    EXPECT_TRUE(ExecuteScript(std::string()));
  }

  void SendUpArrow() {
    omnibox()->model()->OnUpOrDownKeyPressed(-1);
    // Wait for JavaScript to run the key handler by executing a blank script.
    EXPECT_TRUE(ExecuteScript(std::string()));
  }

  void SendEscape() {
    omnibox()->model()->OnEscapeKeyPressed();
    // Wait for JavaScript to run the key handler by executing a blank script.
    EXPECT_TRUE(ExecuteScript(std::string()));
  }

  bool UpdateSearchState(content::WebContents* contents) WARN_UNUSED_RESULT {
    return GetIntFromJS(contents, "onMostVisitedChangedCalls",
                        &on_most_visited_change_calls_) &&
           GetIntFromJS(contents, "mostVisitedItemsCount",
                        &most_visited_items_count_) &&
           GetIntFromJS(contents, "firstMostVisitedItemId",
                        &first_most_visited_item_id_);
  }

  int on_most_visited_change_calls_;
  int most_visited_items_count_;
  int first_most_visited_item_id_;
};

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, ExtendedModeIsOn) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  EXPECT_TRUE(instant()->extended_enabled_);
}

// Test that Instant is preloaded when the omnibox is focused.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxFocusLoadsInstant) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

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

  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);

  // Check that the page supports Instant, but it isn't showing.
  EXPECT_TRUE(instant()->overlay_->supports_instant());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Adding a new tab shouldn't delete or recreate the overlay; otherwise,
  // what's the point of preloading?
  AddBlankTabAndShow(browser());
  EXPECT_EQ(overlay, instant()->GetOverlayContents());

  // Unfocusing and refocusing the omnibox should also preserve the overlay.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  FocusOmnibox();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_EQ(overlay, instant()->GetOverlayContents());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, InputShowsOverlay) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Typing in the omnibox should show the overlay.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  EXPECT_EQ(overlay, instant()->GetOverlayContents());
}

// Test that middle clicking on a suggestion opens the result in a new tab.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MiddleClickOnSuggestionOpensInNewTab) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Typing in the omnibox should show the overlay.
  SetOmniboxTextAndWaitForOverlayToShow("santa");
  EXPECT_TRUE(instant()->IsOverlayingSearchResults());

  // Create an event listener that opens the top suggestion in a new tab.
  EXPECT_TRUE(ExecuteScript(
      "var rid = getApiHandle().nativeSuggestions[0].rid;"
      "document.body.addEventListener('click', function() {"
        "chrome.embeddedSearch.navigateContentWindow(rid, 2);"
      "});"
      ));

  content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());

  // Click to trigger the event listener.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Wait for the new tab to be added.
  observer.Wait();

  // Check that the new tab URL is as expected.
  content::WebContents* new_tab_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(new_tab_contents->GetURL().spec(), instant_url_.spec()+"q=santa");

  // Check that there are now two tabs.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// Test that omnibox text is correctly set when overlay is committed with Enter.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       OmniboxTextUponEnterCommit) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // The page will autocomplete once we set the omnibox value.
  EXPECT_TRUE(ExecuteScript("suggestion = 'santa claus';"));

  // Set the text, and wait for suggestions to show up.
  SetOmniboxTextAndWaitForOverlayToShow("santa");
  EXPECT_EQ(ASCIIToUTF16("santa"), omnibox()->GetText());

  // Test that the current suggestion is correctly set.
  EXPECT_EQ(ASCIIToUTF16(" claus"), omnibox()->GetInstantSuggestion());

  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();

  // 'Enter' commits the query as it was typed.
  EXPECT_EQ(ASCIIToUTF16("santa"), omnibox()->GetText());

  // Suggestion should be cleared at this point.
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
}

// Test that omnibox text is correctly set when committed with focus lost.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       OmniboxTextUponFocusLostCommit) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Set autocomplete text (grey text).
  EXPECT_TRUE(ExecuteScript("suggestion = 'johnny depp';"));

  // Set the text, and wait for suggestions to show up.
  SetOmniboxTextAndWaitForOverlayToShow("johnny");
  EXPECT_EQ(ASCIIToUTF16("johnny"), omnibox()->GetText());

  // Test that the current suggestion is correctly set.
  EXPECT_EQ(ASCIIToUTF16(" depp"), omnibox()->GetInstantSuggestion());

  // Commit the overlay by lost focus (e.g. clicking on the page).
  instant()->CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);

  // Search term extraction should kick in with the autocompleted text.
  EXPECT_EQ(ASCIIToUTF16("johnny depp"), omnibox()->GetText());

  // Suggestion should be cleared at this point.
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
}

// Test that omnibox text is correctly set when clicking on committed SERP.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       OmniboxTextUponFocusedCommittedSERP) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Do a search and commit it.
  SetOmniboxTextAndWaitForOverlayToShow("hello k");
  EXPECT_EQ(ASCIIToUTF16("hello k"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();

  // With a committed results page, do a search by unfocusing the omnibox and
  // focusing the contents.
  SetOmniboxText("hello");
  // Calling handleOnChange manually to make sure it is called before the
  // Focus() call below.
  EXPECT_TRUE(content::ExecuteScript(instant()->instant_tab()->contents(),
                                     "suggestion = 'hello kitty';"
                                     "handleOnChange();"));
  instant()->instant_tab()->contents()->GetView()->Focus();

  // Search term extraction should kick in with the autocompleted text.
  EXPECT_EQ(ASCIIToUTF16("hello kitty"), omnibox()->GetText());

  // Suggestion should be cleared at this point.
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
}

// This test simulates a search provider using the InstantExtended API to
// navigate through the suggested results and back to the original user query.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NavigateSuggestionsWithArrowKeys) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  SetOmniboxTextAndWaitForOverlayToShow("hello");
  EXPECT_EQ("hello", GetOmniboxText());

  SendDownArrow();
  EXPECT_EQ("result 1", GetOmniboxText());
  SendDownArrow();
  EXPECT_EQ("result 2", GetOmniboxText());
  SendUpArrow();
  EXPECT_EQ("result 1", GetOmniboxText());
  SendUpArrow();
  EXPECT_EQ("hello", GetOmniboxText());

  // Ensure that the API's value is set correctly.
  std::string result;
  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "window.chrome.searchBox.value",
                              &result));
  EXPECT_EQ("hello", result);

  EXPECT_TRUE(HasUserInputInProgress());
  // TODO(beaudoin): Figure out why this fails.
  // EXPECT_FALSE(HasTemporaryText());


  // Commit the search by pressing Enter.
  // TODO(sreeram): Enable this check once @mathp's CL lands:
  //     https://codereview.chromium.org/12179025/
  // browser()->window()->GetLocationBar()->AcceptInput();
  // EXPECT_EQ("hello", GetOmniboxText());
}

// This test simulates a search provider using the InstantExtended API to
// navigate through the suggested results and hitting escape to get back to the
// original user query.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NavigateSuggestionsAndHitEscape) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  SetOmniboxTextAndWaitForOverlayToShow("hello");
  EXPECT_EQ("hello", GetOmniboxText());

  SendDownArrow();
  EXPECT_EQ("result 1", GetOmniboxText());
  SendDownArrow();
  EXPECT_EQ("result 2", GetOmniboxText());
  SendEscape();
  EXPECT_EQ("hello", GetOmniboxText());

  // Ensure that the API's value is set correctly.
  std::string result;
  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "window.chrome.searchBox.value",
                              &result));
  EXPECT_EQ("hello", result);

  EXPECT_TRUE(HasUserInputInProgress());
  EXPECT_FALSE(HasTemporaryText());

  // Commit the search by pressing Enter.
  // TODO(sreeram): Enable this check once @mathp's CL lands:
  //     https://codereview.chromium.org/12179025/
  // browser()->window()->GetLocationBar()->AcceptInput();
  // EXPECT_EQ("hello", GetOmniboxText());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NTPIsPreloaded) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPIsUsedInNewTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(ntp_contents, active_tab);
  EXPECT_TRUE(chrome::search::IsInstantNTP(active_tab));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPIsUsedInSameTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(ntp_contents, active_tab);
  EXPECT_TRUE(chrome::search::IsInstantNTP(active_tab));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxHasFocusOnNewTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Explicitly unfocus the omnibox.
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_FALSE(omnibox()->model()->has_focus());

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Omnibox should have focus.
  EXPECT_TRUE(omnibox()->model()->has_focus());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxEmptyOnNewTabPage) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // Omnibox should be empty.
  EXPECT_TRUE(omnibox()->GetText().empty());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoFaviconOnNewTabPage) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // No favicon should be shown.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(active_tab);
  EXPECT_FALSE(favicon_tab_helper->ShouldDisplayFavicon());

  // Favicon should be shown off the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  favicon_tab_helper = FaviconTabHelper::FromWebContents(active_tab);
  EXPECT_TRUE(favicon_tab_helper->ShouldDisplayFavicon());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, InputOnNTPDoesntShowOverlay) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Navigate to the NTP.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // Typing in the omnibox should not show the overlay.
  SetOmniboxText("query");
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, ProcessIsolation) {
  // Prior to setup, Instant has an overlay with a failed "google.com" load in
  // it, which is rendered in the dedicated Instant renderer process.
  //
  // TODO(sreeram): Fix this up when we stop doing crazy things on init.
  InstantService* instant_service =
        InstantServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);
  EXPECT_EQ(1, instant_service->GetInstantProcessCount());

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // The registered Instant render process should still exist.
  EXPECT_EQ(1, instant_service->GetInstantProcessCount());

  // And the Instant overlay and ntp should live inside it.
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      overlay->GetRenderProcessHost()->GetID()));
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      ntp_contents->GetRenderProcessHost()->GetID()));

  // Navigating to the NTP should use the Instant render process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      active_tab->GetRenderProcessHost()->GetID()));

  // Navigating elsewhere should not use the Instant render process.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  EXPECT_FALSE(instant_service->IsInstantProcess(
      active_tab->GetRenderProcessHost()->GetID()));
}

// Verification of fix for BUG=176365.  Ensure that each Instant WebContents in
// a tab uses a new BrowsingInstance, to avoid conflicts in the
// NavigationController.
// Flaky: http://crbug.com/177516
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_UnrelatedSiteInstance) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Check that the uncommited ntp page and uncommited overlay have unrelated
  // site instances.
  // TODO(sreeram): |ntp_| is going away, so this check can be removed in the
  // future.
  content::WebContents* overlay = instant()->GetOverlayContents();
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_FALSE(overlay->GetSiteInstance()->IsRelatedSiteInstance(
      ntp_contents->GetSiteInstance()));

  // Type a query and hit enter to get a results page.  The overlay becomes the
  // active tab.
  SetOmniboxTextAndWaitForOverlayToShow("hello");
  EXPECT_EQ("hello", GetOmniboxText());
  browser()->window()->GetLocationBar()->AcceptInput();
  content::WebContents* first_active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(first_active_tab, overlay);
  scoped_refptr<content::SiteInstance> first_site_instance =
      first_active_tab->GetSiteInstance();
  EXPECT_FALSE(first_site_instance->IsRelatedSiteInstance(
      ntp_contents->GetSiteInstance()));

  // Navigating elsewhere gets us off of the commited page.  The next
  // query will give us a new |overlay| which we will then commit.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));

  // Show and commit the new overlay.
  SetOmniboxTextAndWaitForOverlayToShow("hello again");
  EXPECT_EQ("hello again", GetOmniboxText());
  browser()->window()->GetLocationBar()->AcceptInput();
  content::WebContents* second_active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(first_active_tab, second_active_tab);
  scoped_refptr<content::SiteInstance> second_site_instance =
      second_active_tab->GetSiteInstance();
  EXPECT_NE(first_site_instance, second_site_instance);
  EXPECT_FALSE(first_site_instance->IsRelatedSiteInstance(
      second_site_instance));
}

// Tests that suggestions are sanity checked.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, ValidatesSuggestions) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Do not set gray text that is not a suffix of the query.
  EXPECT_TRUE(ExecuteScript("behavior = 2"));
  EXPECT_TRUE(ExecuteScript("suggestion = 'potato'"));
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());

  omnibox()->RevertAll();

  // Do not set blue text that is not a valid URL completion.
  EXPECT_TRUE(ExecuteScript("behavior = 1"));
  EXPECT_TRUE(ExecuteScript("suggestion = 'this is not a url!'"));
  SetOmniboxTextAndWaitForOverlayToShow("this is");
  EXPECT_EQ(ASCIIToUTF16("this is"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());

  omnibox()->RevertAll();

  // Do not set gray text when blue text is already set.
  // First set up some blue text completion.
  EXPECT_TRUE(ExecuteScript("behavior = 1"));
  EXPECT_TRUE(ExecuteScript("suggestion = 'www.example.com'"));
  SetOmniboxTextAndWaitForOverlayToShow("http://www.ex");
  string16 text = omnibox()->GetText();
  EXPECT_EQ(ASCIIToUTF16("http://www.example.com"), text);
  size_t start = 0, end = 0;
  omnibox()->GetSelectionBounds(&start, &end);
  if (start > end)
    std::swap(start, end);
  EXPECT_EQ(ASCIIToUTF16("ample.com"), text.substr(start, end - start));
  EXPECT_TRUE(ExecuteScript("behavior = 2"));
  EXPECT_TRUE(ExecuteScript("suggestion = 'www.example.com rocks'"));
  // Now try to set gray text for the same query.
  SetOmniboxText("http://www.ex");
  EXPECT_EQ(ASCIIToUTF16("http://www.example.com"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());

  omnibox()->RevertAll();

  // When asked to suggest blue text in verbatim mode, suggest the exact
  // omnibox text rather than using the supplied suggestion text.
  EXPECT_TRUE(ExecuteScript("behavior = 1"));
  EXPECT_TRUE(ExecuteScript("suggestion = 'www.example.com/q'"));
  SetOmniboxText("www.example.com/q");
  omnibox()->OnBeforePossibleChange();
  SetOmniboxText("www.example.com/");
  omnibox()->OnAfterPossibleChange();
  EXPECT_EQ(ASCIIToUTF16("www.example.com/"), omnibox()->GetText());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, MostVisited) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_SENT_MOST_VISITED_ITEMS,
      content::NotificationService::AllSources());
  // Initialize Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Get a handle to the NTP and the current state of the JS.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* overlay = instant()->ntp_->contents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Wait for most visited data to be ready, if necessary.
  if (on_most_visited_change_calls_ == 0) {
    observer.Wait();
    EXPECT_TRUE(UpdateSearchState(overlay));
  }

  EXPECT_EQ(1, on_most_visited_change_calls_);

  // Make sure we have at least two Most Visited Items and save that number.
  // TODO(pedrosimonetti): For now, we're relying on the fact that the Top
  // Sites will have at lease two items in it. The correct approach would be
  // adding those items to the Top Sites manually before starting the test.
  EXPECT_GT(most_visited_items_count_, 1);
  int old_most_visited_items_count = most_visited_items_count_;

  // Delete the fist Most Visited Item.
  int rid = first_most_visited_item_id_;
  std::ostringstream stream;
  stream << "newTabPageHandle.deleteMostVisitedItem(" << rid << ")";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Make sure we have one less item in there.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count - 1);

  // Undo the deletion of the fist Most Visited Item.
  stream.str(std::string());
  stream << "newTabPageHandle.undoMostVisitedDeletion(" << rid << ")";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Make sure we have the same number of items as before.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count);

  // Delete the fist Most Visited Item.
  rid = first_most_visited_item_id_;
  stream.str(std::string());
  stream << "newTabPageHandle.deleteMostVisitedItem(" << rid << ")";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Delete the second Most Visited Item.
  rid = first_most_visited_item_id_;
  stream.str(std::string());
  stream << "newTabPageHandle.deleteMostVisitedItem(" << rid << ")";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Make sure we have two less items in there.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count - 2);

  // Delete the second Most Visited Item.
  stream.str(std::string());
  stream << "newTabPageHandle.undoAllMostVisitedDeletions()";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Make sure we have the same number of items as before.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count);
}

// Only implemented in Views and Mac currently: http://crbug.com/164723
#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
#define MAYBE_HomeButtonAffectsMargin HomeButtonAffectsMargin
#else
#define MAYBE_HomeButtonAffectsMargin DISABLED_HomeButtonAffectsMargin
#endif
// Check that toggling the state of the home button changes the start-edge
// margin and width.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, MAYBE_HomeButtonAffectsMargin) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  // Get the current value of the start-edge margin and width.
  int start_margin;
  int width;
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(GetIntFromJS(overlay, "chrome.searchBox.startMargin",
      &start_margin));
  EXPECT_TRUE(GetIntFromJS(overlay, "chrome.searchBox.width", &width));

  // Toggle the home button visibility pref.
  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  bool show_home = profile_prefs->GetBoolean(prefs::kShowHomeButton);
  profile_prefs->SetBoolean(prefs::kShowHomeButton, !show_home);

  // Make sure the margin and width changed.
  int new_start_margin;
  int new_width;
  EXPECT_TRUE(GetIntFromJS(overlay, "chrome.searchBox.startMargin",
      &new_start_margin));
  EXPECT_TRUE(GetIntFromJS(overlay, "chrome.searchBox.width", &new_width));
  EXPECT_NE(start_margin, new_start_margin);
  EXPECT_NE(width, new_width);
  EXPECT_EQ(new_width - width, start_margin - new_start_margin);
}

// Commit does not happen on Mac: http://crbug.com/178520
#if defined(OS_MACOSX)
#define MAYBE_CommitWhenFocusLostInFullHeight \
        DISABLED_CommitWhenFocusLostInFullHeight
#else
#define MAYBE_CommitWhenFocusLostInFullHeight CommitWhenFocusLostInFullHeight
#endif
// Test that the overlay is committed when the omnibox loses focus when it is
// shown at 100% height.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MAYBE_CommitWhenFocusLostInFullHeight) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());

  // Typing in the omnibox should show the overlay.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_TRUE(instant()->IsOverlayingSearchResults());
  EXPECT_EQ(overlay, instant()->GetOverlayContents());

  // Explicitly unfocus the omnibox without triggering a click. Note that this
  // doesn't actually change the focus state of the omnibox, only what the
  // Instant controller sees it as.
  omnibox()->model()->OnWillKillFocus(NULL);
  omnibox()->model()->OnKillFocus();

  // Confirm that the overlay has been committed.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(overlay, active_tab);
}

// Test that the overlay is committed when shown at 100% height without focus
// in the omnibox.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       CommitWhenShownInFullHeightWithoutFocus) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());

  // Create an observer to wait for the commit.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_COMMITTED,
      content::NotificationService::AllSources());

  // Typing in the omnibox should show the overlay. Don't wait for the overlay
  // to show however.
  SetOmniboxText("query");

  // Explicitly unfocus the omnibox without triggering a click. Note that this
  // doesn't actually change the focus state of the omnibox, only what the
  // Instant controller sees it as.
  omnibox()->model()->OnWillKillFocus(NULL);
  omnibox()->model()->OnKillFocus();

  // Wait for the overlay to show.
  observer.Wait();

  // Confirm that the overlay has been committed.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(overlay, active_tab);
}
