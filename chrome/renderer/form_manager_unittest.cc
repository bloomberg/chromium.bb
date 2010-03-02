// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/form_manager.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebString;

class FormManagerTest : public RenderViewTest {
 public:
  FormManagerTest() {}
};

TEST_F(FormManagerTest, ExtractForms) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\">"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\">"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<string16>& labels = form.labels;
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(string16(), labels[1]);
  EXPECT_EQ(string16(), labels[2]);

  const std::vector<string16>& elements = form.elements;
  ASSERT_EQ(3U, elements.size());
  EXPECT_EQ(ASCIIToUTF16("firstname"), elements[0]);
  EXPECT_EQ(ASCIIToUTF16("lastname"), elements[1]);
  EXPECT_EQ(ASCIIToUTF16("reply-send"), elements[2]);

  const std::vector<string16>& values = form.values;
  ASSERT_EQ(3U, values.size());
  EXPECT_EQ(ASCIIToUTF16("John"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Smith"), values[1]);
  EXPECT_EQ(ASCIIToUTF16("Send"), values[2]);
}

TEST_F(FormManagerTest, ExtractMultipleForms) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\">"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>"
           "<FORM name=\"TestForm2\" action=\"http://zoo.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\">"
           "  <INPUT type=\"submit\" name=\"second\" value=\"Submit\">"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(2U, forms.size());

  // First form.
  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<string16>& labels = form.labels;
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(string16(), labels[1]);

  const std::vector<string16>& elements = form.elements;
  ASSERT_EQ(2U, elements.size());
  EXPECT_EQ(ASCIIToUTF16("firstname"), elements[0]);
  EXPECT_EQ(ASCIIToUTF16("reply-send"), elements[1]);

  const std::vector<string16>& values = form.values;
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("John"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Send"), values[1]);

  // Second form.
  const FormData& form2 = forms[1];
  EXPECT_EQ(ASCIIToUTF16("TestForm2"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://zoo.com"), form2.action);

  const std::vector<string16>& labels2 = form2.labels;
  ASSERT_EQ(2U, labels2.size());
  EXPECT_EQ(string16(), labels2[0]);
  EXPECT_EQ(string16(), labels2[1]);

  const std::vector<string16>& elements2 = form2.elements;
  ASSERT_EQ(2U, elements2.size());
  EXPECT_EQ(ASCIIToUTF16("lastname"), elements2[0]);
  EXPECT_EQ(ASCIIToUTF16("second"), elements2[1]);

  const std::vector<string16>& values2 = form2.values;
  ASSERT_EQ(2U, values2.size());
  EXPECT_EQ(ASCIIToUTF16("Smith"), values2[0]);
  EXPECT_EQ(ASCIIToUTF16("Submit"), values2[1]);
}

TEST_F(FormManagerTest, GetFormsAutocomplete) {
  // Form is not auto-completable due to autocomplete=off.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\""
           " autocomplete=off>"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\">"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we did load the forms.
  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  // autocomplete=off and we're requiring autocomplete, so no forms returned.
  forms.clear();
  form_manager.GetForms(&forms, FormManager::REQUIRE_AUTOCOMPLETE);
  ASSERT_EQ(0U, forms.size());

  // The firstname element is not auto-completable due to autocomplete=off.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://abc.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\""
           "   autocomplete=off>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\">"
           "  <INPUT type=\"submit\" name=\"reply\" value=\"Send\">"
           "</FORM>");

  web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  form_manager.Reset();
  form_manager.ExtractForms(web_frame);

  forms.clear();
  form_manager.GetForms(&forms, FormManager::REQUIRE_AUTOCOMPLETE);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://abc.com"), form.action);

  const std::vector<string16>& labels = form.labels;
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(string16(), labels[1]);

  const std::vector<string16>& elements = form.elements;
  ASSERT_EQ(2U, elements.size());
  EXPECT_EQ(ASCIIToUTF16("lastname"), elements[0]);
  EXPECT_EQ(ASCIIToUTF16("reply"), elements[1]);

  const std::vector<string16>& values = form.values;
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Smith"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Send"), values[1]);
}

TEST_F(FormManagerTest, GetFormsElementsEnabled) {
  // The firstname element is not enabled due to disabled being set.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://xyz.com\" method=\"post\">"
           "  <INPUT disabled type=\"text\" id=\"firstname\" value=\"John\">"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\">"
           "  <INPUT type=\"submit\" name=\"submit\" value=\"Send\">"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_ELEMENTS_ENABLED);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://xyz.com"), form.action);

  const std::vector<string16>& labels = form.labels;
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(string16(), labels[1]);

  const std::vector<string16>& elements = form.elements;
  ASSERT_EQ(2U, elements.size());
  EXPECT_EQ(ASCIIToUTF16("lastname"), elements[0]);
  EXPECT_EQ(ASCIIToUTF16("submit"), elements[1]);

  const std::vector<string16>& values = form.values;
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Smith"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Send"), values[1]);
}

