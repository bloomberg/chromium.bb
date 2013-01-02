// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/instant/instant_model_observer.h"
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
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

class InstantTestModelObserver : public InstantModelObserver {
 public:
  InstantTestModelObserver(const InstantModel* model,
                           chrome::search::Mode::Type desired_mode_type)
      : model_(model),
        desired_mode_type_(desired_mode_type) {
    model_->AddObserver(this);
  }

  ~InstantTestModelObserver() {
    model_->RemoveObserver(this);
  }

  void WaitUntilDesiredPreviewState() {
    run_loop_.Run();
  }

  // Overridden from InstantModelObserver:
  virtual void PreviewStateChanged(const InstantModel& model) OVERRIDE {
    if (model.mode().mode == desired_mode_type_)
      run_loop_.Quit();
  }

 private:
  const InstantModel* const model_;
  const chrome::search::Mode::Type desired_mode_type_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(InstantTestModelObserver);
};

class InstantTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(test_server()->Start());
    instant_url_ = test_server()->GetURL("files/instant.html");
  }

  void SetupInstant() {
    TemplateURLService* service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(service);

    TemplateURLData data;
    data.SetURL("http://does/not/exist?q={searchTerms}");
    data.instant_url = instant_url_.spec();

    TemplateURL* template_url = new TemplateURL(browser()->profile(), data);
    service->Add(template_url);  // Takes ownership of |template_url|.
    service->SetDefaultSearchProvider(template_url);

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kInstantEnabled, true);
  }

  InstantController* instant() {
    return browser()->instant_controller()->instant();
  }

  OmniboxView* omnibox() {
    return browser()->window()->GetLocationBar()->GetLocationEntry();
  }

  void KillInstantRenderView() {
    base::KillProcess(
        instant()->GetPreviewContents()->GetRenderProcessHost()->GetHandle(),
        content::RESULT_CODE_KILLED,
        false);
  }

  void FocusOmnibox() {
    // If the omnibox already has focus, just notify Instant.
    if (omnibox()->model()->has_focus()) {
      instant()->OmniboxFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                                     OMNIBOX_FOCUS_CHANGE_EXPLICIT, NULL);
    } else {
      browser()->window()->GetLocationBar()->FocusLocation(false);
    }
  }

  void FocusOmniboxAndWaitForInstantSupport() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
        content::NotificationService::AllSources());
    FocusOmnibox();
    observer.Wait();
  }

  void SetOmniboxText(const std::string& text) {
    FocusOmnibox();
    omnibox()->SetUserText(UTF8ToUTF16(text));
  }

  void SetOmniboxTextAndWaitForInstantToShow(const std::string& text) {
    InstantTestModelObserver observer(
        instant()->model(), chrome::search::Mode::MODE_SEARCH_SUGGESTIONS);
    SetOmniboxText(text);
    observer.WaitUntilDesiredPreviewState();
  }

  std::string WrapScript(const std::string& script) const {
    return "domAutomationController.send(" + script + ")";
  }

  bool GetBoolFromJS(content::RenderViewHost* rvh,
                     const std::string& script,
                     bool* result) WARN_UNUSED_RESULT {
    return content::ExecuteJavaScriptAndExtractBool(rvh, "", WrapScript(script),
                                                    result);
  }

  bool GetIntFromJS(content::RenderViewHost* rvh,
                    const std::string& script,
                    int* result) WARN_UNUSED_RESULT {
    return content::ExecuteJavaScriptAndExtractInt(rvh, "", WrapScript(script),
                                                   result);
  }

  bool GetStringFromJS(content::RenderViewHost* rvh,
                       const std::string& script,
                       std::string* result) WARN_UNUSED_RESULT {
    return content::ExecuteJavaScriptAndExtractString(rvh, "",
                                                      WrapScript(script),
                                                      result);
  }

  bool UpdateSearchState(content::WebContents* contents) WARN_UNUSED_RESULT {
    content::RenderViewHost* rvh = contents->GetRenderViewHost();
    return GetIntFromJS(rvh, "onvisibilitycalls", &onvisibilitycalls_) &&
           GetIntFromJS(rvh, "onchangecalls", &onchangecalls_) &&
           GetIntFromJS(rvh, "onsubmitcalls", &onsubmitcalls_) &&
           GetIntFromJS(rvh, "oncancelcalls", &oncancelcalls_) &&
           GetIntFromJS(rvh, "onresizecalls", &onresizecalls_) &&
           GetStringFromJS(rvh, "value", &value_) &&
           GetBoolFromJS(rvh, "verbatim", &verbatim_) &&
           GetIntFromJS(rvh, "height", &height_);
  }

  bool ExecuteScript(const std::string& script) WARN_UNUSED_RESULT {
    return content::ExecuteJavaScript(
        instant()->GetPreviewContents()->GetRenderViewHost(),
        "",
        script);
  }

  bool CheckVisibilityIs(content::WebContents* contents,
                         bool expected) WARN_UNUSED_RESULT {
    bool actual = !expected;  // Purposely start with a mis-match.
    // We can only use ASSERT_*() in a method that returns void, hence this
    // convoluted check.
    return GetBoolFromJS(contents->GetRenderViewHost(),
                         "!document.webkitHidden", &actual) &&
           actual == expected;
  }

  GURL instant_url_;

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

  // Doing a search should also use the same preloaded page.
  SetOmniboxTextAndWaitForInstantToShow("query");
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());
}

