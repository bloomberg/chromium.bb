// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// Tests are flaky on Linux because of http://crbug.com/80118.
#if defined(OS_LINUX)
#define MAYBE(TestName) DISABLED_ ## TestName
#else
#define MAYBE(TestName) TestName
#endif

class InstantTest : public InProcessBrowserTest {
 public:
  InstantTest() {
    set_show_window(true);
    EnableDOMAutomation();
  }

  void EnableInstant() {
    InstantController::Enable(browser()->profile());
  }

  void SetupInstantProvider(const std::string& page) {
    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());

    ui_test_utils::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
        content::NotificationService::AllSources());
    if (!model->loaded()) {
      model->Load();
      observer.Wait();
    }

    // TemplateURLService takes ownership of this.
    TemplateURL* template_url = new TemplateURL();

    std::string url = base::StringPrintf(
        "http://%s:%d/files/%s?q={searchTerms}",
        test_server()->host_port_pair().host().c_str(),
        test_server()->host_port_pair().port(),
        page.c_str());
    template_url->SetURL(url, 0, 0);
    template_url->SetInstantURL(url, 0, 0);
    template_url->set_keyword(ASCIIToUTF16("foo"));
    template_url->set_short_name(ASCIIToUTF16("foo"));

    model->Add(template_url);
    model->SetDefaultSearchProvider(template_url);
  }

  // Type a character to get instant to trigger and determine instant support.
  void DetermineInstantSupport() {
    ui_test_utils::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
        content::NotificationService::AllSources());
    // "a" triggers the "about:" provider. "b" begins the "bing.com" keyword.
    // "c" might someday trigger a "chrome:" provider.
    omnibox()->SetUserText(ASCIIToUTF16("d"));
    observer.Wait();
  }

  // Types "def" into the omnibox and waits for the preview to be shown.
  void SearchAndWaitForPreviewToShow() {
    ui_test_utils::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN,
        content::NotificationService::AllSources());
    omnibox()->SetUserText(ASCIIToUTF16("def"));
    observer.Wait();
  }

  // Sends a message to the renderer and waits for the response to come back to
  // the browser. Returns true on success.
  bool WaitForMessageToBeProcessedByRenderer() {
    bool result = false;
    return GetBoolFromJavascript(preview()->tab_contents(), "true", &result) &&
           result;
  }

  InstantController* instant() const {
    return browser()->instant();
  }

  OmniboxView* omnibox() const {
    return browser()->window()->GetLocationBar()->location_entry();
  }

  TabContentsWrapper* preview() const {
    return instant()->GetPreviewContents();
  }

  InstantLoader* loader() const {
    return instant()->loader_.get();
  }

  std::string GetSuggestion() const {
    return UTF16ToUTF8(loader()->complete_suggested_text_);
  }

  bool PressEnter() {
    return ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_RETURN, false, false, false, false);
  }

  bool SetSuggestionsJavascriptArgument(const std::string& argument) {
    std::wstring script = UTF8ToWide(base::StringPrintf(
        "window.setSuggestionsArgument = %s;", argument.c_str()));
    RenderViewHost* rvh = preview()->tab_contents()->render_view_host();
    return ui_test_utils::ExecuteJavaScript(rvh, std::wstring(), script);
  }

  std::wstring WrapScript(const std::string& script) {
    return UTF8ToWide(base::StringPrintf(
        "window.domAutomationController.send(%s)", script.c_str()));
  }

  bool GetStringFromJavascript(TabContents* tab,
                               const std::string& script,
                               std::string* result) {
    return ui_test_utils::ExecuteJavaScriptAndExtractString(
        tab->render_view_host(), std::wstring(), WrapScript(script), result);
  }

  bool GetIntFromJavascript(TabContents* tab,
                            const std::string& script,
                            int* result) {
    return ui_test_utils::ExecuteJavaScriptAndExtractInt(
        tab->render_view_host(), std::wstring(), WrapScript(script), result);
  }

  bool GetBoolFromJavascript(TabContents* tab,
                             const std::string& script,
                             bool* result) {
    return ui_test_utils::ExecuteJavaScriptAndExtractBool(
        tab->render_view_host(), std::wstring(), WrapScript(script), result);
  }

  bool CheckVisibilityIs(TabContents* tab, bool visible) {
    bool hidden = visible;
    return GetBoolFromJavascript(tab, "document.webkitHidden", &hidden) &&
           hidden != visible;
  }

  // Returns the state of the search box as a string. This consists of the
  // following:
  // window.chrome.sv
  // window.onsubmitcalls
  // window.oncancelcalls
  // window.onchangecalls
  // 'true' if any window.onresize call has been sent, otherwise false.
  // window.beforeLoadSearchBox.value
  // window.beforeLoadSearchBox.verbatim
  // window.chrome.searchBox.value
  // window.chrome.searchBox.verbatim
  // window.chrome.searchBox.selectionStart
  // window.chrome.searchBox.selectionEnd
  // If determining any of the values fails, the value is 'fail'.
  //
  // If |use_last| is true, then the last searchBox values are used instead of
  // the current. Set |use_last| to true when testing OnSubmit/OnCancel.
  std::string GetSearchStateAsString(TabContents* tab, bool use_last) {
    bool sv = false;
    int onsubmitcalls = 0;
    int oncancelcalls = 0;
    int onchangecalls = 0;
    int onresizecalls = 0;
    int selection_start = 0;
    int selection_end = 0;
    std::string before_load_value;
    bool before_load_verbatim = false;
    std::string value;
    bool verbatim = false;

    if (!GetBoolFromJavascript(tab, "window.chrome.sv", &sv) ||
        !GetIntFromJavascript(tab, "window.onsubmitcalls", &onsubmitcalls) ||
        !GetIntFromJavascript(tab, "window.oncancelcalls", &oncancelcalls) ||
        !GetIntFromJavascript(tab, "window.onchangecalls", &onchangecalls) ||
        !GetIntFromJavascript(tab, "window.onresizecalls", &onresizecalls) ||
        !GetStringFromJavascript(tab, "window.beforeLoadSearchBox.value",
                                 &before_load_value) ||
        !GetBoolFromJavascript(tab, "window.beforeLoadSearchBox.verbatim",
                               &before_load_verbatim)) {
      return "fail";
    }

    if (use_last &&
        (!GetStringFromJavascript(tab, "window.lastSearchBox.value", &value) ||
         !GetBoolFromJavascript(tab, "window.lastSearchBox.verbatim",
                                &verbatim) ||
         !GetIntFromJavascript(tab, "window.lastSearchBox.selectionStart",
                               &selection_start) ||
         !GetIntFromJavascript(tab, "window.lastSearchBox.selectionEnd",
                               &selection_end))) {
      return "fail";
    }

    if (!use_last &&
        (!GetStringFromJavascript(tab, "window.chrome.searchBox.value",
                                  &value) ||
         !GetBoolFromJavascript(tab, "window.chrome.searchBox.verbatim",
                                &verbatim) ||
         !GetIntFromJavascript(tab, "window.chrome.searchBox.selectionStart",
                               &selection_start) ||
         !GetIntFromJavascript(tab, "window.chrome.searchBox.selectionEnd",
                               &selection_end))) {
      return "fail";
    }

    return base::StringPrintf("%s %d %d %d %s %s %s %s %s %d %d",
                              sv ? "true" : "false",
                              onsubmitcalls,
                              oncancelcalls,
                              onchangecalls,
                              onresizecalls ? "true" : "false",
                              before_load_value.c_str(),
                              before_load_verbatim ? "true" : "false",
                              value.c_str(),
                              verbatim ? "true" : "false",
                              selection_start,
                              selection_end);
  }
};

