// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

class InstantTest : public InProcessBrowserTest {
 protected:
  void SetupInstant(const std::string& page) {
    ASSERT_TRUE(test_server()->Start());

    TemplateURLService* service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(service);

    TemplateURLData data;
    data.SetURL("http://does/not/exist?q={searchTerms}");
    data.instant_url = base::StringPrintf(
        "http://%s:%d/files/%s",
        test_server()->host_port_pair().host().c_str(),
        test_server()->host_port_pair().port(),
        page.c_str());

    TemplateURL* template_url = new TemplateURL(browser()->profile(), data);
    service->Add(template_url);  // Takes ownership of |template_url|.
    service->SetDefaultSearchProvider(template_url);

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kInstantEnabled, true);
  }

  OmniboxView* omnibox() const {
    return browser()->window()->GetLocationBar()->GetLocationEntry();
  }

  InstantController* instant() const {
    return browser()->instant_controller()->instant();
  }

  void WaitFor(chrome::NotificationType notification_type) const {
    content::WindowedNotificationObserver observer(
        notification_type, content::NotificationService::AllSources());
    observer.Wait();
  }

  std::wstring WrapScript(const std::string& script) const {
    return UTF8ToWide("domAutomationController.send(" + script + ")");
  }

  bool GetBoolFromJS(content::RenderViewHost* rvh,
                     const std::string& script,
                     bool* result) const WARN_UNUSED_RESULT {
    return content::ExecuteJavaScriptAndExtractBool(
        rvh, std::wstring(), WrapScript(script), result);
  }

  bool GetIntFromJS(content::RenderViewHost* rvh,
                    const std::string& script,
                    int* result) const WARN_UNUSED_RESULT {
    return content::ExecuteJavaScriptAndExtractInt(
        rvh, std::wstring(), WrapScript(script), result);
  }

  bool GetStringFromJS(content::RenderViewHost* rvh,
                       const std::string& script,
                       std::string* result) const WARN_UNUSED_RESULT {
    return content::ExecuteJavaScriptAndExtractString(
        rvh, std::wstring(), WrapScript(script), result);
  }

  bool UpdateSearchState(TabContents* tab) WARN_UNUSED_RESULT {
    content::RenderViewHost* rvh = tab->web_contents()->GetRenderViewHost();
    return GetIntFromJS(rvh, "onchangecalls", &onchangecalls_) &&
           GetIntFromJS(rvh, "onsubmitcalls", &onsubmitcalls_) &&
           GetIntFromJS(rvh, "oncancelcalls", &oncancelcalls_) &&
           GetIntFromJS(rvh, "onresizecalls", &onresizecalls_) &&
           GetStringFromJS(rvh, "value", &value_) &&
           GetBoolFromJS(rvh, "verbatim", &verbatim_) &&
           GetIntFromJS(rvh, "height", &height_);
  }

  bool ExecuteScript(const std::string& script) const WARN_UNUSED_RESULT {
    return content::ExecuteJavaScript(
        instant()->GetPreviewContents()->web_contents()->GetRenderViewHost(),
        std::wstring(), ASCIIToWide(script));
  }

  void SetOmniboxText(const std::string& text) const {
    omnibox()->SetUserText(ASCIIToUTF16(text));
  }

  bool CheckVisibilityIs(TabContents* tab,
                         bool expected) const WARN_UNUSED_RESULT {
    bool actual = !expected;  // Purposely start with a mis-match.
    // We can only use ASSERT_*() in a method that returns void, hence this
    // convoluted check.
    return GetBoolFromJS(tab->web_contents()->GetRenderViewHost(),
                         "!document.webkitHidden", &actual) &&
           actual == expected;
  }

  int onchangecalls_;
  int onsubmitcalls_;
  int oncancelcalls_;
  int onresizecalls_;

  std::string value_;
  bool verbatim_;
  int height_;
};