// Test that the onchange event is dispatched upon typing in the omnibox.
IN_PROC_BROWSER_TEST_F(InstantTest, OnChangeEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Use the Instant page as the active tab, so we can exploit its visibility
  // handler to check visibility transitions.
  ui_test_utils::NavigateToURL(browser(), instant_url_);
  content::RenderViewHost* active_rvh =
      chrome::GetActiveWebContents(browser())->GetRenderViewHost();

  int active_tab_onvisibilitycalls = -1;
  EXPECT_TRUE(GetIntFromJS(active_rvh, "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(0, active_tab_onvisibilitycalls);

  // Typing "query" into the omnibox causes one or more onchange events. The
  // page suggested "query suggestion" is inline autocompleted into the omnibox,
  // causing another onchange event.
  SetOmniboxTextAndWaitForInstantToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query suggestion"), omnibox()->GetText());
  int min_onchangecalls = 2;

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Change the query and confirm more onchange events are sent.
  SetOmniboxText("search");
  ++min_onchangecalls;

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_LE(min_onchangecalls, onchangecalls_);

  // The preview was shown once, and the active tab was never hidden.
  EXPECT_EQ(1, onvisibilitycalls_);
  active_tab_onvisibilitycalls = -1;
  EXPECT_TRUE(GetIntFromJS(active_rvh, "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(0, active_tab_onvisibilitycalls);
}

// Test that the onsubmit event is dispatched upon pressing Enter.
IN_PROC_BROWSER_TEST_F(InstantTest, OnSubmitEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();
  SetOmniboxTextAndWaitForInstantToShow("search");

  // Stash a reference to the preview, so we can refer to it after commit.
  content::WebContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);

  // The state of the searchbox before the commit.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_FALSE(verbatim_);
  EXPECT_EQ(0, onsubmitcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);

  // Before the commit, the active tab is the NTP (i.e., not Instant).
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(preview_tab, active_tab);
  EXPECT_EQ(1, active_tab->GetController().GetEntryCount());
  EXPECT_EQ(std::string(chrome::kAboutBlankURL),
            omnibox()->model()->PermanentURL().spec());

  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();

  // After the commit, Instant should not be showing.
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // The old loader is deleted and a new one is created.
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_NE(instant()->GetPreviewContents(), preview_tab);

  // Check that the current active tab is indeed what was once the preview.
  EXPECT_EQ(preview_tab, browser()->tab_strip_model()->GetActiveWebContents());

  // We should have two navigation entries, one for the NTP, and one for the
  // Instant search that was committed.
  EXPECT_EQ(2, preview_tab->GetController().GetEntryCount());

  // Check that the omnibox contains the Instant URL we loaded.
  EXPECT_EQ(instant_url_, omnibox()->model()->PermanentURL());

  // Check that the searchbox API values have been reset.
  std::string value;
  EXPECT_TRUE(GetStringFromJS(preview_tab->GetRenderViewHost(),
                              "chrome.searchBox.value", &value));
  EXPECT_EQ("", value);

  // However, the page should've correctly received the committed query.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_TRUE(verbatim_);
  EXPECT_EQ(1, onsubmitcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);
}

// Test that the oncancel event is dispatched upon clicking on the preview.
IN_PROC_BROWSER_TEST_F(InstantTest, OnCancelEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  FocusOmniboxAndWaitForInstantSupport();
  SetOmniboxTextAndWaitForInstantToShow("search");

  // Stash a reference to the preview, so we can refer to it after commit.
  content::WebContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);

  // The state of the searchbox before the commit.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_FALSE(verbatim_);
  EXPECT_EQ(0, oncancelcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);

  // Before the commit, the active tab is the NTP (i.e., not Instant).
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(preview_tab, active_tab);
  EXPECT_EQ(1, active_tab->GetController().GetEntryCount());
  EXPECT_EQ(std::string(chrome::kAboutBlankURL),
            omnibox()->model()->PermanentURL().spec());

  // Commit the search by clicking on the preview.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // After the commit, Instant should not be showing.
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // The old loader is deleted and a new one is created.
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_NE(instant()->GetPreviewContents(), preview_tab);

  // Check that the current active tab is indeed what was once the preview.
  EXPECT_EQ(preview_tab, browser()->tab_strip_model()->GetActiveWebContents());

  // We should have two navigation entries, one for the NTP, and one for the
  // Instant search that was committed.
  EXPECT_EQ(2, preview_tab->GetController().GetEntryCount());

  // Check that the omnibox contains the Instant URL we loaded.
  EXPECT_EQ(instant_url_, omnibox()->model()->PermanentURL());

  // Check that the searchbox API values have been reset.
  std::string value;
  EXPECT_TRUE(GetStringFromJS(preview_tab->GetRenderViewHost(),
                              "chrome.searchBox.value", &value));
  EXPECT_EQ("", value);

  // However, the page should've correctly received the committed query.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_TRUE(verbatim_);
  EXPECT_EQ(1, oncancelcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);
}

// Test that the onreisze event is dispatched upon typing in the omnibox.
IN_PROC_BROWSER_TEST_F(InstantTest, OnResizeEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(0, onresizecalls_);
  EXPECT_EQ(0, height_);

  // Type a query into the omnibox. This should cause an onresize() event, with
  // a valid (non-zero) height.
  SetOmniboxTextAndWaitForInstantToShow("search");

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ(1, onresizecalls_);
  EXPECT_LT(0, height_);
}

// Test that the INSTANT_COMPLETE_NOW behavior works as expected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsCompletedNow) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Tell the JS to request the given behavior.
  EXPECT_TRUE(ExecuteScript("behavior = 'now'"));

  // Type a query, causing the hardcoded "query suggestion" to be returned.
  SetOmniboxTextAndWaitForInstantToShow("query");

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Tell the JS to request the given behavior.
  EXPECT_TRUE(ExecuteScript("behavior = 'never'"));

  // Type a query, causing the hardcoded "query suggestion" to be returned.
  SetOmniboxTextAndWaitForInstantToShow("query");

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
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Tell the JS to use the given suggestion.
  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: 'query completion' } ]"));

  // Type a query, causing "query completion" to be returned as the suggestion.
  SetOmniboxTextAndWaitForInstantToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query completion"), omnibox()->GetText());
}

