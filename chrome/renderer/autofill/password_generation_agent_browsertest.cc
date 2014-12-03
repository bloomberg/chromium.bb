// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
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

  void SetNotBlacklistedMessage(const char* form_str) {
    autofill::PasswordForm form;
    form.origin =
        GURL(base::StringPrintf("data:text/html;charset=utf-8,%s", form_str));
    AutofillMsg_FormNotBlacklisted msg(0, form);
    password_generation_->OnMessageReceived(msg);
  }

  void SetAccountCreationFormsDetectedMessage(const char* form_str) {
    autofill::FormData form;
    form.origin =
        GURL(base::StringPrintf("data:text/html;charset=utf-8,%s", form_str));
    std::vector<autofill::FormData> forms;
    forms.push_back(form);
    AutofillMsg_AccountCreationFormsDetected msg(0, forms);
    password_generation_->OnMessageReceived(msg);
  }

  void ExpectPasswordGenerationAvailable(const char* element_id,
                                         bool available) {
    WebDocument document = GetMainFrame()->document();
    WebElement element =
        document.getElementById(WebString::fromUTF8(element_id));
    ASSERT_FALSE(element.isNull());
    ExecuteJavaScript(
        base::StringPrintf("document.getElementById('%s').focus();",
                           element_id).c_str());
    if (available) {
      ASSERT_EQ(1u, password_generation_->messages().size());
      EXPECT_EQ(AutofillHostMsg_ShowPasswordGenerationPopup::ID,
                password_generation_->messages()[0]->type());
    } else {
      EXPECT_EQ(0u, password_generation_->messages().size());
    }
    password_generation_->clear_messages();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationAgentTest);
};

const char kSigninFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'password'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password' "
    "         autocomplete = 'off' size = 5/>"
    "  <INPUT type = 'password' id = 'second_password' size = 5/> "
    "  <INPUT type = 'text' id = 'address'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kHiddenPasswordAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'http://www.random.com/'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password'/> "
    "  <INPUT type = 'password' id = 'second_password' style='display:none'/> "
    "  <INPUT type = 'submit' value = 'LOGIN' />"
    "</FORM>";

const char kInvalidActionAccountCreationFormHTML[] =
    "<FORM name = 'blah' action = 'invalid'> "
    "  <INPUT type = 'text' id = 'username'/> "
    "  <INPUT type = 'password' id = 'first_password'/> "
    "  <INPUT type = 'password' id = 'second_password'/> "
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