TEST_F(FormManagerTest, FindForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\">"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\">"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element =
      web_frame->document().getElementById(WebString::fromUTF8("firstname"));
  WebInputElement input_element = element.toElement<WebInputElement>();

  // Find the form and verify it's the correct form.
  FormData form;
  EXPECT_TRUE(form_manager.FindForm(input_element, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<string16>& labels = form.labels;
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(string16(), labels[1]);
  EXPECT_EQ(string16(), labels[2]);

  const std::vector<string16>& elements = form.elements;
  ASSERT_EQ(3U, elements.size());
  EXPECT_EQ(ASCIIToUTF16("firstname"), elements[0]);
  EXPECT_EQ(ASCIIToUTF16("lastname"), elements[1]);
  EXPECT_EQ(ASCIIToUTF16("reply-send"), elements[2]);

  const std::vector<string16>& values = form.values;
  ASSERT_EQ(3U, values.size());
  EXPECT_EQ(ASCIIToUTF16("John"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Smith"), values[1]);
  EXPECT_EQ(ASCIIToUTF16("Send"), values[2]);
}

TEST_F(FormManagerTest, FillForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\">"
           "  <INPUT type=\"text\" id=\"lastname\">"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element =
      web_frame->document().getElementById(WebString::fromUTF8("firstname"));
  WebInputElement input_element = element.toElement<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindForm(input_element, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<string16>& labels = form.labels;
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(string16(), labels[1]);
  EXPECT_EQ(string16(), labels[2]);

  const std::vector<string16>& elements = form.elements;
  ASSERT_EQ(3U, elements.size());
  EXPECT_EQ(ASCIIToUTF16("firstname"), elements[0]);
  EXPECT_EQ(ASCIIToUTF16("lastname"), elements[1]);
  EXPECT_EQ(ASCIIToUTF16("reply-send"), elements[2]);

  // Verify that the text fields have no values.
  const std::vector<string16>& values = form.values;
  ASSERT_EQ(3U, values.size());
  EXPECT_EQ(string16(), values[0]);
  EXPECT_EQ(string16(), values[1]);
  EXPECT_EQ(ASCIIToUTF16("Send"), values[2]);

  // Fill the form.
  form.values[0] = ASCIIToUTF16("Wyatt");
  form.values[1] = ASCIIToUTF16("Earp");
  EXPECT_TRUE(form_manager.FillForm(form));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindForm(input_element, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<string16>& labels2 = form2.labels;
  ASSERT_EQ(3U, labels2.size());
  EXPECT_EQ(string16(), labels2[0]);
  EXPECT_EQ(string16(), labels2[1]);
  EXPECT_EQ(string16(), labels2[2]);

  const std::vector<string16>& elements2 = form2.elements;
  ASSERT_EQ(3U, elements2.size());
  EXPECT_EQ(ASCIIToUTF16("firstname"), elements2[0]);
  EXPECT_EQ(ASCIIToUTF16("lastname"), elements2[1]);
  EXPECT_EQ(ASCIIToUTF16("reply-send"), elements2[2]);

  // Verify that the text fields have no values.
  const std::vector<string16>& values2 = form2.values;
  ASSERT_EQ(3U, values2.size());
  EXPECT_EQ(ASCIIToUTF16("Wyatt"), values2[0]);
  EXPECT_EQ(ASCIIToUTF16("Earp"), values2[1]);
  EXPECT_EQ(ASCIIToUTF16("Send"), values2[2]);
}

TEST_F(FormManagerTest, Reset) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\">"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\">"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  // There should be no forms after the call to Reset.
  form_manager.Reset();

  forms.clear();
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(0U, forms.size());
}

TEST_F(FormManagerTest, Labels) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <LABEL for=\"firstname\"> First name: </LABEL>"
           "    <INPUT type=\"text\" id=\"firstname\" value=\"John\">"
           "  <LABEL for=\"lastname\"> Last name: </LABEL>"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\">"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<string16>& labels = form.labels;
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("First name:"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Last name:"), labels[1]);
  EXPECT_EQ(string16(), labels[2]);

  const std::vector<string16>& elements = form.elements;
  ASSERT_EQ(3U, elements.size());
  EXPECT_EQ(ASCIIToUTF16("firstname"), elements[0]);
  EXPECT_EQ(ASCIIToUTF16("lastname"), elements[1]);
  EXPECT_EQ(ASCIIToUTF16("reply-send"), elements[2]);

  const std::vector<string16>& values = form.values;
  ASSERT_EQ(3U, values.size());
  EXPECT_EQ(ASCIIToUTF16("John"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Smith"), values[1]);
  EXPECT_EQ(ASCIIToUTF16("Send"), values[2]);
}
