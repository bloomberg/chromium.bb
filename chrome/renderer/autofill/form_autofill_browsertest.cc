// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/format_macros.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/autofill/web_element_descriptor.h"
#include "chrome/common/form_data.h"
#include "chrome/renderer/autofill/form_autofill_util.h"
#include "chrome/renderer/autofill/form_cache.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSelectElement.h"
#include "webkit/glue/web_io_operators.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebSelectElement;
using WebKit::WebNode;
using WebKit::WebString;
using WebKit::WebVector;

using autofill::ClearPreviewedFormWithElement;
using autofill::ClickElement;
using autofill::FillForm;
using autofill::FindFormAndFieldForInputElement;
using autofill::FormWithElementIsAutofilled;
using autofill::FormCache;
using autofill::PreviewForm;
using autofill::WebFormControlElementToFormField;

class FormAutofillTest : public ChromeRenderViewTest {
 public:
  FormAutofillTest() : ChromeRenderViewTest() {}
  virtual ~FormAutofillTest() {}

  void ExpectLabels(const char* html,
                    const std::vector<string16>& labels,
                    const std::vector<string16>& names,
                    const std::vector<string16>& values) {
    std::vector<std::string> control_types(labels.size(), "text");
    ExpectLabelsAndTypes(html, labels, names, values, control_types);
  }

  void ExpectLabelsAndTypes(const char* html,
                            const std::vector<string16>& labels,
                            const std::vector<string16>& names,
                            const std::vector<string16>& values,
                            const std::vector<std::string>& control_types) {
    ASSERT_EQ(labels.size(), names.size());
    ASSERT_EQ(labels.size(), values.size());
    ASSERT_EQ(labels.size(), control_types.size());

    LoadHTML(html);

    WebFrame* web_frame = GetMainFrame();
    ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

    FormCache form_cache;
    std::vector<FormData> forms;
    form_cache.ExtractForms(*web_frame, &forms);
    ASSERT_EQ(1U, forms.size());

    const FormData& form = forms[0];
    EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
    EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
    EXPECT_EQ(GURL("http://cnn.com"), form.action);

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(labels.size(), fields.size());
    for (size_t i = 0; i < labels.size(); ++i) {
      int max_length = control_types[i] == "text" ?
                       WebInputElement::defaultMaxLength() : 0;
      FormFieldData expected;
      expected.label = labels[i];
      expected.name = names[i];
      expected.value = values[i];
      expected.form_control_type = control_types[i];
      expected.max_length = max_length;
      SCOPED_TRACE(StringPrintf("i: %" PRIuS, i));
      EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[i]);
    }
  }

  void ExpectJohnSmithLabels(const char* html) {
    std::vector<string16> labels, names, values;

    labels.push_back(ASCIIToUTF16("First name:"));
    names.push_back(ASCIIToUTF16("firstname"));
    values.push_back(ASCIIToUTF16("John"));

    labels.push_back(ASCIIToUTF16("Last name:"));
    names.push_back(ASCIIToUTF16("lastname"));
    values.push_back(ASCIIToUTF16("Smith"));

    labels.push_back(ASCIIToUTF16("Email:"));
    names.push_back(ASCIIToUTF16("email"));
    values.push_back(ASCIIToUTF16("john@example.com"));

    ExpectLabels(html, labels, names, values);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FormAutofillTest);
};

// We should be able to extract a normal text field.
TEST_F(FormAutofillTest, WebFormControlElementToFormField) {
  LoadHTML("<INPUT type=\"text\" id=\"element\" value=\"value\"/>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebFormControlElement element = web_element.to<WebFormControlElement>();
  FormFieldData result1;
  WebFormControlElementToFormField(element, autofill::EXTRACT_NONE, &result1);

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("element");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result1);

  FormFieldData result2;
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result2);

  expected.name = ASCIIToUTF16("element");
  expected.value = ASCIIToUTF16("value");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result2);
}

// We should be able to extract a text field with autocomplete="off".
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutocompleteOff) {
  LoadHTML("<INPUT type=\"text\" id=\"element\" value=\"value\""
           "       autocomplete=\"off\"/>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebFormControlElement element = web_element.to<WebFormControlElement>();
  FormFieldData result;
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result);

  FormFieldData expected;
  expected.name = ASCIIToUTF16("element");
  expected.value = ASCIIToUTF16("value");
  expected.form_control_type = "text";
  expected.autocomplete_attribute = "off";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a text field with maxlength specified.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldMaxLength) {
  LoadHTML("<INPUT type=\"text\" id=\"element\" value=\"value\""
           "       maxlength=\"5\"/>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebFormControlElement element = web_element.to<WebFormControlElement>();
  FormFieldData result;
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result);

  FormFieldData expected;
  expected.name = ASCIIToUTF16("element");
  expected.value = ASCIIToUTF16("value");
  expected.form_control_type = "text";
  expected.max_length = 5;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a text field that has been autofilled.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutofilled) {
  LoadHTML("<INPUT type=\"text\" id=\"element\" value=\"value\"/>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebInputElement element = web_element.to<WebInputElement>();
  element.setAutofilled(true);
  FormFieldData result;
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result);

  FormFieldData expected;
  expected.name = ASCIIToUTF16("element");
  expected.value = ASCIIToUTF16("value");
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a <select> field.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldSelect) {
  LoadHTML("<SELECT id=\"element\"/>"
           "  <OPTION value=\"CA\">California</OPTION>"
           "  <OPTION value=\"TX\">Texas</OPTION>"
           "</SELECT>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebFormControlElement element = web_element.to<WebFormControlElement>();
  FormFieldData result1;
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result1);

  FormFieldData expected;
  expected.name = ASCIIToUTF16("element");
  expected.max_length = 0;
  expected.form_control_type = "select-one";

  expected.value = ASCIIToUTF16("CA");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result1);

  FormFieldData result2;
  WebFormControlElementToFormField(
      element,
      static_cast<autofill::ExtractMask>(autofill::EXTRACT_VALUE |
                                         autofill::EXTRACT_OPTION_TEXT),
      &result2);
  expected.value = ASCIIToUTF16("California");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result2);

  FormFieldData result3;
  WebFormControlElementToFormField(element, autofill::EXTRACT_OPTIONS,
                                   &result3);
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result3);

  ASSERT_EQ(2U, result3.option_values.size());
  ASSERT_EQ(2U, result3.option_contents.size());
  EXPECT_EQ(ASCIIToUTF16("CA"), result3.option_values[0]);
  EXPECT_EQ(ASCIIToUTF16("California"), result3.option_contents[0]);
  EXPECT_EQ(ASCIIToUTF16("TX"), result3.option_values[1]);
  EXPECT_EQ(ASCIIToUTF16("Texas"), result3.option_contents[1]);
}

