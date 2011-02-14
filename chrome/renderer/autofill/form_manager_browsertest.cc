// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/renderer/autofill/form_manager.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSelectElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/form_data.h"
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

using autofill::FormManager;

using webkit_glue::FormData;
using webkit_glue::FormField;

class FormManagerTest : public RenderViewTest {
 public:
  FormManagerTest() : RenderViewTest() {}
  virtual ~FormManagerTest() {}

  void ExpectLabels(const char* html,
                    const std::vector<string16>& labels,
                    const std::vector<string16>& names,
                    const std::vector<string16>& values) {
    ASSERT_EQ(labels.size(), names.size());
    ASSERT_EQ(labels.size(), values.size());

    LoadHTML(html);

    WebFrame* web_frame = GetMainFrame();
    ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

    FormManager form_manager;
    form_manager.ExtractForms(web_frame);

    std::vector<FormData> forms;
    form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_AUTOCOMPLETE,
                                 &forms);
    ASSERT_EQ(1U, forms.size());

    const FormData& form = forms[0];
    EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
    EXPECT_EQ(GURL(web_frame->url()), form.origin);
    EXPECT_EQ(GURL("http://cnn.com"), form.action);

    const std::vector<FormField>& fields = form.fields;
    ASSERT_EQ(labels.size(), fields.size());
    for (size_t i = 0; i < labels.size(); ++i) {
      FormField expected = FormField(labels[i],
                                     names[i],
                                     values[i],
                                     ASCIIToUTF16("text"),
                                     WebInputElement::defaultMaxLength(),
                                     false);
      EXPECT_TRUE(fields[i].StrictlyEqualsHack(expected))
          << "Expected \"" << expected << "\", got \"" << fields[i] << "\"";
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
  DISALLOW_COPY_AND_ASSIGN(FormManagerTest);
};

// We should be able to extract a normal text field.
TEST_F(FormManagerTest, WebFormControlElementToFormField) {
  LoadHTML("<INPUT type=\"text\" id=\"element\" value=\"value\"/>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebFormControlElement element = web_element.to<WebFormControlElement>();
  FormField result1;
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_NONE,
                                                &result1);
  EXPECT_TRUE(result1.StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("element"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  FormField result2;
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result2);
  EXPECT_TRUE(result2.StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("element"),
                ASCIIToUTF16("value"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
}

// We should be able to extract a text field with autocomplete="off".
TEST_F(FormManagerTest, WebFormControlElementToFormFieldAutocompleteOff) {
  LoadHTML("<INPUT type=\"text\" id=\"element\" value=\"value\""
           "       autocomplete=\"off\"/>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebFormControlElement element = web_element.to<WebFormControlElement>();
  FormField result;
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result);
  EXPECT_TRUE(result.StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("element"),
                ASCIIToUTF16("value"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
}

// We should be able to extract a text field with maxlength specified.
TEST_F(FormManagerTest, WebFormControlElementToFormFieldMaxLength) {
  LoadHTML("<INPUT type=\"text\" id=\"element\" value=\"value\""
           "       maxlength=\"5\"/>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebFormControlElement element = web_element.to<WebFormControlElement>();
  FormField result;
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result);
  EXPECT_TRUE(result.StrictlyEqualsHack(FormField(string16(),
                                                  ASCIIToUTF16("element"),
                                                  ASCIIToUTF16("value"),
                                                  ASCIIToUTF16("text"),
                                                  5,
                                                  false)));
}

// We should be able to extract a text field that has been autofilled.
TEST_F(FormManagerTest, WebFormControlElementToFormFieldAutofilled) {
  LoadHTML("<INPUT type=\"text\" id=\"element\" value=\"value\"/>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebInputElement element = web_element.to<WebInputElement>();
  element.setAutofilled(true);
  FormField result;
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result);
  EXPECT_TRUE(result.StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("element"),
                ASCIIToUTF16("value"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                true)));
}

// We should be able to extract a <select> field.
TEST_F(FormManagerTest, WebFormControlElementToFormFieldSelect) {
  LoadHTML("<SELECT id=\"element\"/>"
           "  <OPTION value=\"CA\">California</OPTION>"
           "  <OPTION value=\"TX\">Texas</OPTION>"
           "</SELECT>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebElement web_element = frame->document().getElementById("element");
  WebFormControlElement element = web_element.to<WebFormControlElement>();
  FormField result1;
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result1);
  EXPECT_TRUE(result1.StrictlyEqualsHack(FormField(string16(),
                                                   ASCIIToUTF16("element"),
                                                   ASCIIToUTF16("CA"),
                                                   ASCIIToUTF16("select-one"),
                                                   0,
                                                   false)));
  FormField result2;
  FormManager::WebFormControlElementToFormField(
      element,
      static_cast<FormManager::ExtractMask>(FormManager::EXTRACT_VALUE |
                                            FormManager::EXTRACT_OPTION_TEXT),
      &result2);
  EXPECT_TRUE(result2.StrictlyEqualsHack(FormField(string16(),
                                                   ASCIIToUTF16("element"),
                                                   ASCIIToUTF16("California"),
                                                   ASCIIToUTF16("select-one"),
                                                   0,
                                                   false)));
  FormField result3;
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_OPTIONS,
                                                &result3);
  EXPECT_TRUE(result3.StrictlyEqualsHack(FormField(string16(),
                                                   ASCIIToUTF16("element"),
                                                   string16(),
                                                   ASCIIToUTF16("select-one"),
                                                   0,
                                                   false)));
  ASSERT_EQ(2U, result3.option_strings().size());
  EXPECT_EQ(ASCIIToUTF16("CA"), result3.option_strings()[0]);
  EXPECT_EQ(ASCIIToUTF16("TX"), result3.option_strings()[1]);
}

// We should be not extract the value for non-text and non-select fields.
TEST_F(FormManagerTest, WebFormControlElementToFormFieldInvalidType) {
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
  FormField result;
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result);
  EXPECT_TRUE(result.StrictlyEqualsHack(FormField(string16(),
                                                  ASCIIToUTF16("hidden"),
                                                  string16(),
                                                  ASCIIToUTF16("hidden"),
                                                  0,
                                                  false)));

  web_element = frame->document().getElementById("password");
  element = web_element.to<WebFormControlElement>();
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result);
  EXPECT_TRUE(result.StrictlyEqualsHack(FormField(string16(),
                                                  ASCIIToUTF16("password"),
                                                  string16(),
                                                  ASCIIToUTF16("password"),
                                                  0,
                                                  false)));

  web_element = frame->document().getElementById("checkbox");
  element = web_element.to<WebFormControlElement>();
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result);
  EXPECT_TRUE(result.StrictlyEqualsHack(FormField(string16(),
                                                  ASCIIToUTF16("checkbox"),
                                                  string16(),
                                                  ASCIIToUTF16("checkbox"),
                                                  0,
                                                  false)));

  web_element = frame->document().getElementById("radio");
  element = web_element.to<WebFormControlElement>();
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result);
  EXPECT_TRUE(result.StrictlyEqualsHack(FormField(string16(),
                                                  ASCIIToUTF16("radio"),
                                                  string16(),
                                                  ASCIIToUTF16("radio"),
                                                  0,
                                                  false)));

