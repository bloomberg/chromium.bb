// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/test/mock_render_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebString;
using blink::WebURLError;

typedef ChromeRenderViewTest FormAutocompleteTest;

namespace autofill {

namespace {

// Helper function to verify the form-related messages received from the
// renderer. The same data is expected in both messages. Depending on
// |expect_submitted_message|, will verify presence of FormSubmitted message.
void VerifyReceivedRendererMessages(content::MockRenderThread* render_thread,
                                    bool expect_submitted_message) {
  const IPC::Message* will_submit_message =
      render_thread->sink().GetFirstMessageMatching(
          AutofillHostMsg_WillSubmitForm::ID);
  const IPC::Message* submitted_message =
      render_thread->sink().GetFirstMessageMatching(
          AutofillHostMsg_FormSubmitted::ID);
  ASSERT_TRUE(will_submit_message != NULL);
  ASSERT_EQ(expect_submitted_message, submitted_message != NULL);

  // The tuple also includes a timestamp, which is ignored.
  Tuple<FormData, base::TimeTicks> will_submit_forms;
  AutofillHostMsg_WillSubmitForm::Read(will_submit_message, &will_submit_forms);
  ASSERT_EQ(2U, get<0>(will_submit_forms).fields.size());

  FormFieldData& will_submit_form_field = get<0>(will_submit_forms).fields[0];
  EXPECT_EQ(WebString("fname"), will_submit_form_field.name);
  EXPECT_EQ(WebString("Rick"), will_submit_form_field.value);
  will_submit_form_field = get<0>(will_submit_forms).fields[1];
  EXPECT_EQ(WebString("lname"), will_submit_form_field.name);
  EXPECT_EQ(WebString("Deckard"), will_submit_form_field.value);

  if (expect_submitted_message) {
    Tuple<FormData> submitted_forms;
    AutofillHostMsg_FormSubmitted::Read(submitted_message, &submitted_forms);
    ASSERT_EQ(2U, get<0>(submitted_forms).fields.size());

    FormFieldData& submitted_field = get<0>(submitted_forms).fields[0];
    EXPECT_EQ(WebString("fname"), submitted_field.name);
    EXPECT_EQ(WebString("Rick"), submitted_field.value);
    submitted_field = get<0>(submitted_forms).fields[1];
    EXPECT_EQ(WebString("lname"), submitted_field.name);
    EXPECT_EQ(WebString("Deckard"), submitted_field.value);
  }
}

}  // end namespace

// Tests that submitting a form generates WillSubmitForm and FormSubmitted
// messages with the form fields.
TEST_F(FormAutocompleteTest, NormalFormSubmit) {
  // Load a form.
  LoadHTML("<html><form id='myForm'><input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/></form></html>");

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(),
                                 true /* expect_submitted_message */);
}

// Tests that submitting a form that prevents the submit event from propagating
// will only send the WillSubmitForm message.
TEST_F(FormAutocompleteTest, SubmitEventPrevented) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'><input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "</html>");

  // Submit the form.
  ExecuteJavaScript(
      "var form = document.forms[0];"
      "form.onsubmit = function(event) { event.preventDefault(); };"
      "document.querySelector('input[type=submit]').click();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(),
                                 false /* expect_submitted_message */);
}

// Tests that submitting a form that has autocomplete="off" generates
// WillSubmitForm and FormSubmitted messages.
TEST_F(FormAutocompleteTest, AutoCompleteOffFormSubmit) {
  // Load a form.
  LoadHTML("<html><form id='myForm' autocomplete='off'>"
           "<input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/>"
           "</form></html>");

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(),
                                 true /* expect_submitted_message */);
}

// Tests that fields with autocomplete off are submitted.
TEST_F(FormAutocompleteTest, AutoCompleteOffInputSubmit) {
  // Load a form.
  LoadHTML("<html><form id='myForm'>"
           "<input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard' autocomplete='off'/>"
           "</form></html>");

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(),
                                 true /* expect_submitted_message */);
}

// Tests that submitting a form that has been dynamically set as autocomplete
// off generates WillSubmitForm and FormSubmitted messages.
// Note: We previously did the opposite, for bug http://crbug.com/36520
TEST_F(FormAutocompleteTest, DynamicAutoCompleteOffFormSubmit) {
  LoadHTML("<html><form id='myForm'><input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/></form></html>");

  blink::WebElement element =
      GetMainFrame()->document().getElementById(blink::WebString("myForm"));
  ASSERT_FALSE(element.isNull());
  blink::WebFormElement form = element.to<blink::WebFormElement>();
  EXPECT_TRUE(form.autoComplete());

  // Dynamically mark the form as autocomplete off.
  ExecuteJavaScript("document.getElementById('myForm')."
                    "setAttribute('autocomplete', 'off');");
  ProcessPendingMessages();
  EXPECT_FALSE(form.autoComplete());

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(),
                                 true /* expect_submitted_message */);
}

}  // namespace autofill
