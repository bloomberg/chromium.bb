// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_commit_type.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_overlay.h"
#include "chrome/browser/ui/search/instant_tab.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// Creates a bitmap of the specified color. Caller takes ownership.
gfx::Image CreateBitmap(SkColor color) {
  SkBitmap thumbnail;
  thumbnail.setConfig(SkBitmap::kARGB_8888_Config, 4, 4);
  thumbnail.allocPixels();
  thumbnail.eraseColor(color);
  return gfx::Image::CreateFrom1xBitmap(thumbnail);  // adds ref.
}

}  // namespace

class InstantExtendedTest : public InProcessBrowserTest,
                            public InstantTestBase {
 public:
  InstantExtendedTest()
      : on_most_visited_change_calls_(0),
        most_visited_items_count_(0),
        first_most_visited_item_id_(0),
        on_native_suggestions_calls_(0),
        on_change_calls_(0) {
  }
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::search::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url = https_test_server().GetURL(
        "files/instant_extended.html?strk=1&");
    InstantTestBase::Init(instant_url);
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
                        &first_most_visited_item_id_) &&
           GetIntFromJS(contents, "onNativeSuggestionsCalls",
                        &on_native_suggestions_calls_) &&
           GetIntFromJS(contents, "onChangeCalls",
                        &on_change_calls_);
  }

  int on_most_visited_change_calls_;
  int most_visited_items_count_;
  int first_most_visited_item_id_;
  int on_native_suggestions_calls_;
  int on_change_calls_;
};

// Test class used to verify chrome-search: scheme and access policy from the
// Instant overlay.  This is a subclass of |ExtensionBrowserTest| because it
// loads a theme that provides a background image.
class InstantPolicyTest : public ExtensionBrowserTest, public InstantTestBase {
 public:
  InstantPolicyTest() {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::search::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url = https_test_server().GetURL(
        "files/instant_extended.html?strk=1&");
    InstantTestBase::Init(instant_url);
  }

  void InstallThemeSource() {
    ThemeSource* theme = new ThemeSource(profile());
    content::URLDataSource::Add(profile(), theme);
  }

  void InstallThemeAndVerify(const std::string& theme_dir,
                             const std::string& theme_name) {
    const base::FilePath theme_path = test_data_dir_.AppendASCII(theme_dir);
    ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(
        theme_path, 1, ExtensionBrowserTest::browser()));
    const extensions::Extension* theme =
        ThemeServiceFactory::GetThemeForProfile(
            ExtensionBrowserTest::browser()->profile());
    ASSERT_NE(static_cast<extensions::Extension*>(NULL), theme);
    ASSERT_EQ(theme->name(), theme_name);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantPolicyTest);
};

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, ExtendedModeIsOn) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  EXPECT_TRUE(instant()->extended_enabled_);
}

// Test that Instant is preloaded when the omnibox is focused.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxFocusLoadsInstant) {
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
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantExtendedSupport();
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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Typing in the omnibox should show the overlay.
  SetOmniboxTextAndWaitForOverlayToShow("http://www.example.com/");

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
  EXPECT_EQ("http://www.example.com/", new_tab_contents->GetURL().spec());

  // Check that there are now two tabs.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       UnfocusingOmniboxDoesNotChangeSuggestions) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Get a committed tab to work with.
  content::WebContents* instant_tab = instant()->GetOverlayContents();
  SetOmniboxTextAndWaitForOverlayToShow("committed");
  browser()->window()->GetLocationBar()->AcceptInput();

  // Put focus back into the omnibox, type, and wait for some gray text.
  EXPECT_TRUE(content::ExecuteScript(instant_tab,
                                     "suggestion = 'santa claus';"));
  SetOmniboxTextAndWaitForSuggestion("santa ");
  EXPECT_EQ(ASCIIToUTF16("claus"), omnibox()->GetInstantSuggestion());
  EXPECT_TRUE(content::ExecuteScript(instant_tab,
      "onChangeCalls = onNativeSuggestionsCalls = 0;"));

  // Now unfocus the omnibox.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(UpdateSearchState(instant_tab));
  EXPECT_EQ(0, on_change_calls_);
  EXPECT_EQ(0, on_native_suggestions_calls_);
}

// Test that omnibox text is correctly set when overlay is committed with Enter.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxTextUponEnterCommit) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxTextUponFocusLostCommit) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
// Disabled on Mac because omnibox focus loss is not working correctly.
#if defined(OS_MACOSX)
#define MAYBE_OmniboxTextUponFocusedCommittedSERP \
    DISABLED_OmniboxTextUponFocusedCommittedSERP
