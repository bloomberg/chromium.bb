// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
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
#include "chrome/test/test_navigation_observer.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"

#define EXPECT_STR_EQ(ascii, utf16) \
  EXPECT_EQ(ASCIIToWide(ascii), UTF16ToWide(utf16))

#if defined(OS_LINUX)
// These tests are disabled on linux because of http://crbug.com/80118 .
#define MAYBE_OnChangeEvent DISABLED_OnChangeEvent
#define MAYBE_SetSuggestionsArrayOfStrings DISABLED_SetSuggestionsArrayOfStrings
#define MAYBE_SetSuggestionsEmptyArray DISABLED_SetSuggestionsEmptyArray
#define MAYBE_SetSuggestionsValidJson DISABLED_SetSuggestionsValidJson
#define MAYBE_SetSuggestionsInvalidSuggestions \
    DISABLED_SetSuggestionsInvalidSuggestions
#define MAYBE_SetSuggestionsEmptyJson DISABLED_SetSuggestionsEmptyJson
#define MAYBE_SetSuggestionsEmptySuggestions \
    DISABLED_SetSuggestionsEmptySuggestions
#define MAYBE_SetSuggestionsEmptySuggestion \
    DISABLED_SetSuggestionsEmptySuggestion
#define MAYBE_ShowPreviewNonSearch DISABLED_ShowPreviewNonSearch
#define MAYBE_NonSearchToSearch DISABLED_NonSearchToSearch
#define MAYBE_SearchToNonSearch DISABLED_SearchToNonSearch
#define MAYBE_ValidHeight DISABLED_ValidHeight
#define MAYBE_OnSubmitEvent DISABLED_OnSubmitEvent
#define MAYBE_OnCancelEvent DISABLED_OnCancelEvent
#define MAYBE_InstantCompleteNever DISABLED_InstantCompleteNever
#define MAYBE_InstantCompleteDelayed DISABLED_InstantCompleteDelayed
#define MAYBE_DontCrashOnBlockedJS DISABLED_DontCrashOnBlockedJS
#define MAYBE_DontPersistSearchbox DISABLED_DontPersistSearchbox
#define MAYBE_PreloadsInstant DISABLED_PreloadsInstant
#define MAYBE_PageVisibilityTest DISABLED_PageVisibilityTest
#define MAYBE_ExperimentEnabled DISABLED_ExperimentEnabled
#define MAYBE_IntranetPathLooksLikeSearch DISABLED_IntranetPathLooksLikeSearch
#else
#define MAYBE_OnChangeEvent OnChangeEvent
#define MAYBE_SetSuggestionsArrayOfStrings SetSuggestionsArrayOfStrings
#define MAYBE_SetSuggestionsEmptyArray SetSuggestionsEmptyArray
#define MAYBE_SetSuggestionsValidJson SetSuggestionsValidJson
#define MAYBE_SetSuggestionsInvalidSuggestions SetSuggestionsInvalidSuggestions
#define MAYBE_SetSuggestionsEmptyJson SetSuggestionsEmptyJson
#define MAYBE_SetSuggestionsEmptySuggestions SetSuggestionsEmptySuggestions
#define MAYBE_SetSuggestionsEmptySuggestion SetSuggestionsEmptySuggestion
#define MAYBE_ShowPreviewNonSearch ShowPreviewNonSearch
#define MAYBE_NonSearchToSearch NonSearchToSearch
#define MAYBE_SearchToNonSearch SearchToNonSearch
#define MAYBE_ValidHeight ValidHeight
#define MAYBE_OnSubmitEvent OnSubmitEvent
#define MAYBE_OnCancelEvent OnCancelEvent
#define MAYBE_InstantCompleteNever InstantCompleteNever
#define MAYBE_InstantCompleteDelayed InstantCompleteDelayed
#define MAYBE_DontCrashOnBlockedJS DontCrashOnBlockedJS
#define MAYBE_DontPersistSearchbox DontPersistSearchbox
#define MAYBE_PreloadsInstant PreloadsInstant
#define MAYBE_PageVisibilityTest PageVisibilityTest
#define MAYBE_ExperimentEnabled ExperimentEnabled
#define MAYBE_IntranetPathLooksLikeSearch IntranetPathLooksLikeSearch
#endif

#if defined(OS_MACOSX) || defined(OS_LINUX)
// Showing as flaky on Mac and Linux.
// http://crbug.com/70860
#define MAYBE_SearchServerDoesntSupportInstant \
    DISABLED_SearchServerDoesntSupportInstant
#define MAYBE_NonSearchToSearchDoesntSupportInstant \
    DISABLED_NonSearchToSearchDoesntSupportInstant