// We should not extract the value for non-text and non-select fields.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldInvalidType) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"hidden\" id=\"hidden\" value=\"apple\"/>"
           "  <INPUT type=\"password\" id=\"password\" value=\"secret\"/>"
           "  <INPUT type=\"checkbox\" id=\"checkbox\" value=\"mail\"/>"
           "  <INPUT type=\"radio\" id=\"radio\" value=\"male\"/>"
           "  <INPUT type=\"submit\" id=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("hidden");
  WebFormControlElement element = web_element.to<WebFormControlElement>();
  FormFieldData result;
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result);

  FormFieldData expected;
  expected.max_length = 0;

  expected.name = ASCIIToUTF16("hidden");
  expected.form_control_type = "hidden";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);

  web_element = frame->document().getElementById("password");
  element = web_element.to<WebFormControlElement>();
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result);
  expected.name = ASCIIToUTF16("password");
  expected.form_control_type = "password";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);

  web_element = frame->document().getElementById("checkbox");
  element = web_element.to<WebFormControlElement>();
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result);
  expected.name = ASCIIToUTF16("checkbox");
  expected.form_control_type = "checkbox";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);

  web_element = frame->document().getElementById("radio");
  element = web_element.to<WebFormControlElement>();
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result);
  expected.name = ASCIIToUTF16("radio");
  expected.form_control_type = "radio";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);


  web_element = frame->document().getElementById("submit");
  element = web_element.to<WebFormControlElement>();
  WebFormControlElementToFormField(element, autofill::EXTRACT_VALUE, &result);
  expected.name = ASCIIToUTF16("submit");
  expected.form_control_type = "submit";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract the autocompletetype attribute.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutocompletetype) {
  std::string html =
      "<INPUT type=\"text\" id=\"absent\"/>"
      "<INPUT type=\"text\" id=\"empty\" autocomplete=\"\"/>"
      "<INPUT type=\"text\" id=\"off\" autocomplete=\"off\"/>"
      "<INPUT type=\"text\" id=\"regular\" autocomplete=\"email\"/>"
      "<INPUT type=\"text\" id=\"multi-valued\" "
      "       autocomplete=\"billing email\"/>"
      "<INPUT type=\"text\" id=\"experimental\" x-autocompletetype=\"email\"/>"
      "<SELECT id=\"select\" autocomplete=\"state\"/>"
      "  <OPTION value=\"CA\">California</OPTION>"
      "  <OPTION value=\"TX\">Texas</OPTION>"
      "</SELECT>";
  html +=
      "<INPUT type=\"text\" id=\"malicious\" autocomplete=\"" +
      std::string(10000, 'x') + "\"/>";
  LoadHTML(html.c_str());

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  struct TestCase {
    const std::string element_id;
    const std::string form_control_type;
    const std::string autocomplete_attribute;
  };
  TestCase test_cases[] = {
    // An absent attribute is equivalent to an empty one.
    { "absent", "text", "" },
    // Make sure there are no issues parsing an empty attribute.
    { "empty", "text", "" },
    // Make sure there are no issues parsing an attribute value that isn't a
    // type hint.
    { "off", "text", "off" },
    // Common case: exactly one type specified.
    { "regular", "text", "email" },
    // Verify that we correctly extract multiple tokens as well.
    { "multi-valued", "text", "billing email" },
    // We previously extracted this data from the experimental
    // 'x-autocompletetype' attribute.  Now that the field type hints are part
    // of the spec under the autocomplete attribute, we no longer support the
    // experimental version.
    { "experimental", "text", "" },
    // <select> elements should behave no differently from text fields here.
    { "select", "select-one", "state" },
    // Very long attribute values should be replaced by a default string, to
    // prevent malicious websites from DOSing the browser process.
    { "malicious", "text", "x-max-data-length-exceeded" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    WebElement web_element = frame->document().getElementById(
        ASCIIToUTF16(test_cases[i].element_id));
    WebFormControlElement element = web_element.to<WebFormControlElement>();
    FormFieldData result;
    WebFormControlElementToFormField(element, autofill::EXTRACT_NONE, &result);

    FormFieldData expected;
    expected.name = ASCIIToUTF16(test_cases[i].element_id);
    expected.form_control_type = test_cases[i].form_control_type;
    expected.autocomplete_attribute = test_cases[i].autocomplete_attribute;
    if (test_cases[i].form_control_type == "text")
      expected.max_length = WebInputElement::defaultMaxLength();
    else
      expected.max_length = 0;

    SCOPED_TRACE(test_cases[i].element_id);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
  }
}

TEST_F(FormAutofillTest, WebFormElementToFormData) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           " <LABEL for=\"firstname\">First name:</LABEL>"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           " <LABEL for=\"lastname\">Last name:</LABEL>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           " <LABEL for=\"state\">State:</LABEL>"
           "  <SELECT id=\"state\"/>"
           "    <OPTION value=\"CA\">California</OPTION>"
           "    <OPTION value=\"TX\">Texas</OPTION>"
           "  </SELECT>"
           // The below inputs should be ignored
           " <LABEL for=\"notvisible\">Hidden:</LABEL>"
           "  <INPUT type=\"hidden\" id=\"notvisible\" value=\"apple\"/>"
           " <LABEL for=\"password\">Password:</LABEL>"
           "  <INPUT type=\"password\" id=\"password\" value=\"secret\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->document().forms(forms);
  ASSERT_EQ(1U, forms.size());

  WebElement element = frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  FormData form;
  FormFieldData field;
  EXPECT_TRUE(WebFormElementToFormData(forms[0],
                                       input_element,
                                       autofill::REQUIRE_NONE,
                                       autofill::EXTRACT_VALUE,
                                       &form,
                                       &field));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("John");
  expected.label = ASCIIToUTF16("First name:");
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Smith");
  expected.label = ASCIIToUTF16("Last name:");
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("state");
  expected.value = ASCIIToUTF16("CA");
  expected.label = ASCIIToUTF16("State:");
  expected.form_control_type = "select-one";
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
}

// We should not be able to serialize a form with too many fillable fields.
TEST_F(FormAutofillTest, WebFormElementToFormDataTooManyFields) {
  std::string html =
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">";
  for (size_t i = 0; i < (autofill::kMaxParseableFields + 1); ++i) {
    html += "<INPUT type=\"text\"/>";
  }
  html += "</FORM>";
  LoadHTML(html.c_str());

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->document().forms(forms);
  ASSERT_EQ(1U, forms.size());

  WebElement element = frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  FormData form;
  FormFieldData field;
  EXPECT_FALSE(WebFormElementToFormData(forms[0],
                                        input_element,
                                        autofill::REQUIRE_NONE,
                                        autofill::EXTRACT_VALUE,
                                        &form,
                                        &field));
}

TEST_F(FormAutofillTest, ExtractForms) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  First name: <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "  Last name: <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "  Email: <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, ExtractMultipleForms) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>"
           "<FORM name=\"TestForm2\" action=\"http://zoo.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"Jack\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Adams\"/>"
           "  <INPUT type=\"text\" id=\"email\" value=\"jack@example.com\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(2U, forms.size());

  // First form.
  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("John");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Smith");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("email");
  expected.value = ASCIIToUTF16("john@example.com");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  // Second form.
  const FormData& form2 = forms[1];
  EXPECT_EQ(ASCIIToUTF16("TestForm2"), form2.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form2.origin);
  EXPECT_EQ(GURL("http://zoo.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("Jack");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Adams");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.name = ASCIIToUTF16("email");
  expected.value = ASCIIToUTF16("jack@example.com");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
}

// We should not extract a form if it has too few fillable fields.
TEST_F(FormAutofillTest, ExtractFormsTooFewFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  EXPECT_EQ(0U, forms.size());
}

TEST_F(FormAutofillTest, WebFormElementToFormDataAutocomplete) {
  {
    // Form is not auto-completable due to autocomplete=off.
    LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\""
             " autocomplete=off>"
             "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
             "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
             "  <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
             "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
             "</FORM>");

    WebFrame* web_frame = GetMainFrame();
    ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

    WebVector<WebFormElement> web_forms;
    web_frame->document().forms(web_forms);
    ASSERT_EQ(1U, web_forms.size());
    WebFormElement web_form = web_forms[0];

    FormData form;
    EXPECT_TRUE(WebFormElementToFormData(
        web_form, WebFormControlElement(), autofill::REQUIRE_NONE,
        autofill::EXTRACT_NONE, &form, NULL));
    EXPECT_FALSE(WebFormElementToFormData(
        web_form, WebFormControlElement(), autofill::REQUIRE_AUTOCOMPLETE,
        autofill::EXTRACT_NONE, &form, NULL));
  }

  {
    // The firstname element is not auto-completable due to autocomplete=off.
    LoadHTML("<FORM name=\"TestForm\" action=\"http://abc.com\" "
             "      method=\"post\">"
             "  <INPUT type=\"text\" id=\"firstname\" value=\"John\""
             "   autocomplete=off>"
             "  <INPUT type=\"text\" id=\"middlename\" value=\"Jack\"/>"
             "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
             "  <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
             "  <INPUT type=\"submit\" name=\"reply\" value=\"Send\"/>"
             "</FORM>");

    WebFrame* web_frame = GetMainFrame();
    ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

    WebVector<WebFormElement> web_forms;
    web_frame->document().forms(web_forms);
    ASSERT_EQ(1U, web_forms.size());
    WebFormElement web_form = web_forms[0];

    FormData form;
    EXPECT_TRUE(WebFormElementToFormData(
        web_form, WebFormControlElement(), autofill::REQUIRE_AUTOCOMPLETE,
        autofill::EXTRACT_VALUE, &form, NULL));

    EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
    EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
    EXPECT_EQ(GURL("http://abc.com"), form.action);

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;
    expected.form_control_type = "text";
    expected.max_length = WebInputElement::defaultMaxLength();

    expected.name = ASCIIToUTF16("middlename");
    expected.value = ASCIIToUTF16("Jack");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.name = ASCIIToUTF16("lastname");
    expected.value = ASCIIToUTF16("Smith");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.name = ASCIIToUTF16("email");
    expected.value = ASCIIToUTF16("john@example.com");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
  }
}