// Test that an invalid suggestion is rejected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsInvalidObject) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Tell the JS to use an object in an invalid format.
  EXPECT_TRUE(ExecuteScript("suggestion = { value: 'query completion' }"));

  // Type a query, but expect no suggestion.
  SetOmniboxTextAndWaitForInstantToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query"), omnibox()->GetText());
}

// Test that various forms of empty suggestions are rejected.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionIsEmpty) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_TRUE(ExecuteScript("suggestion = {}"));
  SetOmniboxTextAndWaitForInstantToShow("query");
  EXPECT_EQ(ASCIIToUTF16("query"), omnibox()->GetText());

  omnibox()->RevertAll();

  EXPECT_TRUE(ExecuteScript("suggestion = []"));
  SetOmniboxTextAndWaitForInstantToShow("query sugg");
  EXPECT_EQ(ASCIIToUTF16("query sugg"), omnibox()->GetText());

  omnibox()->RevertAll();

  EXPECT_TRUE(ExecuteScript("suggestion = [{}]"));
  SetOmniboxTextAndWaitForInstantToShow("query suggest");
  EXPECT_EQ(ASCIIToUTF16("query suggest"), omnibox()->GetText());
}

// Test that Instant doesn't process URLs.
IN_PROC_BROWSER_TEST_F(InstantTest, RejectsURLs) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Note that we are not actually navigating to these URLs yet. We are just
  // typing them into the omnibox (without pressing Enter) and checking that
  // Instant doesn't try to process them.
  SetOmniboxText(chrome::kChromeUICrashURL);
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  SetOmniboxText(chrome::kChromeUIHangURL);
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  SetOmniboxText(chrome::kChromeUIKillURL);
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Make sure that the URLs were never sent to the preview page.
  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_EQ("", value_);
}

