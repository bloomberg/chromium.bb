// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_provider.h"
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

namespace autofill {

namespace {

class FakeContentAutofillDriver : public mojom::AutofillDriver {
 public:
  FakeContentAutofillDriver() : did_unfocus_form_(false) {}

  ~FakeContentAutofillDriver() override {}

  void BindRequest(mojom::AutofillDriverRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  bool did_unfocus_form() const { return did_unfocus_form_; }

  const FormData* form_will_submit() const { return form_will_submit_.get(); }

  const FormData* form_submitted() const { return form_submitted_.get(); }

 private:
  // mojom::AutofillDriver:
  void FormsSeen(const std::vector<FormData>& forms,
                 base::TimeTicks timestamp) override {}

  void WillSubmitForm(const FormData& form,
                      base::TimeTicks timestamp) override {
    form_will_submit_.reset(new FormData(form));
  }

  void FormSubmitted(const FormData& form) override {
    form_submitted_.reset(new FormData(form));
  }

  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          base::TimeTicks timestamp) override {}

  void QueryFormFieldAutofill(int32_t id,
                              const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override {}

  void HidePopup() override {}

  void FocusNoLongerOnForm() override { did_unfocus_form_ = true; }

  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override {}

  void DidPreviewAutofillFormData() override {}

  void DidEndTextFieldEditing() override {}

  void SetDataList(const std::vector<base::string16>& values,
                   const std::vector<base::string16>& labels) override {}

  // Records whether FocusNoLongerOnForm() get called.
  bool did_unfocus_form_;
  // Records the form data received via WillSubmitForm() call.
  std::unique_ptr<FormData> form_will_submit_;
  // Records the form data received via FormSubmitted() call.
  std::unique_ptr<FormData> form_submitted_;

  mojo::BindingSet<mojom::AutofillDriver> bindings_;
};

// Helper function to verify the form-related messages received from the
// renderer. The same data is expected in both messages. Depending on
// |expect_submitted_message|, will verify presence of FormSubmitted message.
void VerifyReceivedRendererMessages(
    const FakeContentAutofillDriver& fake_driver,
    const std::string& fname,
    const std::string& lname,
    bool expect_submitted_message) {
  ASSERT_TRUE(fake_driver.form_will_submit());
  ASSERT_EQ(expect_submitted_message, fake_driver.form_submitted() != nullptr);

  // The tuple also includes a timestamp, which is ignored.
  const FormData& will_submit_form = *(fake_driver.form_will_submit());
  ASSERT_LE(2U, will_submit_form.fields.size());

  EXPECT_EQ(base::ASCIIToUTF16("fname"), will_submit_form.fields[0].name);
  EXPECT_EQ(base::UTF8ToUTF16(fname), will_submit_form.fields[0].value);
  EXPECT_EQ(base::ASCIIToUTF16("lname"), will_submit_form.fields[1].name);
  EXPECT_EQ(base::UTF8ToUTF16(lname), will_submit_form.fields[1].value);

  if (expect_submitted_message) {
    const FormData& submitted_form = *(fake_driver.form_submitted());
    ASSERT_LE(2U, submitted_form.fields.size());

    EXPECT_EQ(base::ASCIIToUTF16("fname"), submitted_form.fields[0].name);
    EXPECT_EQ(base::UTF8ToUTF16(fname), submitted_form.fields[0].value);
    EXPECT_EQ(base::ASCIIToUTF16("lname"), submitted_form.fields[1].name);
    EXPECT_EQ(base::UTF8ToUTF16(lname), submitted_form.fields[1].value);
  }
}

// Helper function to verify that NO form-related messages are received from the
// renderer.
void VerifyNoSubmitMessagesReceived(
    const FakeContentAutofillDriver& fake_driver) {
  // No submission messages sent.
  EXPECT_EQ(nullptr, fake_driver.form_will_submit());
  EXPECT_EQ(nullptr, fake_driver.form_submitted());
}

// Simulates receiving a message from the browser to fill a form.
void SimulateOnFillForm(autofill::AutofillAgent* autofill_agent,
                        blink::WebFrame* main_frame) {
  WebDocument document = main_frame->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());

  // This call is necessary to setup the autofill agent appropriate for the
  // user selection; simulates the menu actually popping up.
  static_cast<autofill::PageClickListener*>(autofill_agent)
      ->FormControlElementClicked(element.To<WebInputElement>(), false);

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

  autofill_agent->FillForm(0, data);
}

}  // end namespace

class FormAutocompleteTest : public ChromeRenderViewTest {
 public:
  FormAutocompleteTest() {}
  ~FormAutocompleteTest() override {}

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // We only use the fake driver for main frame
    // because our test cases only involve the main frame.
    service_manager::InterfaceProvider* remote_interfaces =
        view_->GetMainRenderFrame()->GetRemoteInterfaces();
    service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
    test_api.SetBinderForName(
        mojom::AutofillDriver::Name_,
        base::Bind(&FormAutocompleteTest::BindAutofillDriver,
                   base::Unretained(this)));
  }

  void BindAutofillDriver(mojo::ScopedMessagePipeHandle handle) {
    fake_driver_.BindRequest(mojom::AutofillDriverRequest(std::move(handle)));
  }

  FakeContentAutofillDriver fake_driver_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormAutocompleteTest);
};