// Test that Instant is preloaded when the omnibox is focused.
#if defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_OmniboxFocusLoadsInstant DISABLED_OmniboxFocusLoadsInstant
#else
#define MAYBE_OmniboxFocusLoadsInstant OmniboxFocusLoadsInstant
#endif
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OmniboxFocusLoadsInstant) {
  // The omnibox gets focus before the test begins. At this time, there's no
  // InstantController (which is only created in SetupInstant() below), so no
  // preloading has happened yet.
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  EXPECT_FALSE(instant()->GetPreviewContents());

  // Explicitly unfocus and refocus the omnibox. Since an InstantController now
  // exists, it will preload Instant.
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  browser()->window()->GetLocationBar()->FocusLocation(false);
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  TabContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);

  // Check that the page supports Instant, but it isn't showing.
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);
  EXPECT_TRUE(instant()->loader()->supports_instant());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());

  // Adding a new tab shouldn't delete or recreate the TabContents; otherwise,
  // what's the point of preloading?
  AddBlankTabAndShow(browser());
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());

  // Unfocusing and refocusing the omnibox should also preserve the preview.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  browser()->window()->GetLocationBar()->FocusLocation(false);
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());

  // Doing a search should also use the same preloaded page.
  SetOmniboxText("query");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);
  EXPECT_TRUE(instant()->is_showing());
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());
}

#if defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_OnChangeEvent DISABLED_OnChangeEvent
#else
#define MAYBE_OnChangeEvent OnChangeEvent
#endif
// Test that the onchange event is dispatched upon typing in the omnibox.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnChangeEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));

  // Typing "query" into the omnibox causes the first onchange event.
  SetOmniboxText("query");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

  // The page suggested "query suggestion" is inline autocompleted into the
  // omnibox, causing the second onchange event.
  EXPECT_EQ(ASCIIToUTF16("query suggestion"), omnibox()->GetText());
  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(2, onchangecalls_);

  // Change the query and confirm that another onchange is sent. Since the new
  // query is not a prefix of the hardcoded "query suggestion", no inline
  // autocompletion happens, and thus, no fourth onchange event.
  SetOmniboxText("search");
  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(3, onchangecalls_);
}

#if defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_OnSubmitEvent DISABLED_OnSubmitEvent
#else
#define MAYBE_OnSubmitEvent OnSubmitEvent
#endif
// Test that the onsubmit event is dispatched upon pressing Enter.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnSubmitEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  SetOmniboxText("search");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

  // Stash a reference to the preview, so we can refer to it after commit.
  TabContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);

  // The state of the searchbox before the commit.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_FALSE(verbatim_);
  EXPECT_EQ(0, onsubmitcalls_);

  // Before the commit, the active tab is the NTP (i.e., not Instant).
  TabContents* active_tab = chrome::GetActiveTabContents(browser());
  EXPECT_NE(preview_tab, active_tab);
  EXPECT_EQ(1, active_tab->web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(std::string(chrome::kAboutBlankURL),
            omnibox()->model()->PermanentURL().spec());

  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();

  // After the commit, Instant should not be showing.
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());

  // The old loader is deleted and a new one is created.
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_NE(instant()->GetPreviewContents(), preview_tab);

  // Check that the current active tab is indeed what was once the preview.
  EXPECT_EQ(preview_tab, chrome::GetActiveTabContents(browser()));

  // We should have two navigation entries, one for the NTP, and one for the
  // Instant search that was committed.
  EXPECT_EQ(2, preview_tab->web_contents()->GetController().GetEntryCount());

  // Check that the omnibox contains the Instant URL we loaded.
  std::string instant_url = TemplateURLServiceFactory::GetForProfile(
      browser()->profile())->GetDefaultSearchProvider()->instant_url_ref().
      ReplaceSearchTerms(TemplateURLRef::SearchTermsArgs(string16()));
  EXPECT_EQ(instant_url, omnibox()->model()->PermanentURL().spec());

  // Check that the searchbox API values have been reset.
  std::string value;
  EXPECT_TRUE(GetStringFromJS(preview_tab->web_contents()->GetRenderViewHost(),
                              "chrome.searchBox.value", &value));
  EXPECT_EQ("", value);

  // However, the page should've correctly received the committed query.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_TRUE(verbatim_);
  EXPECT_EQ(1, onsubmitcalls_);
}

