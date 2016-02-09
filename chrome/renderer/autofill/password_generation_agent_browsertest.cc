// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/renderer/autofill/password_generation_test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/test_password_generation_agent.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebInputElement;
using blink::WebNode;
using blink::WebString;

namespace autofill {

class PasswordGenerationAgentTest : public ChromeRenderViewTest {
 public:
  PasswordGenerationAgentTest() {}

  void TearDown() override {
    LoadHTML("");
    ChromeRenderViewTest::TearDown();
  }

  void LoadHTMLWithUserGesture(const char* html) {
    LoadHTML(html);

    // Enable show-ime event when element is focused by indicating that a user
    // gesture has been processed since load.
    EXPECT_TRUE(SimulateElementClick("dummy"));
  }

  void FocusField(const char* element_id) {
    WebDocument document = GetMainFrame()->document();
    blink::WebElement element =
        document.getElementById(blink::WebString::fromUTF8(element_id));
    ASSERT_FALSE(element.isNull());
    ExecuteJavaScriptForTests(
        base::StringPrintf("document.getElementById('%s').focus();",
                           element_id).c_str());
  }

  void ExpectGenerationAvailable(const char* element_id,
                                 bool available) {
    FocusField(element_id);
    const IPC::Message* message =
        render_thread_->sink().GetFirstMessageMatching(
            AutofillHostMsg_ShowPasswordGenerationPopup::ID);
    if (available)
      ASSERT_TRUE(message);
    else
      ASSERT_FALSE(message);

    render_thread_->sink().ClearMessages();
  }