TEST_F(FormAutofillTest, FindForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"text\" id=\"email\" value=\"john@example.com\""
                     "autocomplete=\"off\" />"
           "  <INPUT type=\"text\" id=\"phone\" value=\"1.800.555.1234\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form and verify it's the correct form.
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form, &field,
                                              autofill::REQUIRE_NONE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(4U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("John");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, field);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Smith");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("email");
  expected.value = ASCIIToUTF16("john@example.com");
  expected.autocomplete_attribute = "off";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
  expected.autocomplete_attribute = std::string();  // reset

  expected.name = ASCIIToUTF16("phone");
  expected.value = ASCIIToUTF16("1.800.555.1234");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  // Try again, but require autocomplete.
  FormData form2;
  FormFieldData field2;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form2, &field2,
                                              autofill::REQUIRE_AUTOCOMPLETE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());

  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("John");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, field);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Smith");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.name = ASCIIToUTF16("phone");
  expected.value = ASCIIToUTF16("1.800.555.1234");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
}

TEST_F(FormAutofillTest, FillForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"hidden\" id=\"imhidden\"/>"
           "  <INPUT type=\"text\" id=\"notempty\" value=\"Hi\"/>"
           "  <INPUT type=\"text\" autocomplete=\"off\" id=\"noautocomplete\"/>"
           "  <INPUT type=\"text\" disabled=\"disabled\" id=\"notenabled\"/>"
           "  <INPUT type=\"text\" readonly id=\"readonly\"/>"
           "  <INPUT type=\"text\" style=\"visibility: hidden\""
           "         id=\"invisible\"/>"
           "  <INPUT type=\"text\" style=\"display: none\" id=\"displaynone\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form, &field,
                                              autofill::REQUIRE_AUTOCOMPLETE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(7U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("notempty");
  expected.value = ASCIIToUTF16("Hi");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.name = ASCIIToUTF16("notenabled");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  expected.name = ASCIIToUTF16("readonly");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[4]);

  expected.name = ASCIIToUTF16("invisible");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[5]);

  expected.name = ASCIIToUTF16("displaynone");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[6]);

  // Fill the form.
  form.fields[0].value = ASCIIToUTF16("Wyatt");
  form.fields[1].value = ASCIIToUTF16("Earp");
  form.fields[2].value = ASCIIToUTF16("Alpha");
  form.fields[3].value = ASCIIToUTF16("Beta");
  form.fields[4].value = ASCIIToUTF16("Gamma");
  form.fields[5].value = ASCIIToUTF16("Delta");
  form.fields[6].value = ASCIIToUTF16("Epsilon");
  FillForm(form, input_element);

  // Verify the filled elements.
  WebDocument document = web_frame->document();
  WebInputElement firstname =
      document.getElementById("firstname").to<WebInputElement>();
  EXPECT_TRUE(firstname.isAutofilled());
  EXPECT_EQ(ASCIIToUTF16("Wyatt"), firstname.value());
  EXPECT_EQ(5, firstname.selectionStart());
  EXPECT_EQ(5, firstname.selectionEnd());

  WebInputElement lastname =
      document.getElementById("lastname").to<WebInputElement>();
  EXPECT_TRUE(lastname.isAutofilled());
  EXPECT_EQ(ASCIIToUTF16("Earp"), lastname.value());

  // Non-empty fields are not filled.
  WebInputElement notempty =
      document.getElementById("notempty").to<WebInputElement>();
  EXPECT_FALSE(notempty.isAutofilled());
  EXPECT_EQ(ASCIIToUTF16("Hi"), notempty.value());

  // autocomplete=off fields are not filled.
  WebInputElement noautocomplete =
      document.getElementById("noautocomplete").to<WebInputElement>();
  EXPECT_FALSE(noautocomplete.isAutofilled());
  EXPECT_TRUE(noautocomplete.value().isEmpty());

  // Disabled fields are not filled.
  WebInputElement notenabled =
      document.getElementById("notenabled").to<WebInputElement>();
  EXPECT_FALSE(notenabled.isAutofilled());
  EXPECT_TRUE(notenabled.value().isEmpty());

  // Read-only fields are not filled.
  WebInputElement readonly =
      document.getElementById("readonly").to<WebInputElement>();
  EXPECT_FALSE(readonly.isAutofilled());
  EXPECT_TRUE(readonly.value().isEmpty());

  // |visibility:hidden| fields are not filled.
  WebInputElement invisible =
      document.getElementById("invisible").to<WebInputElement>();
  EXPECT_FALSE(invisible.isAutofilled());
  EXPECT_TRUE(invisible.value().isEmpty());

  // |display:none| fields are not filled.
  WebInputElement display_none =
      document.getElementById("displaynone").to<WebInputElement>();
  EXPECT_FALSE(display_none.isAutofilled());
  EXPECT_TRUE(display_none.value().isEmpty());
}