#else
#define MAYBE_SearchServerDoesntSupportInstant \
    SearchServerDoesntSupportInstant
#define MAYBE_NonSearchToSearchDoesntSupportInstant \
    NonSearchToSearchDoesntSupportInstant
#endif


class InstantTest : public InProcessBrowserTest {
 public:
  InstantTest()
      : location_bar_(NULL),
        preview_(NULL),
        template_url_id_(0) {
    set_show_window(true);
    EnableDOMAutomation();
  }

  void EnableInstant() {
    InstantController::Enable(browser()->profile());
  }

  void SetupInstantProvider(const std::string& page) {
    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);

    ui_test_utils::WindowedNotificationObserver service_loaded_observer(
        chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
        content::NotificationService::AllSources());
    if (!model->loaded()) {
      model->Load();
      service_loaded_observer.Wait();
    }
    ASSERT_TRUE(model->loaded());

    // TemplateURLService takes ownership of this.
    TemplateURL* template_url = new TemplateURL();

    std::string url = StringPrintf(
        "http://%s:%d/files/instant/%s?q={searchTerms}",
        test_server()->host_port_pair().host().c_str(),
        test_server()->host_port_pair().port(),
        page.c_str());
    template_url->SetURL(url, 0, 0);
    template_url->SetInstantURL(url, 0, 0);
    template_url->set_keyword(ASCIIToUTF16("foo"));
    template_url->set_short_name(ASCIIToUTF16("foo"));

