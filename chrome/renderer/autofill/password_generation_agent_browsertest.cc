// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/test_password_generation_agent.h"
#include "components/autofill/core/common/form_data.h"
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

  virtual void TearDown() {
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
    WebInputElement target_element = element.to<WebInputElement>();
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

  // This doesn't trigger because hidden password fields are ignored.
  LoadHTML(kHiddenPasswordAccountCreationFormHTML);
  SetNotBlacklistedMessage(kHiddenPasswordAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(
      kHiddenPasswordAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", false);

  // This doesn't trigger because the form action is invalid.
  LoadHTML(kInvalidActionAccountCreationFormHTML);
  SetNotBlacklistedMessage(kInvalidActionAccountCreationFormHTML);
  SetAccountCreationFormsDetectedMessage(kInvalidActionAccountCreationFormHTML);
  ExpectPasswordGenerationAvailable("first_password", false);
}

TEST_F(PasswordGenerationAgentTest, FillTest) {
  // Make sure that we are enabled before loading HTML.
  LoadHTML(kAccountCreationFormHTML);

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

}  // namespace autofill