TEST_F(FormAutofillTest, PreviewForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"notempty\" value=\"Hi\"/>"
           "  <INPUT type=\"text\" autocomplete=\"off\" id=\"noautocomplete\"/>"
           "  <INPUT type=\"text\" disabled=\"disabled\" id=\"notenabled\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form, &field,
                                              autofill::REQUIRE_AUTOCOMPLETE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(4U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("notempty");
  expected.value = ASCIIToUTF16("Hi");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.name = ASCIIToUTF16("notenabled");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  // Preview the form.
  form.fields[0].value = ASCIIToUTF16("Wyatt");
  form.fields[1].value = ASCIIToUTF16("Earp");
  form.fields[2].value = ASCIIToUTF16("Alpha");
  form.fields[3].value = ASCIIToUTF16("Beta");
  PreviewForm(form, input_element);

  // Verify the previewed elements.
  WebDocument document = web_frame->document();
  WebInputElement firstname =
      document.getElementById("firstname").to<WebInputElement>();
  EXPECT_TRUE(firstname.isAutofilled());
  EXPECT_EQ(ASCIIToUTF16("Wyatt"), firstname.suggestedValue());
  EXPECT_EQ(0, firstname.selectionStart());
  EXPECT_EQ(5, firstname.selectionEnd());

  WebInputElement lastname =
      document.getElementById("lastname").to<WebInputElement>();
  EXPECT_TRUE(lastname.isAutofilled());
  EXPECT_EQ(ASCIIToUTF16("Earp"), lastname.suggestedValue());

  // Non-empty fields are not previewed.
  WebInputElement notempty =
      document.getElementById("notempty").to<WebInputElement>();
  EXPECT_FALSE(notempty.isAutofilled());
  EXPECT_TRUE(notempty.suggestedValue().isEmpty());

  // autocomplete=off fields are not previewed.
  WebInputElement noautocomplete =
      document.getElementById("noautocomplete").to<WebInputElement>();
  EXPECT_FALSE(noautocomplete.isAutofilled());
  EXPECT_TRUE(noautocomplete.suggestedValue().isEmpty());

  // Disabled fields are not previewed.
  WebInputElement notenabled =
      document.getElementById("notenabled").to<WebInputElement>();
  EXPECT_FALSE(notenabled.isAutofilled());
  EXPECT_TRUE(notenabled.suggestedValue().isEmpty());
}

TEST_F(FormAutofillTest, Labels) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  <LABEL for=\"firstname\"> First name: </LABEL>"
      "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "  <LABEL for=\"lastname\"> Last name: </LABEL>"
      "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "  <LABEL for=\"email\"> Email: </LABEL>"
      "    <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsWithSpans) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  <LABEL for=\"firstname\"><span>First name: </span></LABEL>"
      "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "  <LABEL for=\"lastname\"><span>Last name: </span></LABEL>"
      "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "  <LABEL for=\"email\"><span>Email: </span></LABEL>"
      "    <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

// This test is different from FormAutofillTest.Labels in that the label
// elements for= attribute is set to the name of the form control element it is
// a label for instead of the id of the form control element.  This is invalid
// because the for= attribute must be set to the id of the form control element;
// however, current label parsing code will extract the text from the previous
// label element and apply it to the following input field.
TEST_F(FormAutofillTest, InvalidLabels) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  <LABEL for=\"firstname\"> First name: </LABEL>"
      "    <INPUT type=\"text\" name=\"firstname\" value=\"John\"/>"
      "  <LABEL for=\"lastname\"> Last name: </LABEL>"
      "    <INPUT type=\"text\" name=\"lastname\" value=\"Smith\"/>"
      "  <LABEL for=\"email\"> Email: </LABEL>"
      "    <INPUT type=\"text\" name=\"email\" value=\"john@example.com\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

// This test has three form control elements, only one of which has a label
// element associated with it.
TEST_F(FormAutofillTest, OneLabelElement) {
  ExpectJohnSmithLabels(
           "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  First name:"
           "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <LABEL for=\"lastname\">Last name: </LABEL>"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  Email:"
           "    <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromText) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  First name:"
      "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "  Last name:"
      "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "  Email:"
      "    <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromParagraph) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  <P>First name:</P><INPUT type=\"text\" "
      "                           id=\"firstname\" value=\"John\"/>"
      "  <P>Last name:</P>"
      "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "  <P>Email:</P>"
      "    <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromBold) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  <B>First name:</B><INPUT type=\"text\" "
      "                           id=\"firstname\" value=\"John\"/>"
      "  <B>Last name:</B>"
      "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "  <B>Email:</B>"
      "    <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredPriorToImgOrBr) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  First name:<IMG/><INPUT type=\"text\" "
      "                          id=\"firstname\" value=\"John\"/>"
      "  Last name:<IMG/>"
      "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "  Email:<BR/>"
      "    <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableCell) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TD>First name:</TD>"
      "    <TD><INPUT type=\"text\" id=\"firstname\" value=\"John\"/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>Last name:</TD>"
      "    <TD><INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>Email:</TD>"
      "    <TD><INPUT type=\"text\" id=\"email\""
      "               value=\"john@example.com\"/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableCellTH) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TH>First name:</TH>"
      "    <TD><INPUT type=\"text\" id=\"firstname\" value=\"John\"/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TH>Last name:</TH>"
      "    <TD><INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TH>Email:</TH>"
      "    <TD><INPUT type=\"text\" id=\"email\""
      "               value=\"john@example.com\"/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableCellNested) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("First name: Bogus"));
  names.push_back(ASCIIToUTF16("firstname"));
  values.push_back(ASCIIToUTF16("John"));

  labels.push_back(ASCIIToUTF16("Last name:"));
  names.push_back(ASCIIToUTF16("lastname"));
  values.push_back(ASCIIToUTF16("Smith"));

  labels.push_back(ASCIIToUTF16("Email:"));
  names.push_back(ASCIIToUTF16("email"));
  values.push_back(ASCIIToUTF16("john@example.com"));

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <FONT>"
      "        First name:"
      "      </FONT>"
      "      <FONT>"
      "        Bogus"
      "      </FONT>"
      "    </TD>"
      "    <TD>"
      "      <FONT>"
      "        <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "      </FONT>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <FONT>"
      "        Last name:"
      "      </FONT>"
      "    </TD>"
      "    <TD>"
      "      <FONT>"
      "        <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "      </FONT>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <FONT>"
      "        Email:"
      "      </FONT>"
      "    </TD>"
      "    <TD>"
      "      <FONT>"
      "        <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "      </FONT>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
      labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredFromTableEmptyTDs) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("* First Name"));
  names.push_back(ASCIIToUTF16("firstname"));
  values.push_back(ASCIIToUTF16("John"));

  labels.push_back(ASCIIToUTF16("* Last Name"));
  names.push_back(ASCIIToUTF16("lastname"));
  values.push_back(ASCIIToUTF16("Smith"));

  labels.push_back(ASCIIToUTF16("* Email"));
  names.push_back(ASCIIToUTF16("email"));
  values.push_back(ASCIIToUTF16("john@example.com"));

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>First Name</B>"
      "    </TD>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Last Name</B>"
      "    </TD>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Email</B>"
      "    </TD>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
      labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredFromPreviousTD) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("* First Name"));
  names.push_back(ASCIIToUTF16("firstname"));
  values.push_back(ASCIIToUTF16("John"));

  labels.push_back(ASCIIToUTF16("* Last Name"));
  names.push_back(ASCIIToUTF16("lastname"));
  values.push_back(ASCIIToUTF16("Smith"));

  labels.push_back(ASCIIToUTF16("* Email"));
  names.push_back(ASCIIToUTF16("email"));
  values.push_back(ASCIIToUTF16("john@example.com"));

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TD>* First Name</TD>"
      "    <TD>"
      "      Bogus"
      "      <INPUT type=\"hidden\"/>"
      "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>* Last Name</TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>* Email</TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
      labels, names, values);
}

// <script>, <noscript> and <option> tags are excluded when the labels are
// inferred.
// Also <!-- comment --> is excluded.
TEST_F(FormAutofillTest, LabelsInferredFromTableWithSpecialElements) {
  std::vector<string16> labels, names, values;
  std::vector<std::string> control_types;

  labels.push_back(ASCIIToUTF16("* First Name"));
  names.push_back(ASCIIToUTF16("firstname"));
  values.push_back(ASCIIToUTF16("John"));
  control_types.push_back("text");

  labels.push_back(ASCIIToUTF16("* Middle Name"));
  names.push_back(ASCIIToUTF16("middlename"));
  values.push_back(ASCIIToUTF16("Joe"));
  control_types.push_back("text");

  labels.push_back(ASCIIToUTF16("* Last Name"));
  names.push_back(ASCIIToUTF16("lastname"));
  values.push_back(ASCIIToUTF16("Smith"));
  control_types.push_back("text");

  labels.push_back(ASCIIToUTF16("* Country"));
  names.push_back(ASCIIToUTF16("country"));
  values.push_back(ASCIIToUTF16("US"));
  control_types.push_back("select-one");

  labels.push_back(ASCIIToUTF16("* Email"));
  names.push_back(ASCIIToUTF16("email"));
  values.push_back(ASCIIToUTF16("john@example.com"));
  control_types.push_back("text");

  ExpectLabelsAndTypes(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>First Name</B>"
      "    </TD>"
      "    <TD>"
      "      <SCRIPT> <!-- function test() { alert('ignored as label'); } -->"
      "      </SCRIPT>"
      "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Middle Name</B>"
      "    </TD>"
      "    <TD>"
      "      <NOSCRIPT>"
      "        <P>Bad</P>"
      "      </NOSCRIPT>"
      "      <INPUT type=\"text\" id=\"middlename\" value=\"Joe\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Last Name</B>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Country</B>"
      "    </TD>"
      "    <TD>"
      "      <SELECT id=\"country\">"
      "        <OPTION VALUE=\"US\">The value should be ignored as label."
      "        </OPTION>"
      "        <OPTION VALUE=\"JP\">JAPAN</OPTION>"
      "      </SELECT>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Email</B>"
      "    </TD>"
      "    <TD>"
      "      <!-- This comment should be ignored as inferred label.-->"
      "      <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
      labels, names, values, control_types);
}

TEST_F(FormAutofillTest, LabelsInferredFromTableLabels) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <LABEL>First name:</LABEL>"
      "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <LABEL>Last name:</LABEL>"
      "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <LABEL>Email:</LABEL>"
      "      <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "<INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableTDInterveningElements) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      First name:"
      "      <BR>"
      "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      Last name:"
      "      <BR>"
      "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      Email:"
      "      <BR>"
      "      <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "<INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>");
}

// Verify that we correctly infer labels when the label text spans multiple
// adjacent HTML elements, not separated by whitespace.
TEST_F(FormAutofillTest, LabelsInferredFromTableAdjacentElements) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("*First Name"));
  names.push_back(ASCIIToUTF16("firstname"));
  values.push_back(ASCIIToUTF16("John"));

  labels.push_back(ASCIIToUTF16("*Last Name"));
  names.push_back(ASCIIToUTF16("lastname"));
  values.push_back(ASCIIToUTF16("Smith"));

  labels.push_back(ASCIIToUTF16("*Email"));
  names.push_back(ASCIIToUTF16("email"));
  values.push_back(ASCIIToUTF16("john@example.com"));

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN><B>First Name</B>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN><B>Last Name</B>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN><B>Email</B>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
      labels, names, values);
}