    model->Add(template_url);
    model->SetDefaultSearchProvider(template_url);
    template_url_id_ = template_url->id();
  }

  void FindLocationBar() {
    if (location_bar_)
      return;
    location_bar_ = browser()->window()->GetLocationBar();
    ASSERT_TRUE(location_bar_);
  }

  // Type a character to get instant to trigger.
  void SetupLocationBar() {
    FindLocationBar();
    // "a" triggers the "about:" provider.  "b" begins the "bing.com" keyword.
    // "c" might someday trigger a "chrome:" provider.
    location_bar_->location_entry()->SetUserText(ASCIIToUTF16("d"));
  }

  // Waits for preview to be shown.
  void WaitForPreviewToNavigate() {
    InstantController* instant = browser()->instant();
    ASSERT_TRUE(instant);
    TabContentsWrapper* tab = instant->GetPreviewContents();
    ASSERT_TRUE(tab);
    preview_ = tab->tab_contents();
    ASSERT_TRUE(preview_);
    // TODO(gbillock): This should really be moved into calling code. It is
    // still race-prone here.
    TestNavigationObserver observer(content::Source<NavigationController>(
        &preview_->controller()), NULL, 1);
    observer.WaitForObservation();
  }

  // Wait for instant to load and ensure it is in the state we expect.
  void SetupPreview() {
    // Wait for the preview to navigate.
    WaitForPreviewToNavigate();

    ASSERT_FALSE(browser()->instant()->is_displayable());
    ASSERT_TRUE(HasPreview());

    // When the page loads, the initial searchBox values are set and only a
    // resize will have been sent.
    ASSERT_EQ("true 0 0 0 true d false d false 1 1",
              GetSearchStateAsString(preview_, false));
  }

  void SetLocationBarText(const std::string& text) {
    ASSERT_NO_FATAL_FAILURE(FindLocationBar());
    ui_test_utils::WindowedNotificationObserver controller_shown_observer(
        chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN,
        content::NotificationService::AllSources());
    location_bar_->location_entry()->SetUserText(UTF8ToUTF16(text));
    controller_shown_observer.Wait();
  }

  const string16& GetSuggestion() const {
    return browser()->instant()->loader_->complete_suggested_text_;
  }

  GURL GetCurrentURL() {
    return browser()->instant()->loader_.get() ?
        browser()->instant()->loader_.get()->url() : GURL();
  }

  bool LoaderIsReady() const {
    return browser()->instant()->loader_->ready();
  }

  const string16& GetUserText() const {
    return browser()->instant()->loader_->user_text();
  }

  void SendKey(ui::KeyboardCode key) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), key, false, false, false, false));
  }

  void SetSuggestionsJavascriptArgument(TabContents* tab_contents,
                                        const std::string& argument) {
    std::string script = StringPrintf(
        "window.setSuggestionsArgument = %s;", argument.c_str());
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
        tab_contents->render_view_host(),
        std::wstring(),
        UTF8ToWide(script)));
  }

  bool GetStringFromJavascript(TabContents* tab_contents,
                               const std::string& function,
                               std::string* result) {
    std::string script = StringPrintf(
        "window.domAutomationController.send(%s)", function.c_str());
    return ui_test_utils::ExecuteJavaScriptAndExtractString(
        tab_contents->render_view_host(),
        std::wstring(), UTF8ToWide(script), result);
  }

  bool GetIntFromJavascript(TabContents* tab_contents,
                            const std::string& function,
                            int* result) {
    std::string script = StringPrintf(
        "window.domAutomationController.send(%s)", function.c_str());
    return ui_test_utils::ExecuteJavaScriptAndExtractInt(
        tab_contents->render_view_host(),
        std::wstring(), UTF8ToWide(script), result);
  }

  bool GetBoolFromJavascript(TabContents* tab_contents,
                             const std::string& function,
                             bool* result) {
    std::string script = StringPrintf(
        "window.domAutomationController.send(%s)", function.c_str());
    return ui_test_utils::ExecuteJavaScriptAndExtractBool(
        tab_contents->render_view_host(),
        std::wstring(), UTF8ToWide(script), result);
  }

  bool HasPreview() {
    return browser()->instant()->GetPreviewContents() != NULL;
  }

  bool IsVisible(TabContents* tab_contents) {
    std::string visibility;
    if (!GetStringFromJavascript(tab_contents, "document.webkitVisibilityState",
            &visibility))
      return false;
    return visibility == "visible";
  }

  // Returns the state of the search box as a string. This consists of the
  // following:
  // window.chrome.sv
  // window.onsubmitcalls
  // window.oncancelcalls
  // window.onchangecalls
  // 'true' if window.onresizecalls has been sent, otherwise false.
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
  std::string GetSearchStateAsString(TabContents* tab_contents,
                                     bool use_last) {
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

    if (!GetBoolFromJavascript(tab_contents, "window.chrome.sv", &sv) ||
        !GetIntFromJavascript(tab_contents, "window.onsubmitcalls",
                              &onsubmitcalls) ||
        !GetIntFromJavascript(tab_contents, "window.oncancelcalls",
                              &oncancelcalls) ||
        !GetIntFromJavascript(tab_contents, "window.onchangecalls",
                              &onchangecalls) ||
        !GetIntFromJavascript(tab_contents, "window.onresizecalls",
                              &onresizecalls) ||
        !GetStringFromJavascript(
            tab_contents, "window.beforeLoadSearchBox.value",
            &before_load_value) ||
        !GetBoolFromJavascript(
            tab_contents, "window.beforeLoadSearchBox.verbatim",
            &before_load_verbatim)) {
      return "fail";
    }

    if (use_last &&
        (!GetStringFromJavascript(tab_contents, "window.lastSearchBox.value",
                                  &value) ||
         !GetBoolFromJavascript(tab_contents, "window.lastSearchBox.verbatim",
                                &verbatim) ||
         !GetIntFromJavascript(tab_contents,
                               "window.lastSearchBox.selectionStart",
                               &selection_start) ||
         !GetIntFromJavascript(tab_contents,
                               "window.lastSearchBox.selectionEnd",
                               &selection_end))) {
      return "fail";
    }

    if (!use_last &&
        (!GetStringFromJavascript(tab_contents, "window.searchBox.value",
                                  &value) ||
         !GetBoolFromJavascript(tab_contents, "window.searchBox.verbatim",
                                &verbatim) ||
         !GetIntFromJavascript(tab_contents,
                               "window.searchBox.selectionStart",
                               &selection_start) ||
         !GetIntFromJavascript(tab_contents,
                               "window.searchBox.selectionEnd",
                               &selection_end))) {
      return "fail";
    }

    return StringPrintf("%s %d %d %d %s %s %s %s %s %d %d",
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

  void CheckStringValueFromJavascript(
      const std::string& expected,
      const std::string& function,
      TabContents* tab_contents) {
    std::string result;
    ASSERT_TRUE(GetStringFromJavascript(tab_contents, function, &result));
    ASSERT_EQ(expected, result);
  }

  void CheckBoolValueFromJavascript(
      bool expected,
      const std::string& function,
      TabContents* tab_contents) {
    bool result;
    ASSERT_TRUE(GetBoolFromJavascript(tab_contents, function, &result));
    ASSERT_EQ(expected, result);
  }

  void CheckIntValueFromJavascript(
      int expected,
      const std::string& function,
      TabContents* tab_contents) {
    int result;
    ASSERT_TRUE(GetIntFromJavascript(tab_contents, function, &result));
    ASSERT_EQ(expected, result);
  }

  // Sends a message to the renderer and waits for the response to come back to
  // the browser.
  void WaitForMessageToBeProcessedByRenderer(TabContentsWrapper* tab) {
    ASSERT_NO_FATAL_FAILURE(
        CheckBoolValueFromJavascript(true, "true", tab->tab_contents()));
  }

 protected:
  LocationBar* location_bar_;
  TabContents* preview_;

  // ID of the default search engine's template_url (in the installed model).
  TemplateURLID template_url_id_;
};

// TODO(tonyg): Add the following tests:
// - Test that the search box API is not populated for pages other than the
//   default search provider.
// - Test resize events.

// Verify that the onchange event is dispatched upon typing in the box.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnChangeEvent) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));

  ASSERT_EQ(ASCIIToUTF16("defghi"), location_bar_->location_entry()->GetText());

  // Make sure the url that will get committed when we press enter matches that
  // of the default search provider.
  const TemplateURL* default_turl =
      TemplateURLServiceFactory::GetForProfile(browser()->profile())->
      GetDefaultSearchProvider();
  ASSERT_TRUE(default_turl);
  ASSERT_TRUE(default_turl->url());
  EXPECT_EQ(default_turl->url()->ReplaceSearchTerms(
                *default_turl, ASCIIToUTF16("defghi"), 0, string16()),
            GetCurrentURL().spec());

  // Check that the value is reflected and onchange is called.
  EXPECT_EQ("true 0 0 1 true d false def false 3 3",
            GetSearchStateAsString(preview_, true));
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SetSuggestionsArrayOfStrings) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  SetSuggestionsJavascriptArgument(preview_, "['defgh', 'unused']");
  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  EXPECT_STR_EQ("defgh", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SetSuggestionsEmptyArray) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  SetSuggestionsJavascriptArgument(preview_, "[]");
  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  EXPECT_STR_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SetSuggestionsValidJson) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  SetSuggestionsJavascriptArgument(
      preview_,
      "{suggestions:[{value:'defghij'},{value:'unused'}]}");
  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  EXPECT_STR_EQ("defghij", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SetSuggestionsInvalidSuggestions) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  SetSuggestionsJavascriptArgument(
      preview_,
      "{suggestions:{value:'defghi'}}");
  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  EXPECT_STR_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SetSuggestionsEmptyJson) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  SetSuggestionsJavascriptArgument(preview_, "{}");
  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  EXPECT_STR_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SetSuggestionsEmptySuggestions) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  SetSuggestionsJavascriptArgument(preview_, "{suggestions:[]}");
  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  EXPECT_STR_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SetSuggestionsEmptySuggestion) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  SetSuggestionsJavascriptArgument(preview_, "{suggestions:[{}]}");
  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  EXPECT_STR_EQ("", GetSuggestion());
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_ShowPreviewNonSearch) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  GURL url(test_server()->GetURL("files/instant/empty.html"));
  ASSERT_NO_FATAL_FAILURE(FindLocationBar());
  location_bar_->location_entry()->SetUserText(UTF8ToUTF16(url.spec()));

  // The preview should not be active or showing.
  ASSERT_FALSE(HasPreview());
  ASSERT_FALSE(browser()->instant()->is_displayable());
  ASSERT_FALSE(browser()->instant()->IsCurrent());
  ASSERT_EQ(NULL, browser()->instant()->GetPreviewContents());
}