// Test that Instant doesn't fire for intranet paths that look like searches.
// http://crbug.com/99836
IN_PROC_BROWSER_TEST_F(InstantTest, IntranetPathLooksLikeSearch) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  // Navigate to a URL that looks like a search (when the scheme is stripped).
  // It's okay if the host is bogus or the navigation fails, since we only care
  // that Instant doesn't act on it.
  ui_test_utils::NavigateToURL(browser(), GURL("http://baby/beluga"));
  EXPECT_EQ(ASCIIToUTF16("baby/beluga"), omnibox()->GetText());

  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

// Test that transitions between searches and non-searches work as expected.
IN_PROC_BROWSER_TEST_F(InstantTest, TransitionsBetweenSearchAndURL) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Type a search, and immediately a URL, without waiting for Instant to show.
  // The page is told about the search. Though the page isn't told about the
  // subsequent URL, it invalidates the search, so a blank query is sent in its
  // place to indicate that the search is "out of date".
  SetOmniboxText("query");
  SetOmniboxText("http://monstrous/nightmare");
  int min_onchangecalls = 2;

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_EQ("", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Type a search. Instant should show.
  SetOmniboxTextAndWaitForInstantToShow("search");
  ++min_onchangecalls;

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_TRUE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  EXPECT_EQ("search", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Type another URL. The preview should be hidden.
  SetOmniboxText("http://terrible/terror");
  ++min_onchangecalls;

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_EQ("", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Type the same search as before.
  SetOmniboxTextAndWaitForInstantToShow("search");
  min_onchangecalls++;

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_TRUE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  EXPECT_EQ("search", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
  min_onchangecalls = onchangecalls_;

  // Revert the omnibox.
  omnibox()->RevertAll();
  min_onchangecalls++;

  EXPECT_TRUE(UpdateSearchState(instant()->GetPreviewContents()));
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_EQ("", value_);
  EXPECT_LE(min_onchangecalls, onchangecalls_);
}

// Test that Instant can't be fooled into committing a URL.
IN_PROC_BROWSER_TEST_F(InstantTest, DoesNotCommitURLsOne) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Type a URL. The Instant preview shouldn't be showing.
  SetOmniboxText("http://deadly/nadder");
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Unfocus and refocus the omnibox.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  FocusOmnibox();

  content::WebContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);

  // The omnibox text hasn't changed, so Instant still shouldn't be showing.
  EXPECT_EQ(ASCIIToUTF16("http://deadly/nadder"), omnibox()->GetText());
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Commit the URL. The omnibox should reflect the URL minus the scheme.
  browser()->window()->GetLocationBar()->AcceptInput();
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(preview_tab, active_tab);
  EXPECT_EQ(ASCIIToUTF16("deadly/nadder"), omnibox()->GetText());

  // Instant shouldn't have done anything.
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

// Test that Instant can't be fooled into committing a URL.
IN_PROC_BROWSER_TEST_F(InstantTest, DoesNotCommitURLsTwo) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Type a query. This causes the preview to be shown.
  SetOmniboxTextAndWaitForInstantToShow("query");

  content::WebContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);

  // Type a URL. This causes the preview to be hidden.
  SetOmniboxText("http://hideous/zippleback");
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Pretend the omnibox got focus. It already had focus, so we are just trying
  // to tickle a different code path.
  instant()->OmniboxFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                                 OMNIBOX_FOCUS_CHANGE_EXPLICIT, NULL);

  // Commit the URL. As before, check that Instant wasn't committed.
  browser()->window()->GetLocationBar()->AcceptInput();
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(preview_tab, active_tab);
  EXPECT_EQ(ASCIIToUTF16("hideous/zippleback"), omnibox()->GetText());

  // As before, Instant shouldn't have done anything.
  EXPECT_EQ(preview_tab, instant()->GetPreviewContents());
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