// Test that the oncancel event is dispatched upon clicking on the preview.
#if defined(OS_WIN)
// Fails frequently on the win_rel tryserver.  crbug.com/145754
#define MAYBE_OnCancelEvent DISABLED_OnCancelEvent
#elif defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_OnCancelEvent DISABLED_OnCancelEvent
#else
#define MAYBE_OnCancelEvent OnCancelEvent
#endif
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnCancelEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  SetOmniboxText("search");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

  // Stash a reference to the preview, so we can refer to it after commit.
  TabContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);

  // The state of the searchbox before the commit.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_FALSE(verbatim_);
  EXPECT_EQ(0, oncancelcalls_);

  // Before the commit, the active tab is the NTP (i.e., not Instant).
  TabContents* active_tab = chrome::GetActiveTabContents(browser());
  EXPECT_NE(preview_tab, active_tab);
  EXPECT_EQ(1, active_tab->web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(std::string(chrome::kAboutBlankURL),
            omnibox()->model()->PermanentURL().spec());

  // Commit the search by clicking on the preview.
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // After the commit, Instant should not be showing.
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());

  // The old loader is deleted and a new one is created.
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_NE(instant()->GetPreviewContents(), preview_tab);

  // Check that the current active tab is indeed what was once the preview.
  EXPECT_EQ(preview_tab, chrome::GetActiveTabContents(browser()));

  // We should have two navigation entries, one for the NTP, and one for the
  // Instant search that was committed.
  EXPECT_EQ(2, preview_tab->web_contents()->GetController().GetEntryCount());

  // Check that the omnibox contains the Instant URL we loaded.
  std::string instant_url = TemplateURLServiceFactory::GetForProfile(
      browser()->profile())->GetDefaultSearchProvider()->instant_url_ref().
      ReplaceSearchTerms(TemplateURLRef::SearchTermsArgs(string16()));
  EXPECT_EQ(instant_url, omnibox()->model()->PermanentURL().spec());

  // Check that the searchbox API values have been reset.
  std::string value;
  EXPECT_TRUE(GetStringFromJS(preview_tab->web_contents()->GetRenderViewHost(),
                              "chrome.searchBox.value", &value));
  EXPECT_EQ("", value);

  // However, the page should've correctly received the committed query.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_TRUE(verbatim_);
  EXPECT_EQ(1, oncancelcalls_);
}

// Test that the onreisze event is dispatched upon typing in the omnibox.
IN_PROC_BROWSER_TEST_F(InstantTest, OnResizeEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));

  // This makes Instant load the preview, along with an initial onresize() (see
  // SearchBoxExtension::PageSupportsInstant() for why).
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(1, onresizecalls_);
  EXPECT_EQ(0, height_);

  // Type a query into the omnibox. This should cause an onresize() event, with
  // a valid (non-zero) height.
  SetOmniboxText("search");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(2, onresizecalls_);
  EXPECT_LT(0, height_);
}

#if defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_SuggestionIsCompletedNow DISABLED_SuggestionIsCompletedNow
#else
#define MAYBE_SuggestionIsCompletedNow SuggestionIsCompletedNow
#endif
// Test that the INSTANT_COMPLETE_NOW behavior works as expected.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SuggestionIsCompletedNow) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  // Tell the JS to request for the given behavior.
  EXPECT_TRUE(ExecuteScript("behavior = 'now'"));

  SetOmniboxText("query");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

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

// The "delayed" completion behavior is not implemented on the Mac. Strange.
#if !defined(OS_MACOSX)
// Test that the INSTANT_COMPLETE_DELAYED behavior works as expected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsCompletedDelayed) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  // Tell the JS to request for the given behavior.
  EXPECT_TRUE(ExecuteScript("behavior = 'delayed'"));

  SetOmniboxText("query");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

  // Get what's showing in the omnibox, and what's highlighted.
  string16 text = omnibox()->GetText();
  size_t start = 0, end = 0;
  omnibox()->GetSelectionBounds(&start, &end);
  if (start > end)
    std::swap(start, end);

  EXPECT_EQ(ASCIIToUTF16("query"), text);
  EXPECT_EQ(ASCIIToUTF16(""), text.substr(start, end - start));
  EXPECT_EQ(ASCIIToUTF16(" suggestion"), omnibox()->GetInstantSuggestion());

  // Wait for the animation to complete, which causes the omnibox to update.
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED);

  text = omnibox()->GetText();
  omnibox()->GetSelectionBounds(&start, &end);
  if (start > end)
    std::swap(start, end);

  EXPECT_EQ(ASCIIToUTF16("query suggestion"), text);
  EXPECT_EQ(ASCIIToUTF16(" suggestion"), text.substr(start, end - start));
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
}
#endif