  web_element = frame->document().getElementById("submit");
  element = web_element.to<WebFormControlElement>();
  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                &result);
  EXPECT_TRUE(result.StrictlyEqualsHack(FormField(string16(),
                                                  ASCIIToUTF16("submit"),
                                                  string16(),
                                                  ASCIIToUTF16("submit"),
                                                  0,
                                                  false)));
}

TEST_F(FormManagerTest, WebFormElementToFormData) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <SELECT id=\"state\"/>"
           "    <OPTION value=\"CA\">California</OPTION>"
           "    <OPTION value=\"TX\">Texas</OPTION>"
           "  </SELECT>"
           // The below inputs should be ignored
           "  <INPUT type=\"hidden\" id=\"notvisible\" value=\"apple\"/>"
           "  <INPUT type=\"password\" id=\"password\" value=\"secret\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(FormManager::WebFormElementToFormData(forms[0],
                                                    FormManager::REQUIRE_NONE,
                                                    FormManager::EXTRACT_VALUE,
                                                    &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_TRUE(fields[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                ASCIIToUTF16("John"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("lastname"),
                ASCIIToUTF16("Smith"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("state"),
                ASCIIToUTF16("CA"),
                ASCIIToUTF16("select-one"),
                0,
                false)));
}

TEST_F(FormManagerTest, ExtractForms) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      ASCIIToUTF16("john@example.com"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);
}