TEST_F(PasswordGenerationAgentTest, DetectionTest) {
  // Don't shown the icon for non account creation forms.
  LoadHTML(kSigninFormHTML);
  ExpectPasswordGenerationAvailable("password", false);

  // We don't show the decoration yet because the feature isn't enabled.
  LoadHTML(kAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", false);

  // Pretend like We have received message indicating site is not blacklisted,
  // and we have received message indicating the form is classified as
  // ACCOUNT_CREATION_FORM form Autofill server. We should show the icon.
  LoadHTML(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(kAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", true);

  // Hidden fields are not treated differently.
  LoadHTML(kHiddenPasswordAccountCreationFormHTML);
  SetNotBlacklistedMessage(kHiddenPasswordAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(
      kHiddenPasswordAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", true);

  // This doesn't trigger because the form action is invalid.
  LoadHTML(kInvalidActionAccountCreationFormHTML);
  SetNotBlacklistedMessage(kInvalidActionAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(kInvalidActionAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", false);
}

TEST_F(PasswordGenerationAgentTest, FillTest) {
  // Make sure that we are enabled before loading HTML.
  std::string html = std::string(kAccountCreationFormHTML) +
      ChangeDetectionScript;
  LoadHTML(html.c_str());

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
  LoadHTML(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(kAccountCreationFormHTML);

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
  base::string16 edited_password = base::ASCIIToUTF16("edited_password");
  first_password_element.setValue(edited_password);
  // Cast to WebAutofillClient where textFieldDidChange() is public.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->textFieldDidChange(
      first_password_element);
  // textFieldDidChange posts a task, so we need to wait until it's been
  // processed.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(edited_password, first_password_element.value());
  EXPECT_EQ(edited_password, second_password_element.value());

  // Verify that password mirroring works correctly even when the password
  // is deleted.
  base::string16 empty_password;
  first_password_element.setValue(empty_password);
  // Cast to WebAutofillClient where textFieldDidChange() is public.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->textFieldDidChange(
      first_password_element);
  // textFieldDidChange posts a task, so we need to wait until it's been
  // processed.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(empty_password, first_password_element.value());
  EXPECT_EQ(empty_password, second_password_element.value());
}

TEST_F(PasswordGenerationAgentTest, BlacklistedTest) {
  // Did not receive not blacklisted message. Don't show password generation
  // icon.
  LoadHTML(kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(kAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", false);

  // Receive one not blacklisted message for non account creation form. Don't
  // show password generation icon.
  LoadHTML(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(kSigninFormHTML);
  SetAccountCreationFormsDetectedMessage(kAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", false);

  // Receive one not blackliste message for account creation form. Show password
  // generation icon.
  LoadHTML(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(kAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", true);

  // Receive two not blacklisted messages, one is for account creation form and
  // the other is not. Show password generation icon.
  LoadHTML(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(kSigninFormHTML);
  SetAccountCreationFormsDetectedMessage(kAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", true);
}

TEST_F(PasswordGenerationAgentTest, AccountCreationFormsDetectedTest) {
  // Did not receive account creation forms detected messege. Don't show
  // password generation icon.
  LoadHTML(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(kAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", false);

  // Receive the account creation forms detected message. Show password
  // generation icon.
  LoadHTML(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(kAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", true);
}

TEST_F(PasswordGenerationAgentTest, MaximumOfferSize) {
  base::HistogramTester histogram_tester;

  LoadHTML(kAccountCreationFormHTML);
  SetNotBlacklistedMessage(kAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(kAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", true);

  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("first_password"));
  ASSERT_FALSE(element.isNull());
  WebInputElement first_password_element = element.to<WebInputElement>();

  // Make a password just under maximum offer size.
  first_password_element.setValue(
      base::ASCIIToUTF16(
          std::string(password_generation_->kMaximumOfferSize - 1, 'a')));
  // Cast to WebAutofillClient where textFieldDidChange() is public.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->textFieldDidChange(
      first_password_element);
  // textFieldDidChange posts a task, so we need to wait until it's been
  // processed.
  base::MessageLoop::current()->RunUntilIdle();
  // There should now be a message to show the UI.
  ASSERT_EQ(1u, password_generation_->messages().size());
  EXPECT_EQ(AutofillHostMsg_ShowPasswordGenerationPopup::ID,
            password_generation_->messages()[0]->type());
  password_generation_->clear_messages();

  // Simulate a user typing a password just over maximum offer size.
  first_password_element.setValue(
      base::ASCIIToUTF16(
          std::string(password_generation_->kMaximumOfferSize + 1, 'a')));
  // Cast to WebAutofillClient where textFieldDidChange() is public.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->textFieldDidChange(
      first_password_element);
  // textFieldDidChange posts a task, so we need to wait until it's been
  // processed.
  base::MessageLoop::current()->RunUntilIdle();
  // There should now be a message to hide the UI.
  ASSERT_EQ(1u, password_generation_->messages().size());
  EXPECT_EQ(AutofillHostMsg_HidePasswordGenerationPopup::ID,
            password_generation_->messages()[0]->type());
  password_generation_->clear_messages();

  // Simulate the user deleting characters. The generation popup should be shown
  // again.
  first_password_element.setValue(
      base::ASCIIToUTF16(
          std::string(password_generation_->kMaximumOfferSize, 'a')));
  // Cast to WebAutofillClient where textFieldDidChange() is public.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->textFieldDidChange(
      first_password_element);
  // textFieldDidChange posts a task, so we need to wait until it's been
  // processed.
  base::MessageLoop::current()->RunUntilIdle();
  // There should now be a message to show the UI.
  ASSERT_EQ(1u, password_generation_->messages().size());
  EXPECT_EQ(AutofillHostMsg_ShowPasswordGenerationPopup::ID,
            password_generation_->messages()[0]->type());
  password_generation_->clear_messages();

  // Change focus. Bubble should be hidden, but that is handled by AutofilAgent,
  // so no messages are sent.
  ExecuteJavaScript("document.getElementById('username').focus();");
  EXPECT_EQ(0u, password_generation_->messages().size());
  password_generation_->clear_messages();

  // Focusing the password field will bring up the generation UI again.
  ExecuteJavaScript("document.getElementById('first_password').focus();");
  EXPECT_EQ(1u, password_generation_->messages().size());
  EXPECT_EQ(AutofillHostMsg_ShowPasswordGenerationPopup::ID,
            password_generation_->messages()[0]->type());
  password_generation_->clear_messages();

  // Loading a different page triggers UMA stat upload. Verify that only one
  // display event is sent even though
  LoadHTML(kSigninFormHTML);

  histogram_tester.ExpectBucketCount(
      "PasswordGeneration.Event",
      autofill::password_generation::GENERATION_POPUP_SHOWN,
      1);
}

TEST_F(PasswordGenerationAgentTest, DynamicFormTest) {
  LoadHTML(kSigninFormHTML);
  SetNotBlacklistedMessage(kSigninFormHTML);
  SetAccountCreationFormsDetectedMessage(kSigninFormHTML);

  ExecuteJavaScript(
      "var form = document.createElement('form');"
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
  // TODO(gcasto): I'm slighty worried about flakes in this test where
  // didAssociateFormControls() isn't called. If this turns out to be a problem
  // adding a call to OnDynamicFormsSeen(GetMainFrame()) will fix it, though
  // it will weaken the test.
  ExpectPasswordGenerationAvailable("first_password", true);
}

}  // namespace autofill