  void ShowGenerationPopUpManually(const char* element_id) {
    FocusField(element_id);
    AutofillMsg_GeneratePassword msg(0);
    password_generation_->OnMessageReceived(msg);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationAgentTest);
};

const char kSigninFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'password'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' "
    "         autocomplete = 'off' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'text' id = 'address'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kDisabledElementAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' "
    "         autocomplete = 'off' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'text' id = 'address'/> "
    "  <INPUT type = 'text' id = 'disabled' disabled/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kHiddenPasswordAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password'/> "
    "  <INPUT type = 'password' id = 'second_password' style='display:none'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kInvalidActionAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'invalid'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password'/> "
    "  <INPUT type = 'password' id = 'second_password'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kMultipleAccountCreationFormHTML[] =
    "<FORM name = 'login' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'random'/> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'password'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>"
    "<FORM name = 'signup' action = 'http://www.random.com/signup'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' "
    "         autocomplete = 'off' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'text' id = 'address'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kBothAutocompleteAttributesFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' autocomplete='username' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' "
    "         autocomplete = 'new-password' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kUsernameAutocompleteAttributeFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' autocomplete='username' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kNewPasswordAutocompleteAttributeFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' "
    "         autocomplete='new-password' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char ChangeDetectionScript[] =
    "<script>"
    "  firstOnChangeCalled = false;"
    "  secondOnChangeCalled = false;"
    "  document.getElementById('first_password').onchange = function() {"
    "    firstOnChangeCalled = true;"
    "  };"
    "  document.getElementById('second_password').onchange = function() {"
    "    secondOnChangeCalled = true;"
    "  };"
    "</script>";

const char kPasswordChangeFormHTML[] =
    "<FORM name = 'ChangeWithUsernameForm' action = 'http://www.bidule.com'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'password'/> "
    "  <INPUT type = 'password' id = 'newpassword'/> "
    "  <INPUT type = 'password' id = 'confirmpassword'/> "
    "  <INPUT type = 'button' id = 'dummy'/> "
    "  <INPUT type = 'submit' value = 'Login'/> "
    "</FORM>";

TEST_F(PasswordGenerationAgentTest, DetectionTest) {
  // Don't shown the icon for non account creation forms.
  LoadHTMLWithUserGesture(kSigninFormHTML);
  ExpectGenerationAvailable("password", false);

  // We don't show the decoration yet because the feature isn't enabled.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  ExpectGenerationAvailable("first_password", false);

  // Pretend like We have received message indicating site is not blacklisted,
  // and we have received message indicating the form is classified as
  // ACCOUNT_CREATION_FORM form Autofill server. We should show the icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);
  ExpectGenerationAvailable("first_password", true);

  // Hidden fields are not treated differently.
  LoadHTMLWithUserGesture(kHiddenPasswordAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_,
                           kHiddenPasswordAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);
  ExpectGenerationAvailable("first_password", true);

  // This doesn't trigger because the form action is invalid.
  LoadHTMLWithUserGesture(kInvalidActionAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_,
                           kInvalidActionAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);
  ExpectGenerationAvailable("first_password", false);
}

TEST_F(PasswordGenerationAgentTest, FillTest) {
  // Make sure that we are enabled before loading HTML.
  std::string html = std::string(kAccountCreationFormHTML) +
      ChangeDetectionScript;
  LoadHTMLWithUserGesture(html.c_str());
  SetNotBlacklistedMessage(password_generation_, html.c_str());
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);

  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("first_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement first_password_element = element.to<WebInputElement>();
  element = document.getElementById(WebString::fromUTF8("second_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement second_password_element = element.to<WebInputElement>();

  // Both password fields should be empty.
  EXPECT_TRUE(first_password_element.value().isNull());
  EXPECT_TRUE(second_password_element.value().isNull());

  base::string16 password = base::ASCIIToUTF16("random_password");
  AutofillMsg_GeneratedPasswordAccepted msg(0, password);
  password_generation_->OnMessageReceived(msg);

  // Password fields are filled out and set as being autofilled.
  EXPECT_EQ(password, first_password_element.value());
  EXPECT_EQ(password, second_password_element.value());
  EXPECT_TRUE(first_password_element.isAutofilled());
  EXPECT_TRUE(second_password_element.isAutofilled());

  // Make sure onchange events are called.
  int first_onchange_called = -1;
  int second_onchange_called = -1;
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          base::ASCIIToUTF16("firstOnChangeCalled ? 1 : 0"),
          &first_onchange_called));
  EXPECT_EQ(1, first_onchange_called);
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          base::ASCIIToUTF16("secondOnChangeCalled ? 1 : 0"),
          &second_onchange_called));
  EXPECT_EQ(1, second_onchange_called);

  // Focus moved to the next input field.
  // TODO(zysxqn): Change this back to the address element once Bug 90224
  // https://bugs.webkit.org/show_bug.cgi?id=90224 has been fixed.
  element = document.getElementById(WebString::fromUTF8("first_password"));
  ASSERT_FALSE(element.isNull());
  EXPECT_EQ(element, document.focusedElement());
}

TEST_F(PasswordGenerationAgentTest, EditingTest) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);

  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("first_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement first_password_element = element.to<WebInputElement>();
  element = document.getElementById(WebString::fromUTF8("second_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement second_password_element = element.to<WebInputElement>();

  base::string16 password = base::ASCIIToUTF16("random_password");
  AutofillMsg_GeneratedPasswordAccepted msg(0, password);
  password_generation_->OnMessageReceived(msg);

  // Passwords start out the same.
  EXPECT_EQ(password, first_password_element.value());
  EXPECT_EQ(password, second_password_element.value());

  // After editing the first field they are still the same.
  std::string edited_password_ascii = "edited_password";
  SimulateUserInputChangeForElement(&first_password_element,
                                    edited_password_ascii);
  base::string16 edited_password = base::ASCIIToUTF16(edited_password_ascii);
  EXPECT_EQ(edited_password, first_password_element.value());
  EXPECT_EQ(edited_password, second_password_element.value());

  // Clear any uninteresting sent messages.
  render_thread_->sink().ClearMessages();

  // Verify that password mirroring works correctly even when the password
  // is deleted.
  SimulateUserInputChangeForElement(&first_password_element, std::string());
  EXPECT_EQ(base::string16(), first_password_element.value());
  EXPECT_EQ(base::string16(), second_password_element.value());

  // Should have notified the browser that the password is no longer generated
  // and trigger generation again.
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordNoLongerGenerated::ID));
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_ShowPasswordGenerationPopup::ID));
}