// TODO(tonyg): Add the following tests:
// - Test that the search box API is not populated for pages other than the
//   default search provider.
// - Test resize events.

// Verify that the onchange event is dispatched upon typing in the box.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(OnChangeEvent)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();
  SearchAndWaitForPreviewToShow();

  EXPECT_TRUE(preview());
  EXPECT_TRUE(instant()->is_displayable());
  EXPECT_TRUE(instant()->IsCurrent());
  EXPECT_EQ("defghi", UTF16ToUTF8(omnibox()->GetText()));

  // Make sure the URL that will get committed when we press <Enter> matches
  // that of the default search provider.
  const TemplateURL* default_turl =
      TemplateURLServiceFactory::GetForProfile(browser()->profile())->
      GetDefaultSearchProvider();
  EXPECT_TRUE(default_turl);
  EXPECT_TRUE(default_turl->url());
  EXPECT_EQ(default_turl->url()->ReplaceSearchTerms(
                *default_turl, ASCIIToUTF16("defghi"), 0, string16()),
            loader()->url().spec());

  // Check that the value is reflected and onchange is called.
  EXPECT_EQ("true 0 0 1 true d false def false 3 3",
            GetSearchStateAsString(preview()->tab_contents(), false));
}

// Verify that the onsubmit event is dispatched upon pressing <Enter>.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(OnSubmitEvent)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();
  SearchAndWaitForPreviewToShow();

  EXPECT_TRUE(preview());
  EXPECT_TRUE(instant()->is_displayable());
  EXPECT_TRUE(instant()->IsCurrent());
  EXPECT_EQ("defghi", UTF16ToUTF8(omnibox()->GetText()));

  TabContents* preview_tab = preview()->tab_contents();
  EXPECT_TRUE(preview_tab);

  ASSERT_TRUE(PressEnter());

  // Check that the preview has been committed.
  EXPECT_FALSE(preview());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_EQ(preview_tab, browser()->GetSelectedTabContents());

  // We should have two entries. One corresponding to the page the user was
  // first on, and one for the search page.
  EXPECT_EQ(2, preview_tab->controller().entry_count());

  // Check that the value is reflected and onsubmit is called.
  EXPECT_EQ("true 1 0 1 true d false defghi true 3 3",
            GetSearchStateAsString(preview_tab, true));

  // Make sure the searchbox values were reset.
  EXPECT_EQ("true 1 0 1 true d false  false 0 0",
            GetSearchStateAsString(preview_tab, false));
}

