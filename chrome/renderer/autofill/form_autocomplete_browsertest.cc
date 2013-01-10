// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/form_data.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "webkit/glue/web_io_operators.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURLError;

typedef ChromeRenderViewTest FormAutocompleteTest;

// Tests that submitting a form generates a FormSubmitted message
// with the form fields.
TEST_F(FormAutocompleteTest, NormalFormSubmit) {
  // Load a form.
  LoadHTML("<html><form id='myForm'><input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/></form></html>");

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormSubmitted::ID);
  ASSERT_TRUE(message != NULL);

  // The tuple also includes a timestamp, which is ignored.
  Tuple2<FormData, base::TimeTicks> forms;
  AutofillHostMsg_FormSubmitted::Read(message, &forms);
  ASSERT_EQ(2U, forms.a.fields.size());

  FormFieldData& form_field = forms.a.fields[0];
  EXPECT_EQ(WebString("fname"), form_field.name);
  EXPECT_EQ(WebString("Rick"), form_field.value);

  form_field = forms.a.fields[1];
  EXPECT_EQ(WebString("lname"), form_field.name);
  EXPECT_EQ(WebString("Deckard"), form_field.value);
}

// Tests that submitting a form that has autocomplete="off" does not generate a
// FormSubmitted message.
TEST_F(FormAutocompleteTest, AutoCompleteOffFormSubmit) {
  // Load a form.
  LoadHTML("<html><form id='myForm' autocomplete='off'>"
           "<input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/>"
           "</form></html>");

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  // No FormSubmitted message should have been sent.
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormSubmitted::ID));
}

// Tests that fields with autocomplete off are not submitted.
TEST_F(FormAutocompleteTest, AutoCompleteOffInputSubmit) {
  // Load a form.
  LoadHTML("<html><form id='myForm'>"
           "<input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard' autocomplete='off'/>"
           "</form></html>");

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  // No FormSubmitted message should have been sent.
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormSubmitted::ID);
  ASSERT_TRUE(message != NULL);

  // The tuple also includes a timestamp, which is ignored.
  Tuple2<FormData, base::TimeTicks> forms;
  AutofillHostMsg_FormSubmitted::Read(message, &forms);
  ASSERT_EQ(1U, forms.a.fields.size());

  FormFieldData& form_field = forms.a.fields[0];
  EXPECT_EQ(WebString("fname"), form_field.name);
  EXPECT_EQ(WebString("Rick"), form_field.value);
}

// Tests that submitting a form that has been dynamically set as autocomplete
// off does not generate a FormSubmitted message.
// http://crbug.com/36520
TEST_F(FormAutocompleteTest, DynamicAutoCompleteOffFormSubmit) {
  LoadHTML("<html><form id='myForm'><input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/></form></html>");

  WebKit::WebElement element =
      GetMainFrame()->document().getElementById(WebKit::WebString("myForm"));
  ASSERT_FALSE(element.isNull());
  WebKit::WebFormElement form = element.to<WebKit::WebFormElement>();
  EXPECT_TRUE(form.autoComplete());

  // Dynamically mark the form as autocomplete off.
  ExecuteJavaScript("document.getElementById('myForm')."
                    "setAttribute('autocomplete', 'off');");
  ProcessPendingMessages();
  EXPECT_FALSE(form.autoComplete());

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  // No FormSubmitted message should have been sent.
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormSubmitted::ID));
}
