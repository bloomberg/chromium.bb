// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test_utils.h"

namespace {

// This class tests the language dictionary settings.
// This test is part of the interactive_ui_tests instead of browser_tests
// because it is necessary to emulate pushing the tab key.
class LanguageDictionaryWebUITest : public InProcessBrowserTest {
 public:
  LanguageDictionaryWebUITest() {}

  // Navigate to the editDictionary page.
  void SetUpOnMainThread() override {
    disable_md_settings_.InitAndDisableFeature(
        features::kMaterialDesignSettings);
    const GURL url = chrome::GetSettingsUrl("editDictionary");
    ui_test_utils::NavigateToURL(browser(), url);
  }

 protected:
  const std::string kDictionaryListSelector =
      "#language-dictionary-overlay-word-list";

  content::RenderFrameHost* GetActiveFrame() {
    return GetActiveWebContents()->GetFocusedFrame();
  }

  content::RenderViewHost* GetRenderViewHost() {
    return GetActiveWebContents()->GetRenderViewHost();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Add a few test words to the dictionary.
  void SetTestWords(const std::string& list_selector) {
    const std::string script = base::StringPrintf(
        "document.querySelector('%s').setWordList(['cat', 'dog', 'bird']);",
        list_selector.c_str());
    EXPECT_TRUE(content::ExecuteScript(GetActiveFrame(), script));
    // Expected list size is 4: 3 word items + 1 placeholder.
    EXPECT_EQ(4, GetListSize(list_selector));
  }

  // Returns the number of items in the list.
  int GetListSize(const std::string& list_selector) {
    const std::string script = base::StringPrintf(
        "domAutomationController.send("
        "document.querySelector('%s').items.length);",
        list_selector.c_str());
    int length = -1;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        GetActiveFrame(),
        script,
        &length));
    return length;
  }

  // Returns true if element contains document.activeElement.
  bool ContainsActiveElement(const std::string& element_selector) {
    const std::string script = base::StringPrintf(
        "domAutomationController.send("
        "document.querySelector('%s').contains(document.activeElement));",
        element_selector.c_str());
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveFrame(),
        script,
        &result));
    return result;
  }

  // Returns true if list item[|index|] contains document.activeElement.
  bool ListItemContainsActiveElement(const std::string& list_selector,
                                     int index) {
    EXPECT_GE(index, 0);
    // EXPECT_TRUE will fail if index is out of bounds.
    const std::string script = base::StringPrintf(
        "domAutomationController.send("
        "document.querySelector('%s').items[%d].contains("
        "document.activeElement));",
        list_selector.c_str(),
        index);
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveFrame(),
        script,
        &result));
    return result;
  }

  // Returns true if list item[|index|] has 'selected' attribute.
  bool ListItemSelected(const std::string& list_selector, int index) {
    EXPECT_GE(index, 0);
    // EXPECT_TRUE will fail if index is out of bounds.
    const std::string script = base::StringPrintf(
        "domAutomationController.send("
        "document.querySelector('%s').items[%d].hasAttribute('selected'));",
        list_selector.c_str(),
        index);
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveFrame(),
        script,
        &result));
    return result;
  }

  // Returns true if list item[|index|] has 'selected' attribute and contains
  // document.activeElement.
  bool ListItemSelectedAndFocused(const std::string& list_selector,
                                  int index) {
    EXPECT_GE(index, 0);
    return ListItemSelected(list_selector, index) &&
           ListItemContainsActiveElement(list_selector, index);
  }

  // Press and release a key in the browser. This will wait for the element on
  // the page to change.
  bool PressKey(ui::KeyboardCode key_code, bool shift) {
    return ui_test_utils::SendKeyPressAndWait(
        browser(),
        key_code,
        false,
        shift,
        false,
        false,
        content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
        content::Source<content::RenderViewHost>(GetRenderViewHost()));
  }

  void InitializeDomMessageQueue() {
    dom_message_queue_.reset(new content::DOMMessageQueue);
  }

  // Wait for a message from the DOM automation controller.
  void WaitForDomMessage(const std::string& message) {
    const std::string expected = "\"" + message + "\"";
    std::string received;
    do {
      ASSERT_TRUE(dom_message_queue_->WaitForMessage(&received));
    } while (received != expected);
  }

  // Add a JavaScript event listener to send a DOM automation controller message
  // whenever the |selected| property of the list item changes.
  void ListenForItemSelectedChange(const std::string& list_selector,
                                   int index) {
    EXPECT_GE(index, 0);
    // EXPECT_TRUE will fail if index is out of bounds.
    const std::string script = base::StringPrintf(
        "document.querySelector('%s').items[%d].addEventListener("
            "'selectedChange', function() {"
              "domAutomationController.setAutomationId(0);"
              "domAutomationController.send('selected=' + this.selected);"
            "});",
        list_selector.c_str(),
        index);

    EXPECT_TRUE(content::ExecuteScript(
        GetActiveFrame(),
        script));
  }

 private:
  std::unique_ptr<content::DOMMessageQueue> dom_message_queue_;
  base::test::ScopedFeatureList disable_md_settings_;

  DISALLOW_COPY_AND_ASSIGN(LanguageDictionaryWebUITest);
};

}  // namespace

// Test InlineEditableItemList keyboard focus behavior in editDictionary
// overlay.
// editDictionary overlay doesn't exist on OSX so disable it there.
#if !defined(OS_MACOSX)

// Crashes on Win 7. http://crbug.com/500609
#if defined(OS_WIN)
#define MAYBE_TestListKeyboardFocus DISABLED_TestListKeyboardFocus
#else
#define MAYBE_TestListKeyboardFocus TestListKeyboardFocus
#endif

IN_PROC_BROWSER_TEST_F(LanguageDictionaryWebUITest,
                       MAYBE_TestListKeyboardFocus) {
  const std::string list_selector = kDictionaryListSelector;

  // Populate the list with some test words.
  SetTestWords(list_selector);
  int placeholder_index = GetListSize(list_selector) - 1;

  // Listen for changes of the placeholder item's |selected| property so that
  // test can wait until change has taken place after key press before
  // continuing.
  InitializeDomMessageQueue();
  ListenForItemSelectedChange(list_selector, placeholder_index);

  // Press tab to focus the placeholder.
  PressKey(ui::VKEY_TAB, false);

  // Wait for placeholder item to become selected.
  WaitForDomMessage("selected=true");

  // Verify that the placeholder is selected and has focus.
  EXPECT_TRUE(ListItemSelectedAndFocused(list_selector, placeholder_index));

  // Press up arrow to select item above the placeholder.
  PressKey(ui::VKEY_UP, false);

  // Wait for placeholder to become unselected.
  WaitForDomMessage("selected=false");

  // Verify that the placeholder is no longer selected.
  EXPECT_FALSE(ListItemSelected(list_selector, placeholder_index));

  // Verify that the item above the placeholder is selected and has focus.
  EXPECT_TRUE(ListItemSelectedAndFocused(list_selector,
                                         placeholder_index - 1));

  // Press tab to leave the list.
  PressKey(ui::VKEY_TAB, false);

  // Verify that focus has left the list.
  EXPECT_FALSE(ContainsActiveElement(list_selector));

  // Verify that the item above the placeholder is still selected.
  EXPECT_TRUE(ListItemSelected(list_selector, placeholder_index - 1));

  // Press shift+tab to go back to the list.
  PressKey(ui::VKEY_TAB, true);

  // Verify that the item above the placeholder is selected and has focus.
  EXPECT_TRUE(ListItemSelectedAndFocused(list_selector,
                                         placeholder_index - 1));
}
#endif  // !defined(OS_MACOSX)
