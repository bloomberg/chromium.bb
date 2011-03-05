// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/renderer/autofill/autofill_agent.h"
#include "chrome/renderer/autofill/password_autofill_manager.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormField;
using webkit_glue::PasswordFormFillData;
using webkit_glue::PasswordForm;
using webkit_glue::PasswordFormDomManager;
using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebString;

namespace {

// The name of the username/password element in the form.
const char* const kUsernameName = "username";
const char* const kPasswordName = "password";

const char* const kAliceUsername = "alice";
const char* const kAlicePassword = "password";
const char* const kBobUsername = "bob";
const char* const kBobPassword = "secret";
const char* const kCarolUsername = "Carol";
const char* const kCarolPassword = "test";


const char* const kFormHTML =
    "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

}  // namespace

namespace autofill {

class PasswordAutofillManagerTest : public RenderViewTest {
 public:
  PasswordAutofillManagerTest() {
  }

  // Simulates the fill password form message being sent to the renderer.
  // We use that so we don't have to make RenderView::OnFillPasswordForm()
  // protected.
  void SimulateOnFillPasswordForm(
      const PasswordFormFillData& fill_data) {
    AutoFillMsg_FillPasswordForm msg(0, fill_data);
    password_autofill_->OnMessageReceived(msg);
  }

  virtual void SetUp() {
    RenderViewTest::SetUp();

    // Add a preferred login and an additional login to the FillData.
    username1_ = ASCIIToUTF16(kAliceUsername);
    password1_ = ASCIIToUTF16(kAlicePassword);
    username2_ = ASCIIToUTF16(kBobUsername);
    password2_ = ASCIIToUTF16(kBobPassword);
    username3_ = ASCIIToUTF16(kCarolUsername);
    password3_ = ASCIIToUTF16(kCarolPassword);

    fill_data_.basic_data.fields.push_back(
        FormField(string16(), ASCIIToUTF16(kUsernameName),
                  username1_, string16(), 0, false));
    fill_data_.basic_data.fields.push_back(
        FormField(string16(), ASCIIToUTF16(kPasswordName),
                  password1_, string16(), 0, false));
    fill_data_.additional_logins[username2_] = password2_;
    fill_data_.additional_logins[username3_] = password3_;

    // We need to set the origin so it matches the frame URL and the action so
    // it matches the form action, otherwise we won't autocomplete.
    std::string origin("data:text/html;charset=utf-8,");
    origin += kFormHTML;
    fill_data_.basic_data.origin = GURL(origin);
    fill_data_.basic_data.action = GURL("http://www.bidule.com");

    LoadHTML(kFormHTML);

    // Now retrieves the input elements so the test can access them.
    WebDocument document = GetMainFrame()->document();
    WebElement element =
        document.getElementById(WebString::fromUTF8(kUsernameName));
    ASSERT_FALSE(element.isNull());
    username_element_ = element.to<WebKit::WebInputElement>();
    element = document.getElementById(WebString::fromUTF8(kPasswordName));
    ASSERT_FALSE(element.isNull());
    password_element_ = element.to<WebKit::WebInputElement>();
  }

  void ClearUsernameAndPasswordFields() {
    username_element_.setValue("");
    username_element_.setAutofilled(false);
    password_element_.setValue("");
    password_element_.setAutofilled(false);
  }

  void SimulateUsernameChange(const std::string& username,
                              bool move_caret_to_end) {
    username_element_.setValue(WebString::fromUTF8(username));
    if (move_caret_to_end)
      username_element_.setSelectionRange(username.length(), username.length());
    autofill_agent_->textFieldDidChange(username_element_);
    // Processing is delayed because of a WebKit bug, see
    // PasswordAutocompleteManager::TextDidChangeInTextField() for details.
    MessageLoop::current()->RunAllPending();
  }

  void SimulateKeyDownEvent(const WebInputElement& element,
                            ui::KeyboardCode key_code) {
    WebKit::WebKeyboardEvent key_event;
    key_event.windowsKeyCode = key_code;
    autofill_agent_->textFieldDidReceiveKeyDown(element, key_event);
  }