// Test that the INSTANT_COMPLETE_NEVER behavior works as expected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsCompletedNever) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  // Tell the JS to request for the given behavior.
  EXPECT_TRUE(ExecuteScript("behavior = 'never'"));

  SetOmniboxText("query");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  // Tell the JS to use the given suggestion.
  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: 'query completion' } ]"));

  SetOmniboxText("query");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

  EXPECT_EQ(ASCIIToUTF16("query completion"), omnibox()->GetText());
}

#if defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_SuggestionIsInvalidObject DISABLED_SuggestionIsInvalidObject
#else
#define MAYBE_SuggestionIsInvalidObject SuggestionIsInvalidObject
#endif
// Test that an invalid suggestion is rejected.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SuggestionIsInvalidObject) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  // Tell the JS to use the given suggestion.
  EXPECT_TRUE(ExecuteScript("suggestion = { value: 'query completion' }"));

  SetOmniboxText("query");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

  EXPECT_EQ(ASCIIToUTF16("query"), omnibox()->GetText());
}

// Test that various forms of empty suggestions are rejected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsEmpty) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  EXPECT_TRUE(ExecuteScript("suggestion = {}"));
  SetOmniboxText("query1");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);
  EXPECT_EQ(ASCIIToUTF16("query1"), omnibox()->GetText());

  instant()->Hide();

  EXPECT_TRUE(ExecuteScript("suggestion = []"));
  SetOmniboxText("query2");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);
  EXPECT_EQ(ASCIIToUTF16("query2"), omnibox()->GetText());

  instant()->Hide();

  EXPECT_TRUE(ExecuteScript("suggestion = [{}]"));
  SetOmniboxText("query3");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);
  EXPECT_EQ(ASCIIToUTF16("query3"), omnibox()->GetText());
}

// Test that Instant doesn't process URLs.
IN_PROC_BROWSER_TEST_F(InstantTest, RejectsURLs) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));

  // Note that we are not actually navigating to these URLs yet. We are just
  // typing them into the omnibox (without pressing Enter) and checking that
  // Instant doesn't try to process them.
  SetOmniboxText(chrome::kChromeUICrashURL);
  EXPECT_FALSE(instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());

  // Let's try again after creating the preview.
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  SetOmniboxText(chrome::kChromeUIHangURL);
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());

  SetOmniboxText(chrome::kChromeUIKillURL);
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());

  // Make sure that the URLs were never sent to the preview page.
  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(0, onchangecalls_);
  EXPECT_EQ("", value_);
}

// Test that Instant doesn't fire for intranet paths that look like searches.
// http://crbug.com/99836
IN_PROC_BROWSER_TEST_F(InstantTest, IntranetPathLooksLikeSearch) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));

  // Navigate to a URL that looks like a search (when the scheme is stripped).
  // It's okay if the host is bogus or the navigation fails, since we only care
  // that Instant doesn't act on it.
  ui_test_utils::NavigateToURL(browser(), GURL("http://baby/beluga"));
  EXPECT_EQ(ASCIIToUTF16("baby/beluga"), omnibox()->GetText());
  EXPECT_FALSE(instant()->GetPreviewContents());
}

// Test that transitions between searches and non-searches work as expected.
IN_PROC_BROWSER_TEST_F(InstantTest, TransitionsBetweenSearchAndURL) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));

  // Type a search, but without waiting for the page to load, type a URL.
  SetOmniboxText("query");
  SetOmniboxText("http://monstrous/nightmare");

  // Wait for the page to load.
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());
  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(1, onchangecalls_);
  EXPECT_EQ("query", value_);

  // Type a search. Instant should show.
  SetOmniboxText("search");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_TRUE(instant()->IsCurrent());
  EXPECT_TRUE(instant()->is_showing());
  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(2, onchangecalls_);

  // Type another URL. The preview should be hidden.
  SetOmniboxText("http://terrible/terror");
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());
  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(2, onchangecalls_);

  // Type the same search as before. The preview should show, but no onchange()
  // is sent, since query hasn't changed.
  SetOmniboxText("search");
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_TRUE(instant()->IsCurrent());
  EXPECT_TRUE(instant()->is_showing());
  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(2, onchangecalls_);
}