// Verify that we correctly infer labels when the label text resides in the
// previous row.
TEST_F(FormAutofillTest, LabelsInferredFromTableRow) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("*First Name *Last Name *Email"));
  names.push_back(ASCIIToUTF16("firstname"));
  values.push_back(ASCIIToUTF16("John"));

  labels.push_back(ASCIIToUTF16("*First Name *Last Name *Email"));
  names.push_back(ASCIIToUTF16("lastname"));
  values.push_back(ASCIIToUTF16("Smith"));

  labels.push_back(ASCIIToUTF16("*First Name *Last Name *Email"));
  names.push_back(ASCIIToUTF16("email"));
  values.push_back(ASCIIToUTF16("john@example.com"));

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<TABLE>"
      "  <TR>"
      "    <TD>*First Name</TD>"
      "    <TD>*Last Name</TD>"
      "    <TD>*Email</TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>",
      labels, names, values);
}

// Verify that we correctly infer labels when enclosed within a list item.
TEST_F(FormAutofillTest, LabelsInferredFromListItem) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("* Home Phone"));
  names.push_back(ASCIIToUTF16("areacode"));
  values.push_back(ASCIIToUTF16("415"));

  labels.push_back(ASCIIToUTF16("* Home Phone"));
  names.push_back(ASCIIToUTF16("prefix"));
  values.push_back(ASCIIToUTF16("555"));

  labels.push_back(ASCIIToUTF16("* Home Phone"));
  names.push_back(ASCIIToUTF16("suffix"));
  values.push_back(ASCIIToUTF16("1212"));

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<DIV>"
      "  <LI>"
      "    <SPAN>Bogus</SPAN>"
      "  </LI>"
      "  <LI>"
      "    <LABEL><EM>*</EM> Home Phone</LABEL>"
      "    <INPUT type=\"text\" id=\"areacode\" value=\"415\"/>"
      "    <INPUT type=\"text\" id=\"prefix\" value=\"555\"/>"
      "    <INPUT type=\"text\" id=\"suffix\" value=\"1212\"/>"
      "  </LI>"
      "  <LI>"
      "    <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "  </LI>"
      "</DIV>"
      "</FORM>",
      labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredFromDefinitionList) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("* First name: Bogus"));
  names.push_back(ASCIIToUTF16("firstname"));
  values.push_back(ASCIIToUTF16("John"));

  labels.push_back(ASCIIToUTF16("Last name:"));
  names.push_back(ASCIIToUTF16("lastname"));
  values.push_back(ASCIIToUTF16("Smith"));

  labels.push_back(ASCIIToUTF16("Email:"));
  names.push_back(ASCIIToUTF16("email"));
  values.push_back(ASCIIToUTF16("john@example.com"));

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<DL>"
      "  <DT>"
      "    <SPAN>"
      "      *"
      "    </SPAN>"
      "    <SPAN>"
      "      First name:"
      "    </SPAN>"
      "    <SPAN>"
      "      Bogus"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "    </FONT>"
      "  </DD>"
      "  <DT>"
      "    <SPAN>"
      "      Last name:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "    </FONT>"
      "  </DD>"
      "  <DT>"
      "    <SPAN>"
      "      Email:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "    </FONT>"
      "  </DD>"
      "  <DT></DT>"
      "  <DD>"
      "    <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "  </DD>"
      "</DL>"
      "</FORM>",
      labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredWithSameName) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("Address Line 1:"));
  names.push_back(ASCIIToUTF16("Address"));
  values.push_back(string16());

  labels.push_back(ASCIIToUTF16("Address Line 2:"));
  names.push_back(ASCIIToUTF16("Address"));
  values.push_back(string16());

  labels.push_back(ASCIIToUTF16("Address Line 3:"));
  names.push_back(ASCIIToUTF16("Address"));
  values.push_back(string16());

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  Address Line 1:"
      "    <INPUT type=\"text\" name=\"Address\"/>"
      "  Address Line 2:"
      "    <INPUT type=\"text\" name=\"Address\"/>"
      "  Address Line 3:"
      "    <INPUT type=\"text\" name=\"Address\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>",
      labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredWithImageTags) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("Phone:"));
  names.push_back(ASCIIToUTF16("dayphone1"));
  values.push_back(string16());

  labels.push_back(ASCIIToUTF16("-"));
  names.push_back(ASCIIToUTF16("dayphone2"));
  values.push_back(string16());

  labels.push_back(ASCIIToUTF16("-"));
  names.push_back(ASCIIToUTF16("dayphone3"));
  values.push_back(string16());

  labels.push_back(ASCIIToUTF16("ext.:"));
  names.push_back(ASCIIToUTF16("dayphone4"));
  values.push_back(string16());

  labels.push_back(string16());
  names.push_back(ASCIIToUTF16("dummy"));
  values.push_back(string16());

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  Phone:"
      "  <input type=\"text\" name=\"dayphone1\">"
      "  <img/>"
      "  -"
      "  <img/>"
      "  <input type=\"text\" name=\"dayphone2\">"
      "  <img/>"
      "  -"
      "  <img/>"
      "  <input type=\"text\" name=\"dayphone3\">"
      "  ext.:"
      "  <input type=\"text\" name=\"dayphone4\">"
      "  <input type=\"text\" name=\"dummy\">"
      "  <input type=\"submit\" name=\"reply-send\" value=\"Send\">"
      "</FORM>",
      labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredFromDivTable) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<DIV>First name:<BR>"
      "  <SPAN>"
      "    <INPUT type=\"text\" name=\"firstname\" value=\"John\">"
      "  </SPAN>"
      "</DIV>"
      "<DIV>Last name:<BR>"
      "  <SPAN>"
      "    <INPUT type=\"text\" name=\"lastname\" value=\"Smith\">"
      "  </SPAN>"
      "</DIV>"
      "<DIV>Email:<BR>"
      "  <SPAN>"
      "    <INPUT type=\"text\" name=\"email\" value=\"john@example.com\">"
      "  </SPAN>"
      "</DIV>"
      "<input type=\"submit\" name=\"reply-send\" value=\"Send\">"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromDivSiblingTable) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<DIV>First name:</DIV>"
      "<DIV>"
      "  <SPAN>"
      "    <INPUT type=\"text\" name=\"firstname\" value=\"John\">"
      "  </SPAN>"
      "</DIV>"
      "<DIV>Last name:</DIV>"
      "<DIV>"
      "  <SPAN>"
      "    <INPUT type=\"text\" name=\"lastname\" value=\"Smith\">"
      "  </SPAN>"
      "</DIV>"
      "<DIV>Email:</DIV>"
      "<DIV>"
      "  <SPAN>"
      "    <INPUT type=\"text\" name=\"email\" value=\"john@example.com\">"
      "  </SPAN>"
      "</DIV>"
      "<input type=\"submit\" name=\"reply-send\" value=\"Send\">"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromDefinitionListRatherThanDivTable) {
  ExpectJohnSmithLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "<DIV>This is not a label.<BR>"
      "<DL>"
      "  <DT>"
      "    <SPAN>"
      "      First name:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "    </FONT>"
      "  </DD>"
      "  <DT>"
      "    <SPAN>"
      "      Last name:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "    </FONT>"
      "  </DD>"
      "  <DT>"
      "    <SPAN>"
      "      Email:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "    </FONT>"
      "  </DD>"
      "  <DT></DT>"
      "  <DD>"
      "    <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "  </DD>"
      "</DL>"
      "</DIV>"
      "</FORM>");
}

TEST_F(FormAutofillTest, FillFormMaxLength) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" maxlength=\"5\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" maxlength=\"7\"/>"
           "  <INPUT type=\"text\" id=\"email\" maxlength=\"9\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form, &field,
                                              autofill::REQUIRE_NONE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";

  expected.name = ASCIIToUTF16("firstname");
  expected.max_length = 5;
  expected.is_autofilled = false;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.max_length = 7;
  expected.is_autofilled = false;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("email");
  expected.max_length = 9;
  expected.is_autofilled = false;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  // Fill the form.
  form.fields[0].value = ASCIIToUTF16("Brother");
  form.fields[1].value = ASCIIToUTF16("Jonathan");
  form.fields[2].value = ASCIIToUTF16("brotherj@example.com");
  FillForm(form, input_element);

  // Find the newly-filled form that contains the input element.
  FormData form2;
  FormFieldData field2;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form2, &field2,
                                              autofill::REQUIRE_NONE));

  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());

  expected.form_control_type = "text";

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("Broth");
  expected.max_length = 5;
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Jonatha");
  expected.max_length = 7;
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.name = ASCIIToUTF16("email");
  expected.value = ASCIIToUTF16("brotherj@");
  expected.max_length = 9;
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
}

