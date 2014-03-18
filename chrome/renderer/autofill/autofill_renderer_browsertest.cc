// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebInputElement.h"

using base::ASCIIToUTF16;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebString;

namespace autofill {

typedef Tuple5<int,
               autofill::FormData,
               autofill::FormFieldData,
               gfx::RectF,
               bool> AutofillQueryParam;

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
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  AutofillHostMsg_FormsSeen::Param params;
  AutofillHostMsg_FormsSeen::Read(message, &params);
  const std::vector<FormData>& forms = params.a;
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
}

TEST_F(ChromeRenderViewTest, EnsureNoFormSeenIfTooFewFields) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  SendContentStateImmediately();

  LoadHTML("<form method=\"POST\">"
           "  <input type=\"text\" id=\"firstname\"/>"
           "  <input type=\"text\" id=\"middlename\"/>"
           "</form>");

  // Verify that "FormsSeen" isn't sent, as there are too few fields.
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message);
  AutofillHostMsg_FormsSeen::Param params;
  AutofillHostMsg_FormsSeen::Read(message, &params);
  const std::vector<FormData>& forms = params.a;
  ASSERT_EQ(0UL, forms.size());
}

TEST_F(ChromeRenderViewTest, ShowAutofillWarning) {
  // Don't want any delay for form state sync changes.  This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  SendContentStateImmediately();

  LoadHTML("<form method=\"POST\" autocomplete=\"Off\">"
           "  <input id=\"firstname\" autocomplete=\"OFF\"/>"
           "  <input id=\"middlename\"/>"
           "  <input id=\"lastname\"/>"
           "</form>");

  // Verify that "QueryFormFieldAutofill" isn't sent prior to a user
  // interaction.
  const IPC::Message* message0 = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_QueryFormFieldAutofill::ID);
  EXPECT_EQ(static_cast<IPC::Message*>(NULL), message0);

  WebFrame* web_frame = GetMainFrame();
  WebDocument document = web_frame->document();
  WebInputElement firstname =
      document.getElementById("firstname").to<WebInputElement>();
  WebInputElement middlename =
      document.getElementById("middlename").to<WebInputElement>();

  // Simulate attempting to Autofill the form from the first element, which
  // specifies autocomplete="off".  This should still trigger an IPC which
  // shouldn't display warnings.
  autofill_agent_->InputElementClicked(firstname, true, true);
  const IPC::Message* message1 = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_QueryFormFieldAutofill::ID);
  EXPECT_NE(static_cast<IPC::Message*>(NULL), message1);

  AutofillQueryParam query_param;
  AutofillHostMsg_QueryFormFieldAutofill::Read(message1, &query_param);
  EXPECT_FALSE(query_param.e);
  render_thread_->sink().ClearMessages();

  // Simulate attempting to Autofill the form from the second element, which
  // does not specify autocomplete="off".  This should trigger an IPC that will
  // show warnings, as we *do* show warnings for elements that don't themselves
  // set autocomplete="off", but for which the form does.
  autofill_agent_->InputElementClicked(middlename, true, true);
  const IPC::Message* message2 = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_QueryFormFieldAutofill::ID);
  ASSERT_NE(static_cast<IPC::Message*>(NULL), message2);

  AutofillHostMsg_QueryFormFieldAutofill::Read(message2, &query_param);
  EXPECT_TRUE(query_param.e);
}

// Regression test for [ http://crbug.com/346010 ].
TEST_F(ChromeRenderViewTest, DontCrashWhileAssociatingForms) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  SendContentStateImmediately();

  LoadHTML("<form id='form'>"
           "<foo id='foo'>"
           "<script id='script'>"
           "document.documentElement.appendChild(foo);"
           "newDoc = document.implementation.createDocument("
           "    \"http://www.w3.org/1999/xhtml\", \"html\");"
           "foo.insertBefore(form, script);"
           "newDoc.adoptNode(foo);"
           "</script>");

  // Shouldn't crash.
}

}  // namespace autofill