#if defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_DoesNotCommitURLs DISABLED_DoesNotCommitURLs
#else
#define MAYBE_DoesNotCommitURLs DoesNotCommitURLs
#endif
// Test that Instant can't be fooled into committing a URL.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_DoesNotCommitURLs) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Type a URL. No Instant.
  SetOmniboxText("http://deadly/nadder");
  EXPECT_FALSE(instant()->GetPreviewContents());

  // Unfocus and refocus the omnibox.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  browser()->window()->GetLocationBar()->FocusLocation(false);

  // The omnibox text hasn't changed, but the preview should've preloaded due
  // to the omnibox getting focus. It still shouldn't be showing.
  EXPECT_EQ(ASCIIToUTF16("http://deadly/nadder"), omnibox()->GetText());
  TabContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());

  // Commit the URL. The omnibox should reflect the URL minus the scheme.
  browser()->window()->GetLocationBar()->AcceptInput();
  TabContents* active_tab = chrome::GetActiveTabContents(browser());
  EXPECT_NE(preview_tab, active_tab);
  EXPECT_EQ(ASCIIToUTF16("deadly/nadder"), omnibox()->GetText());

  // Instant shouldn't have done anything.
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());

  // Let's try again, using a slightly different approach.

  // Type a query. This causes the preview to be shown.
  browser()->window()->GetLocationBar()->FocusLocation(false);
  SetOmniboxText("query");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

  // Type a URL. This causes the preview to be hidden.
  SetOmniboxText("http://hideous/zippleback");
  EXPECT_FALSE(instant()->is_showing());

  // Pretend the omnibox got focus. It already had focus, so we are just trying
  // to tickle a different code path.
  instant()->OnAutocompleteGotFocus();

  // Commit the URL. As before, check that Instant wasn't committed.
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_EQ(active_tab, chrome::GetActiveTabContents(browser()));
  EXPECT_NE(preview_tab, active_tab);
  EXPECT_EQ(ASCIIToUTF16("hideous/zippleback"), omnibox()->GetText());

  // As before, Instant shouldn't have done anything.
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());
}

#if defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_NonInstantSearchProvider DISABLED_NonInstantSearchProvider
#else
#define MAYBE_NonInstantSearchProvider NonInstantSearchProvider
#endif
// Test that a non-Instant search provider shows no previews.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_NonInstantSearchProvider) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("empty.html"));

  // Type a query. Instant will load the search provider.
  SetOmniboxText("query");
  EXPECT_TRUE(instant()->GetPreviewContents());

  // When the response comes back, Instant will destroy the non-Instant page.
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);
  EXPECT_FALSE(instant()->GetPreviewContents());
}

// Test that the renderer doesn't crash if JavaScript is blocked.
IN_PROC_BROWSER_TEST_F(InstantTest, NoCrashOnBlockedJS) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));

  // Wait for notification that the Instant API has been determined. As long as
  // we get the notification we're good (the renderer didn't crash).
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);
}

// Test that the preview and active tab's visibility states are set correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, PageVisibility) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  TabContents* active_tab = chrome::GetActiveTabContents(browser());
  TabContents* preview_tab = instant()->GetPreviewContents();

  // Inititally, the active tab is showing; the preview is not.
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, false));

  // Type a query and wait for Instant to show.
  SetOmniboxText("search");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);
  EXPECT_TRUE(CheckVisibilityIs(active_tab, false));
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, true));

  // Deleting the omnibox text should hide the preview.
  SetOmniboxText("");
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, false));

  // Typing a query should show the preview again.
  SetOmniboxText("search");
  EXPECT_TRUE(CheckVisibilityIs(active_tab, false));
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, true));

  // Commit the preview.
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_EQ(preview_tab, chrome::GetActiveTabContents(browser()));
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, true));
}

