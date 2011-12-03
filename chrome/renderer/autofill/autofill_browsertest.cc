// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebString;
using webkit_glue::FormData;
using webkit_glue::FormField;

namespace autofill {

TEST_F(ChromeRenderViewTest, SendForms) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  SendContentStateImmediately();

  LoadHTML("<form method=\"POST\">"
           "  <input type=\"text\" id=\"firstname\"/>"
           "  <input type=\"text\" id=\"middlename\"/>"
           "  <input type=\"text\" id=\"lastname\" autoComplete=\"off\"/>"
           "  <input type=\"hidden\" id=\"email\"/>"
           "  <select id=\"state\"/>"
           "    <option>?</option>"
           "    <option>California</option>"
           "    <option>Texas</option>"
           "  </select>"
           "</form>");

  // Verify that "FormsSeen" sends the expected number of fields.
  ProcessPendingMessages();
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  AutofillHostMsg_FormsSeen::Param params;
  AutofillHostMsg_FormsSeen::Read(message, &params);
  const std::vector<FormData>& forms = params.a;
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(4UL, forms[0].fields.size());

  FormField expected;

  expected.name = ASCIIToUTF16("firstname");
  expected.value = string16();
  expected.form_control_type = ASCIIToUTF16("text");
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_EQUALS(expected, forms[0].fields[0]);

  expected.name = ASCIIToUTF16("middlename");
  expected.value = string16();
  expected.form_control_type = ASCIIToUTF16("text");
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_EQUALS(expected, forms[0].fields[1]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = string16();
  expected.form_control_type = ASCIIToUTF16("text");
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_EQUALS(expected, forms[0].fields[2]);

  expected.name = ASCIIToUTF16("state");
  expected.value = ASCIIToUTF16("?");
  expected.form_control_type = ASCIIToUTF16("select-one");
  expected.max_length = 0;
  EXPECT_FORM_FIELD_EQUALS(expected, forms[0].fields[3]);

  // Verify that |didAcceptAutofillSuggestion()| sends the expected number of
  // fields.
  WebFrame* web_frame = GetMainFrame();
  WebDocument document = web_frame->document();
  WebInputElement firstname =
      document.getElementById("firstname").to<WebInputElement>();

  // Accept suggestion that contains a label.  Labeled items indicate Autofill
  // as opposed to Autocomplete.  We're testing this distinction below with
  // the |AutofillHostMsg_FillAutofillFormData::ID| message.
  autofill_agent_->didAcceptAutofillSuggestion(
      firstname,
      WebKit::WebString::fromUTF8("Johnny"),
      WebKit::WebString::fromUTF8("Home"),
      1,
      -1);

  ProcessPendingMessages();
  const IPC::Message* message2 =
      render_thread_->sink().GetUniqueMessageMatching(
          AutofillHostMsg_FillAutofillFormData::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message2);
  AutofillHostMsg_FillAutofillFormData::Param params2;
  AutofillHostMsg_FillAutofillFormData::Read(message2, &params2);
  const FormData& form2 = params2.b;
  ASSERT_EQ(3UL, form2.fields.size());

  expected.name = ASCIIToUTF16("firstname");
  expected.value = string16();
  expected.form_control_type = ASCIIToUTF16("text");
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_EQUALS(expected, form2.fields[0]);

  expected.name = ASCIIToUTF16("middlename");
  expected.value = string16();
  expected.form_control_type = ASCIIToUTF16("text");
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_EQUALS(expected, form2.fields[1]);

  expected.name = ASCIIToUTF16("state");
  expected.value = ASCIIToUTF16("?");
  expected.form_control_type = ASCIIToUTF16("select-one");
  expected.max_length = 0;
  EXPECT_FORM_FIELD_EQUALS(expected, form2.fields[2]);
}

TEST_F(ChromeRenderViewTest, FillFormElement) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  SendContentStateImmediately();

  LoadHTML("<form method=\"POST\">"
           "  <input type=\"text\" id=\"firstname\"/>"
           "  <input type=\"text\" id=\"middlename\"/>"
           "</form>");

  // Verify that "FormsSeen" isn't sent, as there are too few fields.
  ProcessPendingMessages();
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_EQ(static_cast<IPC::Message*>(NULL), message);

  // Verify that |didAcceptAutofillSuggestion()| sets the value of the expected
  // field.
  WebFrame* web_frame = GetMainFrame();
  WebDocument document = web_frame->document();
  WebInputElement firstname =
      document.getElementById("firstname").to<WebInputElement>();
  WebInputElement middlename =
      document.getElementById("middlename").to<WebInputElement>();
  middlename.setAutofilled(true);

  // Accept a suggestion in a form that has been auto-filled.  This triggers
  // the direct filling of the firstname element with value parameter.
  autofill_agent_->didAcceptAutofillSuggestion(firstname,
                                               WebString::fromUTF8("David"),
                                               WebString(),
                                               0,
                                               0);

  ProcessPendingMessages();
  const IPC::Message* message2 =
      render_thread_->sink().GetUniqueMessageMatching(
          AutofillHostMsg_FillAutofillFormData::ID);

  // No message should be sent in this case.  |firstname| is filled directly.
  ASSERT_EQ(static_cast<IPC::Message*>(NULL), message2);
  EXPECT_EQ(firstname.value(), WebKit::WebString::fromUTF8("David"));
}

}  // namespace autofill