// Transition from non-search to search and make sure everything is shown
// correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_NonSearchToSearch) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  GURL url(test_server()->GetURL("files/instant/empty.html"));
  ASSERT_NO_FATAL_FAILURE(FindLocationBar());
  location_bar_->location_entry()->SetUserText(UTF8ToUTF16(url.spec()));
  // The preview not should be active and not showing.
  ASSERT_FALSE(HasPreview());
  ASSERT_FALSE(browser()->instant()->is_displayable());
  ASSERT_EQ(NULL, browser()->instant()->GetPreviewContents());

  // Now type in some search text.
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  location_bar_->location_entry()->SetUserText(ASCIIToUTF16("def"));

  // Wait for the preview to navigate.
  ASSERT_NO_FATAL_FAILURE(WaitForPreviewToNavigate());

  // The controller is still determining if the provider really supports
  // instant.
  TabContentsWrapper* current_tab = browser()->instant()->GetPreviewContents();
  ASSERT_TRUE(current_tab);

  // Instant should be active.
  EXPECT_TRUE(HasPreview());
  EXPECT_FALSE(browser()->instant()->is_displayable());

  // Because we're waiting on the page, instant isn't current.
  ASSERT_FALSE(browser()->instant()->IsCurrent());

  // Bounce a message to the renderer so that we know the instant has gotten a
  // response back from the renderer as to whether the page supports instant.
  ASSERT_NO_FATAL_FAILURE(WaitForMessageToBeProcessedByRenderer(current_tab));

  // Reset the user text so that the page is told the text changed. We should be
  // able to nuke this once 66104 is fixed.
  location_bar_->location_entry()->SetUserText(ASCIIToUTF16("defg"));

  // Wait for the renderer to process it.
  ASSERT_NO_FATAL_FAILURE(WaitForMessageToBeProcessedByRenderer(current_tab));

  // We should have gotten a response back from the renderer that resulted in
  // committing.
  ASSERT_TRUE(HasPreview());
  ASSERT_TRUE(browser()->instant()->is_displayable());
  TabContentsWrapper* new_tab = browser()->instant()->GetPreviewContents();
  ASSERT_TRUE(new_tab);
  RenderWidgetHostView* new_rwhv =
      new_tab->tab_contents()->GetRenderWidgetHostView();
  ASSERT_TRUE(new_rwhv);
  ASSERT_TRUE(new_rwhv->IsShowing());
}

