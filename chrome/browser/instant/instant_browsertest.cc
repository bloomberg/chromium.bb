// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"


class InstantTest : public InProcessBrowserTest {
 public:
  InstantTest()
      : location_bar_(NULL),
        preview_(NULL) {
    EnableDOMAutomation();
  }

  void SetupInstantProvider(const std::string& page) {
    TemplateURLModel* model = browser()->profile()->GetTemplateURLModel();
    ASSERT_TRUE(model);

    if (!model->loaded()) {
      model->Load();
      ui_test_utils::WaitForNotification(
          NotificationType::TEMPLATE_URL_MODEL_LOADED);
    }

    ASSERT_TRUE(model->loaded());

    // TemplateURLModel takes ownership of this.
    TemplateURL* template_url = new TemplateURL();

    std::string url = StringPrintf(
        "http://%s:%d/files/instant/%s?q={searchTerms}",
        test_server()->host_port_pair().host().c_str(),
        test_server()->host_port_pair().port(),
        page.c_str());
    template_url->SetURL(url, 0, 0);
    template_url->SetInstantURL(url, 0, 0);
    template_url->set_keyword(UTF8ToWide("foo"));
    template_url->set_short_name(UTF8ToWide("foo"));

    model->Add(template_url);
    model->SetDefaultSearchProvider(template_url);
  }

  // Type a character to get instant to trigger.
  void SetupLocationBar() {
    location_bar_ = browser()->window()->GetLocationBar();
    ASSERT_TRUE(location_bar_);
    location_bar_->location_entry()->SetUserText(L"a");
  }

  // Wait for instant to load and ensure it is in the state we expect.
  void SetupPreview() {
    preview_ = browser()->instant()->GetPreviewContents();
    ASSERT_TRUE(preview_);
    ui_test_utils::WaitForNavigation(&preview_->controller());

    // Verify the initial setup of the search box.
    ASSERT_TRUE(browser()->instant());
    EXPECT_TRUE(browser()->instant()->IsShowingInstant());
    EXPECT_FALSE(browser()->instant()->is_active());

    // When the page loads, the initial searchBox values are set and no events
    // have been called.
    EXPECT_NO_FATAL_FAILURE(CheckBoolValueFromJavascript(
        true, "window.chrome.sv", preview_));
    EXPECT_NO_FATAL_FAILURE(CheckIntValueFromJavascript(
        0, "window.onsubmitcalls", preview_));
    EXPECT_NO_FATAL_FAILURE(CheckIntValueFromJavascript(
        0, "window.oncancelcalls", preview_));
    EXPECT_NO_FATAL_FAILURE(CheckIntValueFromJavascript(
        0, "window.onchangecalls", preview_));
    EXPECT_NO_FATAL_FAILURE(CheckIntValueFromJavascript(
        0, "window.onresizecalls", preview_));
    EXPECT_NO_FATAL_FAILURE(CheckStringValueFromJavascript(
        "a", "window.chrome.searchBox.value", preview_));
    EXPECT_NO_FATAL_FAILURE(CheckBoolValueFromJavascript(
        false, "window.chrome.searchBox.verbatim", preview_));
  }

  void SetLocationBarText(const std::wstring& text) {
    ASSERT_TRUE(location_bar_);
    location_bar_->location_entry()->SetUserText(text);
    ui_test_utils::WaitForNotification(
        NotificationType::INSTANT_CONTROLLER_SHOWN);
  }

  void SendKey(app::KeyboardCode key) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), key, false, false, false, false));
  }

  void CheckStringValueFromJavascript(
      const std::string& expected,
      const std::string& function,
      TabContents* tab_contents) {
    std::string script = StringPrintf(
        "window.domAutomationController.send(%s)", function.c_str());
    std::string result;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
        tab_contents->render_view_host(),
        std::wstring(), UTF8ToWide(script), &result));
    EXPECT_EQ(expected, result);
  }

  void CheckBoolValueFromJavascript(
      bool expected,
      const std::string& function,
      TabContents* tab_contents) {
    std::string script = StringPrintf(
        "window.domAutomationController.send(%s)", function.c_str());
    bool result;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        tab_contents->render_view_host(),
        std::wstring(), UTF8ToWide(script), &result));
    EXPECT_EQ(expected, result);
  }

  void CheckIntValueFromJavascript(
      int expected,
      const std::string& function,
      TabContents* tab_contents) {
    std::string script = StringPrintf(
        "window.domAutomationController.send(%s)", function.c_str());
    int result;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
        tab_contents->render_view_host(),
        std::wstring(), UTF8ToWide(script), &result));
    EXPECT_EQ(expected, result);
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePredictiveInstant);
  }

  LocationBar* location_bar_;
  TabContents* preview_;
};

