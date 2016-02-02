// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace language_options_ui_test {

namespace {

// This class will test the language options settings.
// This test is part of the interactive_ui_tests isntead of browser_tests
// because it is necessary to emulate pushing a button in order to properly
// test accessibility.
class LanguageOptionsWebUITest : public InProcessBrowserTest {
 public:
  LanguageOptionsWebUITest() {}

  // This method will navigate to the language settings page and show
  // a subset of languages from the list of available languages.
  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    auto setting_name = prefs::kLanguagePreferredLanguages;
#else
    auto setting_name = prefs::kAcceptLanguages;
#endif

    const GURL url = chrome::GetSettingsUrl(chrome::kLanguageOptionsSubPage);
    ui_test_utils::NavigateToURL(browser(), url);
    browser()->profile()->GetPrefs()->SetString(setting_name, "en-US,es,fr");
  }

 protected:
  // Will get the id of the element in the UI that has focus.
  std::string GetActiveElementId() {
    std::string get_element_id_script =
        "domAutomationController.send(document.activeElement.id);";
    std::string element_id;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
          GetActiveFrame(),
          get_element_id_script,
          &element_id));
    return element_id;
  }

  content::RenderFrameHost* GetActiveFrame() {
    return GetActiveWebContents()->GetFocusedFrame();
  }

  content::RenderViewHost* GetRenderViewHost() {
    return GetActiveWebContents()->GetRenderViewHost();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Press and release a key in the browser. This will wait for the element on
  // the page to change.
  bool PressKey(ui::KeyboardCode key_code) {
    return ui_test_utils::SendKeyPressAndWait(
        browser(),
        key_code,
        false,
        false,
        false,
        false,
        content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
        content::Source<content::RenderViewHost>(GetRenderViewHost()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageOptionsWebUITest);
};

}  // namespace

// This test will verify that the appropriate languages are available.
// This test will also fail if the language page is not loaded because a random
// page will not have the language list.
// Test assumes that the default active element is the list of languages.
IN_PROC_BROWSER_TEST_F(LanguageOptionsWebUITest, TestAvailableLanguages) {
  // Verify that the language list is focused by default.
  std::string original_id = GetActiveElementId();
  EXPECT_EQ("language-options-list", original_id);

  content::RenderFrameHost* active_frame = GetActiveFrame();

  std::string count_deletable_items_script =
    "domAutomationController.send("
    "    document.activeElement.querySelectorAll('.deletable-item').length);";

  // Count the number of languages in the list.
  int language_count = 0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      active_frame,
      count_deletable_items_script,
      &language_count));
  EXPECT_EQ(3, language_count);

  std::string get_children_of_current_element_script =
      "domAutomationController.send(document.activeElement.textContent);";

  // Verify that the correct languages are added to the list.
  std::string languages;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      active_frame,
      get_children_of_current_element_script,
      &languages));
  EXPECT_EQ("English (United States)SpanishFrench", languages);
}

// This test will validate that the language webui is accessible through
// the keyboard.
// This test must be updated if the tab order of the elements on this page
// is changed.

// Crashes on Win 7. http://crbug.com/500609
#if defined(OS_WIN)
#define MAYBE_TestListTabAccessibility DISABLED_TestListTabAccessibility
#else
#define MAYBE_TestListTabAccessibility TestListTabAccessibility
#endif

IN_PROC_BROWSER_TEST_F(LanguageOptionsWebUITest,
    MAYBE_TestListTabAccessibility) {
  // Verify that the language list is focused by default.
  std::string original_id = GetActiveElementId();
  EXPECT_EQ("language-options-list", original_id);

  // Press tab to select the next element.
  ASSERT_TRUE(PressKey(ui::VKEY_TAB));

  // Make sure that the element is now the button that is next in the tab order.
  // Checking that the list is no longer selected is not sufficient to validate
  // this use case because this test should fail if an item inside the list is
  // selected.
  std::string new_id = GetActiveElementId();
  EXPECT_EQ("language-options-add-button", new_id);
}

}  // namespace language_options_ui_test