// Verify that the oncancel event is dispatched upon losing focus.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(OnCancelEvent)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();
  SearchAndWaitForPreviewToShow();

  EXPECT_TRUE(preview());
  EXPECT_TRUE(instant()->is_displayable());
  EXPECT_TRUE(instant()->IsCurrent());
  EXPECT_EQ("defghi", UTF16ToUTF8(omnibox()->GetText()));

  TabContents* preview_tab = preview()->tab_contents();
  EXPECT_TRUE(preview_tab);

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Check that the preview has been committed.
  EXPECT_FALSE(preview());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_EQ(preview_tab, browser()->GetSelectedTabContents());

  // Check that the value is reflected and oncancel is called.
  EXPECT_EQ("true 0 1 1 true d false def false 3 3",
            GetSearchStateAsString(preview_tab, true));

  // Make sure the searchbox values were reset.
  EXPECT_EQ("true 0 1 1 true d false  false 0 0",
            GetSearchStateAsString(preview_tab, false));
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(SetSuggestionsArrayOfStrings)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument("['defg', 'unused']"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("defg", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(SetSuggestionsEmptyArray)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument("[]"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(SetSuggestionsValidJson)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument(
      "{suggestions:[{value:'defg'},{value:'unused'}]}"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("defg", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(SetSuggestionsInvalidSuggestions)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument("{suggestions:{value:'defg'}}"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(SetSuggestionsEmptyJson)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument("{}"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(SetSuggestionsEmptySuggestions)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument("{suggestions:[]}"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(SetSuggestionsEmptySuggestion)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument("{suggestions:[{}]}"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(InstantCompleteNever)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument(
      "{suggestions:[{value:'defg'}],complete_behavior:'never'}"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("defg", GetSuggestion());

  EXPECT_EQ(INSTANT_COMPLETE_NEVER,
            omnibox()->model()->instant_complete_behavior());
  EXPECT_EQ("def", UTF16ToUTF8(omnibox()->GetText()));
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(InstantCompleteDelayed)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument(
      "{suggestions:[{value:'defg'}],complete_behavior:'delayed'}"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("defg", GetSuggestion());

  EXPECT_EQ(INSTANT_COMPLETE_DELAYED,
            omnibox()->model()->instant_complete_behavior());
  EXPECT_EQ("def", UTF16ToUTF8(omnibox()->GetText()));
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(InstantCompleteNow)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  ASSERT_TRUE(SetSuggestionsJavascriptArgument(
      "{suggestions:[{value:'defg'}],complete_behavior:'now'}"));
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ("defg", GetSuggestion());

  EXPECT_EQ(INSTANT_COMPLETE_NOW,
            omnibox()->model()->instant_complete_behavior());
  EXPECT_EQ("defg", UTF16ToUTF8(omnibox()->GetText()));
}

// Verifies that instant previews aren't shown for crash URLs.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(CrashUrlCancelsInstant)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");

  omnibox()->SetUserText(ASCIIToUTF16(chrome::kChromeUICrashURL));
  EXPECT_FALSE(preview());
}

// Tests that instant doesn't fire for intranet paths that look like searches.
// http://crbug.com/99836
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(IntranetPathLooksLikeSearch)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");

  // Unfocus the omnibox. This should delete any existing preview contents.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_FALSE(preview());

  // Navigate to a URL that looks like a search (when the scheme is stripped).
  // It's okay if the host is bogus or the navigation fails, since we only care
  // that instant doesn't act on it.
  ui_test_utils::NavigateToURL(browser(), GURL("http://baby/beluga"));
  EXPECT_EQ("baby/beluga", UTF16ToUTF8(omnibox()->GetText()));
  EXPECT_FALSE(preview());
}

// Verifies that instant previews aren't shown for non-search URLs.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(ShowPreviewNonSearch)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");

  GURL url(test_server()->GetURL("files/empty.html"));
  omnibox()->SetUserText(UTF8ToUTF16(url.spec()));
  EXPECT_FALSE(preview());
}

// Transition from non-search to search and make sure everything works.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(NonSearchToSearch)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");

  // Load a non-search URL.
  GURL url(test_server()->GetURL("files/empty.html"));
  omnibox()->SetUserText(UTF8ToUTF16(url.spec()));
  EXPECT_FALSE(preview());

  // Now type in some search text.
  DetermineInstantSupport();

  // We should now have a preview, but it shouldn't be showing yet, because we
  // haven't gotten back suggestions.
  EXPECT_TRUE(preview());
  EXPECT_FALSE(loader()->ready());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());

  // Reset the user text so that the page is told the text changed.
  //
  // Typing into the omnibox sends onchange() to the page, which responds with
  // suggestions, which causes the preview to be shown. However, when we called
  // DetermineInstantSupport(), the resulting onchange was dropped on the floor
  // because the page wasn't loaded yet. This is fine (the user may type before
  // the page loads too). To handle this, we explicitly call onchange after the
  // page loads (see initScript in searchbox_extension.cc). The search provider
  // used in this test (instant.html) doesn't support initScript, so we have to
  // trigger an onchange ourselves.
  SearchAndWaitForPreviewToShow();

  // We should now be showing the preview.
  EXPECT_TRUE(preview());
  EXPECT_TRUE(loader()->ready());
  EXPECT_TRUE(instant()->is_displayable());
  EXPECT_TRUE(instant()->IsCurrent());

  RenderWidgetHostView* rwhv =
      preview()->tab_contents()->GetRenderWidgetHostView();
  EXPECT_TRUE(rwhv);
  EXPECT_TRUE(rwhv->IsShowing());
}

// Transition from search to non-search and make sure instant isn't displayable.
// See bug http://crbug.com/100368 for details.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(SearchToNonSearch)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");

  ui_test_utils::WindowedNotificationObserver instant_support_observer(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Type in some search text.
  omnibox()->SetUserText(ASCIIToUTF16("def"));

  // Load a non search URL. Don't wait for the preview to navigate. It'll still
  // end up loading in the background.
  GURL url(test_server()->GetURL("files/empty.html"));
  omnibox()->SetUserText(UTF8ToUTF16(url.spec()));

  instant_support_observer.Wait();

  // We should now have a preview, but it shouldn't be showing yet.
  EXPECT_TRUE(preview());
  EXPECT_FALSE(loader()->ready());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());

  // Send onchange so that the page sends up suggestions. See the comments in
  // NonSearchToSearch for why this is needed.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      preview()->tab_contents()->render_view_host(), std::wstring(),
      L"window.chrome.searchBox.onchange();"));
  ASSERT_TRUE(WaitForMessageToBeProcessedByRenderer());

  // Instant should be active, but not displaying.
  EXPECT_TRUE(preview());
  EXPECT_TRUE(loader()->ready());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());
}

// Makes sure that if the server doesn't support the instant API we don't show
// anything.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(SearchServerDoesntSupportInstant)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("empty.html");

  ui_test_utils::WindowedNotificationObserver tab_closed_observer(
      content::NOTIFICATION_TAB_CLOSED,
      content::NotificationService::AllSources());

  omnibox()->SetUserText(ASCIIToUTF16("d"));
  EXPECT_TRUE(preview());

  // When the response comes back that the page doesn't support instant the tab
  // should be closed.
  tab_closed_observer.Wait();
  EXPECT_FALSE(preview());
}