#else
#define MAYBE_OmniboxTextUponFocusedCommittedSERP \
    OmniboxTextUponFocusedCommittedSERP
#endif
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MAYBE_OmniboxTextUponFocusedCommittedSERP) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Do a search and commit it.
  SetOmniboxTextAndWaitForOverlayToShow("hello k");
  EXPECT_EQ(ASCIIToUTF16("hello k"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPIsUsedInNewTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPForWrongProvider) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);
  GURL ntp_url = ntp_contents->GetURL();

  // Change providers.
  SetInstantURL("chrome://blank");

  // Open new tab. Preloaded NTP contents should have not been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(ntp_url, active_tab->GetURL());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxHasFocusOnNewTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // Omnibox should be empty.
  EXPECT_TRUE(omnibox()->GetText().empty());
}

// TODO(dhollowa): Fix flakes.  http://crbug.com/179930.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_NoFaviconOnNewTabPage) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantExtendedSupport();
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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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
  // Now try to set gray text for the same query.
  EXPECT_TRUE(ExecuteScript("behavior = 2"));
  EXPECT_TRUE(ExecuteScript("suggestion = 'www.example.com rocks'"));
  SetOmniboxText("http://www.ex");
  EXPECT_EQ(ASCIIToUTF16("http://www.example.com"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());

  omnibox()->RevertAll();

  // Ignore an out-of-date blue text suggestion. (Simulates a laggy
  // SetSuggestion IPC by directly calling into InstantController.)
  SetOmniboxTextAndWaitForOverlayToShow("http://www.example.com/");
  instant()->SetSuggestions(
      instant()->overlay()->contents(),
      std::vector<InstantSuggestion>(
          1,
          InstantSuggestion(ASCIIToUTF16("www.exa"),
                            INSTANT_COMPLETE_NOW,
                            INSTANT_SUGGESTION_URL,
                            ASCIIToUTF16("www.exa"))));
  EXPECT_EQ(
      "http://www.example.com/",
      omnibox()->model()->result().default_match()->destination_url.spec());

  omnibox()->RevertAll();

  // TODO(samarth): uncomment after fixing crbug.com/191656.
  // Use an out-of-date blue text suggestion, if the text typed by the user is
  // contained in the suggestion.
  // SetOmniboxTextAndWaitForOverlayToShow("ex");
  // instant()->SetSuggestions(
  //     instant()->overlay()->contents(),
  //     std::vector<InstantSuggestion>(
  //         1,
  //         InstantSuggestion(ASCIIToUTF16("www.example.com"),
  //                           INSTANT_COMPLETE_NOW,
  //                           INSTANT_SUGGESTION_URL,
  //                           ASCIIToUTF16("e"))));
  // EXPECT_EQ(
  //     "http://www.example.com/",
  //     omnibox()->model()->result().default_match()->destination_url.spec());

  // omnibox()->RevertAll();

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

// Tests that a previous navigation suggestion is not discarded if it's not
// stale.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       NavigationSuggestionIsNotDiscarded) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Tell the page to send a URL suggestion.
  EXPECT_TRUE(ExecuteScript("suggestion = 'http://www.example.com';"
                            "behavior = 0;"));
  SetOmniboxTextAndWaitForOverlayToShow("exa");
  EXPECT_EQ(ASCIIToUTF16("example.com"), omnibox()->GetText());
  SetOmniboxText("exam");
  EXPECT_EQ(ASCIIToUTF16("example.com"), omnibox()->GetText());

  // TODO(jered): Remove this after fixing OnBlur().
  omnibox()->RevertAll();
}

// TODO(dhollowa): Fix flakes.  http://crbug.com/179930.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_MostVisited) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_SENT_MOST_VISITED_ITEMS,
      content::NotificationService::AllSources());
  // Initialize Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

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

IN_PROC_BROWSER_TEST_F(InstantPolicyTest, ThemeBackgroundAccess) {
  InstallThemeSource();
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // The "Instant" New Tab should have access to chrome-search: scheme but not
  // chrome: scheme.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  content::RenderViewHost* rvh =
      browser()->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost();

  const std::string chrome_url("chrome://theme/IDR_THEME_NTP_BACKGROUND");
  const std::string search_url(
      "chrome-search://theme/IDR_THEME_NTP_BACKGROUND");
  bool loaded = false;
  ASSERT_TRUE(LoadImage(rvh, chrome_url, &loaded));
  EXPECT_FALSE(loaded) << chrome_url;
  ASSERT_TRUE(LoadImage(rvh, search_url, &loaded));
  EXPECT_TRUE(loaded) << search_url;
}