TEST_F(PasswordGenerationAgentTest, BlacklistedTest) {
  // Did not receive not blacklisted message. Don't show password generation
  // icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);
  ExpectGenerationAvailable("first_password", false);

  // Receive one not blacklisted message for non account creation form. Don't
  // show password generation icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kSigninFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);
  ExpectGenerationAvailable("first_password", false);

  // Receive one not blacklisted message for account creation form. Show
  // password generation icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);
  ExpectGenerationAvailable("first_password", true);

  // Receive two not blacklisted messages, one is for account creation form and
  // the other is not. Show password generation icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kSigninFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);
  ExpectGenerationAvailable("first_password", true);
}

TEST_F(PasswordGenerationAgentTest, AccountCreationFormsDetectedTest) {
  // Did not receive account creation forms detected message. Don't show
  // password generation icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kAccountCreationFormHTML);
  ExpectGenerationAvailable("first_password", false);

  // Receive the account creation forms detected message. Show password
  // generation icon.
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);
  ExpectGenerationAvailable("first_password", true);
}

TEST_F(PasswordGenerationAgentTest, MaximumOfferSize) {
  base::HistogramTester histogram_tester;

  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);
  ExpectGenerationAvailable("first_password", true);

  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("first_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement first_password_element = element.to<WebInputElement>();

  // Make a password just under maximum offer size.
  SimulateUserInputChangeForElement(
      &first_password_element,
      std::string(password_generation_->kMaximumOfferSize - 1, 'a'));
  // There should now be a message to show the UI.
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_ShowPasswordGenerationPopup::ID));
  render_thread_->sink().ClearMessages();

  // Simulate a user typing a password just over maximum offer size.
  SimulateUserTypingASCIICharacter('a', false);
  SimulateUserTypingASCIICharacter('a', true);
  // There should now be a message to hide the UI.
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_HidePasswordGenerationPopup::ID));
  render_thread_->sink().ClearMessages();

  // Simulate the user deleting characters. The generation popup should be shown
  // again.
  SimulateUserTypingASCIICharacter(ui::VKEY_BACK, true);
  // There should now be a message to show the UI.
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_ShowPasswordGenerationPopup::ID));
  render_thread_->sink().ClearMessages();

  // Change focus. Bubble should be hidden, but that is handled by AutofilAgent,
  // so no messages are sent.
  ExecuteJavaScriptForTests("document.getElementById('username').focus();");
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_ShowPasswordGenerationPopup::ID));
  render_thread_->sink().ClearMessages();

  // Focusing the password field will bring up the generation UI again.
  ExecuteJavaScriptForTests(
      "document.getElementById('first_password').focus();");
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_ShowPasswordGenerationPopup::ID));
  render_thread_->sink().ClearMessages();

  // Loading a different page triggers UMA stat upload. Verify that only one
  // display event is sent even though
  LoadHTMLWithUserGesture(kSigninFormHTML);

  histogram_tester.ExpectBucketCount(
      "PasswordGeneration.Event",
      autofill::password_generation::GENERATION_POPUP_SHOWN,
      1);
}

TEST_F(PasswordGenerationAgentTest, DynamicFormTest) {
  LoadHTMLWithUserGesture(kSigninFormHTML);
  SetNotBlacklistedMessage(password_generation_, kSigninFormHTML);

  ExecuteJavaScriptForTests(
      "var form = document.createElement('form');"
      "form.action='http://www.random.com';"
      "var username = document.createElement('input');"
      "username.type = 'text';"
      "username.id = 'dynamic_username';"
      "var first_password = document.createElement('input');"
      "first_password.type = 'password';"
      "first_password.id = 'first_password';"
      "first_password.name = 'first_password';"
      "var second_password = document.createElement('input');"
      "second_password.type = 'password';"
      "second_password.id = 'second_password';"
      "second_password.name = 'second_password';"
      "form.appendChild(username);"
      "form.appendChild(first_password);"
      "form.appendChild(second_password);"
      "document.body.appendChild(form);");
  ProcessPendingMessages();

  // This needs to come after the DOM has been modified.
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 1, 1);

  // TODO(gcasto): I'm slightly worried about flakes in this test where
  // didAssociateFormControls() isn't called. If this turns out to be a problem
  // adding a call to OnDynamicFormsSeen(GetMainFrame()) will fix it, though
  // it will weaken the test.
  ExpectGenerationAvailable("first_password", true);
}