// Verifies transitioning from loading a non-search string to a search string
// with the provider not supporting instant works (meaning we don't display
// anything).
IN_PROC_BROWSER_TEST_F(InstantTest,
                       MAYBE(NonSearchToSearchDoesntSupportInstant)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("empty.html");

  GURL url(test_server()->GetURL("files/empty.html"));
  omnibox()->SetUserText(UTF8ToUTF16(url.spec()));
  EXPECT_FALSE(preview());

  ui_test_utils::WindowedNotificationObserver tab_closed_observer(
      content::NOTIFICATION_TAB_CLOSED,
      content::NotificationService::AllSources());

  // Now type in some search text.
  omnibox()->SetUserText(ASCIIToUTF16("d"));
  EXPECT_TRUE(preview());

  // When the response comes back that the page doesn't support instant the tab
  // should be closed.
  tab_closed_observer.Wait();
  EXPECT_FALSE(preview());
}

// Verifies the page was told a non-zero height.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(ValidHeight)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();
  SearchAndWaitForPreviewToShow();

  int height = -1;

  // searchBox height is not yet set during initial load.
  ASSERT_TRUE(GetIntFromJavascript(preview()->tab_contents(),
      "window.beforeLoadSearchBox.height", &height));
  EXPECT_EQ(0, height);

  // searchBox height is available by the time the page loads.
  ASSERT_TRUE(GetIntFromJavascript(preview()->tab_contents(),
      "window.chrome.searchBox.height", &height));
  EXPECT_GT(height, 0);
}