// This test uses negative values of the maxlength attribute for input elements.
// In this case, the maxlength of the input elements is set to the default
// maxlength (defined in WebKit.)
TEST_F(FormAutofillTest, FillFormNegativeMaxLength) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" maxlength=\"-1\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" maxlength=\"-10\"/>"
           "  <INPUT type=\"text\" id=\"email\" maxlength=\"-13\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form, &field,
                                              autofill::REQUIRE_NONE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("email");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  // Fill the form.
  form.fields[0].value = ASCIIToUTF16("Brother");
  form.fields[1].value = ASCIIToUTF16("Jonathan");
  form.fields[2].value = ASCIIToUTF16("brotherj@example.com");
  FillForm(form, input_element);

  // Find the newly-filled form that contains the input element.
  FormData form2;
  FormFieldData field2;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form2, &field2,
                                              autofill::REQUIRE_NONE));

  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("Brother");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Jonathan");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("email");
  expected.value = ASCIIToUTF16("brotherj@example.com");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
}

TEST_F(FormAutofillTest, FillFormEmptyName) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"email\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form, &field,
                                              autofill::REQUIRE_NONE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("email");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  // Fill the form.
  form.fields[0].value = ASCIIToUTF16("Wyatt");
  form.fields[1].value = ASCIIToUTF16("Earp");
  form.fields[2].value = ASCIIToUTF16("wyatt@example.com");
  FillForm(form, input_element);

  // Find the newly-filled form that contains the input element.
  FormData form2;
  FormFieldData field2;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form2, &field2,
                                              autofill::REQUIRE_NONE));

  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());

  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("Wyatt");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Earp");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("email");
  expected.value = ASCIIToUTF16("wyatt@example.com");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
}

TEST_F(FormAutofillTest, FillFormEmptyFormNames) {
  LoadHTML("<FORM action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>"
           "<FORM action=\"http://abc.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"apple\"/>"
           "  <INPUT type=\"text\" id=\"banana\"/>"
           "  <INPUT type=\"text\" id=\"cantelope\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(2U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("apple");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form, &field,
                                              autofill::REQUIRE_NONE));
  EXPECT_EQ(string16(), form.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://abc.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("apple");
  expected.is_autofilled = false;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("banana");
  expected.is_autofilled = false;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("cantelope");
  expected.is_autofilled = false;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  // Fill the form.
  form.fields[0].value = ASCIIToUTF16("Red");
  form.fields[1].value = ASCIIToUTF16("Yellow");
  form.fields[2].value = ASCIIToUTF16("Also Yellow");
  FillForm(form, input_element);

  // Find the newly-filled form that contains the input element.
  FormData form2;
  FormFieldData field2;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form2, &field2,
                                              autofill::REQUIRE_NONE));

  EXPECT_EQ(string16(), form2.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form2.origin);
  EXPECT_EQ(GURL("http://abc.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());

  expected.name = ASCIIToUTF16("apple");
  expected.value = ASCIIToUTF16("Red");
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.name = ASCIIToUTF16("banana");
  expected.value = ASCIIToUTF16("Yellow");
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.name = ASCIIToUTF16("cantelope");
  expected.value = ASCIIToUTF16("Also Yellow");
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
}

TEST_F(FormAutofillTest, ThreePartPhone) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  Phone:"
           "  <input type=\"text\" name=\"dayphone1\">"
           "  -"
           "  <input type=\"text\" name=\"dayphone2\">"
           "  -"
           "  <input type=\"text\" name=\"dayphone3\">"
           "  ext.:"
           "  <input type=\"text\" name=\"dayphone4\">"
           "  <input type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");


  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->document().forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(forms[0],
                                       WebFormControlElement(),
                                       autofill::REQUIRE_NONE,
                                       autofill::EXTRACT_VALUE,
                                       &form,
                                       NULL));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(4U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.label = ASCIIToUTF16("Phone:");
  expected.name = ASCIIToUTF16("dayphone1");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.label = ASCIIToUTF16("-");
  expected.name = ASCIIToUTF16("dayphone2");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.label = ASCIIToUTF16("-");
  expected.name = ASCIIToUTF16("dayphone3");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.label = ASCIIToUTF16("ext.:");
  expected.name = ASCIIToUTF16("dayphone4");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);
}


TEST_F(FormAutofillTest, MaxLengthFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  Phone:"
           "  <input type=\"text\" maxlength=\"3\" name=\"dayphone1\">"
           "  -"
           "  <input type=\"text\" maxlength=\"3\" name=\"dayphone2\">"
           "  -"
           "  <input type=\"text\" maxlength=\"4\" size=\"5\""
           "         name=\"dayphone3\">"
           "  ext.:"
           "  <input type=\"text\" maxlength=\"5\" name=\"dayphone4\">"
           "  <input type=\"text\" name=\"default1\">"
           "  <input type=\"text\" maxlength=\"-1\" name=\"invalid1\">"
           "  <input type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->document().forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(forms[0],
                                       WebFormControlElement(),
                                       autofill::REQUIRE_NONE,
                                       autofill::EXTRACT_VALUE,
                                       &form,
                                       NULL));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(6U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";

  expected.label = ASCIIToUTF16("Phone:");
  expected.name = ASCIIToUTF16("dayphone1");
  expected.max_length = 3;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.label = ASCIIToUTF16("-");
  expected.name = ASCIIToUTF16("dayphone2");
  expected.max_length = 3;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.label = ASCIIToUTF16("-");
  expected.name = ASCIIToUTF16("dayphone3");
  expected.max_length = 4;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.label = ASCIIToUTF16("ext.:");
  expected.name = ASCIIToUTF16("dayphone4");
  expected.max_length = 5;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  // When unspecified |size|, default is returned.
  expected.label = string16();
  expected.name = ASCIIToUTF16("default1");
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[4]);

  // When invalid |size|, default is returned.
  expected.label = string16();
  expected.name = ASCIIToUTF16("invalid1");
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[5]);
}

// This test re-creates the experience of typing in a field then selecting a
// profile from the Autofill suggestions popup.  The field that is being typed
// into should be filled even though it's not technically empty.
TEST_F(FormAutofillTest, FillFormNonEmptyField) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"email\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Simulate typing by modifying the field value.
  input_element.setValue(ASCIIToUTF16("Wy"));

  // Find the form that contains the input element.
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form, &field,
                                              autofill::REQUIRE_NONE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("Wy");
  expected.is_autofilled = false;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = string16();
  expected.is_autofilled = false;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("email");
  expected.value = string16();
  expected.is_autofilled = false;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  // Preview the form and verify that the cursor position has been updated.
  form.fields[0].value = ASCIIToUTF16("Wyatt");
  form.fields[1].value = ASCIIToUTF16("Earp");
  form.fields[2].value = ASCIIToUTF16("wyatt@example.com");
  PreviewForm(form, input_element);
  EXPECT_EQ(2, input_element.selectionStart());
  EXPECT_EQ(5, input_element.selectionEnd());

  // Fill the form.
  FillForm(form, input_element);

  // Find the newly-filled form that contains the input element.
  FormData form2;
  FormFieldData field2;
  EXPECT_TRUE(FindFormAndFieldForInputElement(input_element, &form2, &field2,
                                              autofill::REQUIRE_NONE));

  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("Wyatt");
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Earp");
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.name = ASCIIToUTF16("email");
  expected.value = ASCIIToUTF16("wyatt@example.com");
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

  // Verify that the cursor position has been updated.
  EXPECT_EQ(5, input_element.selectionStart());
  EXPECT_EQ(5, input_element.selectionEnd());
}

