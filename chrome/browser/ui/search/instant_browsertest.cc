// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_overlay.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

class InstantTest : public InProcessBrowserTest, public InstantTestBase {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(test_server()->Start());
    GURL instant_url = test_server()->GetURL("files/instant.html?");
    InstantTestBase::Init(instant_url);
  }

  bool UpdateSearchState(content::WebContents* contents) WARN_UNUSED_RESULT {
    return GetIntFromJS(contents, "onvisibilitycalls", &onvisibilitycalls_) &&
           GetIntFromJS(contents, "onchangecalls", &onchangecalls_) &&
           GetIntFromJS(contents, "onsubmitcalls", &onsubmitcalls_) &&
           GetIntFromJS(contents, "oncancelcalls", &oncancelcalls_) &&
           GetIntFromJS(contents, "onresizecalls", &onresizecalls_) &&
           GetStringFromJS(contents, "value", &value_) &&
           GetBoolFromJS(contents, "verbatim", &verbatim_) &&
           GetIntFromJS(contents, "height", &height_);
  }

  int onvisibilitycalls_;
  int onchangecalls_;
  int onsubmitcalls_;
  int oncancelcalls_;
  int onresizecalls_;

  std::string value_;
  bool verbatim_;
  int height_;
};

// Test that Instant is preloaded when the omnibox is focused.
IN_PROC_BROWSER_TEST_F(InstantTest, OmniboxFocusLoadsInstant) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

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

  // Doing a search should also use the same preloaded page.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  EXPECT_EQ(overlay, instant()->GetOverlayContents());
}