// Make sure the renderer doesn't crash if javascript is blocked.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(DontCrashOnBlockedJS)) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");

  // Wait for notification that the instant API has been determined. As long as
  // we get the notification we're good (the renderer didn't crash).
  DetermineInstantSupport();
}

// Makes sure window.chrome.searchbox doesn't persist when a new page is loaded.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(DontPersistSearchbox)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();
  SearchAndWaitForPreviewToShow();

  std::string value;
  ASSERT_TRUE(GetStringFromJavascript(preview()->tab_contents(),
      "window.chrome.searchBox.value", &value));
  EXPECT_EQ("def", value);

  // Commit the preview.
  ASSERT_TRUE(PressEnter());
  EXPECT_FALSE(preview());

  // The searchBox actually gets cleared on commit.
  ASSERT_TRUE(GetStringFromJavascript(browser()->GetSelectedTabContents(),
      "window.chrome.searchBox.value", &value));
  EXPECT_EQ("", value);

  // Navigate to a new URL. The searchBox values should stay cleared.
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/empty.html"));

  ASSERT_TRUE(GetStringFromJavascript(browser()->GetSelectedTabContents(),
      "window.chrome.searchBox.value", &value));
  EXPECT_EQ("", value);
}

// Tests that instant search is preloaded whenever the omnibox gets focus.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(PreloadsInstant)) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kPreloadInstantSearch);
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");

  // The omnibox gets focus before the test begins. At that time, there was no
  // instant controller (which was only created after EnableInstant()), so no
  // preloading happened. Unfocus the omnibox with ClickOnView(), so that when
  // we focus it again, the controller will preload instant search.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Verify that there is no preview.
  EXPECT_FALSE(preview());

  // Focusing the omnibox should cause instant to be preloaded.
  ui_test_utils::WindowedNotificationObserver instant_support_observer(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  browser()->window()->GetLocationBar()->FocusLocation(false);
  instant_support_observer.Wait();

  // Instant should have a preview, but not display it.
  EXPECT_TRUE(preview());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());
  ASSERT_TRUE(CheckVisibilityIs(preview()->tab_contents(), false));

  // Adding a new tab shouldn't delete (or recreate) the TabContentsWrapper.
  TabContentsWrapper* preview_tab = preview();
  AddBlankTabAndShow(browser());
  EXPECT_EQ(preview_tab, preview());

  // Doing a search should still use the same loader for the preview.
  SearchAndWaitForPreviewToShow();
  EXPECT_EQ(preview_tab, preview());

  // Verify that the preview is in fact showing instant search.
  EXPECT_TRUE(instant()->is_displayable());
  EXPECT_TRUE(instant()->IsCurrent());
  ASSERT_TRUE(CheckVisibilityIs(preview()->tab_contents(), true));
}

