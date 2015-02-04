// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/test/browser_test_utils.h"

namespace {

// This class tests the Autofill options settings.
// This test is part of the interactive_ui_tests instead of browser_tests
// because it is necessary to emulate pushing the tab key.
class AutofillOptionsWebUITest : public InProcessBrowserTest {
 public:
  AutofillOptionsWebUITest() {}

  // Navigate to the autofillEditAddress page.
  void SetUpOnMainThread() override {
    const GURL url = chrome::GetSettingsUrl("autofillEditAddress");
    ui_test_utils::NavigateToURL(browser(), url);
  }

 protected:
  const std::string kEditAddressOverlaySelector =
      "#autofill-edit-address-overlay";

  content::RenderFrameHost* GetActiveFrame() {
    return GetActiveWebContents()->GetFocusedFrame();
  }

  content::RenderViewHost* GetRenderViewHost() {
    return GetActiveWebContents()->GetRenderViewHost();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void CreateTestProfile() {
    autofill::AddTestProfile(browser(), autofill::test::GetFullProfile());
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

  // Focus the first input field of the first list item.
  void FocusFirstListItemInput(const std::string& list_selector) {
    const std::string script = base::StringPrintf(
        "document.querySelector('%s input').focus();",
        list_selector.c_str());
    EXPECT_TRUE(content::ExecuteScript(GetActiveFrame(), script));
  }

  // Returns the text of the first item in the list.
  std::string GetFirstListItemText(const std::string& list_selector) {
    // EXPECT_TRUE will fail if there is no first item or first item does not
    // have 'input'.
    const std::string script = base::StringPrintf(
        "domAutomationController.send("
        "document.querySelector('%s input').value);",
        list_selector.c_str());
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        GetActiveFrame(),
        script,
        &result));
    return result;
  }