// Flakes on Windows and Mac: http://crbug.com/170677
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_OnChangeEvent DISABLED_OnChangeEvent
#else
#define MAYBE_OnChangeEvent OnChangeEvent
#endif
// Test that the onchange event is dispatched upon typing in the omnibox.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnChangeEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Use the Instant page as the active tab, so we can exploit its visibility
  // handler to check visibility transitions.
  ui_test_utils::NavigateToURL(browser(), instant_url());
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  int active_tab_onvisibilitycalls = -1;
  EXPECT_TRUE(GetIntFromJS(active_tab, "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(0, active_tab_onvisibilitycalls);

  // Typing "query" into the omnibox causes one or more onchange events. The
  // page suggested "query suggestion" is inline autocompleted into the omnibox,
  // causing another onchange event.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query suggestion"), omnibox()->GetText());
  int min_onchangecalls = 2;

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Change the query and confirm more onchange events are sent.
  SetOmniboxText("search");
  ++min_onchangecalls;

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_LE(min_onchangecalls, onchangecalls_);

  // The overlay was shown once, and the active tab was never hidden.
  EXPECT_EQ(1, onvisibilitycalls_);
  active_tab_onvisibilitycalls = -1;
  EXPECT_TRUE(GetIntFromJS(active_tab, "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(0, active_tab_onvisibilitycalls);
}

// Test that the onsubmit event is dispatched upon pressing Enter.
IN_PROC_BROWSER_TEST_F(InstantTest, OnSubmitEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();
  SetOmniboxTextAndWaitForOverlayToShow("search");

  // Stash a reference to the overlay, so we can refer to it after commit.
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);

  // The state of the searchbox before the commit.
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ("search", value_);
  EXPECT_FALSE(verbatim_);
  EXPECT_EQ(0, onsubmitcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);

  // Before the commit, the active tab is the NTP (i.e., not Instant).
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(overlay, active_tab);
  EXPECT_EQ(1, active_tab->GetController().GetEntryCount());
  EXPECT_EQ(std::string(chrome::kAboutBlankURL),
            omnibox()->model()->PermanentURL().spec());

  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();

  // After the commit, Instant should not be showing.
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // The old overlay is deleted and a new one is created.
  EXPECT_TRUE(instant()->GetOverlayContents());
  EXPECT_NE(instant()->GetOverlayContents(), overlay);

  // Check that the current active tab is indeed what was once the overlay.
  EXPECT_EQ(overlay, browser()->tab_strip_model()->GetActiveWebContents());

  // We should have two navigation entries, one for the NTP, and one for the
  // Instant search that was committed.
  EXPECT_EQ(2, overlay->GetController().GetEntryCount());

  // Check that the omnibox contains the Instant URL we loaded.
  EXPECT_EQ(instant_url(), omnibox()->model()->PermanentURL());

  // Check that the searchbox API values have been reset.
  std::string value;
  EXPECT_TRUE(GetStringFromJS(overlay,
                              "chrome.embeddedSearch.searchBox.value", &value));
  EXPECT_EQ("", value);

  // However, the page should've correctly received the committed query.
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ("search", value_);
  EXPECT_TRUE(verbatim_);
  EXPECT_EQ(1, onsubmitcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);
}

// Test that the oncancel event is dispatched upon clicking on the overlay.
IN_PROC_BROWSER_TEST_F(InstantTest, OnCancelEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  FocusOmniboxAndWaitForInstantSupport();
  SetOmniboxTextAndWaitForOverlayToShow("search");

  // Stash a reference to the overlay, so we can refer to it after commit.
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);

  // The state of the searchbox before the commit.
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ("search", value_);
  EXPECT_FALSE(verbatim_);
  EXPECT_EQ(0, oncancelcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);

  // Before the commit, the active tab is the NTP (i.e., not Instant).
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(overlay, active_tab);
  EXPECT_EQ(1, active_tab->GetController().GetEntryCount());
  EXPECT_EQ(std::string(chrome::kAboutBlankURL),
            omnibox()->model()->PermanentURL().spec());

  // Commit the search by clicking on the overlay.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // After the commit, Instant should not be showing.
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // The old overlay is deleted and a new one is created.
  EXPECT_TRUE(instant()->GetOverlayContents());
  EXPECT_NE(instant()->GetOverlayContents(), overlay);

  // Check that the current active tab is indeed what was once the overlay.
  EXPECT_EQ(overlay, browser()->tab_strip_model()->GetActiveWebContents());

  // We should have two navigation entries, one for the NTP, and one for the
  // Instant search that was committed.
  EXPECT_EQ(2, overlay->GetController().GetEntryCount());

  // Check that the omnibox contains the Instant URL we loaded.
  EXPECT_EQ(instant_url(), omnibox()->model()->PermanentURL());

  // Check that the searchbox API values have been reset.
  std::string value;
  EXPECT_TRUE(GetStringFromJS(overlay,
                              "chrome.embeddedSearch.searchBox.value", &value));
  EXPECT_EQ("", value);

  // However, the page should've correctly received the committed query.
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ("search", value_);
  EXPECT_TRUE(verbatim_);
  EXPECT_EQ(1, oncancelcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);
}

// Test that the onreisze event is dispatched upon typing in the omnibox.
IN_PROC_BROWSER_TEST_F(InstantTest, OnResizeEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_EQ(0, onresizecalls_);
  EXPECT_EQ(0, height_);

  // Type a query into the omnibox. This should cause an onresize() event, with
  // a valid (non-zero) height.
  SetOmniboxTextAndWaitForOverlayToShow("search");

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_EQ(1, onresizecalls_);
  EXPECT_LT(0, height_);
}

// Test that the INSTANT_COMPLETE_NOW behavior works as expected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsCompletedNow) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Tell the JS to request the given behavior.
  EXPECT_TRUE(ExecuteScript("behavior = 'now'"));

  // Type a query, causing the hardcoded "query suggestion" to be returned.
  SetOmniboxTextAndWaitForOverlayToShow("query");

  // Get what's showing in the omnibox, and what's highlighted.
  string16 text = omnibox()->GetText();
  size_t start = 0, end = 0;
  omnibox()->GetSelectionBounds(&start, &end);
  if (start > end)
    std::swap(start, end);

  EXPECT_EQ(ASCIIToUTF16("query suggestion"), text);
  EXPECT_EQ(ASCIIToUTF16(" suggestion"), text.substr(start, end - start));
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
}