// Tests that the instant search page's visibility is set correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(PageVisibilityTest)) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");

  // Initially navigate to the empty page which should be visible.
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(""));
  TabContents* initial_contents = browser()->GetSelectedTabContents();

  ASSERT_TRUE(CheckVisibilityIs(initial_contents, true));

  // Type a search term and wait for the preview to appear.
  browser()->window()->GetLocationBar()->FocusLocation(false);
  DetermineInstantSupport();
  SearchAndWaitForPreviewToShow();
  TabContents* preview_contents = preview()->tab_contents();

  ASSERT_TRUE(CheckVisibilityIs(preview_contents, true));
  ASSERT_TRUE(CheckVisibilityIs(initial_contents, false));

  // Deleting the user text should hide the preview.
  omnibox()->SetUserText(string16());
  ASSERT_TRUE(CheckVisibilityIs(preview_contents, false));
  ASSERT_TRUE(CheckVisibilityIs(initial_contents, true));

  // Set the user text back and we should see the preview again.
  omnibox()->SetUserText(ASCIIToUTF16("def"));
  ASSERT_TRUE(CheckVisibilityIs(preview_contents, true));
  ASSERT_TRUE(CheckVisibilityIs(initial_contents, false));

  // Commit the preview.
  ASSERT_TRUE(PressEnter());
  EXPECT_EQ(preview_contents, browser()->GetSelectedTabContents());
  ASSERT_TRUE(CheckVisibilityIs(preview_contents, true));
}

// Tests that the task manager identifies instant's preview tab correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE(TaskManagerPrefix)) {
  // The browser starts with one new tab, so the task manager should have two
  // rows initially, one for the browser process and one for tab's renderer.
  TaskManagerModel* task_manager = TaskManager::GetInstance()->model();
  task_manager->StartUpdating();
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();
  SearchAndWaitForPreviewToShow();

  // Now there should be three rows, the third being the instant preview.
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_INSTANT_PREVIEW_PREFIX, string16());
  string16 title = task_manager->GetResourceTitle(2);
  EXPECT_TRUE(StartsWith(title, prefix, true)) << title << " vs " << prefix;
}

// Tests the INSTANT experiment of the field trial.
class InstantFieldTrialInstantTest : public InstantTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kInstantFieldTrial,
                                    switches::kInstantFieldTrialInstant);
  }
};

// Tests that instant is active, even without calling EnableInstant().
IN_PROC_BROWSER_TEST_F(InstantFieldTrialInstantTest, MAYBE(ExperimentEnabled)) {
  // Check that instant is enabled, despite not setting the preference.
  Profile* profile = browser()->profile();
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kInstantEnabled));
  EXPECT_TRUE(InstantController::IsEnabled(profile));

  ASSERT_TRUE(test_server()->Start());
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();
  SearchAndWaitForPreviewToShow();

  // Check that instant is active and showing a preview.
  EXPECT_TRUE(preview());
  EXPECT_TRUE(loader()->ready());
  EXPECT_TRUE(instant()->is_displayable());
  EXPECT_TRUE(instant()->IsCurrent());

  // Check that the suggested text has been set in the omnibox.
  EXPECT_EQ("defghi", GetSuggestion());
  EXPECT_EQ("defghi", UTF16ToUTF8(omnibox()->GetText()));

  // Press <Enter> in the omnibox, causing the preview to be committed.
  TabContents* preview_tab = preview()->tab_contents();
  ASSERT_TRUE(PressEnter());

  // The preview contents should now be the active tab contents.
  EXPECT_FALSE(preview());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_EQ(preview_tab, browser()->GetSelectedTabContents());
}

// Tests the HIDDEN experiment of the field trial.
class InstantFieldTrialHiddenTest : public InstantTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kInstantFieldTrial,
                                    switches::kInstantFieldTrialHidden);
  }
};

