// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/common/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_provider.h"
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

namespace {

class FakeContentAutofillDriver : public mojom::AutofillDriver {
 public:
  FakeContentAutofillDriver() : called_field_change_(false) {}
  ~FakeContentAutofillDriver() override {}

  void BindRequest(mojom::AutofillDriverRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  bool called_field_change() const { return called_field_change_; }

  const std::vector<FormData>* forms() const { return forms_.get(); }

  void reset_forms() { return forms_.reset(); }

 private:
  // mojom::AutofillDriver:
  void FormsSeen(const std::vector<FormData>& forms,
                 base::TimeTicks timestamp) override {
    // FormsSeen() could be called multiple times and sometimes even with empty
    // forms array for main frame, but we're interested in only the first time
    // call.
    if (!forms_)
      forms_.reset(new std::vector<FormData>(forms));
  }

  void WillSubmitForm(const FormData& form,
                      base::TimeTicks timestamp) override {}

  void FormSubmitted(const FormData& form) override {}

  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp) override {
    called_field_change_ = true;
  }

  void QueryFormFieldAutofill(int32_t id,
                              const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override {}

  void HidePopup() override {}

  void FocusNoLongerOnForm() override {}

  void FocusOnFormField(const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box) override {}

  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override {}

  void DidPreviewAutofillFormData() override {}

  void DidEndTextFieldEditing() override {}

  void SetDataList(const std::vector<base::string16>& values,
                   const std::vector<base::string16>& labels) override {}

  // Records whether TextFieldDidChange() get called.
  bool called_field_change_;
  // Records data received via FormSeen() call.
  std::unique_ptr<std::vector<FormData>> forms_;

  mojo::BindingSet<mojom::AutofillDriver> bindings_;
};

}  // namespace

using AutofillQueryParam =
    std::tuple<int, autofill::FormData, autofill::FormFieldData, gfx::RectF>;

class AutofillRendererTest : public ChromeRenderViewTest {
 public:
  AutofillRendererTest() {}

  ~AutofillRendererTest() override {}

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
        base::Bind(&AutofillRendererTest::BindAutofillDriver,
                   base::Unretained(this)));
  }

  void BindAutofillDriver(mojo::ScopedMessagePipeHandle handle) {
    fake_driver_.BindRequest(mojom::AutofillDriverRequest(std::move(handle)));
  }

  FakeContentAutofillDriver fake_driver_;

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

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  // Verify that "FormsSeen" sends the expected number of fields.
  ASSERT_TRUE(fake_driver_.forms());
  std::vector<FormData> forms = *(fake_driver_.forms());
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(4UL, forms[0].fields.size());

  FormFieldData expected;

  expected.name = ASCIIToUTF16("firstname");
  expected.value = base::string16();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[0]);

  expected.name = ASCIIToUTF16("middlename");
  expected.value = base::string16();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[1]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = base::string16();
  expected.form_control_type = "text";
  expected.autocomplete_attribute = "off";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[2]);
  expected.autocomplete_attribute = std::string();  // reset

  expected.name = ASCIIToUTF16("state");
  expected.value = ASCIIToUTF16("?");
  expected.form_control_type = "select-one";
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[3]);

  fake_driver_.reset_forms();

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

  WaitForAutofillDidAssociateFormControl();
  ASSERT_TRUE(fake_driver_.forms());
  forms = *(fake_driver_.forms());
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(3UL, forms[0].fields.size());

  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();

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

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  // Verify that "FormsSeen" isn't sent, as there are too few fields.
  ASSERT_TRUE(fake_driver_.forms());
  const std::vector<FormData>& forms = *(fake_driver_.forms());
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

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  // Verify that "FormsSeen" sends the expected number of fields.
  ASSERT_TRUE(fake_driver_.forms());
  std::vector<FormData> forms = *(fake_driver_.forms());
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(7UL, forms[0].fields.size());

  fake_driver_.reset_forms();

  ExecuteJavaScriptForTests("AddFields()");

  WaitForAutofillDidAssociateFormControl();
  ASSERT_TRUE(fake_driver_.forms());
  forms = *(fake_driver_.forms());
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(9UL, forms[0].fields.size());

  FormFieldData expected;

  expected.name = ASCIIToUTF16("EMAIL_ADDRESS");
  expected.value.clear();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[7]);

  expected.name = ASCIIToUTF16("PHONE_HOME_WHOLE_NUMBER");
  expected.value.clear();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[8]);
}

TEST_F(AutofillRendererTest, IgnoreNonUserGestureTextFieldChanges) {
  LoadHTML("<form method='post'>"
           "  <input type='text' id='full_name'/>"
           "</form>");

  blink::WebInputElement full_name = GetMainFrame()
                                         ->GetDocument()
                                         .GetElementById("full_name")
                                         .To<blink::WebInputElement>();
  while (!full_name.Focused())
    GetMainFrame()->View()->AdvanceFocus(false);

  // Not a user gesture, so no IPC message to browser.
  DisableUserGestureSimulationForAutofill();
  ASSERT_FALSE(fake_driver_.called_field_change());
  full_name.SetValue("Alice", true);
  GetMainFrame()->AutofillClient()->TextFieldDidChange(full_name);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_driver_.called_field_change());

  // A user gesture will send a message to the browser.
  EnableUserGestureSimulationForAutofill();
  SimulateUserInputChangeForElement(&full_name, "Alice");
  ASSERT_TRUE(fake_driver_.called_field_change());
}

}  // namespace autofill