// Test that the INSTANT_COMPLETE_NEVER behavior works as expected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsCompletedNever) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Tell the JS to request the given behavior.
  EXPECT_TRUE(ExecuteScript("behavior = 'never'"));

  // Type a query, causing the hardcoded "query suggestion" to be returned.
  SetOmniboxTextAndWaitForOverlayToShow("query");

  // Get what's showing in the omnibox, and what's highlighted.
  string16 text = omnibox()->GetText();
  size_t start = 0, end = 0;
  omnibox()->GetSelectionBounds(&start, &end);
  if (start > end)
    std::swap(start, end);

  EXPECT_EQ(ASCIIToUTF16("query"), text);
  EXPECT_EQ(ASCIIToUTF16(""), text.substr(start, end - start));
  EXPECT_EQ(ASCIIToUTF16(" suggestion"), omnibox()->GetInstantSuggestion());
}

// Test that a valid suggestion is accepted.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsValidObject) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Tell the JS to use the given suggestion.
  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: 'query completion' } ]"));

  // Type a query, causing "query completion" to be returned as the suggestion.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query completion"), omnibox()->GetText());
}

// Test that an invalid suggestion is rejected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsInvalidObject) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Tell the JS to use an object in an invalid format.
  EXPECT_TRUE(ExecuteScript("suggestion = { value: 'query completion' }"));

  // Type a query, but expect no suggestion.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query"), omnibox()->GetText());
}

// Test that various forms of empty suggestions are rejected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsEmpty) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_TRUE(ExecuteScript("suggestion = {}"));
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query"), omnibox()->GetText());

  omnibox()->RevertAll();

  EXPECT_TRUE(ExecuteScript("suggestion = []"));
  SetOmniboxTextAndWaitForOverlayToShow("query sugg");
  EXPECT_EQ(ASCIIToUTF16("query sugg"), omnibox()->GetText());

  omnibox()->RevertAll();

  EXPECT_TRUE(ExecuteScript("suggestion = [{}]"));
  SetOmniboxTextAndWaitForOverlayToShow("query suggest");
  EXPECT_EQ(ASCIIToUTF16("query suggest"), omnibox()->GetText());
}

// Tests that a previous search suggestion is not discarded if it's not stale.
IN_PROC_BROWSER_TEST_F(InstantTest, SearchSuggestionIsNotDiscarded) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query suggestion"), omnibox()->GetText());
  SetOmniboxText("query sugg");
  EXPECT_EQ(ASCIIToUTF16("query suggestion"), omnibox()->GetText());
}

// Test that Instant doesn't process URLs.
IN_PROC_BROWSER_TEST_F(InstantTest, RejectsURLs) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Note that we are not actually navigating to these URLs yet. We are just
  // typing them into the omnibox (without pressing Enter) and checking that
  // Instant doesn't try to process them.
  SetOmniboxText(content::kChromeUICrashURL);
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  SetOmniboxText(content::kChromeUIHangURL);
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  SetOmniboxText(content::kChromeUIKillURL);
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Make sure that the URLs were never sent to the overlay page.
  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_EQ("", value_);
}

// Test that Instant doesn't fire for intranet paths that look like searches.
// http://crbug.com/99836
IN_PROC_BROWSER_TEST_F(InstantTest, IntranetPathLooksLikeSearch) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Navigate to a URL that looks like a search (when the scheme is stripped).
  // It's okay if the host is bogus or the navigation fails, since we only care
  // that Instant doesn't act on it.
  ui_test_utils::NavigateToURL(browser(), GURL("http://baby/beluga"));
  EXPECT_EQ(ASCIIToUTF16("baby/beluga"), omnibox()->GetText());

  EXPECT_TRUE(instant()->GetOverlayContents());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