// Transition from search to non-search and make sure instant isn't displayable.
// See bug http://crbug.com/100368 for details.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SearchToNonSearch) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  GURL url(test_server()->GetURL("files/instant/empty.html"));
  ASSERT_NO_FATAL_FAILURE(FindLocationBar());

  // Type in some search text.
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  location_bar_->location_entry()->SetUserText(ASCIIToUTF16("def"));

  // Load a non search url. Don't wait for the preview to navigate. It'll still
  // end up loading in the background.
  location_bar_->location_entry()->SetUserText(UTF8ToUTF16(url.spec()));

  // Wait for the preview to navigate.
  ASSERT_NO_FATAL_FAILURE(WaitForPreviewToNavigate());

  // Send onchange so that the page sends up suggestions.
  TabContentsWrapper* current_tab = browser()->instant()->GetPreviewContents();
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      current_tab->tab_contents()->render_view_host(), std::wstring(),
      L"window.chrome.searchBox.onchange();"));
  ASSERT_NO_FATAL_FAILURE(WaitForMessageToBeProcessedByRenderer(current_tab));

  // Instant should be active, but not displaying
  EXPECT_TRUE(HasPreview());
  EXPECT_FALSE(browser()->instant()->is_displayable());
}

// Makes sure that if the server doesn't support the instant API we don't show
// anything.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_SearchServerDoesntSupportInstant) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("empty.html"));
  ASSERT_NO_FATAL_FAILURE(FindLocationBar());

  ui_test_utils::WindowedNotificationObserver tab_closed_observer(
      content::NOTIFICATION_TAB_CLOSED,
      content::NotificationService::AllSources());

  location_bar_->location_entry()->SetUserText(ASCIIToUTF16("d"));
  ASSERT_TRUE(browser()->instant());
  // But because we're waiting to determine if the page really supports instant
  // we shouldn't be showing the preview.
  EXPECT_FALSE(browser()->instant()->is_displayable());
  // But instant should still be active.
  EXPECT_TRUE(HasPreview());

  // When the response comes back that the page doesn't support instant the tab
  // should be closed.
  tab_closed_observer.Wait();
  EXPECT_FALSE(browser()->instant()->is_displayable());
  EXPECT_FALSE(HasPreview());
  EXPECT_FALSE(browser()->instant()->IsCurrent());
}

// Verifies that Instant previews aren't shown for crash URLs.
IN_PROC_BROWSER_TEST_F(InstantTest, CrashUrlCancelsInstant) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("empty.html"));
  ASSERT_NO_FATAL_FAILURE(FindLocationBar());
  location_bar_->location_entry()->SetUserText(ASCIIToUTF16("chrome://crash"));
  ASSERT_TRUE(browser()->instant());
  EXPECT_FALSE(browser()->instant()->is_displayable());
}

// Verifies transitioning from loading a non-search string to a search string
// with the provider not supporting instant works (meaning we don't display
// anything).
IN_PROC_BROWSER_TEST_F(InstantTest,
                       MAYBE_NonSearchToSearchDoesntSupportInstant) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("empty.html"));
  GURL url(test_server()->GetURL("files/instant/empty.html"));
  ASSERT_NO_FATAL_FAILURE(FindLocationBar());
  location_bar_->location_entry()->SetUserText(UTF8ToUTF16(url.spec()));
  // The preview should not be showing or active.
  EXPECT_FALSE(browser()->instant()->is_displayable());
  EXPECT_FALSE(HasPreview());

  ui_test_utils::WindowedNotificationObserver tab_closed_observer(
      content::NOTIFICATION_TAB_CLOSED,
      content::NotificationService::AllSources());

  // Now type in some search text.
  location_bar_->location_entry()->SetUserText(ASCIIToUTF16("d"));

  // Instant should be active.
  ASSERT_TRUE(HasPreview());
  // Instant should not be current (it's still loading).
  EXPECT_FALSE(browser()->instant()->IsCurrent());

  // When the response comes back that the page doesn't support instant the tab
  // should be closed.
  tab_closed_observer.Wait();
  EXPECT_FALSE(browser()->instant()->is_displayable());
  EXPECT_FALSE(HasPreview());
  EXPECT_FALSE(browser()->instant()->IsCurrent());
  EXPECT_EQ(NULL, browser()->instant()->GetPreviewContents());
}