  void CheckTextFieldsState(const std::string& username,
                            bool username_autofilled,
                            const std::string& password,
                            bool password_autofilled) {
    EXPECT_EQ(username,
              static_cast<std::string>(username_element_.value().utf8()));
    EXPECT_EQ(username_autofilled, username_element_.isAutofilled());
    EXPECT_EQ(password,
              static_cast<std::string>(password_element_.value().utf8()));
    EXPECT_EQ(password_autofilled, password_element_.isAutofilled());
  }

  void CheckUsernameSelection(int start, int end) {
    EXPECT_EQ(start, username_element_.selectionStart());
    EXPECT_EQ(end, username_element_.selectionEnd());
  }

  string16 username1_;
  string16 username2_;
  string16 username3_;
  string16 password1_;
  string16 password2_;
  string16 password3_;
  PasswordFormFillData fill_data_;

  WebInputElement username_element_;
  WebInputElement password_element_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillManagerTest);
};

}

using autofill::PasswordAutofillManagerTest;

// Tests that the password login is autocompleted as expected when the browser
// sends back the password info.
TEST_F(PasswordAutofillManagerTest, InitialAutocomplete) {
  /*
   * Right now we are not sending the message to the browser because we are
   * loading a data URL and the security origin canAccessPasswordManager()
   * returns false.  May be we should mock URL loading to cirmcuvent this?
   TODO(jcivelli): find a way to make the security origin not deny access to the
                   password manager and then reenable this code.

  // The form has been loaded, we should have sent the browser a message about
  // the form.
  const IPC::Message* msg = render_thread_.sink().GetFirstMessageMatching(
      AutoFillHostMsg_PasswordFormsFound::ID);
  ASSERT_TRUE(msg != NULL);

  Tuple1<std::vector<PasswordForm> > forms;
  AutoFillHostMsg_PasswordFormsFound::Read(msg, &forms);
  ASSERT_EQ(1U, forms.a.size());
  PasswordForm password_form = forms.a[0];
  EXPECT_EQ(PasswordForm::SCHEME_HTML, password_form.scheme);
  EXPECT_EQ(ASCIIToUTF16(kUsernameName), password_form.username_element);
  EXPECT_EQ(ASCIIToUTF16(kPasswordName), password_form.password_element);
  */

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that changing the username does not fill a read-only password field.
TEST_F(PasswordAutofillManagerTest, NoInitialAutocompleteForReadOnly) {
  password_element_.setAttribute(WebString::fromUTF8("readonly"),
                                 WebString::fromUTF8("true"));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Only the username should have been autocompleted.
  // TODO(jcivelli): may be we should not event fill the username?
  CheckTextFieldsState(kAliceUsername, true, "", false);
}

// Tests that having a non-empty username precludes the autocomplete.
TEST_F(PasswordAutofillManagerTest, NoInitialAutocompleteForFilledField) {
  username_element_.setValue(WebString::fromUTF8("bogus"));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsState("bogus", false, "", false);
}

// Tests that editing the password clears the autocompleted password field.
TEST_F(PasswordAutofillManagerTest, PasswordClearOnEdit) {
  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate the user changing the username to some unknown username.
  SimulateUsernameChange("alicia", true);

  // The password should have been cleared.
  CheckTextFieldsState("alicia", false, "", false);
}

// Tests that we only autocomplete on focus lost and with a full username match
// when |wait_for_username| is true.
TEST_F(PasswordAutofillManagerTest, WaitUsername) {
  // Simulate the browser sending back the login info.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // No auto-fill should have taken place.
  CheckTextFieldsState("", false, "", false);

  // No autocomplete should happen when text is entered in the username.
  SimulateUsernameChange("a", true);
  CheckTextFieldsState("a", false, "", false);
  SimulateUsernameChange("al", true);
  CheckTextFieldsState("al", false, "", false);
  SimulateUsernameChange(kAliceUsername, true);
  CheckTextFieldsState(kAliceUsername, false, "", false);

  // Autocomplete should happen only when the username textfield is blurred with
  // a full match.
  username_element_.setValue("a");
  autofill_agent_->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState("a", false, "", false);
  username_element_.setValue("al");
  autofill_agent_->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState("al", false, "", false);
  username_element_.setValue("alices");
  autofill_agent_->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState("alices", false, "", false);
  username_element_.setValue(ASCIIToUTF16(kAliceUsername));
  autofill_agent_->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that inline autocompletion works properly.
TEST_F(PasswordAutofillManagerTest, InlineAutocomplete) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Simulate the user typing in the first letter of 'alice', a stored username.
  SimulateUsernameChange("a", true);
  // Both the username and password text fields should reflect selection of the
  // stored login.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  // And the selection should have been set to 'lice', the last 4 letters.
  CheckUsernameSelection(1, 5);

  // Now the user types the next letter of the same username, 'l'.
  SimulateUsernameChange("al", true);
  // Now the fields should have the same value, but the selection should have a
  // different start value.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  CheckUsernameSelection(2, 5);

  // Test that deleting does not trigger autocomplete.
  SimulateKeyDownEvent(username_element_, ui::VKEY_BACK);
  SimulateUsernameChange("alic", true);
  CheckTextFieldsState("alic", false, "", false);
  CheckUsernameSelection(4, 4);  // No selection.
  // Reset the last pressed key to something other than backspace.
  SimulateKeyDownEvent(username_element_, ui::VKEY_A);

  // Now lets say the user goes astray from the stored username and types the
  // letter 'f', spelling 'alf'.  We don't know alf (that's just sad), so in
  // practice the username should no longer be 'alice' and the selected range
  // should be empty.
  SimulateUsernameChange("alf", true);
  CheckTextFieldsState("alf", false, "", false);
  CheckUsernameSelection(3, 3);  // No selection.

  // Ok, so now the user removes all the text and enters the letter 'b'.
  SimulateUsernameChange("b", true);
  // The username and password fields should match the 'bob' entry.
  CheckTextFieldsState(kBobUsername, true, kBobPassword, true);
  CheckUsernameSelection(1, 3);

  // Then, the user again removes all the text and types an uppercase 'C'.
  SimulateUsernameChange("C", true);
  // The username and password fields should match the 'Carol' entry.
  CheckTextFieldsState(kCarolUsername, true, kCarolPassword, true);
  CheckUsernameSelection(1, 5);
  // Finally, the user removes all the text and types a lowercase 'c'.  We only
  // want case-sensitive autocompletion, so the username and the selected range
  // should be empty.
  SimulateUsernameChange("c", true);
  CheckTextFieldsState("c", false, "", false);
  CheckUsernameSelection(1, 1);
}

// Tests that accepting an item in the suggestion drop-down works.
TEST_F(PasswordAutofillManagerTest, SuggestionAccept) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // To simulate accepting an item in the suggestion drop-down we just mimic
  // what the WebView does: it sets the element value then calls
  // didAcceptAutoFillSuggestion on the renderer.
  autofill_agent_->didAcceptAutoFillSuggestion(username_element_,
                                               ASCIIToUTF16(kAliceUsername),
                                               WebKit::WebString(),
                                               0,
                                               0);
  // Autocomplete should have kicked in.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that selecting an item in the suggestion drop-down no-ops.
TEST_F(PasswordAutofillManagerTest, SuggestionSelect) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // To simulate accepting an item in the suggestion drop-down we just mimic
  // what the WebView does: it sets the element value then calls
  // didSelectAutoFillSuggestion on the renderer.
  autofill_agent_->didSelectAutoFillSuggestion(username_element_,
                                               ASCIIToUTF16(kAliceUsername),
                                               WebKit::WebString(),
                                               0);
  // Autocomplete should not have kicked in.
  CheckTextFieldsState("", false, "", false);
}