// TODO(dhollowa): Fix flakes.  http://crbug.com/179930.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_FaviconAccess) {
  // Create a favicon.
  history::TopSites* top_sites = browser()->profile()->GetTopSites();
  GURL url("http://www.google.com/foo.html");
  gfx::Image thumbnail(CreateBitmap(SK_ColorWHITE));
  ThumbnailScore high_score(0.0, true, true, base::Time::Now());
  EXPECT_TRUE(top_sites->SetPageThumbnail(url, thumbnail, high_score));

  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // The "Instant" New Tab should have access to chrome-search: scheme but not
  // chrome: scheme.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  content::RenderViewHost* rvh =
      browser()->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost();

  // Get the favicons.
  const std::string chrome_favicon_url(
      "chrome://favicon/largest/http://www.google.com/foo.html");
  const std::string search_favicon_url(
      "chrome-search://favicon/largest/http://www.google.com/foo.html");
  bool loaded = false;
  ASSERT_TRUE(LoadImage(rvh, chrome_favicon_url, &loaded));
  EXPECT_FALSE(loaded) << chrome_favicon_url;
  ASSERT_TRUE(LoadImage(rvh, search_favicon_url, &loaded));
  EXPECT_TRUE(loaded) << search_favicon_url;
}

// WebUIBindings should never be enabled on ANY Instant web contents.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoWebUIBindingsOnNTP) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  const content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Instant-provided NTP should not have any bindings enabled.
  EXPECT_EQ(0, tab->GetRenderViewHost()->GetEnabledBindings());
}

// WebUIBindings should never be enabled on ANY Instant web contents.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoWebUIBindingsOnPreview) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Typing in the omnibox shows the overlay.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  content::WebContents* preview = instant()->GetOverlayContents();
  ASSERT_NE(static_cast<content::WebContents*>(NULL), preview);

  // Instant preview should not have any bindings enabled.
  EXPECT_EQ(0, preview->GetRenderViewHost()->GetEnabledBindings());
}

// WebUIBindings should never be enabled on ANY Instant web contents.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoWebUIBindingsOnResults) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  // Typing in the omnibox shows the overlay.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  content::WebContents* preview = instant()->GetOverlayContents();
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_TRUE(instant()->model()->mode().is_default());
  const content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(preview, tab);

  // The commited Instant page should not have any bindings enabled.
  EXPECT_EQ(0, tab->GetRenderViewHost()->GetEnabledBindings());
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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantExtendedSupport();
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
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantExtendedSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());

  // Create an observer to wait for the commit.
  content::WindowedNotificationObserver commit_observer(
      chrome::NOTIFICATION_INSTANT_COMMITTED,
      content::NotificationService::AllSources());

  // Typing in the omnibox should show the overlay. Don't wait for the overlay
  // to show however.
  SetOmniboxText("query");

  // The autocomplete controller isn't done until all the providers are done.
  // Though we don't care about the SearchProvider when we send autocomplete
  // results to the page, we do need to cause it to be "done" to make this test
  // work. Setting the suggestion calls SearchProvider::FinalizeInstantQuery(),
  // thus causing it to be done.
  omnibox()->model()->SetInstantSuggestion(
      InstantSuggestion(ASCIIToUTF16("query"),
                        INSTANT_COMPLETE_NOW,
                        INSTANT_SUGGESTION_SEARCH,
                        ASCIIToUTF16("query")));
  while (!omnibox()->model()->autocomplete_controller()->done()) {
    content::WindowedNotificationObserver autocomplete_observer(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::NotificationService::AllSources());
    autocomplete_observer.Wait();
  }

  // Explicitly unfocus the omnibox without triggering a click. Note that this
  // doesn't actually change the focus state of the omnibox, only what the
  // Instant controller sees it as.
  omnibox()->model()->OnWillKillFocus(NULL);
  omnibox()->model()->OnKillFocus();

  // Wait for the overlay to show.
  commit_observer.Wait();

  // Confirm that the overlay has been committed.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(overlay, active_tab);
}