// Verifies the page was told a non-zero height.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_ValidHeight) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));

  int height;

  // searchBox height is not yet set during initial load.
  ASSERT_TRUE(GetIntFromJavascript(preview_,
      "window.beforeLoadSearchBox.height",
      &height));
  EXPECT_EQ(0, height);

  // searchBox height is available by the time the page loads.
  ASSERT_TRUE(GetIntFromJavascript(preview_,
      "window.chrome.searchBox.height",
      &height));
  EXPECT_GT(height, 0);
}

// Verify that the onsubmit event is dispatched upon pressing enter.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnSubmitEvent) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN));

  // Check that the preview contents have been committed.
  ASSERT_FALSE(browser()->instant()->GetPreviewContents());
  ASSERT_FALSE(HasPreview());
  TabContents* contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);

  // We should have two entries. One corresponding to the page the user was
  // first on, and one for the search page.
  ASSERT_EQ(2, contents->controller().entry_count());

  // Check that the value is reflected and onsubmit is called.
  EXPECT_EQ("true 1 0 1 true d false defghi true 3 3",
            GetSearchStateAsString(preview_, true));

  // Make sure the searchbox values were reset.
  EXPECT_EQ("true 1 0 1 true d false  false 0 0",
            GetSearchStateAsString(preview_, false));
}

// Verify that the oncancel event is dispatched upon losing focus.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnCancelEvent) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                     VIEW_ID_TAB_CONTAINER));

  // Check that the preview contents has been committed.
  ASSERT_FALSE(browser()->instant()->GetPreviewContents());
  ASSERT_FALSE(HasPreview());
  TabContents* contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);

  // Check that the value is reflected and oncancel is called.
  EXPECT_EQ("true 0 1 1 true d false def false 3 3",
            GetSearchStateAsString(preview_, true));

  // Make sure the searchbox values were reset.
  EXPECT_EQ("true 0 1 1 true d false  false 0 0",
            GetSearchStateAsString(preview_, false));
}

IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_InstantCompleteNever) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  SetSuggestionsJavascriptArgument(
      preview_,
      "{suggestions:[{value:'defghij'}],complete_behavior:'never'}");
  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  EXPECT_STR_EQ("defghij", GetSuggestion());
  AutocompleteEditModel* edit_model = location_bar_->location_entry()->model();
  EXPECT_EQ(INSTANT_COMPLETE_NEVER, edit_model->instant_complete_behavior());
  ASSERT_EQ(ASCIIToUTF16("def"), location_bar_->location_entry()->GetText());
}

// DISABLED http://crbug.com/80118
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_InstantCompleteDelayed) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  SetSuggestionsJavascriptArgument(
      preview_,
      "{suggestions:[{value:'defghij'}],complete_behavior:'delayed'}");
  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  EXPECT_STR_EQ("defghij", GetSuggestion());
  AutocompleteEditModel* edit_model = location_bar_->location_entry()->model();
  EXPECT_EQ(INSTANT_COMPLETE_DELAYED, edit_model->instant_complete_behavior());
  ASSERT_EQ(ASCIIToUTF16("def"), location_bar_->location_entry()->GetText());
}

// Make sure the renderer doesn't crash if javascript is blocked.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_DontCrashOnBlockedJS) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::WindowedNotificationObserver instant_support_observer(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  // Wait for notification that the instant API has been determined.
  instant_support_observer.Wait();
  // As long as we get the notification we're good (the renderer didn't crash).
}

// Makes sure window.chrome.searchbox doesn't persist when a new page is loaded.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_DontPersistSearchbox) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  ASSERT_NO_FATAL_FAILURE(SetLocationBarText("def"));
  ASSERT_NO_FATAL_FAILURE(SendKey(ui::VKEY_RETURN));

  // Check that the preview contents have been committed.
  ASSERT_FALSE(browser()->instant()->GetPreviewContents());
  ASSERT_FALSE(HasPreview());

  TabContents* contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);

  // Navigate to a new URL. This should reset the searchbox values.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL(test_server()->GetURL("files/instant/empty.html")));
  bool result;
  ASSERT_TRUE(GetBoolFromJavascript(
                  browser()->GetSelectedTabContents(),
                  "window.chrome.searchBox.value.length == 0",
                  &result));
  EXPECT_TRUE(result);
}

