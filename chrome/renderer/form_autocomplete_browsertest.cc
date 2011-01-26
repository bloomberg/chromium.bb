// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/autofill_messages.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/web_io_operators.h"

using webkit_glue::FormData;
using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebTextDirection;
using WebKit::WebURLError;

typedef RenderViewTest FormAutocompleteTest;

// Tests that submitting a form generates a FormSubmitted message
// with the form fields.
TEST_F(FormAutocompleteTest, NormalFormSubmit) {
  // Load a form.
  LoadHTML("<html><form id='myForm'><input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/></form></html>");

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  const IPC::Message* message = render_thread_.sink().GetFirstMessageMatching(
      AutoFillHostMsg_FormSubmitted::ID);
  ASSERT_TRUE(message != NULL);

  Tuple1<FormData> forms;
  AutoFillHostMsg_FormSubmitted::Read(message, &forms);
  ASSERT_EQ(2U, forms.a.fields.size());

  webkit_glue::FormField& form_field = forms.a.fields[0];
  EXPECT_EQ(WebString("fname"), form_field.name());
  EXPECT_EQ(WebString("Rick"), form_field.value());

  form_field = forms.a.fields[1];
  EXPECT_EQ(WebString("lname"), form_field.name());
  EXPECT_EQ(WebString("Deckard"), form_field.value());
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
  EXPECT_FALSE(render_thread_.sink().GetFirstMessageMatching(
      AutoFillHostMsg_FormSubmitted::ID));
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
  const IPC::Message* message = render_thread_.sink().GetFirstMessageMatching(
      AutoFillHostMsg_FormSubmitted::ID);
  ASSERT_TRUE(message != NULL);

  Tuple1<FormData> forms;
  AutoFillHostMsg_FormSubmitted::Read(message, &forms);
  ASSERT_EQ(1U, forms.a.fields.size());

  webkit_glue::FormField& form_field = forms.a.fields[0];
  EXPECT_EQ(WebString("fname"), form_field.name());
  EXPECT_EQ(WebString("Rick"), form_field.value());
}

// Tests that submitting a form that has been dynamically set as autocomplete
// off does not generate a FormSubmitted message.
// http://crbug.com/36520
// TODO(jcampan): Waiting on WebKit bug 35823.
TEST_F(FormAutocompleteTest, FAILS_DynamicAutoCompleteOffFormSubmit) {
  LoadHTML("<html><form id='myForm'><input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/></form></html>");

  WebKit::WebElement element =
      GetMainFrame()->document().getElementById(WebKit::WebString("myForm"));
  ASSERT_FALSE(element.isNull());
  WebKit::WebFormElement form = element.to<WebKit::WebFormElement>();
  EXPECT_TRUE(form.autoComplete());

  // Dynamically mark the form as autocomplete off.
  ExecuteJavaScript("document.getElementById('myForm').autocomplete='off';");
  ProcessPendingMessages();
  EXPECT_FALSE(form.autoComplete());

  // Submit the form.
  ExecuteJavaScript("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  // No FormSubmitted message should have been sent.
  EXPECT_FALSE(render_thread_.sink().GetFirstMessageMatching(
      AutoFillHostMsg_FormSubmitted::ID));
}
