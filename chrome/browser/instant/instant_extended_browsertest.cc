// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/instant/instant_test_utils.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"

class InstantExtendedTest : public InstantTestBase {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::search::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server_.Start());
    instant_url_ = https_test_server_.
        GetURL("files/instant_extended.html?strk=1&");
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

  // Delete any existing preview.
  instant()->loader_.reset();
  EXPECT_FALSE(instant()->GetPreviewContents());

  // Refocus the omnibox. The InstantController should've preloaded Instant.
  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(omnibox()->model()->has_focus());

  content::WebContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);

  // Check that the page supports Instant, but it isn't showing.
  EXPECT_TRUE(instant()->loader_->supports_instant());
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Adding a new tab shouldn't delete or recreate the preview; otherwise,
  // what's the point of preloading?
  AddBlankTabAndShow(browser());
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());

  // Unfocusing and refocusing the omnibox should also preserve the preview.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  FocusOmnibox();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, InputShowsOverlay) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantSupport();
  content::WebContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Typing in the omnibox should show the overlay.
  SetOmniboxTextAndWaitForInstantToShow("query");
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());
}

// Test that omnibox text is correctly set when overlay is committed with Enter.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxTextUponEnterCommit) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // The page will autocomplete once we set the omnibox value.
  EXPECT_TRUE(ExecuteScript("suggestion = 'santa claus';"));

  // Set the text, and wait for suggestions to show up.
  SetOmniboxTextAndWaitForInstantToShow("santa");
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

// Test that omnibox text is correctly set when overlay is committed with focus
// lost.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxTextUponFocusLostCommit) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Set autocomplete text (grey text).
  EXPECT_TRUE(ExecuteScript("suggestion = 'johnny depp';"));

  // Set the text, and wait for suggestions to show up.
  SetOmniboxTextAndWaitForInstantToShow("johnny");
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

// This test simulates a search provider using the InstantExtended API to
// navigate through the suggested results and back to the original user query.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NavigateSuggestionsWithArrowKeys) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  SetOmniboxTextAndWaitForInstantToShow("hello");
  EXPECT_EQ("hello", GetOmniboxText());

  SendDownArrow();
  EXPECT_EQ("result 1", GetOmniboxText());
  SendDownArrow();
  EXPECT_EQ("result 2", GetOmniboxText());
  SendUpArrow();
  EXPECT_EQ("result 1", GetOmniboxText());
  SendUpArrow();
  EXPECT_EQ("hello", GetOmniboxText());
}