// Test that a non-Instant search provider shows no previews.
IN_PROC_BROWSER_TEST_F(InstantTest, NonInstantSearchProvider) {
  instant_url_ = test_server()->GetURL("files/empty.html");
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  // Focus the omnibox. When the support determination response comes back,
  // Instant will destroy the non-Instant page, and attempt to recreate it.
  // We can know this happened by looking at the blacklist.
  EXPECT_EQ(0, instant()->blacklisted_urls_[instant_url_.spec()]);
  FocusOmniboxAndWaitForInstantSupport();
  EXPECT_EQ(1, instant()->blacklisted_urls_[instant_url_.spec()]);
}

// Test that the renderer doesn't crash if JavaScript is blocked.
IN_PROC_BROWSER_TEST_F(InstantTest, NoCrashOnBlockedJS) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  ASSERT_NO_FATAL_FAILURE(SetupInstant());

  // Wait for notification that the Instant API has been determined. As long as
  // we get the notification we're good (the renderer didn't crash).
  FocusOmniboxAndWaitForInstantSupport();
}

// Test that the preview and active tab's visibility states are set correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, PageVisibility) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* preview_tab = instant()->GetPreviewContents();

  // Inititally, the active tab is showing; the preview is not.
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, false));

  // Type a query and wait for Instant to show.
  SetOmniboxTextAndWaitForInstantToShow("query");
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, true));

  // Deleting the omnibox text should hide the preview.
  SetOmniboxText("");
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, false));

  // Typing a query should show the preview again.
  SetOmniboxTextAndWaitForInstantToShow("query");
  EXPECT_TRUE(CheckVisibilityIs(active_tab, true));
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, true));

  // Commit the preview.
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_EQ(preview_tab, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(CheckVisibilityIs(preview_tab, true));
}

// Test that the task manager identifies Instant's preview tab correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, TaskManagerPrefix) {
  // The browser starts with a new tab, so there's just one renderer initially.
  TaskManagerModel* task_manager = TaskManager::GetInstance()->model();
  task_manager->StartUpdating();
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);

  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_INSTANT_PREVIEW_PREFIX, string16());

  // There should be no Instant preview yet.
  for (int i = 0; i < task_manager->ResourceCount(); ++i) {
    string16 title = task_manager->GetResourceTitle(i);
    EXPECT_FALSE(StartsWith(title, prefix, true)) << title << " vs " << prefix;
  }

  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmnibox();

  // Now there should be two renderers, the second being the Instant preview.
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

  int instant_previews = 0;
  for (int i = 0; i < task_manager->ResourceCount(); ++i) {
    string16 title = task_manager->GetResourceTitle(i);
    if (StartsWith(title, prefix, true))
      ++instant_previews;
  }
  EXPECT_EQ(1, instant_previews);
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
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
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
  SetOmniboxTextAndWaitForInstantToShow("search");

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
  history->QueryURL(instant_url_, false, &consumer,
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
// Test that creating a new window hides any currently showing Instant preview.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_NewWindowDismissesInstant) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  FocusOmniboxAndWaitForInstantSupport();
  SetOmniboxTextAndWaitForInstantToShow("search");

  Browser* previous_window = browser();
  EXPECT_TRUE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());

  InstantTestModelObserver observer(instant()->model(),
                                    chrome::search::Mode::MODE_DEFAULT);
  chrome::NewEmptyWindow(browser()->profile());
  observer.WaitUntilDesiredPreviewState();

  // Even though we just created a new Browser object (for the new window), the
  // browser() accessor should still give us the first window's Browser object.
  EXPECT_EQ(previous_window, browser());
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

