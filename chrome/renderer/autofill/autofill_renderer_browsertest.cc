// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using base::ASCIIToUTF16;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebInputElement;
using blink::WebString;
using blink::WebURLRequest;
using blink::WebVector;

namespace autofill {

using AutofillQueryParam =
    std::tuple<int, autofill::FormData, autofill::FormFieldData, gfx::RectF>;

class AutofillRendererTest : public ChromeRenderViewTest {
 public:
  AutofillRendererTest() {}
  ~AutofillRendererTest() override {}

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillRendererTest);
};

TEST_F(AutofillRendererTest, SendForms) {
  LoadHTML("<form method='POST'>"
           "  <input type='text' id='firstname'/>"
           "  <input type='text' id='middlename'/>"
           "  <input type='text' id='lastname' autoComplete='off'/>"
           "  <input type='hidden' id='email'/>"
           "  <select id='state'/>"
           "    <option>?</option>"
           "    <option>California</option>"
           "    <option>Texas</option>"
           "  </select>"
           "</form>");

  // Verify that "FormsSeen" sends the expected number of fields.
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Param params;
  AutofillHostMsg_FormsSeen::Read(message, &params);
  std::vector<FormData> forms = std::get<0>(params);
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(4UL, forms[0].fields.size());

  FormFieldData expected;

  expected.name = ASCIIToUTF16("firstname");
  expected.value = base::string16();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[0]);

  expected.name = ASCIIToUTF16("middlename");
  expected.value = base::string16();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[1]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = base::string16();
  expected.form_control_type = "text";
  expected.autocomplete_attribute = "off";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[2]);
  expected.autocomplete_attribute = std::string();  // reset

  expected.name = ASCIIToUTF16("state");
  expected.value = ASCIIToUTF16("?");
  expected.form_control_type = "select-one";
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[3]);

  render_thread_->sink().ClearMessages();

  // Dynamically create a new form. A new message should be sent for it, but
  // not for the previous form.
  ExecuteJavaScriptForTests(
      "var newForm=document.createElement('form');"
      "newForm.id='new_testform';"
      "newForm.action='http://google.com';"
      "newForm.method='post';"
      "var newFirstname=document.createElement('input');"
      "newFirstname.setAttribute('type', 'text');"
      "newFirstname.setAttribute('id', 'second_firstname');"
      "newFirstname.value = 'Bob';"
      "var newLastname=document.createElement('input');"
      "newLastname.setAttribute('type', 'text');"
      "newLastname.setAttribute('id', 'second_lastname');"
      "newLastname.value = 'Hope';"
      "var newEmail=document.createElement('input');"
      "newEmail.setAttribute('type', 'text');"
      "newEmail.setAttribute('id', 'second_email');"
      "newEmail.value = 'bobhope@example.com';"
      "newForm.appendChild(newFirstname);"
      "newForm.appendChild(newLastname);"
      "newForm.appendChild(newEmail);"
      "document.body.appendChild(newForm);");
  msg_loop_.RunUntilIdle();

  message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Read(message, &params);
  forms = std::get<0>(params);
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(3UL, forms[0].fields.size());

  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("second_firstname");
  expected.value = ASCIIToUTF16("Bob");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[0]);

  expected.name = ASCIIToUTF16("second_lastname");
  expected.value = ASCIIToUTF16("Hope");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[1]);

  expected.name = ASCIIToUTF16("second_email");
  expected.value = ASCIIToUTF16("bobhope@example.com");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[2]);
}

TEST_F(AutofillRendererTest, EnsureNoFormSeenIfTooFewFields) {
  LoadHTML("<form method='POST'>"
           "  <input type='text' id='firstname'/>"
           "  <input type='text' id='middlename'/>"
           "</form>");

  // Verify that "FormsSeen" isn't sent, as there are too few fields.
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Param params;
  AutofillHostMsg_FormsSeen::Read(message, &params);
  const std::vector<FormData>& forms = std::get<0>(params);
  ASSERT_EQ(0UL, forms.size());
}

// Regression test for [ http://crbug.com/346010 ].
TEST_F(AutofillRendererTest, DontCrashWhileAssociatingForms) {
  LoadHTML("<form id='form'>"
           "<foo id='foo'>"
           "<script id='script'>"
           "document.documentElement.appendChild(foo);"
           "newDoc = document.implementation.createDocument("
           "    'http://www.w3.org/1999/xhtml', 'html');"
           "foo.insertBefore(form, script);"
           "newDoc.adoptNode(foo);"
           "</script>");

  // Shouldn't crash.
}

TEST_F(AutofillRendererTest, DynamicallyAddedUnownedFormElements) {
  std::string html_data;
  base::FilePath test_path = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("autofill")),
      base::FilePath(FILE_PATH_LITERAL("autofill_noform_dynamic.html")));
  ASSERT_TRUE(base::ReadFileToString(test_path, &html_data));
  LoadHTML(html_data.c_str());

  // Verify that "FormsSeen" sends the expected number of fields.
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Param params;
  AutofillHostMsg_FormsSeen::Read(message, &params);
  std::vector<FormData> forms = std::get<0>(params);
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(7UL, forms[0].fields.size());

  render_thread_->sink().ClearMessages();

  ExecuteJavaScriptForTests("AddFields()");
  msg_loop_.RunUntilIdle();

  message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Read(message, &params);
  forms = std::get<0>(params);
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(9UL, forms[0].fields.size());

  FormFieldData expected;

  expected.name = ASCIIToUTF16("EMAIL_ADDRESS");
  expected.value.clear();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[7]);

  expected.name = ASCIIToUTF16("PHONE_HOME_WHOLE_NUMBER");
  expected.value.clear();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[8]);
}

TEST_F(AutofillRendererTest, IgnoreNonUserGestureTextFieldChanges) {
  LoadHTML("<form method='post'>"
           "  <input type='text' id='full_name'/>"
           "</form>");

  blink::WebInputElement full_name =
      GetMainFrame()->document().getElementById("full_name")
          .to<blink::WebInputElement>();
  while (!full_name.focused())
    GetMainFrame()->view()->advanceFocus(false);

  // Not a user gesture, so no IPC message to browser.
  DisableUserGestureSimulationForAutofill();
  full_name.setValue("Alice", true);
  GetMainFrame()->autofillClient()->textFieldDidChange(full_name);
  base::MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(nullptr, render_thread_->sink().GetFirstMessageMatching(
                         AutofillHostMsg_TextFieldDidChange::ID));

  // A user gesture will send a message to the browser.
  EnableUserGestureSimulationForAutofill();
  SimulateUserInputChangeForElement(&full_name, "Alice");
  ASSERT_NE(nullptr, render_thread_->sink().GetFirstMessageMatching(
                         AutofillHostMsg_TextFieldDidChange::ID));
}

}  // namespace autofill
