// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

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

typedef Tuple<int,
              autofill::FormData,
              autofill::FormFieldData,
              gfx::RectF,
              bool> AutofillQueryParam;

class AutofillRendererTest : public ChromeRenderViewTest {
 public:
  AutofillRendererTest() {}
  ~AutofillRendererTest() override {}

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Don't want any delay for form state sync changes. This will still post a
    // message so updates will get coalesced, but as soon as we spin the message
    // loop, it will generate an update.
    SendContentStateImmediately();
  }

  void SimulateRequestAutocompleteResult(
      blink::WebFrame* invoking_frame,
      const blink::WebFormElement::AutocompleteResult& result,
      const base::string16& message) {
    AutofillMsg_RequestAutocompleteResult msg(0, result, message, FormData());
    content::RenderFrame::FromWebFrame(invoking_frame)->OnMessageReceived(msg);
  }

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

  // Verify that "FormsSeen" sends the expected number of fields.
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Param params;
  AutofillHostMsg_FormsSeen::Read(message, &params);
  std::vector<FormData> forms = get<0>(params);
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(4UL, forms[0].fields.size());

  FormFieldData expected;

  expected.name = ASCIIToUTF16("firstname");
  expected.value = base::string16();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[0]);

  expected.name = ASCIIToUTF16("middlename");
  expected.value = base::string16();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[1]);

  expected.name = ASCIIToUTF16("lastname");
  expected.value = base::string16();
  expected.form_control_type = "text";
  expected.autocomplete_attribute = "off";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[2]);
  expected.autocomplete_attribute = std::string();  // reset

  expected.name = ASCIIToUTF16("state");
  expected.value = ASCIIToUTF16("?");
  expected.form_control_type = "select-one";
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[3]);

  render_thread_->sink().ClearMessages();

  // Dynamically create a new form. A new message should be sent for it, but
  // not for the previous form.
  ExecuteJavaScript(
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
  msg_loop_.RunUntilIdle();

  message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Read(message, &params);
  forms = get<0>(params);
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(3UL, forms[0].fields.size());

  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();

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

  // Verify that "FormsSeen" isn't sent, as there are too few fields.
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Param params;
  AutofillHostMsg_FormsSeen::Read(message, &params);
  const std::vector<FormData>& forms = get<0>(params);
  ASSERT_EQ(0UL, forms.size());
}

TEST_F(AutofillRendererTest, ShowAutofillWarning) {
  LoadHTML("<form method='POST' autocomplete='Off'>"
           "  <input id='firstname' autocomplete='OFF'/>"
           "  <input id='middlename'/>"
           "  <input id='lastname'/>"
           "</form>");

  // Verify that "QueryFormFieldAutofill" isn't sent prior to a user
  // interaction.
  const IPC::Message* message0 = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_QueryFormFieldAutofill::ID);
  EXPECT_EQ(nullptr, message0);

  WebFrame* web_frame = GetMainFrame();
  WebDocument document = web_frame->document();
  WebInputElement firstname =
      document.getElementById("firstname").to<WebInputElement>();
  WebInputElement middlename =
      document.getElementById("middlename").to<WebInputElement>();

  // Simulate attempting to Autofill the form from the first element, which
  // specifies autocomplete="off".  This should still trigger an IPC which
  // shouldn't display warnings.
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(firstname, true);
  const IPC::Message* message1 = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_QueryFormFieldAutofill::ID);
  EXPECT_NE(nullptr, message1);

  AutofillQueryParam query_param;
  AutofillHostMsg_QueryFormFieldAutofill::Read(message1, &query_param);
  EXPECT_FALSE(get<4>(query_param));
  render_thread_->sink().ClearMessages();

  // Simulate attempting to Autofill the form from the second element, which
  // does not specify autocomplete="off".  This should trigger an IPC that will
  // show warnings, as we *do* show warnings for elements that don't themselves
  // set autocomplete="off", but for which the form does.
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(middlename, true);
  const IPC::Message* message2 = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_QueryFormFieldAutofill::ID);
  ASSERT_NE(nullptr, message2);

  AutofillHostMsg_QueryFormFieldAutofill::Read(message2, &query_param);
  EXPECT_TRUE(get<4>(query_param));
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