// Test that the Instant loader is recreated when all these conditions are met:
// - The stale loader timer has fired.
// - The preview is not showing.
// - The omnibox doesn't have focus.
IN_PROC_BROWSER_TEST_F(InstantTest, InstantLoaderRefresh) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // The preview is refreshed only after all three conditions above are met.
  SetOmniboxTextAndWaitForInstantToShow("query");
  instant()->stale_loader_timer_.Stop();
  instant()->OnStaleLoader();
  EXPECT_TRUE(instant()->loader_->supports_instant());
  instant()->HideLoader();
  EXPECT_TRUE(instant()->loader_->supports_instant());
  instant()->OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                 OMNIBOX_FOCUS_CHANGE_EXPLICIT, NULL);
  EXPECT_FALSE(instant()->loader_->supports_instant());

  // Try with a different ordering.
  SetOmniboxTextAndWaitForInstantToShow("query");
  instant()->stale_loader_timer_.Stop();
  instant()->OnStaleLoader();
  EXPECT_TRUE(instant()->loader_->supports_instant());
  instant()->OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                 OMNIBOX_FOCUS_CHANGE_EXPLICIT, NULL);
  // TODO(sreeram): Currently, OmniboxLostFocus() calls HideLoader(). When it
  // stops hiding the preview eventually, uncomment these two lines:
  //     EXPECT_TRUE(instant()->loader_->supports_instant());
  //     instant()->HideLoader();
  EXPECT_FALSE(instant()->loader_->supports_instant());
}