TEST_F(FormManagerTest, ExtractMultipleForms) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(2U, forms.size());

  // First form.
  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      ASCIIToUTF16("john@example.com"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);

  // Second form.
  const FormData& form2 = forms[1];
  EXPECT_EQ(ASCIIToUTF16("TestForm2"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://zoo.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Jack"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Adams"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      ASCIIToUTF16("jack@example.com"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);
}

// We should not extract a form if it has too few fillable fields.
TEST_F(FormManagerTest, ExtractFormsTooFewFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  EXPECT_EQ(0U, forms.size());
}

TEST_F(FormManagerTest, GetFormsAutocomplete) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we did load the forms.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // autocomplete=off and we're requiring autocomplete, so no forms returned.
  forms.clear();
  form_manager.GetFormsInFrame(
      web_frame, FormManager::REQUIRE_AUTOCOMPLETE, &forms);
  ASSERT_EQ(0U, forms.size());

  // The firstname element is not auto-completable due to autocomplete=off.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://abc.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\""
           "   autocomplete=off>"
           "  <INPUT type=\"text\" id=\"middlename\" value=\"Jack\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
           "  <INPUT type=\"submit\" name=\"reply\" value=\"Send\"/>"
           "</FORM>");

  web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  form_manager.Reset();
  form_manager.ExtractForms(web_frame);

  forms.clear();
  form_manager.GetFormsInFrame(
      web_frame, FormManager::REQUIRE_AUTOCOMPLETE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://abc.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("middlename"),
                      ASCIIToUTF16("Jack"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      ASCIIToUTF16("john@example.com"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);
}

TEST_F(FormManagerTest, GetFormsElementsEnabled) {
  // The firstname element is not enabled due to disabled being set.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://xyz.com\" method=\"post\">"
           "  <INPUT disabled type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"middlename\" value=\"Jack\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"text\" id=\"email\" value=\"jack@example.com\"/>"
           "  <INPUT type=\"submit\" name=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_ENABLED, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://xyz.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("middlename"),
                      ASCIIToUTF16("Jack"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      ASCIIToUTF16("jack@example.com"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);
}

TEST_F(FormManagerTest, FindForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form and verify it's the correct form.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      ASCIIToUTF16("john@example.com"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);
}

TEST_F(FormManagerTest, FillForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"hidden\" id=\"imhidden\"/>"
           "  <INPUT type=\"text\" id=\"notempty\" value=\"Hi\"/>"
           "  <INPUT type=\"text\" autocomplete=\"off\" id=\"noautocomplete\"/>"
           "  <INPUT type=\"text\" disabled=\"disabled\" id=\"notenabled\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(5U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("notempty"),
                      ASCIIToUTF16("Hi"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("noautocomplete"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[3]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("notenabled"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[4]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Wyatt"));
  form.fields[1].set_value(ASCIIToUTF16("Earp"));
  form.fields[2].set_value(ASCIIToUTF16("Alpha"));
  form.fields[3].set_value(ASCIIToUTF16("Beta"));
  form.fields[4].set_value(ASCIIToUTF16("Gamma"));
  EXPECT_TRUE(form_manager.FillForm(form, input_element));

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
}

TEST_F(FormManagerTest, PreviewForm) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(5U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("notempty"),
                      ASCIIToUTF16("Hi"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("noautocomplete"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[3]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("notenabled"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[4]);

  // Preview the form.
  form.fields[0].set_value(ASCIIToUTF16("Wyatt"));
  form.fields[1].set_value(ASCIIToUTF16("Earp"));
  form.fields[2].set_value(ASCIIToUTF16("Alpha"));
  form.fields[3].set_value(ASCIIToUTF16("Beta"));
  form.fields[4].set_value(ASCIIToUTF16("Gamma"));
  EXPECT_TRUE(form_manager.PreviewForm(form, input_element));

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

TEST_F(FormManagerTest, Reset) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"text\" id=\"email\" value=\"john@exmaple.com\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // There should be no forms after the call to Reset.
  form_manager.Reset();

  forms.clear();
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(0U, forms.size());
}

TEST_F(FormManagerTest, Labels) {
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

TEST_F(FormManagerTest, LabelsWithSpans) {
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

// This test is different from FormManagerTest.Labels in that the label elements
// for= attribute is set to the name of the form control element it is a label
// for instead of the id of the form control element.  This is invalid because
// the for= attribute must be set to the id of the form control element;
// however, current label parsing code will extract the text from the previous
// label element and apply it to the following input field.
TEST_F(FormManagerTest, InvalidLabels) {
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

// This test has four form control elements, only one of which has a label
// element associated with it.  The second element is disabled because of the
// autocomplete=off attribute.
TEST_F(FormManagerTest, OneLabelElementFirstControlElementDisabled) {
  ExpectJohnSmithLabels(
           "<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  First name:"
           "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  Middle name:"
           "    <INPUT type=\"text\" id=\"middlename\" value=\"Jack\""
           "           autocomplete=\"off\"/>"
           "  <LABEL for=\"lastname\">Last name: </LABEL>"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  Email:"
           "    <INPUT type=\"text\" id=\"email\" value=\"john@example.com\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");
}

TEST_F(FormManagerTest, LabelsInferredFromText) {
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

TEST_F(FormManagerTest, LabelsInferredFromParagraph) {
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

TEST_F(FormManagerTest, LabelsInferredFromTableCell) {
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

TEST_F(FormManagerTest, LabelsInferredFromTableCellNested) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("First name:Bogus"));
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

TEST_F(FormManagerTest, LabelsInferredFromTableEmptyTDs) {
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

TEST_F(FormManagerTest, LabelsInferredFromTableLabels) {
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

TEST_F(FormManagerTest, LabelsInferredFromTableTDInterveningElements) {
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

TEST_F(FormManagerTest, LabelsInferredFromDefinitionList) {
  std::vector<string16> labels, names, values;

  labels.push_back(ASCIIToUTF16("*First name:Bogus"));
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

TEST_F(FormManagerTest, LabelsInferredWithSameName) {
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

TEST_F(FormManagerTest, LabelsInferredWithImageTags) {
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

TEST_F(FormManagerTest, LabelsInferredFromDivTable) {
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

TEST_F(FormManagerTest, FillFormMaxLength) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" maxlength=\"5\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" maxlength=\"7\"/>"
           "  <INPUT type=\"text\" id=\"email\" maxlength=\"9\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      5,
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      7,
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      string16(),
                      ASCIIToUTF16("text"),
                      9,
                      false),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Brother"));
  form.fields[1].set_value(ASCIIToUTF16("Jonathan"));
  form.fields[2].set_value(ASCIIToUTF16("brotherj@example.com"));
  EXPECT_TRUE(form_manager.FillForm(form, WebNode()));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_TRUE(fields2[0].StrictlyEqualsHack(FormField(string16(),
                                                      ASCIIToUTF16("firstname"),
                                                      ASCIIToUTF16("Broth"),
                                                      ASCIIToUTF16("text"),
                                                      5,
                                                      false)));
  EXPECT_TRUE(fields2[1].StrictlyEqualsHack(FormField(string16(),
                                                      ASCIIToUTF16("lastname"),
                                                      ASCIIToUTF16("Jonatha"),
                                                      ASCIIToUTF16("text"),
                                                      7,
                                                      false)));
  EXPECT_TRUE(fields2[2].StrictlyEqualsHack(FormField(string16(),
                                                      ASCIIToUTF16("email"),
                                                      ASCIIToUTF16("brotherj@"),
                                                      ASCIIToUTF16("text"),
                                                      9,
                                                      false)));
}

// This test uses negative values of the maxlength attribute for input elements.
// In this case, the maxlength of the input elements is set to the default
// maxlength (defined in WebKit.)
TEST_F(FormManagerTest, FillFormNegativeMaxLength) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" maxlength=\"-1\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" maxlength=\"-10\"/>"
           "  <INPUT type=\"text\" id=\"email\" maxlength=\"-13\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Brother"));
  form.fields[1].set_value(ASCIIToUTF16("Jonathan"));
  form.fields[2].set_value(ASCIIToUTF16("brotherj@example.com"));
  EXPECT_TRUE(form_manager.FillForm(form, WebNode()));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_TRUE(fields2[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                ASCIIToUTF16("Brother"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields2[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("lastname"),
                ASCIIToUTF16("Jonathan"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields2[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("email"),
                ASCIIToUTF16("brotherj@example.com"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
}

// This test sends a FormData object to FillForm with more fields than are in
// the cached WebFormElement.  In this case, we only fill out the fields that
// match between the FormData object and the WebFormElement.
TEST_F(FormManagerTest, FillFormMoreFormDataFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // After the field modification, the fields in |form| will look like:
  //  prefix
  //  firstname
  //  hidden
  //  middlename
  //  second
  //  lastname
  //  postfix
  FormData* form = &forms[0];

  FormField field1(string16(),
                   ASCIIToUTF16("prefix"),
                   string16(),
                   ASCIIToUTF16("text"),
                   WebInputElement::defaultMaxLength(),
                   false);
  form->fields.insert(form->fields.begin(), field1);

  FormField field2(string16(),
                   ASCIIToUTF16("hidden"),
                   string16(),
                   ASCIIToUTF16("text"),
                   WebInputElement::defaultMaxLength(),
                   false);
  form->fields.insert(form->fields.begin() + 2, field2);

  FormField field3(string16(),
                   ASCIIToUTF16("second"),
                   string16(),
                   ASCIIToUTF16("text"),
                   WebInputElement::defaultMaxLength(),
                   false);
  form->fields.insert(form->fields.begin() + 4, field3);

  FormField field4(string16(),
                   ASCIIToUTF16("postfix"),
                   string16(),
                   ASCIIToUTF16("text"),
                   WebInputElement::defaultMaxLength(),
                   false);
  form->fields.insert(form->fields.begin() + 6, field4);

  // Fill the form.
  form->fields[0].set_value(ASCIIToUTF16("Alpha"));
  form->fields[1].set_value(ASCIIToUTF16("Brother"));
  form->fields[2].set_value(ASCIIToUTF16("Abracadabra"));
  form->fields[3].set_value(ASCIIToUTF16("Joseph"));
  form->fields[4].set_value(ASCIIToUTF16("Beta"));
  form->fields[5].set_value(ASCIIToUTF16("Jonathan"));
  form->fields[6].set_value(ASCIIToUTF16("Omega"));
  EXPECT_TRUE(form_manager.FillForm(*form, WebNode()));

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields = form2.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_TRUE(fields[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                ASCIIToUTF16("Brother"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("middlename"),
                ASCIIToUTF16("Joseph"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("lastname"),
                ASCIIToUTF16("Jonathan"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
}

// This test sends a FormData object to FillForm with fewer fields than are in
// the cached WebFormElement.  In this case, we only fill out the fields that
// match between the FormData object and the WebFormElement.
TEST_F(FormManagerTest, FillFormFewerFormDataFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"prefix\"/>"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"hidden\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"second\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"postfix\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // After the field modification, the fields in |form| will look like:
  //  firstname
  //  middlename
  //  lastname
  FormData* form = &forms[0];
  form->fields.erase(form->fields.begin());
  form->fields.erase(form->fields.begin() + 1);
  form->fields.erase(form->fields.begin() + 2);
  form->fields.erase(form->fields.begin() + 3);

  // Fill the form.
  form->fields[0].set_value(ASCIIToUTF16("Brother"));
  form->fields[1].set_value(ASCIIToUTF16("Joseph"));
  form->fields[2].set_value(ASCIIToUTF16("Jonathan"));
  EXPECT_TRUE(form_manager.FillForm(*form, WebNode()));

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields = form2.fields;
  ASSERT_EQ(7U, fields.size());
  EXPECT_TRUE(fields[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("prefix"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                ASCIIToUTF16("Brother"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("hidden"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[3].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("middlename"),
                ASCIIToUTF16("Joseph"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[4].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("second"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[5].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("lastname"),
                ASCIIToUTF16("Jonathan"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[6].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("postfix"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
}

// This test sends a FormData object to FillForm with a field changed from
// those in the cached WebFormElement.  In this case, we only fill out the
// fields that match between the FormData object and the WebFormElement.
TEST_F(FormManagerTest, FillFormChangedFormDataFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // After the field modification, the fields in |form| will look like:
  //  firstname
  //  middlename
  //  lastname
  FormData* form = &forms[0];

  // Fill the form.
  form->fields[0].set_value(ASCIIToUTF16("Brother"));
  form->fields[1].set_value(ASCIIToUTF16("Joseph"));
  form->fields[2].set_value(ASCIIToUTF16("Jonathan"));

  // Alter the label and name used for matching.
  form->fields[1].set_label(ASCIIToUTF16("bogus"));
  form->fields[1].set_name(ASCIIToUTF16("bogus"));

  EXPECT_TRUE(form_manager.FillForm(*form, WebNode()));

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields = form2.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_TRUE(fields[0].StrictlyEqualsHack(FormField(string16(),
                                           ASCIIToUTF16("firstname"),
                                           ASCIIToUTF16("Brother"),
                                           ASCIIToUTF16("text"),
                                           WebInputElement::defaultMaxLength(),
                                           false)));
  EXPECT_TRUE(fields[1].StrictlyEqualsHack(FormField(string16(),
                                           ASCIIToUTF16("middlename"),
                                           string16(),
                                           ASCIIToUTF16("text"),
                                           WebInputElement::defaultMaxLength(),
                                           false)));
  EXPECT_TRUE(fields[2].StrictlyEqualsHack(FormField(string16(),
                                           ASCIIToUTF16("lastname"),
                                           ASCIIToUTF16("Jonathan"),
                                           ASCIIToUTF16("text"),
                                           WebInputElement::defaultMaxLength(),
                                           false)));
}

// This test sends a FormData object to FillForm with fewer fields than are in
// the cached WebFormElement.  In this case, we only fill out the fields that
// match between the FormData object and the WebFormElement.
TEST_F(FormManagerTest, FillFormExtraFieldInCache) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"postfix\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // After the field modification, the fields in |form| will look like:
  //  firstname
  //  middlename
  //  lastname
  FormData* form = &forms[0];
  form->fields.pop_back();

  // Fill the form.
  form->fields[0].set_value(ASCIIToUTF16("Brother"));
  form->fields[1].set_value(ASCIIToUTF16("Joseph"));
  form->fields[2].set_value(ASCIIToUTF16("Jonathan"));
  EXPECT_TRUE(form_manager.FillForm(*form, WebNode()));

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields = form2.fields;
  ASSERT_EQ(4U, fields.size());
  EXPECT_TRUE(fields[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                ASCIIToUTF16("Brother"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("middlename"),
                ASCIIToUTF16("Joseph"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("lastname"),
                ASCIIToUTF16("Jonathan"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[3].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("postfix"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
}

TEST_F(FormManagerTest, FillFormEmptyName) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"email\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Wyatt"));
  form.fields[1].set_value(ASCIIToUTF16("Earp"));
  form.fields[2].set_value(ASCIIToUTF16("wyatt@example.com"));
  EXPECT_TRUE(form_manager.FillForm(form, WebNode()));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Wyatt"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Earp"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      ASCIIToUTF16("wyatt@example.com"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[2]);
}

TEST_F(FormManagerTest, FillFormEmptyFormNames) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(2U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("apple");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(string16(), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://abc.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("apple"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("banana"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("cantelope"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Red"));
  form.fields[1].set_value(ASCIIToUTF16("Yellow"));
  form.fields[2].set_value(ASCIIToUTF16("Also Yellow"));
  EXPECT_TRUE(form_manager.FillForm(form, WebNode()));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(string16(), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://abc.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("apple"),
                      ASCIIToUTF16("Red"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("banana"),
                      ASCIIToUTF16("Yellow"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("cantelope"),
                      ASCIIToUTF16("Also Yellow"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[2]);
}

TEST_F(FormManagerTest, ThreePartPhone) {
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
  frame->forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(FormManager::WebFormElementToFormData(forms[0],
                                                    FormManager::REQUIRE_NONE,
                                                    FormManager::EXTRACT_VALUE,
                                                    &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(4U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("Phone:"),
                      ASCIIToUTF16("dayphone1"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone2"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone3"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);
  EXPECT_EQ(FormField(ASCIIToUTF16("ext.:"),
                      ASCIIToUTF16("dayphone4"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[3]);
}


TEST_F(FormManagerTest, MaxLengthFields) {
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
  frame->forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(FormManager::WebFormElementToFormData(forms[0],
                                                    FormManager::REQUIRE_NONE,
                                                    FormManager::EXTRACT_VALUE,
                                                    &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(6U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("Phone:"),
                      ASCIIToUTF16("dayphone1"),
                      string16(),
                      ASCIIToUTF16("text"),
                      3,
                      false),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone2"),
                      string16(),
                      ASCIIToUTF16("text"),
                      3,
                      false),
            fields[1]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone3"),
                      string16(),
                      ASCIIToUTF16("text"),
                      4,
                      false),
            fields[2]);
  EXPECT_EQ(FormField(ASCIIToUTF16("ext.:"),
                      ASCIIToUTF16("dayphone4"),
                      string16(),
                      ASCIIToUTF16("text"),
                      5,
                      false),
            fields[3]);
  // When unspecified |size|, default is returned.
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("default1"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[4]);
  // When invalid |size|, default is returned.
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("invalid1"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[5]);
}

// This test re-creates the experience of typing in a field then selecting a
// profile from the AutoFill suggestions popup.  The field that is being typed
// into should be filled even though it's not technically empty.
TEST_F(FormManagerTest, FillFormNonEmptyField) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"email\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Simulate typing by modifying the field value.
  input_element.setValue(ASCIIToUTF16("Wy"));

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      string16(),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Wyatt"));
  form.fields[1].set_value(ASCIIToUTF16("Earp"));
  form.fields[2].set_value(ASCIIToUTF16("wyatt@example.com"));
  EXPECT_TRUE(form_manager.FillForm(form, input_element));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Wyatt"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Earp"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("email"),
                      ASCIIToUTF16("wyatt@example.com"),
                      ASCIIToUTF16("text"),
                      WebInputElement::defaultMaxLength(),
                      false),
            fields2[2]);

  // Verify that the cursor position has been updated.
  EXPECT_EQ(5, input_element.selectionStart());
  EXPECT_EQ(5, input_element.selectionEnd());
}

TEST_F(FormManagerTest, ClearFormWithNode) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
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
  EXPECT_TRUE(form_manager.ClearFormWithNode(firstname));

  // Verify that the auto-filled attribute has been turned off.
  EXPECT_FALSE(firstname.isAutofilled());

  // Verify the form is cleared.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      firstname, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(4U, fields2.size());
  EXPECT_TRUE(fields2[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields2[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("lastname"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields2[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("noAC"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields2[3].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("notenabled"),
                ASCIIToUTF16("no clear"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));

  // Verify that the cursor position has been updated.
  EXPECT_EQ(0, firstname.selectionStart());
  EXPECT_EQ(0, firstname.selectionEnd());
}

TEST_F(FormManagerTest, ClearFormWithNodeContainingSelectOne) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
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
  EXPECT_TRUE(form_manager.ClearFormWithNode(firstname));

  // Verify that the auto-filled attribute has been turned off.
  EXPECT_FALSE(firstname.isAutofilled());

  // Verify the form is cleared.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      firstname, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_TRUE(fields2[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields2[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("lastname"),
                string16(),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields2[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("state"),
                ASCIIToUTF16("?"),
                ASCIIToUTF16("select-one"),
                0,
                false)));

  // Verify that the cursor position has been updated.
  EXPECT_EQ(0, firstname.selectionStart());
  EXPECT_EQ(0, firstname.selectionEnd());
}

TEST_F(FormManagerTest, ClearPreviewedFormWithNode) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
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
  EXPECT_TRUE(form_manager.ClearPreviewedFormWithNode(lastname, false));

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

TEST_F(FormManagerTest, ClearPreviewedFormWithNonEmptyInitiatingNode) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
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
  EXPECT_TRUE(form_manager.ClearPreviewedFormWithNode(firstname, false));

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

TEST_F(FormManagerTest, ClearPreviewedFormWithAutofilledInitiatingNode) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
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
  EXPECT_TRUE(form_manager.ClearPreviewedFormWithNode(firstname, true));

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

TEST_F(FormManagerTest, FormWithNodeIsAutoFilled) {
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

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetFormsInFrame(web_frame, FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  WebInputElement firstname =
      web_frame->document().getElementById("firstname").to<WebInputElement>();

  // Auto-filled attribute not set yet.
  EXPECT_FALSE(form_manager.FormWithNodeIsAutoFilled(firstname));

  // Set the auto-filled attribute.
  firstname.setAutofilled(true);

  EXPECT_TRUE(form_manager.FormWithNodeIsAutoFilled(firstname));
}

TEST_F(FormManagerTest, LabelForElementHidden) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <LABEL for=\"firstname\"> First name: </LABEL>"
           "    <INPUT type=\"hidden\" id=\"firstname\" value=\"John\"/>"
           "  <LABEL for=\"lastname\"> Last name: </LABEL>"
           "    <INPUT type=\"hidden\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  WebElement e = web_frame->document().getElementById("firstname");
  WebFormControlElement firstname = e.to<WebFormControlElement>();

  // Hidden form control element should not have a label set.
  FormManager form_manager;
  EXPECT_EQ(string16(), form_manager.LabelForElement(firstname));
}

TEST_F(FormManagerTest, SelectOneAsText) {
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
  frame->forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;

  // Extract the country select-one value as text.
  EXPECT_TRUE(FormManager::WebFormElementToFormData(
      forms[0], FormManager::REQUIRE_NONE,
      static_cast<FormManager::ExtractMask>(FormManager::EXTRACT_VALUE |
          FormManager::EXTRACT_OPTION_TEXT),
          &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_TRUE(fields[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                ASCIIToUTF16("John"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("lastname"),
                ASCIIToUTF16("Smith"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("country"),
                ASCIIToUTF16("Albania"),
                ASCIIToUTF16("select-one"),
                0,
                false)));

  form.fields.clear();
  // Extract the country select-one value as value.
  EXPECT_TRUE(FormManager::WebFormElementToFormData(forms[0],
                                                    FormManager::REQUIRE_NONE,
                                                    FormManager::EXTRACT_VALUE,
                                                    &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  ASSERT_EQ(3U, fields.size());
  EXPECT_TRUE(fields[0].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("firstname"),
                ASCIIToUTF16("John"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[1].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("lastname"),
                ASCIIToUTF16("Smith"),
                ASCIIToUTF16("text"),
                WebInputElement::defaultMaxLength(),
                false)));
  EXPECT_TRUE(fields[2].StrictlyEqualsHack(
      FormField(string16(),
                ASCIIToUTF16("country"),
                ASCIIToUTF16("AL"),
                ASCIIToUTF16("select-one"),
                0,
                false)));
}