TEST_F(AutofillRendererTest, DISABLED_DynamicallyAddedUnownedFormElements) {
  std::string html_data;
  base::FilePath test_path = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("autofill")),
      base::FilePath(FILE_PATH_LITERAL("autofill_noform_dynamic.html")));
  ASSERT_TRUE(base::ReadFileToString(test_path, &html_data));
  LoadHTML(html_data.c_str());

  // Verify that "FormsSeen" sends the expected number of fields.
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Param params;
  AutofillHostMsg_FormsSeen::Read(message, &params);
  std::vector<FormData> forms = get<0>(params);
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(7UL, forms[0].fields.size());

  render_thread_->sink().ClearMessages();

  ExecuteJavaScript("AddFields()");
  msg_loop_.RunUntilIdle();

  message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_FormsSeen::ID);
  ASSERT_NE(nullptr, message);
  AutofillHostMsg_FormsSeen::Read(message, &params);
  forms = get<0>(params);
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(9UL, forms[0].fields.size());

  FormFieldData expected;

  expected.name = ASCIIToUTF16("EMAIL_ADDRESS");
  expected.value.clear();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[7]);

  expected.name = ASCIIToUTF16("PHONE_HOME_WHOLE_NUMBER");
  expected.value.clear();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::defaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[8]);
}

class RequestAutocompleteRendererTest : public AutofillRendererTest {
 public:
  RequestAutocompleteRendererTest()
      : invoking_frame_(NULL), sibling_frame_(NULL) {}
  ~RequestAutocompleteRendererTest() override {}

 protected:
  void SetUp() override {
    AutofillRendererTest::SetUp();

    // Bypass the HTTPS-only restriction to show requestAutocomplete.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(::switches::kReduceSecurityForTesting);

    GURL url("data:text/html;charset=utf-8,"
             "<form><input autocomplete=cc-number></form>");
    const char kDoubleIframeHtml[] = "<iframe id=subframe src='%s'></iframe>"
                                     "<iframe id=sibling></iframe>";
    LoadHTML(base::StringPrintf(kDoubleIframeHtml, url.spec().c_str()).c_str());

    WebElement subframe = GetMainFrame()->document().getElementById("subframe");
    ASSERT_FALSE(subframe.isNull());
    invoking_frame_ = WebLocalFrame::fromFrameOwnerElement(subframe);
    ASSERT_TRUE(invoking_frame());
    ASSERT_EQ(GetMainFrame(), invoking_frame()->parent());

    WebElement sibling = GetMainFrame()->document().getElementById("sibling");
    ASSERT_FALSE(sibling.isNull());
    sibling_frame_ = WebLocalFrame::fromFrameOwnerElement(sibling);
    ASSERT_TRUE(sibling_frame());

    WebVector<WebFormElement> forms;
    invoking_frame()->document().forms(forms);
    ASSERT_EQ(1U, forms.size());
    invoking_form_ = forms[0];
    ASSERT_FALSE(invoking_form().isNull());

    render_thread_->sink().ClearMessages();

    // Invoke requestAutocomplete to show the dialog.
    invoking_frame_->autofillClient()->didRequestAutocomplete(invoking_form());
    ASSERT_TRUE(render_thread_->sink().GetFirstMessageMatching(
        AutofillHostMsg_RequestAutocomplete::ID));

    render_thread_->sink().ClearMessages();
  }

  void TearDown() override {
    invoking_form_.reset();
    AutofillRendererTest::TearDown();
  }

  void NavigateFrame(WebFrame* frame) {
    frame->loadRequest(WebURLRequest(GURL("about:blank")));
    ProcessPendingMessages();
  }

  const WebFormElement& invoking_form() const { return invoking_form_; }
  WebLocalFrame* invoking_frame() { return invoking_frame_; }
  WebFrame* sibling_frame() { return sibling_frame_; }

 protected:
  WebFormElement invoking_form_;
  WebLocalFrame* invoking_frame_;
  WebFrame* sibling_frame_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestAutocompleteRendererTest);
};

TEST_F(RequestAutocompleteRendererTest, InvokingTwiceOnlyShowsOnce) {
  // Attempting to show the requestAutocomplete dialog again should be ignored.
  invoking_frame_->autofillClient()->didRequestAutocomplete(invoking_form());
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_RequestAutocomplete::ID));
}

}  // namespace autofill