// Test that transitions between searches and non-searches work as expected.
IN_PROC_BROWSER_TEST_F(InstantTest, TransitionsBetweenSearchAndURL) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Type a search, and immediately a URL, without waiting for Instant to show.
  // The page is told about the search. Though the page isn't told about the
  // subsequent URL, it invalidates the search, so a blank query is sent in its
  // place to indicate that the search is "out of date".
  SetOmniboxText("query");
  SetOmniboxText("http://monstrous/nightmare");
  int min_onchangecalls = 2;

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_EQ("", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Type a search. Instant should show.
  SetOmniboxTextAndWaitForOverlayToShow("search");
  ++min_onchangecalls;

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_TRUE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  EXPECT_EQ("search", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Type another URL. The overlay should be hidden.
  SetOmniboxText("http://terrible/terror");
  ++min_onchangecalls;

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_EQ("", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Type the same search as before.
  SetOmniboxTextAndWaitForOverlayToShow("search");
  min_onchangecalls++;

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_TRUE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  EXPECT_EQ("search", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Revert the omnibox.
  omnibox()->RevertAll();
  min_onchangecalls++;

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_EQ("", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
}

// Test that Instant can't be fooled into committing a URL.
IN_PROC_BROWSER_TEST_F(InstantTest, DoesNotCommitURLsOne) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Type a URL. The Instant overlay shouldn't be showing.
  SetOmniboxText("http://deadly/nadder");
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Unfocus and refocus the omnibox.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  FocusOmnibox();

  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);

  // The omnibox text hasn't changed, so Instant still shouldn't be showing.
  EXPECT_EQ(ASCIIToUTF16("http://deadly/nadder"), omnibox()->GetText());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Commit the URL. The omnibox should reflect the URL minus the scheme.
  browser()->window()->GetLocationBar()->AcceptInput();
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(overlay, active_tab);
  EXPECT_EQ(ASCIIToUTF16("deadly/nadder"), omnibox()->GetText());

  // Instant shouldn't have done anything.
  EXPECT_EQ(overlay, instant()->GetOverlayContents());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

// Test that Instant can't be fooled into committing a URL.
IN_PROC_BROWSER_TEST_F(InstantTest, DoesNotCommitURLsTwo) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Type a query. This causes the overlay to be shown.
  SetOmniboxTextAndWaitForOverlayToShow("query");

  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);

  // Type a URL. This causes the overlay to be hidden.
  SetOmniboxText("http://hideous/zippleback");
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Pretend the omnibox got focus. It already had focus, so we are just trying
  // to tickle a different code path.
  instant()->OmniboxFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                                 OMNIBOX_FOCUS_CHANGE_EXPLICIT, NULL);

  // Commit the URL. As before, check that Instant wasn't committed.
  browser()->window()->GetLocationBar()->AcceptInput();
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(overlay, active_tab);
  EXPECT_EQ(ASCIIToUTF16("hideous/zippleback"), omnibox()->GetText());

  // As before, Instant shouldn't have done anything.
  EXPECT_EQ(overlay, instant()->GetOverlayContents());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

// Test that a non-Instant search provider shows no overlays.
IN_PROC_BROWSER_TEST_F(InstantTest, NonInstantSearchProvider) {
  GURL instant_url = test_server()->GetURL("files/empty.html");
  InstantTestBase::Init(instant_url);
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus the omnibox. When the support determination response comes back,
  // Instant will destroy the non-Instant page, and attempt to recreate it.
  // We can know this happened by looking at the blacklist.
  EXPECT_EQ(0, instant()->blacklisted_urls_[instant_url.spec()]);
  FocusOmniboxAndWaitForInstantSupport();
  EXPECT_EQ(1, instant()->blacklisted_urls_[instant_url.spec()]);
}

// Test that the renderer doesn't crash if JavaScript is blocked.
IN_PROC_BROWSER_TEST_F(InstantTest, NoCrashOnBlockedJS) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Wait for notification that the Instant API has been determined. As long as
  // we get the notification we're good (the renderer didn't crash).
  FocusOmniboxAndWaitForInstantSupport();
}

// Test that the overlay and active tab's visibility states are set correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, PageVisibility) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* overlay = instant()->GetOverlayContents();

  // Inititally, the active tab is showing; the overlay is not.
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(overlay, false));

  // Type a query and wait for Instant to show.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(overlay, true));

  // Deleting the omnibox text should hide the overlay.
  SetOmniboxText("");
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(overlay, false));

  // Typing a query should show the overlay again.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(overlay, true));

  // Commit the overlay.
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_EQ(overlay, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(CheckVisibilityIs(overlay, true));
}