TEST_F(PasswordGenerationAgentTest, MultiplePasswordFormsTest) {
  // If two forms on the page looks like possible account creation forms, make
  // sure to trigger on the one that is specified from Autofill.
  LoadHTMLWithUserGesture(kMultipleAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_,
                           kMultipleAccountCreationFormHTML);

  // Should trigger on the second form.
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 1, 1);

  ExpectGenerationAvailable("password", false);
  ExpectGenerationAvailable("first_password", true);
}

TEST_F(PasswordGenerationAgentTest, MessagesAfterAccountSignupFormFound) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_, kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);

  // Generation should be enabled.
  ExpectGenerationAvailable("first_password", true);

  // Extra not blacklisted messages can be sent. Make sure that they are handled
  // correctly (generation should still be available).
  SetNotBlacklistedMessage(password_generation_, kAccountCreationFormHTML);

  // Need to focus another field first for verification to work.
  ExpectGenerationAvailable("second_password", false);
  ExpectGenerationAvailable("first_password", true);
}

// Losing focus should not trigger a password generation popup.
TEST_F(PasswordGenerationAgentTest, BlurTest) {
  render_thread_->sink().ClearMessages();
  LoadHTMLWithUserGesture(kDisabledElementAccountCreationFormHTML);
  SetNotBlacklistedMessage(password_generation_,
                           kDisabledElementAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 1);

  // Focus on the first password field: password generation popup should show
  // up.
  ExpectGenerationAvailable("first_password", true);

  // Remove focus from everywhere by clicking an unfocusable element: password
  // generation popup should not show up.
  EXPECT_TRUE(SimulateElementClick("disabled"));
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_GenerationAvailableForForm::ID));
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_ShowPasswordGenerationPopup::ID));
}

TEST_F(PasswordGenerationAgentTest, AutocompleteAttributesTest) {
  // Verify that autocomplete attributes can override Autofill to enable
  // generation
  LoadHTMLWithUserGesture(kBothAutocompleteAttributesFormHTML);
  SetNotBlacklistedMessage(password_generation_,
                           kBothAutocompleteAttributesFormHTML);

  ExpectGenerationAvailable("first_password", true);

  // Only setting one of the two attributes doesn't trigger generation.
  LoadHTMLWithUserGesture(kUsernameAutocompleteAttributeFormHTML);
  SetNotBlacklistedMessage(password_generation_,
                           kUsernameAutocompleteAttributeFormHTML);

  ExpectGenerationAvailable("first_password", false);

  LoadHTMLWithUserGesture(kNewPasswordAutocompleteAttributeFormHTML);
  SetNotBlacklistedMessage(password_generation_,
                           kNewPasswordAutocompleteAttributeFormHTML);

  ExpectGenerationAvailable("first_password", false);
}

TEST_F(PasswordGenerationAgentTest, ChangePasswordFormDetectionTest) {
  // Verify that generation is shown on correct field after message receiving.
  LoadHTMLWithUserGesture(kPasswordChangeFormHTML);
  SetNotBlacklistedMessage(password_generation_, kPasswordChangeFormHTML);
  ExpectGenerationAvailable("password", false);
  ExpectGenerationAvailable("newpassword", false);

  SetAccountCreationFormsDetectedMessage(password_generation_,
                                         GetMainFrame()->document(), 0, 2);
  ExpectGenerationAvailable("password", false);
  ExpectGenerationAvailable("newpassword", true);
}

TEST_F(PasswordGenerationAgentTest, ManualGenerationTest) {
  LoadHTMLWithUserGesture(kAccountCreationFormHTML);
  ShowGenerationPopUpManually("first_password");
  ExpectGenerationAvailable("first_password", true);
  ExpectGenerationAvailable("second_password", false);
}

}  // namespace autofill