// Test that a transient entry is set properly when the overlay is committed
// without a navigation.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, TransientEntrySet) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());

  // Commit the overlay without triggering a navigation.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_COMMITTED,
      content::NotificationService::AllSources());
  SetOmniboxTextAndWaitForOverlayToShow("query");
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Confirm that the overlay has been committed.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(overlay, active_tab);

  // The page hasn't navigated so there should be a transient entry with the
  // same URL but different page ID as the last committed entry.
  const content::NavigationEntry* transient_entry =
      active_tab->GetController().GetTransientEntry();
  const content::NavigationEntry* committed_entry =
      active_tab->GetController().GetLastCommittedEntry();
  EXPECT_EQ(transient_entry->GetURL(), committed_entry->GetURL());
  EXPECT_NE(transient_entry->GetPageID(), committed_entry->GetPageID());
}

// Test that the a transient entry is cleared when the overlay is committed
// with a navigation.
// TODO(samarth) : this test fails, http://crbug.com/181070.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_TransientEntryRemoved) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

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

  // Trigger a navigation on commit.
  EXPECT_TRUE(ExecuteScript(
      "getApiHandle().oncancel = function() {"
      "  location.replace(location.href + '#q=query');"
      "};"
      ));

  // Commit the overlay.
  SetOmniboxTextAndWaitForOverlayToShow("query");
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Confirm that the overlay has been committed.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(overlay, active_tab);

  // The page has navigated so there should be no transient entry.
  const content::NavigationEntry* transient_entry =
      active_tab->GetController().GetTransientEntry();
  EXPECT_EQ(NULL, transient_entry);

  // The last committed entry should be the URL the page navigated to.
  const content::NavigationEntry* committed_entry =
      active_tab->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(EndsWith(committed_entry->GetURL().spec(), "#q=query", true));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, RestrictedItemReadback) {
  // Initialize Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantSupport();

  // Get a handle to the NTP and the current state of the JS.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* preview_tab = instant()->ntp()->contents();
  EXPECT_TRUE(preview_tab);

  // Manufacture a few autocomplete results and get them down to the page.
  std::vector<InstantAutocompleteResult> autocomplete_results;
  for (int i = 0; i < 3; ++i) {
    std::string description(base::StringPrintf("Test Description %d", i));
    std::string url(base::StringPrintf("http://www.testurl%d.com", i));

    InstantAutocompleteResult res;
    res.provider = ASCIIToUTF16(AutocompleteProvider::TypeToString(
        AutocompleteProvider::TYPE_BUILTIN));
    res.type = ASCIIToUTF16(AutocompleteMatch::TypeToString(
        AutocompleteMatch::SEARCH_WHAT_YOU_TYPED)),
    res.description = ASCIIToUTF16(description);
    res.destination_url = ASCIIToUTF16(url);
    res.transition = content::PAGE_TRANSITION_TYPED;
    res.relevance = 42 + i;

    autocomplete_results.push_back(res);
  }
  instant()->overlay()->SendAutocompleteResults(autocomplete_results);

  // Apparently, one needs to access nativeSuggestions before
  // apiHandle.setRestrictedValue can work.
  EXPECT_TRUE(ExecuteScript("var foo = apiHandle.nativeSuggestions;"));

  const char kQueryString[] = "Hippos go berzerk!";

  // First set the query text to a non restricted value and ensure it can be
  // read back.
  std::ostringstream stream;
  stream << "apiHandle.setValue('" << kQueryString << "');";
  EXPECT_TRUE(ExecuteScript(stream.str()));

  std::string result;
  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "apiHandle.value",
                              &result));
  EXPECT_EQ(kQueryString, result);

  // Set the query text to the first restricted autocomplete item.
  int rid = 1;
  stream.str(std::string());
  stream << "apiHandle.setRestrictedValue(" << rid << ")";
  EXPECT_TRUE(ExecuteScript(stream.str()));

  // Expect that we now receive the empty string when reading the value back.
  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "apiHandle.value",
                              &result));
  EXPECT_EQ("", result);

  // Now set the query text to a non restricted value and ensure that the
  // visibility has been reset and the string can again be read back.
  stream.str(std::string());
  stream << "apiHandle.setValue('" << kQueryString << "');";
  EXPECT_TRUE(ExecuteScript(stream.str()));

  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "apiHandle.value",
                              &result));
  EXPECT_EQ(kQueryString, result);
}

// Test that autocomplete results are sent to the page only when all the
// providers are done.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, AutocompleteProvidersDone) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantExtendedSupport();

  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ(0, on_native_suggestions_calls_);

  SetOmniboxTextAndWaitForOverlayToShow("railroad");

  EXPECT_EQ(overlay, instant()->GetOverlayContents());
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ(1, on_native_suggestions_calls_);
}
