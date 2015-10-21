// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/test/mock_render_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebInputElement;
using blink::WebString;

typedef ChromeRenderViewTest FormAutocompleteTest;

namespace autofill {

namespace {

// Helper function to verify the form-related messages received from the
// renderer. The same data is expected in both messages. Depending on
// |expect_submitted_message|, will verify presence of FormSubmitted message.
void VerifyReceivedRendererMessages(content::MockRenderThread* render_thread,
                                    const std::string& fname,
                                    const std::string& lname,
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
  base::Tuple<FormData, base::TimeTicks> will_submit_forms;
  AutofillHostMsg_WillSubmitForm::Read(will_submit_message, &will_submit_forms);
  ASSERT_EQ(2U, base::get<0>(will_submit_forms).fields.size());

  FormFieldData& will_submit_form_field =
      base::get<0>(will_submit_forms).fields[0];
  EXPECT_EQ(WebString("fname"), will_submit_form_field.name);
  EXPECT_EQ(WebString(base::UTF8ToUTF16(fname)), will_submit_form_field.value);
  will_submit_form_field = base::get<0>(will_submit_forms).fields[1];
  EXPECT_EQ(WebString("lname"), will_submit_form_field.name);
  EXPECT_EQ(WebString(base::UTF8ToUTF16(lname)), will_submit_form_field.value);

  if (expect_submitted_message) {
    base::Tuple<FormData> submitted_forms;
    AutofillHostMsg_FormSubmitted::Read(submitted_message, &submitted_forms);
    ASSERT_EQ(2U, base::get<0>(submitted_forms).fields.size());

    FormFieldData& submitted_field = base::get<0>(submitted_forms).fields[0];
    EXPECT_EQ(WebString("fname"), submitted_field.name);
    EXPECT_EQ(WebString(base::UTF8ToUTF16(fname)), submitted_field.value);
    submitted_field = base::get<0>(submitted_forms).fields[1];
    EXPECT_EQ(WebString("lname"), submitted_field.name);
    EXPECT_EQ(WebString(base::UTF8ToUTF16(lname)), submitted_field.value);
  }
}

// Helper function to verify that NO form-related messages are received from the
// renderer.
void VerifyNoSubmitMessagesReceived(content::MockRenderThread* render_thread) {
  // No submission messages sent.
  const IPC::Message* will_submit_message =
      render_thread->sink().GetFirstMessageMatching(
          AutofillHostMsg_WillSubmitForm::ID);
  const IPC::Message* submitted_message =
      render_thread->sink().GetFirstMessageMatching(
          AutofillHostMsg_FormSubmitted::ID);
  EXPECT_EQ(NULL, will_submit_message);
  EXPECT_EQ(NULL, submitted_message);
}

// Simulates receiving a message from the browser to fill a form.
void SimulateOnFillForm(content::MockRenderThread* render_thread,
                        autofill::AutofillAgent* autofill_agent,
                        blink::WebFrame* main_frame) {
  WebDocument document = main_frame->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("fname"));
  ASSERT_FALSE(element.isNull());

  // This call is necessary to setup the autofill agent appropriate for the
  // user selection; simulates the menu actually popping up.
  render_thread->sink().ClearMessages();
  static_cast<autofill::PageClickListener*>(autofill_agent)
      ->FormControlElementClicked(element.to<WebInputElement>(), false);

  FormData data;
  data.name = base::ASCIIToUTF16("name");
  data.origin = GURL("http://example.com/");
  data.action = GURL("http://example.com/blade.php");
  data.is_form_tag = true;  // Default value.

  FormFieldData field_data;
  field_data.name = base::ASCIIToUTF16("fname");
  field_data.value = base::ASCIIToUTF16("John");
  field_data.is_autofilled = true;
  data.fields.push_back(field_data);

  field_data.name = base::ASCIIToUTF16("lname");
  field_data.value = base::ASCIIToUTF16("Smith");
  field_data.is_autofilled = true;
  data.fields.push_back(field_data);

  AutofillMsg_FillForm msg(0, 0, data);
  static_cast<content::RenderFrameObserver*>(autofill_agent)
      ->OnMessageReceived(msg);
}

}  // end namespace

// Tests that submitting a form generates WillSubmitForm and FormSubmitted
// messages with the form fields.
TEST_F(FormAutocompleteTest, NormalFormSubmit) {
  // Load a form.
  LoadHTML("<html><form id='myForm'><input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/></form></html>");

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "Rick", "Deckard",
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
  ExecuteJavaScriptForTests(
      "var form = document.forms[0];"
      "form.onsubmit = function(event) { event.preventDefault(); };"
      "document.querySelector('input[type=submit]').click();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "Rick", "Deckard",
                                 false /* expect_submitted_message */);
}

// Tests that completing an Ajax request and having the form disappear will
// trigger submission from Autofill's point of view.
TEST_F(FormAutocompleteTest, AjaxSucceeded_NoLongerVisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->document();
  WebElement element = document.getElementById(WebString::fromUTF8("fname"));
  ASSERT_FALSE(element.isNull());
  WebInputElement fname_element = element.to<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->ajaxSucceeded();
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "Rick", "Deckard",
                                 true /* expect_submitted_message */);
}