// Tests that instant is active, even without calling EnableInstant().
IN_PROC_BROWSER_TEST_F(InstantFieldTrialHiddenTest, MAYBE(ExperimentEnabled)) {
  // Check that instant is enabled, despite not setting the preference.
  Profile* profile = browser()->profile();
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kInstantEnabled));
  EXPECT_TRUE(InstantController::IsEnabled(profile));

  ASSERT_TRUE(test_server()->Start());
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  // Type into the omnibox, but don't press <Enter> yet.
  omnibox()->SetUserText(ASCIIToUTF16("def"));
  ASSERT_TRUE(WaitForMessageToBeProcessedByRenderer());

  // Check that instant is active, but the preview is not showing.
  EXPECT_TRUE(preview());
  EXPECT_TRUE(loader()->ready());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());

  // Check that the suggested text hasn't actually been set in the omnibox.
  EXPECT_EQ("defghi", GetSuggestion());
  EXPECT_EQ("def", UTF16ToUTF8(omnibox()->GetText()));

  // Press <Enter> in the omnibox, causing the preview to be committed.
  TabContents* preview_tab = preview()->tab_contents();
  ASSERT_TRUE(PressEnter());

  // The preview contents should now be the active tab contents.
  EXPECT_FALSE(preview());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_EQ(preview_tab, browser()->GetSelectedTabContents());
}

// Tests the SILENT experiment of the field trial.
class InstantFieldTrialSilentTest : public InstantTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kInstantFieldTrial,
                                    switches::kInstantFieldTrialSilent);
  }
};

// Tests that instant is active, even without calling EnableInstant().
IN_PROC_BROWSER_TEST_F(InstantFieldTrialSilentTest, MAYBE(ExperimentEnabled)) {
  // Check that instant is enabled, despite not setting the preference.
  Profile* profile = browser()->profile();
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kInstantEnabled));
  EXPECT_TRUE(InstantController::IsEnabled(profile));

  ASSERT_TRUE(test_server()->Start());
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  // Type into the omnibox, but don't press <Enter> yet.
  omnibox()->SetUserText(ASCIIToUTF16("def"));
  ASSERT_TRUE(WaitForMessageToBeProcessedByRenderer());

  // Check that instant is active, but the preview is not showing.
  EXPECT_TRUE(preview());
  EXPECT_FALSE(loader()->ready());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());

  // There are no suggestions, as the loader hasn't seen the query yet.
  EXPECT_EQ("", GetSuggestion());
  EXPECT_EQ("def", UTF16ToUTF8(omnibox()->GetText()));

  // Press <Enter> in the omnibox, causing the preview to be committed.
  TabContents* preview_tab = preview()->tab_contents();
  ASSERT_TRUE(PressEnter());

  // The preview contents should now be the active tab contents.
  EXPECT_FALSE(preview());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_EQ(preview_tab, browser()->GetSelectedTabContents());
}

// Tests the SUGGEST experiment of the field trial.
class InstantFieldTrialSuggestTest : public InstantTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kInstantFieldTrial,
                                    switches::kInstantFieldTrialSuggest);
  }
};

// Tests that instant is active, even without calling EnableInstant().
IN_PROC_BROWSER_TEST_F(InstantFieldTrialSuggestTest, MAYBE(ExperimentEnabled)) {
  // Check that instant is enabled, despite not setting the preference.
  Profile* profile = browser()->profile();
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kInstantEnabled));
  EXPECT_TRUE(InstantController::IsEnabled(profile));

  ASSERT_TRUE(test_server()->Start());
  SetupInstantProvider("instant.html");
  DetermineInstantSupport();

  // Type into the omnibox, but don't press <Enter> yet.
  omnibox()->SetUserText(ASCIIToUTF16("def"));
  ASSERT_TRUE(WaitForMessageToBeProcessedByRenderer());

  // Check that instant is active, but the preview is not showing.
  EXPECT_TRUE(preview());
  EXPECT_TRUE(loader()->ready());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());

  // Check that the suggested text has actually been set in the omnibox.
  EXPECT_EQ("defghi", GetSuggestion());
  EXPECT_EQ("defghi", UTF16ToUTF8(omnibox()->GetText()));

  // Press <Enter> in the omnibox, causing the preview to be committed.
  TabContents* preview_tab = preview()->tab_contents();
  ASSERT_TRUE(PressEnter());

  // The preview contents should now be the active tab contents.
  EXPECT_FALSE(preview());
  EXPECT_FALSE(instant()->is_displayable());
  EXPECT_FALSE(instant()->IsCurrent());
  EXPECT_EQ(preview_tab, browser()->GetSelectedTabContents());
}