TEST_F(FormAutofillTest, ClearFormWithNode) {
  LoadHTML(
      "<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
      "  <INPUT type=\"text\" id=\"firstname\" value=\"Wyatt\"/>"
      "  <INPUT type=\"text\" id=\"lastname\" value=\"Earp\"/>"
      "  <INPUT type=\"text\" autocomplete=\"off\" id=\"noAC\" value=\"one\"/>"
      "  <INPUT type=\"text\" id=\"notenabled\" disabled=\"disabled\">"
      "  <INPUT type=\"submit\" value=\"Send\"/>"
      "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Set the auto-filled attribute on the firstname element.
  WebInputElement firstname =
      web_frame->document().getElementById("firstname").to<WebInputElement>();
  firstname.setAutofilled(true);

  // Set the value of the disabled attribute.
  WebInputElement notenabled =
      web_frame->document().getElementById("notenabled").to<WebInputElement>();
  notenabled.setValue(WebString::fromUTF8("no clear"));

  // Clear the form.
  EXPECT_TRUE(form_cache.ClearFormWithElement(firstname));

  // Verify that the auto-filled attribute has been turned off.
  EXPECT_FALSE(firstname.isAutofilled());

  // Verify the form is cleared.
  FormData form2;
  FormFieldData field2;
  EXPECT_TRUE(FindFormAndFieldForInputElement(firstname, &form2, &field2,
                                              autofill::REQUIRE_NONE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(4U, fields2.size());

  FormFieldData expected;
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

  expected.name = ASCIIToUTF16("firstname");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = string16();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.name = ASCIIToUTF16("noAC");
  expected.value = string16();
  expected.autocomplete_attribute = "off";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
  expected.autocomplete_attribute = std::string();  // reset

  expected.name = ASCIIToUTF16("notenabled");
  expected.value = ASCIIToUTF16("no clear");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[3]);

  // Verify that the cursor position has been updated.
  EXPECT_EQ(0, firstname.selectionStart());
  EXPECT_EQ(0, firstname.selectionEnd());
}

TEST_F(FormAutofillTest, ClearFormWithNodeContainingSelectOne) {
  LoadHTML(
      "<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
      "  <INPUT type=\"text\" id=\"firstname\" value=\"Wyatt\"/>"
      "  <INPUT type=\"text\" id=\"lastname\" value=\"Earp\"/>"
      "  <SELECT id=\"state\" name=\"state\">"
      "    <OPTION selected>?</OPTION>"
      "    <OPTION>AA</OPTION>"
      "    <OPTION>AE</OPTION>"
      "    <OPTION>AK</OPTION>"
      "  </SELECT>"
      "  <INPUT type=\"submit\" value=\"Send\"/>"
      "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Set the auto-filled attribute on the firstname element.
  WebInputElement firstname =
      web_frame->document().getElementById("firstname").to<WebInputElement>();
  firstname.setAutofilled(true);

  // Set the value of the select-one.
  WebSelectElement select_element =
      web_frame->document().getElementById("state").to<WebSelectElement>();
  select_element.setValue(WebString::fromUTF8("AK"));

  // Clear the form.
  EXPECT_TRUE(form_cache.ClearFormWithElement(firstname));

  // Verify that the auto-filled attribute has been turned off.
  EXPECT_FALSE(firstname.isAutofilled());

  // Verify the form is cleared.
  FormData form2;
  FormFieldData field2;
  EXPECT_TRUE(FindFormAndFieldForInputElement(firstname, &form2, &field2,
                                              autofill::REQUIRE_NONE));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->document().url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());

  FormFieldData expected;

  expected.name = ASCIIToUTF16("firstname");
  expected.value = string16();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = string16();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.name = ASCIIToUTF16("state");
  expected.value = ASCIIToUTF16("?");
  expected.form_control_type = "select-one";
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

  // Verify that the cursor position has been updated.
  EXPECT_EQ(0, firstname.selectionStart());
  EXPECT_EQ(0, firstname.selectionEnd());
}

TEST_F(FormAutofillTest, ClearPreviewedFormWithElement) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"Wyatt\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"email\"/>"
           "  <INPUT type=\"email\" id=\"email2\"/>"
           "  <INPUT type=\"tel\" id=\"phone\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Set the auto-filled attribute.
  WebInputElement firstname =
      web_frame->document().getElementById("firstname").to<WebInputElement>();
  firstname.setAutofilled(true);
  WebInputElement lastname =
      web_frame->document().getElementById("lastname").to<WebInputElement>();
  lastname.setAutofilled(true);
  WebInputElement email =
      web_frame->document().getElementById("email").to<WebInputElement>();
  email.setAutofilled(true);
  WebInputElement email2 =
      web_frame->document().getElementById("email2").to<WebInputElement>();
  email2.setAutofilled(true);
  WebInputElement phone =
      web_frame->document().getElementById("phone").to<WebInputElement>();
  phone.setAutofilled(true);

  // Set the suggested values on two of the elements.
  lastname.setSuggestedValue(ASCIIToUTF16("Earp"));
  email.setSuggestedValue(ASCIIToUTF16("wyatt@earp.com"));
  email2.setSuggestedValue(ASCIIToUTF16("wyatt@earp.com"));
  phone.setSuggestedValue(ASCIIToUTF16("650-777-9999"));

  // Clear the previewed fields.
  EXPECT_TRUE(ClearPreviewedFormWithElement(lastname, false));

  // Fields with empty suggestions suggestions are not modified.
  EXPECT_EQ(ASCIIToUTF16("Wyatt"), firstname.value());
  EXPECT_TRUE(firstname.suggestedValue().isEmpty());
  EXPECT_TRUE(firstname.isAutofilled());

  // Verify the previewed fields are cleared.
  EXPECT_TRUE(lastname.value().isEmpty());
  EXPECT_TRUE(lastname.suggestedValue().isEmpty());
  EXPECT_FALSE(lastname.isAutofilled());
  EXPECT_TRUE(email.value().isEmpty());
  EXPECT_TRUE(email.suggestedValue().isEmpty());
  EXPECT_FALSE(email.isAutofilled());
  EXPECT_TRUE(email2.value().isEmpty());
  EXPECT_TRUE(email2.suggestedValue().isEmpty());
  EXPECT_FALSE(email2.isAutofilled());
  EXPECT_TRUE(phone.value().isEmpty());
  EXPECT_TRUE(phone.suggestedValue().isEmpty());
  EXPECT_FALSE(phone.isAutofilled());

  // Verify that the cursor position has been updated.
  EXPECT_EQ(0, lastname.selectionStart());
  EXPECT_EQ(0, lastname.selectionEnd());
}

TEST_F(FormAutofillTest, ClearPreviewedFormWithNonEmptyInitiatingNode) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"W\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"email\"/>"
           "  <INPUT type=\"email\" id=\"email2\"/>"
           "  <INPUT type=\"tel\" id=\"phone\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Set the auto-filled attribute.
  WebInputElement firstname =
      web_frame->document().getElementById("firstname").to<WebInputElement>();
  firstname.setAutofilled(true);
  WebInputElement lastname =
      web_frame->document().getElementById("lastname").to<WebInputElement>();
  lastname.setAutofilled(true);
  WebInputElement email =
      web_frame->document().getElementById("email").to<WebInputElement>();
  email.setAutofilled(true);
  WebInputElement email2 =
      web_frame->document().getElementById("email2").to<WebInputElement>();
  email2.setAutofilled(true);
  WebInputElement phone =
      web_frame->document().getElementById("phone").to<WebInputElement>();
  phone.setAutofilled(true);


  // Set the suggested values on all of the elements.
  firstname.setSuggestedValue(ASCIIToUTF16("Wyatt"));
  lastname.setSuggestedValue(ASCIIToUTF16("Earp"));
  email.setSuggestedValue(ASCIIToUTF16("wyatt@earp.com"));
  email2.setSuggestedValue(ASCIIToUTF16("wyatt@earp.com"));
  phone.setSuggestedValue(ASCIIToUTF16("650-777-9999"));

  // Clear the previewed fields.
  EXPECT_TRUE(ClearPreviewedFormWithElement(firstname, false));

  // Fields with non-empty values are restored.
  EXPECT_EQ(ASCIIToUTF16("W"), firstname.value());
  EXPECT_TRUE(firstname.suggestedValue().isEmpty());
  EXPECT_FALSE(firstname.isAutofilled());
  EXPECT_EQ(1, firstname.selectionStart());
  EXPECT_EQ(1, firstname.selectionEnd());

  // Verify the previewed fields are cleared.
  EXPECT_TRUE(lastname.value().isEmpty());
  EXPECT_TRUE(lastname.suggestedValue().isEmpty());
  EXPECT_FALSE(lastname.isAutofilled());
  EXPECT_TRUE(email.value().isEmpty());
  EXPECT_TRUE(email.suggestedValue().isEmpty());
  EXPECT_FALSE(email.isAutofilled());
  EXPECT_TRUE(email2.value().isEmpty());
  EXPECT_TRUE(email2.suggestedValue().isEmpty());
  EXPECT_FALSE(email2.isAutofilled());
  EXPECT_TRUE(phone.value().isEmpty());
  EXPECT_TRUE(phone.suggestedValue().isEmpty());
  EXPECT_FALSE(phone.isAutofilled());
}