// Test that suggestions are case insensitive. http://crbug.com/150728
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionsAreCaseInsensitive) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: 'INSTANT' } ]"));

  SetOmniboxTextAndWaitForInstantToShow("in");
  EXPECT_EQ(ASCIIToUTF16("instant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForInstantToShow("IN");
  EXPECT_EQ(ASCIIToUTF16("INSTANT"), omnibox()->GetText());

  // U+0130 == LATIN CAPITAL LETTER I WITH DOT ABOVE
  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: '\\u0130NSTANT' } ]"));

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForInstantToShow("i");
  EXPECT_EQ(WideToUTF16(L"i\u0307nstant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForInstantToShow("I");
  EXPECT_EQ(WideToUTF16(L"I\u0307nstant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForInstantToShow(WideToUTF8(L"i\u0307"));
  EXPECT_EQ(WideToUTF16(L"i\u0307nstant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForInstantToShow(WideToUTF8(L"I\u0307"));
  EXPECT_EQ(WideToUTF16(L"I\u0307nstant"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForInstantToShow(WideToUTF8(L"\u0130"));
  EXPECT_EQ(WideToUTF16(L"\u0130NSTANT"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForInstantToShow("in");
  EXPECT_EQ(ASCIIToUTF16("in"), omnibox()->GetText());

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForInstantToShow("IN");
  EXPECT_EQ(ASCIIToUTF16("IN"), omnibox()->GetText());

  // Check that a d with a dot above and below it is completed regardless of
  // how that is encoded.
  // U+1E0D = LATIN SMALL LETTER D WITH DOT BELOW
  // U+1E0B = LATIN SMALL LETTER D WITH DOT ABOVE
  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: '\\u1e0d\\u0307oh' } ]"));

  omnibox()->RevertAll();
  SetOmniboxTextAndWaitForInstantToShow(WideToUTF8(L"\u1e0b\u0323"));
  EXPECT_EQ(WideToUTF16(L"\u1e0b\u0323oh"), omnibox()->GetText());
}

// Test that the preview can be committed onto a new tab.
IN_PROC_BROWSER_TEST_F(InstantTest, CommitInNewTab) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Use the Instant page as the active tab, so we can exploit its visibility
  // handler to check visibility transitions.
  ui_test_utils::NavigateToURL(browser(), instant_url_);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  int active_tab_onvisibilitycalls = -1;
  EXPECT_TRUE(GetIntFromJS(active_tab->GetRenderViewHost(), "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(0, active_tab_onvisibilitycalls);

  SetOmniboxTextAndWaitForInstantToShow("search");

  // Stash a reference to the preview, so we can refer to it after commit.
  content::WebContents* preview_tab = instant()->GetPreviewContents();
  EXPECT_TRUE(preview_tab);

  // The state of the searchbox before the commit.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_FALSE(verbatim_);
  EXPECT_EQ(0, onsubmitcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);

  // The state of the active tab before the commit.
  EXPECT_NE(preview_tab, active_tab);
  EXPECT_EQ(2, active_tab->GetController().GetEntryCount());
  EXPECT_EQ(instant_url_, omnibox()->model()->PermanentURL());
  active_tab_onvisibilitycalls = -1;
  EXPECT_TRUE(GetIntFromJS(active_tab->GetRenderViewHost(), "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(0, active_tab_onvisibilitycalls);

  // Commit the search by pressing Alt-Enter.
  omnibox()->model()->AcceptInput(NEW_FOREGROUND_TAB, false);

  // After the commit, Instant should not be showing.
  EXPECT_FALSE(instant()->IsPreviewingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // The old loader is deleted and a new one is created.
  EXPECT_TRUE(instant()->GetPreviewContents());
  EXPECT_NE(instant()->GetPreviewContents(), preview_tab);

  // Check that we have two tabs and that the new active tab is indeed what was
  // once the preview. The preview tab should have just one navigation entry,
  // for the Instant search that was committed.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(preview_tab, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(1, preview_tab->GetController().GetEntryCount());

  // Check that the omnibox contains the Instant URL we loaded.
  EXPECT_EQ(instant_url_, omnibox()->model()->PermanentURL());

  // Check that the searchbox API values have been reset.
  std::string value;
  EXPECT_TRUE(GetStringFromJS(preview_tab->GetRenderViewHost(),
                              "chrome.searchBox.value", &value));
  EXPECT_EQ("", value);

  // However, the page should've correctly received the committed query.
  EXPECT_TRUE(UpdateSearchState(preview_tab));
  EXPECT_EQ("search", value_);
  EXPECT_TRUE(verbatim_);
  EXPECT_EQ(1, onsubmitcalls_);
  EXPECT_EQ(1, onvisibilitycalls_);

  // The ex-active tab should've gotten a visibility change marking it hidden.
  EXPECT_NE(active_tab, preview_tab);
  EXPECT_TRUE(GetIntFromJS(active_tab->GetRenderViewHost(), "onvisibilitycalls",
                           &active_tab_onvisibilitycalls));
  EXPECT_EQ(1, active_tab_onvisibilitycalls);
}

// Test that suggestions are reusable.
IN_PROC_BROWSER_TEST_F(InstantTest, SuggestionsAreReusable) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  EXPECT_TRUE(ExecuteScript("suggestion = [ { value: 'instant' } ];"
                            "behavior = 'never';"));

  SetOmniboxTextAndWaitForInstantToShow("in");
  EXPECT_EQ(ASCIIToUTF16("stant"), omnibox()->GetInstantSuggestion());

  SetOmniboxText("ins");
  EXPECT_EQ(ASCIIToUTF16("tant"), omnibox()->GetInstantSuggestion());

  SetOmniboxText("in");
  EXPECT_EQ(ASCIIToUTF16("stant"), omnibox()->GetInstantSuggestion());

  SetOmniboxText("insane");
  EXPECT_EQ(ASCIIToUTF16(""), omnibox()->GetInstantSuggestion());
}

// Test that instant loader is recreated if it gets destroyed.
IN_PROC_BROWSER_TEST_F(InstantTest, InstantRenderViewGone) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant());
  FocusOmniboxAndWaitForInstantSupport();

  // Type partial query, get suggestion to show.
  SetOmniboxTextAndWaitForInstantToShow("q");
  EXPECT_EQ(ASCIIToUTF16("query suggestion"), omnibox()->GetText());

  // Kill the instant renderer and wait for instant support again.
  KillInstantRenderView();
  FocusOmniboxAndWaitForInstantSupport();

  SetOmniboxTextAndWaitForInstantToShow("qu");
  EXPECT_EQ(ASCIIToUTF16("query suggestion"), omnibox()->GetText());
}