// Tests that instant search is preloaded whenever the omnibox gets focus.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_PreloadsInstant) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kPreloadInstantSearch);

  // The omnibox gets focus before the test begins. At that time, there's no
  // instant controller (which is only created after EnableInstant()), so no
  // preloading happens. Unfocus the omnibox with ClickOnView(), so that when
  // we focus it again, the controller will preload instant search.
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("search.html");
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Verify that there is no previews contents.
  EXPECT_EQ(NULL, browser()->instant()->GetPreviewContents());

  ui_test_utils::WindowedNotificationObserver instant_support_observer(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Focusing the omnibox should cause instant to be preloaded.
  FindLocationBar();
  location_bar_->FocusLocation(false);
  TabContentsWrapper* tab_contents = browser()->instant()->GetPreviewContents();
  EXPECT_TRUE(tab_contents);

  instant_support_observer.Wait();

  // Instant should have a preview, but not display it.
  EXPECT_TRUE(HasPreview());
  EXPECT_FALSE(browser()->instant()->is_displayable());
  EXPECT_FALSE(IsVisible(tab_contents->tab_contents()));

  // Adding a new tab shouldn't delete (or recreate) the TabContentsWrapper.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(tab_contents, browser()->instant()->GetPreviewContents());

  // Doing a search should still use the same loader for the preview.
  SetLocationBarText("def");
  EXPECT_EQ(tab_contents, browser()->instant()->GetPreviewContents());

  // Verify that the preview is in fact showing instant search.
  EXPECT_TRUE(HasPreview());
  EXPECT_TRUE(browser()->instant()->is_displayable());
  EXPECT_TRUE(browser()->instant()->IsCurrent());
}

// Tests that instant doesn't fire for intranet paths that look like searches.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_IntranetPathLooksLikeSearch) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  SetupInstantProvider("search.html");

  // Unfocus the omnibox. This should delete any existing preview contents.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  EXPECT_TRUE(browser()->instant());
  EXPECT_FALSE(HasPreview());

  // Navigate to a URL that looks like a search (when the scheme is stripped).
  // It's okay if the host is bogus or the navigation fails, since we only care
  // that instant doesn't act on it.
  ui_test_utils::NavigateToURL(browser(), GURL("http://baby/beluga"));

  // Instant should not have tried to load a preview for this "search".
  EXPECT_FALSE(HasPreview());
}