TEST_F(FormAutofillTest, ClearPreviewedFormWithAutofilledInitiatingNode) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"W\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"email\"/>"
           "  <INPUT type=\"email\" id=\"email2\"/>"
           "  <INPUT type=\"tel\" id=\"phone\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  // Set the auto-filled attribute.
  WebInputElement firstname =
      web_frame->document().getElementById("firstname").to<WebInputElement>();
  firstname.setAutofilled(true);
  WebInputElement lastname =
      web_frame->document().getElementById("lastname").to<WebInputElement>();
  lastname.setAutofilled(true);
  WebInputElement email =
      web_frame->document().getElementById("email").to<WebInputElement>();
  email.setAutofilled(true);
  WebInputElement email2 =
      web_frame->document().getElementById("email2").to<WebInputElement>();
  email2.setAutofilled(true);
  WebInputElement phone =
      web_frame->document().getElementById("phone").to<WebInputElement>();
  phone.setAutofilled(true);

  // Set the suggested values on all of the elements.
  firstname.setSuggestedValue(ASCIIToUTF16("Wyatt"));
  lastname.setSuggestedValue(ASCIIToUTF16("Earp"));
  email.setSuggestedValue(ASCIIToUTF16("wyatt@earp.com"));
  email2.setSuggestedValue(ASCIIToUTF16("wyatt@earp.com"));
  phone.setSuggestedValue(ASCIIToUTF16("650-777-9999"));

  // Clear the previewed fields.
  EXPECT_TRUE(ClearPreviewedFormWithElement(firstname, true));

  // Fields with non-empty values are restored.
  EXPECT_EQ(ASCIIToUTF16("W"), firstname.value());
  EXPECT_TRUE(firstname.suggestedValue().isEmpty());
  EXPECT_TRUE(firstname.isAutofilled());
  EXPECT_EQ(1, firstname.selectionStart());
  EXPECT_EQ(1, firstname.selectionEnd());

  // Verify the previewed fields are cleared.
  EXPECT_TRUE(lastname.value().isEmpty());
  EXPECT_TRUE(lastname.suggestedValue().isEmpty());
  EXPECT_FALSE(lastname.isAutofilled());
  EXPECT_TRUE(email.value().isEmpty());
  EXPECT_TRUE(email.suggestedValue().isEmpty());
  EXPECT_FALSE(email.isAutofilled());
  EXPECT_TRUE(email2.value().isEmpty());
  EXPECT_TRUE(email2.suggestedValue().isEmpty());
  EXPECT_FALSE(email2.isAutofilled());
  EXPECT_TRUE(phone.value().isEmpty());
  EXPECT_TRUE(phone.suggestedValue().isEmpty());
  EXPECT_FALSE(phone.isAutofilled());
}

TEST_F(FormAutofillTest, FormWithNodeIsAutofilled) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"Wyatt\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"email\"/>"
           "  <INPUT type=\"email\" id=\"email2\"/>"
           "  <INPUT type=\"tel\" id=\"phone\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormCache form_cache;
  std::vector<FormData> forms;
  form_cache.ExtractForms(*web_frame, &forms);
  ASSERT_EQ(1U, forms.size());

  WebInputElement firstname =
      web_frame->document().getElementById("firstname").to<WebInputElement>();

  // Auto-filled attribute not set yet.
  EXPECT_FALSE(FormWithElementIsAutofilled(firstname));

  // Set the auto-filled attribute.
  firstname.setAutofilled(true);

  EXPECT_TRUE(FormWithElementIsAutofilled(firstname));
}

// If we have multiple labels per id, the labels concatenated into label string.
TEST_F(FormAutofillTest, MultipleLabelsPerElement) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("First Name:"));
  names.push_back(ASCIIToUTF16("firstname"));
  values.push_back(ASCIIToUTF16("John"));

  labels.push_back(ASCIIToUTF16("Last Name:"));
  names.push_back(ASCIIToUTF16("lastname"));
  values.push_back(ASCIIToUTF16("Smith"));

  labels.push_back(ASCIIToUTF16("Email: xxx@yyy.com"));
  names.push_back(ASCIIToUTF16("email"));
  values.push_back(ASCIIToUTF16("john@example.com"));

  ExpectLabels(
      "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
      "  <LABEL for=\"firstname\"> First Name: </LABEL>"
      "  <LABEL for=\"firstname\"></LABEL>"
      "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
      "  <LABEL for=\"lastname\"></LABEL>"
      "  <LABEL for=\"lastname\"> Last Name: </LABEL>"
      "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
      "  <LABEL for=\"email\"> Email: </LABEL>"
      "  <LABEL for=\"email\"> xxx@yyy.com </LABEL>"
      "    <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
      "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
      "</FORM>",
      labels, names, values);
}

TEST_F(FormAutofillTest, ClickElement) {
  LoadHTML("<BUTTON id=\"link\">Button</BUTTON>"
           "<BUTTON name=\"button\">Button</BUTTON>");
  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  // Successful retrieval by id.
  autofill::WebElementDescriptor clicker;
  clicker.retrieval_method = autofill::WebElementDescriptor::ID;
  clicker.descriptor = "link";
  EXPECT_TRUE(ClickElement(frame->document(), clicker));

  // Successful retrieval by css selector.
  clicker.retrieval_method = autofill::WebElementDescriptor::CSS_SELECTOR;
  clicker.descriptor = "button[name=\"button\"]";
  EXPECT_TRUE(ClickElement(frame->document(), clicker));

  // Unsuccessful retrieval due to invalid CSS selector.
  clicker.descriptor = "^*&";
  EXPECT_FALSE(ClickElement(frame->document(), clicker));

  // Unsuccessful retrieval because element does not exist.
  clicker.descriptor = "#junk";
  EXPECT_FALSE(ClickElement(frame->document(), clicker));
}

TEST_F(FormAutofillTest, SelectOneAsText) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <SELECT id=\"country\">"
           "    <OPTION value=\"AF\">Afghanistan</OPTION>"
           "    <OPTION value=\"AL\">Albania</OPTION>"
           "    <OPTION value=\"DZ\">Algeria</OPTION>"
           "  </SELECT>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  // Set the value of the select-one.
  WebSelectElement select_element =
      frame->document().getElementById("country").to<WebSelectElement>();
  select_element.setValue(WebString::fromUTF8("AL"));

  WebVector<WebFormElement> forms;
  frame->document().forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;

  // Extract the country select-one value as text.
  EXPECT_TRUE(WebFormElementToFormData(
      forms[0], WebFormControlElement(), autofill::REQUIRE_NONE,
      static_cast<autofill::ExtractMask>(
          autofill::EXTRACT_VALUE | autofill::EXTRACT_OPTION_TEXT),
      &form, NULL));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("John");
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Smith");
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("country");
  expected.value = ASCIIToUTF16("Albania");
  expected.form_control_type = "select-one";
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  form.fields.clear();
  // Extract the country select-one value as value.
  EXPECT_TRUE(WebFormElementToFormData(forms[0],
                                       WebFormControlElement(),
                                       autofill::REQUIRE_NONE,
                                       autofill::EXTRACT_VALUE,
                                       &form,
                                       NULL));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->document().url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  ASSERT_EQ(3U, fields.size());

  expected.name = ASCIIToUTF16("firstname");
  expected.value = ASCIIToUTF16("John");
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = ASCIIToUTF16("Smith");
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name = ASCIIToUTF16("country");
  expected.value = ASCIIToUTF16("AL");
  expected.form_control_type = "select-one";
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
}