// Test that the task manager identifies Instant's overlay correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, TaskManagerPrefix) {
  // The browser starts with a new tab, so there's just one renderer initially.
  TaskManagerModel* task_manager = TaskManager::GetInstance()->model();
  task_manager->StartUpdating();
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);

  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_INSTANT_OVERLAY_PREFIX, string16());

  // There should be no Instant overlay yet.
  for (int i = 0; i < task_manager->ResourceCount(); ++i) {
    string16 title = task_manager->GetResourceTitle(i);
    EXPECT_FALSE(StartsWith(title, prefix, true)) << title << " vs " << prefix;
  }

  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  // Now there should be two renderers, the second being the Instant overlay.
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

  int instant_overlays = 0;
  for (int i = 0; i < task_manager->ResourceCount(); ++i) {
    string16 title = task_manager->GetResourceTitle(i);
    if (StartsWith(title, prefix, true))
      ++instant_overlays;
  }
  EXPECT_EQ(1, instant_overlays);
}

void HistoryQueryDone(base::RunLoop* run_loop,
                      bool* result,
                      HistoryService::Handle /* handle */,
                      bool success,
                      const history::URLRow* /* urlrow */,
                      history::VisitVector* /* visitvector */) {
  *result = success;
  run_loop->Quit();
}

void KeywordQueryDone(base::RunLoop* run_loop,
                      std::vector<string16>* result,
                      HistoryService::Handle  /* handle */,
                      std::vector<history::KeywordSearchTermVisit>* terms) {
  for (size_t i = 0; i < terms->size(); ++i)
    result->push_back((*terms)[i].term);
  run_loop->Quit();
}

// Test that the Instant page load is not added to history.
IN_PROC_BROWSER_TEST_F(InstantTest, History) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  const TemplateURL* template_url = TemplateURLServiceFactory::GetForProfile(
      browser()->profile())->GetDefaultSearchProvider();

  // |instant_url| is the URL Instant loads. |search_url| is the fake URL we
  // enter into history for search terms extraction to work correctly.
  std::string search_url = template_url->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("search")));

  HistoryService* history = HistoryServiceFactory::GetForProfile(
      browser()->profile(), Profile::EXPLICIT_ACCESS);
  ui_test_utils::WaitForHistoryToLoad(history);

  // Perform a search.
  SetOmniboxTextAndWaitForOverlayToShow("search");

  // Commit the search.
  browser()->window()->GetLocationBar()->AcceptInput();

  bool found = false;
  CancelableRequestConsumer consumer;

  // The fake search URL should be in history.
  base::RunLoop run_loop1;
  history->QueryURL(GURL(search_url), false, &consumer,
                    base::Bind(&HistoryQueryDone, &run_loop1, &found));
  run_loop1.Run();
  EXPECT_TRUE(found);

  // The Instant URL should not be in history.
  base::RunLoop run_loop2;
  history->QueryURL(instant_url(), false, &consumer,
                    base::Bind(&HistoryQueryDone, &run_loop2, &found));
  run_loop2.Run();
  EXPECT_FALSE(found);

  // The search terms should have been extracted into history.
  base::RunLoop run_loop3;
  std::vector<string16> queries;
  history->GetMostRecentKeywordSearchTerms(template_url->id(),
      ASCIIToUTF16("s"), 1, &consumer,
      base::Bind(&KeywordQueryDone, &run_loop3, &queries));
  run_loop3.Run();
  ASSERT_TRUE(queries.size());
  EXPECT_EQ(ASCIIToUTF16("search"), queries[0]);
}

// TODO(jered): Fix this test on Mac. It fails currently, but the behavior is
// actually closer to what we'd like.
#if defined(OS_MACOSX)
#define MAYBE_NewWindowDismissesInstant DISABLED_NewWindowDismissesInstant
#else
#define MAYBE_NewWindowDismissesInstant NewWindowDismissesInstant
#endif
// Test that creating a new window hides any currently showing Instant overlay.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_NewWindowDismissesInstant) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  FocusOmniboxAndWaitForInstantSupport();
  SetOmniboxTextAndWaitForOverlayToShow("search");

  Browser* previous_window = browser();
  EXPECT_TRUE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());

  InstantTestModelObserver observer(instant()->model(),
                                    SearchMode::MODE_DEFAULT);
  chrome::NewEmptyWindow(browser()->profile(),
                         chrome::HOST_DESKTOP_TYPE_NATIVE);
  observer.WaitForDesiredOverlayState();

  // Even though we just created a new Browser object (for the new window), the
  // browser() accessor should still give us the first window's Browser object.
  EXPECT_EQ(previous_window, browser());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