// Tests that the instant search page's visibility is set correctly.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_PageVisibilityTest) {
  ASSERT_TRUE(test_server()->Start());
  EnableInstant();
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));

  // Initially navigate to the empty page which should be visible.
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(""));
  TabContents* initial_contents = browser()->GetSelectedTabContents();
  EXPECT_TRUE(IsVisible(initial_contents));

  // Type something for instant to trigger and wait for preview to navigate.
  ASSERT_NO_FATAL_FAILURE(FindLocationBar());
  location_bar_->FocusLocation(false);
  SetupLocationBar();
  SetupPreview();
  SetLocationBarText("def");
  TabContents* preview_contents =
      browser()->instant()->GetPreviewContents()->tab_contents();
  EXPECT_TRUE(IsVisible(preview_contents));
  EXPECT_FALSE(IsVisible(initial_contents));

  // Delete the user text we should show the previous page.
  location_bar_->location_entry()->SetUserText(UTF8ToUTF16(""));
  EXPECT_FALSE(IsVisible(preview_contents));
  EXPECT_TRUE(IsVisible(initial_contents));

  // Set the user text back and we should see the preview again.
  location_bar_->location_entry()->SetUserText(UTF8ToUTF16("def"));
  EXPECT_TRUE(IsVisible(preview_contents));
  EXPECT_FALSE(IsVisible(initial_contents));

  // Commit the preview.
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(preview_contents, browser()->GetSelectedTabContents());
  EXPECT_TRUE(IsVisible(preview_contents));
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
IN_PROC_BROWSER_TEST_F(InstantFieldTrialInstantTest, MAYBE_ExperimentEnabled) {
  // Check that instant is enabled, despite not setting the preference.
  Profile* profile = browser()->profile();
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kInstantEnabled));
  EXPECT_TRUE(InstantController::IsEnabled(profile));

  ASSERT_TRUE(test_server()->Start());
  SetupInstantProvider("search.html");
  SetupLocationBar();
  SetupPreview();
  SetLocationBarText("def");

  // Check that instant is active and showing a preview.
  EXPECT_TRUE(HasPreview());
  EXPECT_TRUE(browser()->instant()->is_displayable());
  EXPECT_TRUE(browser()->instant()->IsCurrent());
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
IN_PROC_BROWSER_TEST_F(InstantFieldTrialHiddenTest, MAYBE_ExperimentEnabled) {
  // Check that instant is enabled, despite not setting the preference.
  Profile* profile = browser()->profile();
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kInstantEnabled));
  EXPECT_TRUE(InstantController::IsEnabled(profile));

  ASSERT_TRUE(test_server()->Start());
  SetupInstantProvider("search.html");
  ui_test_utils::WindowedNotificationObserver instant_support_observer(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  SetupLocationBar();
  WaitForPreviewToNavigate();
  instant_support_observer.Wait();

  // Type into the omnibox, but don't press <Enter> yet.
  location_bar_->location_entry()->SetUserText(UTF8ToUTF16("def"));

  // Check that instant is active, but the preview is not showing.
  EXPECT_TRUE(HasPreview());
  EXPECT_FALSE(browser()->instant()->is_displayable());
  EXPECT_FALSE(browser()->instant()->IsCurrent());

  TabContentsWrapper* tab_contents = browser()->instant()->GetPreviewContents();
  EXPECT_TRUE(tab_contents);

  // Wait for the underlying loader to finish processing.
  WaitForMessageToBeProcessedByRenderer(tab_contents);

  EXPECT_STR_EQ("def", location_bar_->location_entry()->GetText());
  EXPECT_STR_EQ("defghi", GetUserText());
  EXPECT_TRUE(LoaderIsReady());

  // Press <Enter> in the omnibox, causing the preview to be committed.
  SendKey(ui::VKEY_RETURN);

  // The preview contents should now be the active tab contents.
  EXPECT_FALSE(browser()->instant()->GetPreviewContents());
  EXPECT_FALSE(HasPreview());
  EXPECT_FALSE(browser()->instant()->is_displayable());
  EXPECT_FALSE(browser()->instant()->IsCurrent());
  EXPECT_EQ(tab_contents, browser()->GetSelectedTabContentsWrapper());
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
IN_PROC_BROWSER_TEST_F(InstantFieldTrialSilentTest, MAYBE_ExperimentEnabled) {
  // Check that instant is enabled, despite not setting the preference.
  Profile* profile = browser()->profile();
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(prefs::kInstantEnabled));
  EXPECT_TRUE(InstantController::IsEnabled(profile));

  ASSERT_TRUE(test_server()->Start());
  SetupInstantProvider("search.html");
  ui_test_utils::WindowedNotificationObserver instant_support_observer(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  SetupLocationBar();
  WaitForPreviewToNavigate();
  instant_support_observer.Wait();

  // Type into the omnibox, but don't press <Enter> yet.
  location_bar_->location_entry()->SetUserText(UTF8ToUTF16("def"));

  // Check that instant is active, but the preview is not showing.
  EXPECT_TRUE(HasPreview());
  EXPECT_FALSE(browser()->instant()->is_displayable());
  EXPECT_FALSE(browser()->instant()->IsCurrent());

  TabContentsWrapper* tab_contents = browser()->instant()->GetPreviewContents();
  EXPECT_TRUE(tab_contents);

  // Wait for the underlying loader to finish processing.
  WaitForMessageToBeProcessedByRenderer(tab_contents);

  EXPECT_STR_EQ("def", location_bar_->location_entry()->GetText());
  EXPECT_STR_EQ("", GetUserText());
  EXPECT_FALSE(LoaderIsReady());

  // Press <Enter> in the omnibox, causing the preview to be committed.
  SendKey(ui::VKEY_RETURN);

  // The preview contents should now be the active tab contents.
  EXPECT_FALSE(browser()->instant()->GetPreviewContents());
  EXPECT_FALSE(HasPreview());
  EXPECT_FALSE(browser()->instant()->is_displayable());
  EXPECT_FALSE(browser()->instant()->IsCurrent());
  EXPECT_EQ(tab_contents, browser()->GetSelectedTabContentsWrapper());
}

// Tests the SearchToNonSearch scenario under the SILENT field trial.
IN_PROC_BROWSER_TEST_F(InstantFieldTrialSilentTest, MAYBE_SearchToNonSearch) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::WindowedNotificationObserver instant_support_observer(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Type in some search text.
  SetupInstantProvider("search.html");
  SetupLocationBar();

  // Load a non-search URL; don't wait for the preview to navigate.
  GURL url(test_server()->GetURL("files/instant/empty.html"));
  location_bar_->location_entry()->SetUserText(UTF8ToUTF16(url.spec()));

  // Wait for the preview to navigate.
  WaitForPreviewToNavigate();
  instant_support_observer.Wait();

  // Instant should be active, but not displayable or committable.
  EXPECT_TRUE(HasPreview());
  EXPECT_FALSE(browser()->instant()->is_displayable());
  EXPECT_FALSE(browser()->instant()->PrepareForCommit());
}