  // Returns true if the first item in the list has 'selected' attribute.
  bool GetFirstListItemSelected(const std::string& list_selector) {
    // EXPECT_TRUE will fail if there is no first item.
    const std::string script = base::StringPrintf(
        "domAutomationController.send("
        "document.querySelector('%s').items[0].hasAttribute('selected'));",
        list_selector.c_str());
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveFrame(),
        script,
        &result));
    return result;
  }

  // Returns true if a row delete button ('X' button) is focused.
  bool GetDeleteButtonFocused() {
    const std::string script =
        "domAutomationController.send("
        "document.activeElement.classList.contains('row-delete-button'));";
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveFrame(),
        script,
        &result));
    return result;
  }

  // Insert text into currently focused element.
  void InsertText(const std::string& text) {
    ASSERT_EQ(std::string::npos, text.find("'"));
    const std::string script = base::StringPrintf(
        "document.execCommand('insertText', false, '%s');",
        text.c_str());
    EXPECT_TRUE(content::ExecuteScript(GetActiveFrame(), script));
  }

  // Press and release tab key in the browser. This will wait for the element on
  // the page to change.
  bool PressTab(bool shift) {
    return ui_test_utils::SendKeyPressAndWait(
        browser(),
        ui::VKEY_TAB,
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

  void ListenForFirstItemSelected(const std::string& list_selector) {
    const std::string script = base::StringPrintf(
        "document.querySelector('%s').items[0].addEventListener("
            "'selectedChange', function(e) {"
          "if (e.newValue) {"
            "domAutomationController.setAutomationId(0);"
            "domAutomationController.send('first item selected');"
          "}"
        "});",
        list_selector.c_str());

    EXPECT_TRUE(content::ExecuteScript(
        GetActiveFrame(),
        script));
  }

  void ListenForCommitEdit(const std::string& list_selector) {
    const std::string script = base::StringPrintf(
        "document.querySelector('%s').addEventListener("
            "'commitedit', function() {"
          "domAutomationController.setAutomationId(0);"
          "domAutomationController.send('done commitedit');"
        "});",
        list_selector.c_str());

    EXPECT_TRUE(content::ExecuteScript(
        GetActiveFrame(),
        script));
  }

  // Add an event listener to send a DOM automation controller message from
  // JavaScript each time validation completes for the list.
  void ListenForDoneValidating(const std::string& list_selector) {
    // doneValidating will execute the 'then' function immediately if no
    // validations are pending, so wait for 'commitedit' event before calling
    // doneValidating.
    const std::string script = base::StringPrintf(
        "document.querySelector('%s').addEventListener('commitedit',"
                                                      "function() {"
          "document.querySelector('%s').doneValidating().then(function() {"
            "domAutomationController.setAutomationId(0);"
            "domAutomationController.send('done validating');"
          "});"
        "});",
        list_selector.c_str(),
        list_selector.c_str());

    EXPECT_TRUE(content::ExecuteScript(
        GetActiveFrame(),
        script));
  }

  // Verifies that everything is the way it should be after list item is
  // added or edited.
  void VerifyEditAddressListPostConditions(const std::string& list_selector,
                                           const std::string& input_text,
                                           bool list_requires_validation) {
    // Verify that neither the list nor any of its children still have focus.
    EXPECT_FALSE(ContainsActiveElement(list_selector));

    // Verify that focus moved to a different element of the overlay.
    EXPECT_TRUE(ContainsActiveElement(kEditAddressOverlaySelector));

    // Verify that list has exactly two items. They will be the item that was
    // just added/modified + the placeholder.
    EXPECT_EQ(2, GetListSize(list_selector));

    // Verify that the first list item has the string that was inserted.
    EXPECT_EQ(input_text, GetFirstListItemText(list_selector));

    // TODO(bondd): phone list doesn't select first item after validation.
    // It becomes selected later when the list is given focus.
    if (!list_requires_validation) {
      // Verify that the first list item is the selected item in the list.
      EXPECT_TRUE(GetFirstListItemSelected(list_selector));
    }
  }

  // Make sure that when text is entered in the placeholder of an empty list and
  // the tab key is pressed:
  //   + Focus leaves the list and goes to a different element on the page.
  //   + The text stays added and a new placeholder is created.
  //   + The list item with the newly added text is the selected list item (not
  //         the placeholder).
  //
  // Added to prevent http://crbug.com/440760 from regressing again.
  void TestEditAddressListTabKeyAddItem(const std::string& list_selector,
                                        const std::string& input_text,
                                        bool list_requires_validation) {
    // Focus the input field and insert test string.
    FocusFirstListItemInput(list_selector);
    WaitForDomMessage("first item selected");

    InsertText(input_text);

    // Press tab key to move to next element after the list.
    PressTab(false);

    if (list_requires_validation)
      WaitForDomMessage("done validating");
    else
      WaitForDomMessage("done commitedit");

    // Make sure everything ended up the way it should be.
    VerifyEditAddressListPostConditions(list_selector, input_text,
                                        list_requires_validation);
  }

  // Depends on state set up by TestEditAddressListTabKeyAddItem. Should be
  // called immediately after that method.
  //
  // Make sure that when a list item's text is edited and the tab key is
  // pressed twice:
  //   + After the first tab press the item's delete button is focused.
  //   + After the second tab press focus leaves the list and goes to a
  //         different element on the page.
  //   + The edited text persists.
  //   + The edited list item is the selected list item.
  //
  // Added to prevent http://crbug.com/443491 from regressing again.
  void TestEditAddressListTabKeyEditItem(const std::string& list_selector,
                                         const std::string& input_text,
                                         bool list_requires_validation,
                                         bool skip_ok_button) {
    if (skip_ok_button)
      PressTab(true);

    // Press shift+tab to move back to the first list item's delete button.
    PressTab(true);
    EXPECT_TRUE(GetDeleteButtonFocused());

    // Press shift+tab to move back to the first list item's input field.
    PressTab(true);
    // Verify that the first item in the list is focused.
    EXPECT_TRUE(ContainsActiveElement(list_selector + " input"));

    // Insert modified text in the first list item.
    std::string second_input = "second" + input_text;
    InsertText(second_input);

    // Press tab key to focus the list item's delete button.
    PressTab(false);
    EXPECT_TRUE(GetDeleteButtonFocused());

    // Press tab key again to move to next element after the list.
    PressTab(false);

    if (list_requires_validation)
      WaitForDomMessage("done validating");
    else
      WaitForDomMessage("done commitedit");

    // Make sure everything ended up the way it should be.
    VerifyEditAddressListPostConditions(list_selector, second_input,
                                        list_requires_validation);
  }

  void TestEditAddressListTabKey(const std::string& field_name,
                                 const std::string& input_text,
                                 bool list_requires_validation,
                                 bool skip_ok_button) {
    const std::string list_selector = kEditAddressOverlaySelector + " [field=" +
        field_name + "]";

    InitializeDomMessageQueue();
    ListenForFirstItemSelected(list_selector);
    if (list_requires_validation)
      ListenForDoneValidating(list_selector);
    else
      ListenForCommitEdit(list_selector);

    TestEditAddressListTabKeyAddItem(list_selector, input_text,
                                     list_requires_validation);
    TestEditAddressListTabKeyEditItem(list_selector, input_text,
                                      list_requires_validation, skip_ok_button);
  }

 private:
  scoped_ptr<content::DOMMessageQueue> dom_message_queue_;

  DISALLOW_COPY_AND_ASSIGN(AutofillOptionsWebUITest);
};

}  // namespace

// Test the 'fullName' InlineEditableItemList in autofillEditAddress overlay.
IN_PROC_BROWSER_TEST_F(AutofillOptionsWebUITest,
                       TestEditAddressNameTabKey) {
  TestEditAddressListTabKey("fullName", "Test Name", false, false);
}

// Test the 'phone' InlineEditableItemList in autofillEditAddress overlay.
IN_PROC_BROWSER_TEST_F(AutofillOptionsWebUITest,
                       TestEditAddressPhoneTabKey) {
  CreateTestProfile();
  TestEditAddressListTabKey("phone", "123-456-7890", true, false);
}

// Test the 'email' InlineEditableItemList in autofillEditAddress overlay.
IN_PROC_BROWSER_TEST_F(AutofillOptionsWebUITest,
                       TestEditAddressEmailTabKey) {
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  // Button strip order is 'Cancel' and then 'OK' on Linux and Mac. 'OK' and
  // then 'Cancel' on Windows and CrOS.
  bool skip_ok_button = true;
#else
  bool skip_ok_button = false;
#endif

  TestEditAddressListTabKey("email", "test@example.com", false, skip_ok_button);
}