// TODO(tonyg): Add the following tests:
// 1. Test that setSuggestions() works.
// 2. Test that the search box API is not populated for pages other than the
//    default search provider.
// 3. Test resize events.

#if defined(OS_WIN)
#define MAYBE_OnChangeEvent OnChangeEvent
#else
#define MAYBE_OnChangeEvent DISABLED_OnChangeEvent
#endif
// Verify that the onchange event is dispatched upon typing in the box.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnChangeEvent) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  ASSERT_NO_FATAL_FAILURE(SetLocationBarText(L"abc"));

  // Check that the value is reflected and onchange is called.
  EXPECT_NO_FATAL_FAILURE(CheckStringValueFromJavascript(
      "abc", "window.chrome.searchBox.value", preview_));
  EXPECT_NO_FATAL_FAILURE(CheckBoolValueFromJavascript(
      false, "window.chrome.searchBox.verbatim", preview_));
  EXPECT_NO_FATAL_FAILURE(CheckIntValueFromJavascript(
      1, "window.onchangecalls", preview_));
}

#if defined(OS_WIN)
#define MAYBE_OnSubmitEvent OnSubmitEvent
#else
#define MAYBE_OnSubmitEvent DISABLED_OnSubmitEvent
#endif
// Verify that the onsubmit event is dispatched upon pressing enter.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnSubmitEvent) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  ASSERT_NO_FATAL_FAILURE(SetLocationBarText(L"abc"));
  ASSERT_NO_FATAL_FAILURE(SendKey(app::VKEY_RETURN));

  // Check that the preview contents have been committed.
  ASSERT_FALSE(browser()->instant()->GetPreviewContents());
  TabContents* contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);

  // Check that the value is reflected and onsubmit is called.
  EXPECT_NO_FATAL_FAILURE(CheckBoolValueFromJavascript(
      true, "window.chrome.sv", contents));
  EXPECT_NO_FATAL_FAILURE(CheckStringValueFromJavascript(
      "abc", "window.chrome.searchBox.value", contents));
  EXPECT_NO_FATAL_FAILURE(CheckBoolValueFromJavascript(
      true, "window.chrome.searchBox.verbatim", contents));
  EXPECT_NO_FATAL_FAILURE(CheckIntValueFromJavascript(
      1, "window.onsubmitcalls", contents));
}

#if defined(OS_WIN)
#define MAYBE_OnCancelEvent OnCancelEvent
#else
#define MAYBE_OnCancelEvent DISABLED_OnCancelEvent
#endif
// Verify that the oncancel event is dispatched upon losing focus.
IN_PROC_BROWSER_TEST_F(InstantTest, MAYBE_OnCancelEvent) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(SetupInstantProvider("search.html"));
  ASSERT_NO_FATAL_FAILURE(SetupLocationBar());
  ASSERT_NO_FATAL_FAILURE(SetupPreview());

  ASSERT_NO_FATAL_FAILURE(SetLocationBarText(L"abc"));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                     VIEW_ID_TAB_CONTAINER));

  // Check that the preview contents have been committed.
  ASSERT_FALSE(browser()->instant()->GetPreviewContents());
  TabContents* contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);

  // Check that the value is reflected and oncancel is called.
  EXPECT_NO_FATAL_FAILURE(CheckBoolValueFromJavascript(
      true, "window.chrome.sv", contents));
  EXPECT_NO_FATAL_FAILURE(CheckStringValueFromJavascript(
      "abc", "window.chrome.searchBox.value", contents));
  EXPECT_NO_FATAL_FAILURE(CheckBoolValueFromJavascript(
      false, "window.chrome.searchBox.verbatim", contents));
  EXPECT_NO_FATAL_FAILURE(CheckIntValueFromJavascript(
      1, "window.oncancelcalls", contents));
}