// Test that the Instant overlay is recreated when all these conditions are met:
// - The stale overlay timer has fired.
// - The overlay is not showing.
// - The omnibox doesn't have focus.
IN_PROC_BROWSER_TEST_F(InstantTest, InstantOverlayRefresh) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // The overlay is refreshed only after all three conditions above are met.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  instant()->overlay_->is_stale_ = true;
  instant()->ReloadOverlayIfStale();
  EXPECT_TRUE(instant()->overlay_->supports_instant());
  instant()->HideOverlay();
  EXPECT_TRUE(instant()->overlay_->supports_instant());
  instant()->OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                 OMNIBOX_FOCUS_CHANGE_EXPLICIT, NULL);
  EXPECT_FALSE(instant()->overlay_->supports_instant());

  // Try with a different ordering.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  instant()->overlay_->is_stale_ = true;
  instant()->ReloadOverlayIfStale();
  EXPECT_TRUE(instant()->overlay_->supports_instant());
  instant()->OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                 OMNIBOX_FOCUS_CHANGE_EXPLICIT, NULL);
  // TODO(sreeram): Currently, OmniboxLostFocus() calls HideOverlay(). When it
  // stops hiding the overlay eventually, uncomment these two lines:
  //     EXPECT_TRUE(instant()->overlay_->supports_instant());
  //     instant()->HideOverlay();
  EXPECT_FALSE(instant()->overlay_->supports_instant());
}