// Tests that completing an Ajax request and having the form with a specific
// action disappear will trigger submission from Autofill's point of view, even
// if there is another form with the same data but different action on the page.
TEST_F(FormAutocompleteTest,
       AjaxSucceeded_NoLongerVisible_DifferentActionsSameData) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "<form id='myForm2' action='http://example.com/runner.php'>"
      "<input name='fname' id='fname2' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->document();
  WebElement element = document.getElementById(WebString::fromUTF8("fname"));
  ASSERT_FALSE(element.isNull());
  WebInputElement fname_element = element.to<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->ajaxSucceeded();
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "Rick", "Deckard",
                                 true /* expect_submitted_message */);
}

// Tests that completing an Ajax request and having the form with no action
// specified disappear will trigger submission from Autofill's point of view,
// even if there is still another form with no action in the page. It will
// compare field data within the forms.
// TODO(kolos) Re-enable when the implementation of IsFormVisible is on-par
// for these platforms.
#if defined(OS_MACOSX) || defined(OS_ANDROID)
#define MAYBE_NoLongerVisibleBothNoActions DISABLED_NoLongerVisibleBothNoActions
#else
#define MAYBE_NoLongerVisibleBothNoActions NoLongerVisibleBothNoActions
#endif
TEST_F(FormAutocompleteTest, MAYBE_NoLongerVisibleBothNoActions) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "<form id='myForm2'>"
      "<input name='fname' id='fname2' value='John'/>"
      "<input name='lname' value='Doe'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->document();
  WebElement element = document.getElementById(WebString::fromUTF8("fname"));
  ASSERT_FALSE(element.isNull());
  WebInputElement fname_element = element.to<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->ajaxSucceeded();
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "Rick", "Deckard",
                                 true /* expect_submitted_message */);
}

// Tests that completing an Ajax request and having the form with no action
// specified disappear will trigger submission from Autofill's point of view.
TEST_F(FormAutocompleteTest, AjaxSucceeded_NoLongerVisible_NoAction) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("fname"));
  ASSERT_FALSE(element.isNull());
  WebInputElement fname_element = element.to<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests("var element = document.getElementById('myForm');"
                            "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->ajaxSucceeded();
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "Rick", "Deckard",
                                 true /* expect_submitted_message */);
}

// Tests that completing an Ajax request but leaving a form visible will not
// trigger submission from Autofill's point of view.
TEST_F(FormAutocompleteTest, AjaxSucceeded_StillVisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8("fname"));
  ASSERT_FALSE(element.isNull());
  WebInputElement fname_element = element.to<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->ajaxSucceeded();
  ProcessPendingMessages();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(render_thread_.get());
}

// Tests that completing an Ajax request without any prior form interaction
// does not trigger form submission from Autofill's point of view.
TEST_F(FormAutocompleteTest, AjaxSucceeded_NoFormInteractionInvisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // No form interaction.

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests("var element = document.getElementById('myForm');"
                            "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing without prior user interaction.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->ajaxSucceeded();
  ProcessPendingMessages();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(render_thread_.get());
}

// Tests that completing an Ajax request after having autofilled a form,
// with the form disappearing, will trigger submission from Autofill's
// point of view.
TEST_F(FormAutocompleteTest, AjaxSucceeded_FilledFormIsInvisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname'/>"
      "<input name='lname'/></form></html>");

  // Simulate filling a form using Autofill.
  SimulateOnFillForm(render_thread_.get(), autofill_agent_, GetMainFrame());

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests("var element = document.getElementById('myForm');"
                            "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->ajaxSucceeded();
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "John", "Smith",
                                 true /* expect_submitted_message */);
}

// Tests that completing an Ajax request after having autofilled a form,
// without the form disappearing, will not trigger submission from Autofill's
// point of view.
TEST_F(FormAutocompleteTest, AjaxSucceeded_FilledFormStillVisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/></form></html>");

  // Simulate filling a form using Autofill.
  SimulateOnFillForm(render_thread_.get(), autofill_agent_, GetMainFrame());

  // Form still visible.

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->ajaxSucceeded();
  ProcessPendingMessages();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(render_thread_.get());
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
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "Rick", "Deckard",
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
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "Rick", "Deckard",
                                 true /* expect_submitted_message */);
}

// Tests that submitting a form that has been dynamically set as autocomplete
// off generates WillSubmitForm and FormSubmitted messages.
// Note: We previously did the opposite, for bug http://crbug.com/36520
TEST_F(FormAutocompleteTest, DynamicAutoCompleteOffFormSubmit) {
  LoadHTML("<html><form id='myForm'><input name='fname' value='Rick'/>"
           "<input name='lname' value='Deckard'/></form></html>");

  WebElement element =
      GetMainFrame()->document().getElementById(blink::WebString("myForm"));
  ASSERT_FALSE(element.isNull());
  blink::WebFormElement form = element.to<blink::WebFormElement>();
  EXPECT_TRUE(form.autoComplete());

  // Dynamically mark the form as autocomplete off.
  ExecuteJavaScriptForTests(
      "document.getElementById('myForm')."
      "setAttribute('autocomplete', 'off');");
  ProcessPendingMessages();
  EXPECT_FALSE(form.autoComplete());

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  ProcessPendingMessages();

  VerifyReceivedRendererMessages(render_thread_.get(), "Rick", "Deckard",
                                 true /* expect_submitted_message */);
}

}  // namespace autofill