// Tests that submitting a form generates WillSubmitForm and FormSubmitted
// messages with the form fields.
TEST_F(FormAutocompleteTest, NormalFormSubmit) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='about:blank'>"
      "<input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/></form></html>");

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
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
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
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
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
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
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
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
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
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
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests("var element = document.getElementById('myForm');"
                            "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
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
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(fake_driver_);
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
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(fake_driver_);
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
  SimulateOnFillForm(autofill_agent_, GetMainFrame());

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests("var element = document.getElementById('myForm');"
                            "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "John", "Smith",
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
  SimulateOnFillForm(autofill_agent_, GetMainFrame());

  // Form still visible.

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(fake_driver_);
}

// Tests that completing an Ajax request without a form present will still
// trigger submission, if all the inputs the user has modified disappear.
TEST_F(FormAutocompleteTest, AjaxSucceeded_FormlessElements) {
  // Load a "form." Note that kRequiredFieldsForUpload fields are required
  // for the formless logic to trigger, so we add a throwaway third field.
  LoadHTML(
      "<head><title>Checkout</title></head>"
      "<input type='text' name='fname' id='fname'/>"
      "<input type='text' name='lname' value='Puckett'/>"
      "<input type='number' name='number' value='34'/>");

  // Simulate user input.
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Kirby"));

  // Remove element from view.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('fname');"
      "element.style.display = 'none';");

  // Simulate AJAX request.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Kirby", "Puckett",
                                 /* expect_submitted_message = */ true);
}

// Unit test for CollectFormlessElements.
TEST_F(FormAutocompleteTest, CollectFormlessElements) {
  LoadHTML(
    "<html><title>Checkout</title></head>"
    "<input type='text' name='text_input'/>"
    "<input type='checkbox' name='check_input'/>"
    "<input type='number' name='number_input'/>"
    "<select name='select_input'/>"
    "  <option value='option_1'></option>"
    "  <option value='option_2'></option>"
    "</select>"
    "<form><input type='text' name='excluded'/></form>"
    "</html>");

  FormData result;
  autofill_agent_->CollectFormlessElements(&result);

  // Asserting size 4 also ensures that 'excluded' field inside <form> is not
  // collected.
  ASSERT_EQ(4U, result.fields.size());
  EXPECT_EQ(base::ASCIIToUTF16("text_input"), result.fields[0].name);
  EXPECT_EQ(base::ASCIIToUTF16("check_input"), result.fields[1].name);
  EXPECT_EQ(base::ASCIIToUTF16("number_input"), result.fields[2].name);
  EXPECT_EQ(base::ASCIIToUTF16("select_input"), result.fields[3].name);
}

// Test that a FocusNoLongerOnForm message is sent if focus goes from an
// interacted form to an element outside the form.
TEST_F(FormAutocompleteTest,
       InteractedFormNoLongerFocused_FocusNoLongerOnForm) {
  // Load a form.
  LoadHTML(
      "<html><input type='text' id='different'/>"
      "<form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  ASSERT_FALSE(fake_driver_.did_unfocus_form());

  // Change focus to a different node outside the form.
  WebElement different =
      document.GetElementById(WebString::FromUTF8("different"));
  SetFocused(different);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_driver_.did_unfocus_form());
}

// Test that a FocusNoLongerOnForm message is sent if focus goes from one
// interacted form to another.
TEST_F(FormAutocompleteTest, InteractingInDifferentForms_FocusNoLongerOnForm) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "<form id='myForm2' action='http://example.com/runner.php'>"
      "<input name='fname' id='fname2' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input in the first form so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  ASSERT_FALSE(fake_driver_.did_unfocus_form());

  // Simulate user input in the second form so that a "no longer focused"
  // message is sent for the first form.
  document = GetMainFrame()->GetDocument();
  element = document.GetElementById(WebString::FromUTF8("fname2"));
  ASSERT_FALSE(element.IsNull());
  fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("John"));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_driver_.did_unfocus_form());
}

// Tests that submitting a form that has autocomplete="off" generates
// WillSubmitForm and FormSubmitted messages.
TEST_F(FormAutocompleteTest, AutoCompleteOffFormSubmit) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' autocomplete='off' action='about:blank'>"
      "<input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/>"
      "</form></html>");

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 true /* expect_submitted_message */);
}

// Tests that fields with autocomplete off are submitted.
TEST_F(FormAutocompleteTest, AutoCompleteOffInputSubmit) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='about:blank'>"
      "<input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard' autocomplete='off'/>"
      "</form></html>");

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 true /* expect_submitted_message */);
}

// Tests that submitting a form that has been dynamically set as autocomplete
// off generates WillSubmitForm and FormSubmitted messages.
// Note: We previously did the opposite, for bug http://crbug.com/36520
TEST_F(FormAutocompleteTest, DynamicAutoCompleteOffFormSubmit) {
  LoadHTML(
      "<html><form id='myForm' action='about:blank'>"
      "<input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/></form></html>");

  WebElement element =
      GetMainFrame()->GetDocument().GetElementById(blink::WebString("myForm"));
  ASSERT_FALSE(element.IsNull());
  blink::WebFormElement form = element.To<blink::WebFormElement>();
  EXPECT_TRUE(form.AutoComplete());

  // Dynamically mark the form as autocomplete off.
  ExecuteJavaScriptForTests(
      "document.getElementById('myForm')."
      "setAttribute('autocomplete', 'off');");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(form.AutoComplete());

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 true /* expect_submitted_message */);
}

}  // namespace autofill