// Test that suggestions are case insensitive. http://crbug.com/150728
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionsAreCaseInsensitive) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: 'INSTANT' } ]"));

  SetOmniboxTextAndWaitForOverlayToShow("in");
  EXPECT_EQ(ASCIIToUTF16("instant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForOverlayToShow("IN");
  EXPECT_EQ(ASCIIToUTF16("INSTANT"), omnibox()->GetText());

  // U+0130 == LATIN CAPITAL LETTER I WITH DOT ABOVE
  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: '\\u0130NSTANT' } ]"));

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForOverlayToShow("i");
  EXPECT_EQ(WideToUTF16(L"i\u0307nstant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForOverlayToShow("I");
  EXPECT_EQ(WideToUTF16(L"I\u0307nstant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForOverlayToShow(WideToUTF8(L"i\u0307"));
  EXPECT_EQ(WideToUTF16(L"i\u0307nstant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForOverlayToShow(WideToUTF8(L"I\u0307"));
  EXPECT_EQ(WideToUTF16(L"I\u0307nstant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForOverlayToShow(WideToUTF8(L"\u0130"));
  EXPECT_EQ(WideToUTF16(L"\u0130NSTANT"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForOverlayToShow("in");
  EXPECT_EQ(ASCIIToUTF16("in"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForOverlayToShow("IN");
  EXPECT_EQ(ASCIIToUTF16("IN"), omnibox()->GetText());

  // Check that a d with a dot above and below it is completed regardless of
  // how that is encoded.
  // U+1E0D = LATIN SMALL LETTER D WITH DOT BELOW
  // U+1E0B = LATIN SMALL LETTER D WITH DOT ABOVE
  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: '\\u1e0d\\u0307oh' } ]"));

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForOverlayToShow(WideToUTF8(L"\u1e0b\u0323"));
  EXPECT_EQ(WideToUTF16(L"\u1e0b\u0323oh"), omnibox()->GetText());
}

// Flakes on Windows and Mac: http://crbug.com/170677
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_CommitInNewTab DISABLED_CommitInNewTab
#else
#define MAYBE_CommitInNewTab CommitInNewTab
#endif
// Test that the overlay can be committed onto a new tab.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_CommitInNewTab) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Use the Instant page as the active tab, so we can exploit its visibility
  // handler to check visibility transitions.
  ui_test_utils::NavigateToURL(browser(), instant_url());
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  int active_tab_onvisibilitycalls = -1;
  EXPECT_TRUE(GetIntFromJS(active_tab, "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(0, active_tab_onvisibilitycalls);

  SetOmniboxTextAndWaitForOverlayToShow("search");

  // Stash a reference to the overlay, so we can refer to it after commit.
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);

  // The state of the searchbox before the commit.
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ("search", value_);
  EXPECT_FALSE(verbatim_);
  EXPECT_EQ(0, onsubmitcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);

  // The state of the active tab before the commit.
  EXPECT_NE(overlay, active_tab);
  EXPECT_EQ(2, active_tab->GetController().GetEntryCount());
  EXPECT_EQ(instant_url(), omnibox()->model()->PermanentURL());
  active_tab_onvisibilitycalls = -1;
  EXPECT_TRUE(GetIntFromJS(active_tab, "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(0, active_tab_onvisibilitycalls);

  // Commit the search by pressing Alt-Enter.
  omnibox()->model()->AcceptInput(NEW_FOREGROUND_TAB, false);

  // After the commit, Instant should not be showing.
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // The old overlay is deleted and a new one is created.
  EXPECT_TRUE(instant()->GetOverlayContents());
  EXPECT_NE(instant()->GetOverlayContents(), overlay);

  // Check that we have two tabs and that the new active tab is indeed what was
  // once the overlay. The overlay should have just one navigation entry, for
  // the Instant search that was committed.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(overlay, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(1, overlay->GetController().GetEntryCount());

  // Check that the omnibox contains the Instant URL we loaded.
  EXPECT_EQ(instant_url(), omnibox()->model()->PermanentURL());

  // Check that the searchbox API values have been reset.
  std::string value;
  EXPECT_TRUE(GetStringFromJS(overlay,
                              "chrome.embeddedSearch.searchBox.value", &value));
  EXPECT_EQ("", value);

  // However, the page should've correctly received the committed query.
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ("search", value_);
  EXPECT_TRUE(verbatim_);
  EXPECT_EQ(1, onsubmitcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);

  // The ex-active tab should've gotten a visibility change marking it hidden.
  EXPECT_NE(active_tab, overlay);
  EXPECT_TRUE(GetIntFromJS(active_tab, "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(1, active_tab_onvisibilitycalls);
}

// Test that suggestions are reusable.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionsAreReusable) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: 'instant' } ];"
                            "behavior = 'never';"));

  SetOmniboxTextAndWaitForOverlayToShow("in");
  EXPECT_EQ(ASCIIToUTF16("stant"), omnibox()->GetInstantSuggestion());

  SetOmniboxText("ins");
  EXPECT_EQ(ASCIIToUTF16("tant"), omnibox()->GetInstantSuggestion());

  SetOmniboxText("in");
  EXPECT_EQ(ASCIIToUTF16("stant"), omnibox()->GetInstantSuggestion());

  SetOmniboxText("insane");
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
}

// Test that the Instant overlay is recreated if it gets destroyed.
IN_PROC_BROWSER_TEST_F(InstantTest, InstantRenderViewGone) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Type partial query, get suggestion to show.
  SetOmniboxTextAndWaitForOverlayToShow("q");
  EXPECT_EQ(ASCIIToUTF16("query suggestion"), omnibox()->GetText());

  // Kill the Instant renderer and wait for Instant support again.
  KillInstantRenderView();
  FocusOmniboxAndWaitForInstantSupport();

  SetOmniboxTextAndWaitForOverlayToShow("qu");
  EXPECT_EQ(ASCIIToUTF16("query suggestion"), omnibox()->GetText());
}

IN_PROC_BROWSER_TEST_F(InstantTest, ProcessIsolation) {
  // Prior to setup no render process is dedicated to Instant.
  InstantService* instant_service =
        InstantServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);
  EXPECT_EQ(0, instant_service->GetInstantProcessCount());

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Now there should be a registered Instant render process.
  EXPECT_LT(0, instant_service->GetInstantProcessCount());

  // And the Instant overlay should live inside it.
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      overlay->GetRenderProcessHost()->GetID()));

  // Search and commit the search by pressing Alt-Enter.
  SetOmniboxTextAndWaitForOverlayToShow("tractor");
  omnibox()->model()->AcceptInput(NEW_FOREGROUND_TAB, false);

  // The committed search results page should also live inside the
  // Instant process.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      active_tab->GetRenderProcessHost()->GetID()));

  // Navigating away should change the process.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_FALSE(instant_service->IsInstantProcess(
      active_tab->GetRenderProcessHost()->GetID()));
}
