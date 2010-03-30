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
#include "webkit/glue/form_data.h"

using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebString;

using webkit_glue::FormData;
using webkit_glue::FormField;

namespace {

typedef RenderViewTest FormManagerTest;

TEST_F(FormManagerTest, ExtractForms) {
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
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
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
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text")),
                      fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[2]);
}

TEST_F(FormManagerTest, ExtractMultipleForms) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>"
           "<FORM name=\"TestForm2\" action=\"http://zoo.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"second\" value=\"Submit\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

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

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(2U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[1]);

  // Second form.
  const FormData& form2 = forms[1];
  EXPECT_EQ(ASCIIToUTF16("TestForm2"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://zoo.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(2U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text")),
                      fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("second"),
                      ASCIIToUTF16("Submit"),
                      ASCIIToUTF16("submit")),
                      fields2[1]);
}

TEST_F(FormManagerTest, GetFormsAutocomplete) {
  // Form is not auto-completable due to autocomplete=off.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\""
           " autocomplete=off>"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

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
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply\" value=\"Send\"/>"
           "</FORM>");

  web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  form_manager.Reset();
  form_manager.ExtractForms(web_frame);

  forms.clear();
  form_manager.GetForms(&forms, FormManager::REQUIRE_AUTOCOMPLETE);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://abc.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(2U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[1]);
}

TEST_F(FormManagerTest, GetFormsElementsEnabled) {
  // The firstname element is not enabled due to disabled being set.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://xyz.com\" method=\"post\">"
           "  <INPUT disabled type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_ELEMENTS_ENABLED);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://xyz.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(2U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("submit"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[1]);
}

TEST_F(FormManagerTest, FindForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

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

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text")),
                      fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[2]);
}

TEST_F(FormManagerTest, FillForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

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

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text")),
                      fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Wyatt"));
  form.fields[1].set_value(ASCIIToUTF16("Earp"));
  EXPECT_TRUE(form_manager.FillForm(form));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindForm(input_element, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Wyatt"),
                      ASCIIToUTF16("text")),
                      fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Earp"),
                      ASCIIToUTF16("text")),
                      fields2[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields2[2]);
}

TEST_F(FormManagerTest, Reset) {
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
           "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <LABEL for=\"lastname\"> Last name: </LABEL>"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text")),
                      fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredFromText) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  First name:"
           "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  Last name:"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text")),
                      fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredFromParagraph) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <P>First name:</P><INPUT type=\"text\" "
           "                           id=\"firstname\" value=\"John\"/>"
           "  <P>Last name:</P>"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text")),
                      fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredFromTableCell) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
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
           "    <TD></TD>"
           "    <TD>"
           "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "    </TD>"
           "  </TR>"
           "</TABLE>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(&forms, FormManager::REQUIRE_NONE);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text")),
                      fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text")),
                      fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit")),
                      fields[2]);
}

}  // namespace