// Test that the task manager identifies Instant's preview tab correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, TaskManagerPrefix) {
  // The browser starts with one new tab, so the task manager should have two
  // rows initially, one for the browser process and one for tab's renderer.
  TaskManagerModel* task_manager = TaskManager::GetInstance()->model();
  task_manager->StartUpdating();
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);

  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  // Now there should be three rows, the third being the Instant preview.
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_INSTANT_PREVIEW_PREFIX, string16());
  string16 title = task_manager->GetResourceTitle(2);
  EXPECT_TRUE(StartsWith(title, prefix, true)) << title << " vs " << prefix;
}

void HistoryQueryDone(base::RunLoop* run_loop,
                      bool* result,
                      HistoryService::Handle  /* handle */,
                      bool success,
                      const history::URLRow*  /* urlrow */,
                      history::VisitVector*  /* visitvector */) {
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

#if defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_History DISABLED_History
#else
#define MAYBE_History History
#endif
// Test that the Instant page load is not added to history.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_History) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  const TemplateURL* template_url = TemplateURLServiceFactory::GetForProfile(
      browser()->profile())->GetDefaultSearchProvider();

  // |instant_url| is the URL Instant loads. |search_url| is the fake URL we
  // enter into history for search terms extraction to work correctly.
  std::string search_url = template_url->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("search")));
  std::string instant_url = template_url->instant_url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(string16()));

  HistoryService* history = HistoryServiceFactory::GetForProfile(
      browser()->profile(), Profile::EXPLICIT_ACCESS);
  ui_test_utils::WaitForHistoryToLoad(history);

  // Perform a search.
  SetOmniboxText("search");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);
  EXPECT_EQ(instant_url, instant()->loader()->instant_url());

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
  history->QueryURL(GURL(instant_url), false, &consumer,
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

// On Windows, the call to NewEmptyWindow() fails the "GetBackingStore called
// while hidden" DCHECK(). It's not clear why; maybe because the active tab is
// in a hidden state when the Instant preview is showing, and somebody's trying
// to get its backing store?
#if !defined(OS_WIN)

#if defined(OS_LINUX) && defined(NDEBUG)
// Flakily fails in Release builds - http://crbug.com/146419
#define MAYBE_NewWindowDismissesInstant DISABLED_NewWindowDismissesInstant
#else
#define MAYBE_NewWindowDismissesInstant NewWindowDismissesInstant
#endif
// Test that creating a new window hides any currently showing Instant preview.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_NewWindowDismissesInstant) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  SetOmniboxText("search");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_TRUE(instant()->IsCurrent());
  EXPECT_TRUE(instant()->is_showing());

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_CONTROLLER_HIDDEN,
      content::NotificationService::AllSources());
  chrome::NewEmptyWindow(browser()->profile());
  observer.Wait();
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_FALSE(instant()->is_showing());
}
#endif

// Tests that:
// - Instant loader is recreated on OnStaleLoader call when it is hidden.
// - Instant loader is not recreated on OnStaleLoader call when it is visible.
// - Instant loader will be recreated when omnibox loses focus after the timer
//    has stopped.
IN_PROC_BROWSER_TEST_F(InstantTest, InstantLoaderRefresh) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant("instant.html"));
  instant()->OnAutocompleteGotFocus();
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  // Instant is not showing, so a refresh should create a new preview contents.
  EXPECT_TRUE(instant()->loader()->supports_instant());
  instant()->OnStaleLoader();
  EXPECT_FALSE(instant()->loader()->supports_instant());
  WaitFor(chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED);

  // Show Instant.
  SetOmniboxText("query");
  WaitFor(chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN);

  // Refresh the loader, the preview contents should remain the same.
  instant()->OnStaleLoader();
  EXPECT_TRUE(instant()->is_showing());

  instant()->stale_loader_timer_.Stop();
  // The refresh should happen once the omnibox loses focus.
  EXPECT_TRUE(instant()->loader()->supports_instant());
  instant()->OnAutocompleteLostFocus(NULL);
  EXPECT_FALSE(instant()->loader()->supports_instant());
}
