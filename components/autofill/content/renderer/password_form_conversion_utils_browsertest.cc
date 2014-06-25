// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebFormElement;
using blink::WebFrame;
using blink::WebVector;

namespace autofill {
namespace {

const char kTestFormActionURL[] = "http://cnn.com";

// A builder to produce HTML code for a password form composed of the desired
// number and kinds of username and password fields.
class PasswordFormBuilder {
 public:
  // Creates a builder to start composing a new form. The form will have the
  // specified |action| URL.
  explicit PasswordFormBuilder(const char* action) {
    base::StringAppendF(
        &html_, "<FORM name=\"Test\" action=\"%s\" method=\"post\">", action);
  }

  // Appends a new text-type field at the end of the form, having the specified
  // |name_and_id| and |value| attributes.
  void AddUsernameField(const char* name_and_id, const char* value) {
    base::StringAppendF(
        &html_,
        "<INPUT type=\"text\" name=\"%s\" id=\"%s\" value=\"%s\"/>",
        name_and_id, name_and_id, value);
  }

  // Appends a new password-type field at the end of the form, having the
  // specified |name_and_id| and |value| attributes.
  void AddPasswordField(const char* name_and_id, const char* value) {
    base::StringAppendF(
        &html_,
        "<INPUT type=\"password\" name=\"%s\" id=\"%s\" value=\"%s\"/>",
        name_and_id, name_and_id, value);
  }

  // Appends a new submit-type field at the end of the form.
  void AddSubmitButton() {
    html_ += "<INPUT type=\"submit\" name=\"submit\" value=\"Submit\"/>";
  }

  // Returns the HTML code for the form containing the fields that have been
  // added so far.
  std::string ProduceHTML() const {
    return html_ + "</FORM>";
  }

 private:
  std::string html_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormBuilder);
};

class PasswordFormConversionUtilsTest : public content::RenderViewTest {
 public:
  PasswordFormConversionUtilsTest() : content::RenderViewTest() {}
  virtual ~PasswordFormConversionUtilsTest() {}

 protected:
  // Loads the given |html|, retrieves the sole WebFormElement from it, and then
  // calls CreatePasswordForm() to convert it into a |password_form|. Note that
  // ASSERT() can only be used in void functions, this is why |password_form| is
  // passed in as a pointer to a scoped_ptr.
  void LoadHTMLAndConvertForm(const std::string& html,
                              scoped_ptr<PasswordForm>* password_form) {
    LoadHTML(html.c_str());

    WebFrame* frame = GetMainFrame();
    ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

    WebVector<WebFormElement> forms;
    frame->document().forms(forms);
    ASSERT_EQ(1U, forms.size());

    *password_form = CreatePasswordForm(forms[0]);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordFormConversionUtilsTest);
};

}  // namespace

TEST_F(PasswordFormConversionUtilsTest, ValidWebFormElementToPasswordForm) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddUsernameField("username", "johnsmith");
  builder.AddSubmitButton();
  builder.AddPasswordField("password", "secret");
  std::string html = builder.ProduceHTML();

  scoped_ptr<PasswordForm> password_form;
  ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
  ASSERT_NE(static_cast<PasswordForm*>(NULL), password_form.get());

  EXPECT_EQ("data:", password_form->signon_realm);
  EXPECT_EQ(GURL(kTestFormActionURL), password_form->action);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("johnsmith"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
  EXPECT_EQ(PasswordForm::SCHEME_HTML, password_form->scheme);
  EXPECT_FALSE(password_form->ssl_valid);
  EXPECT_FALSE(password_form->preferred);
  EXPECT_FALSE(password_form->blacklisted_by_user);
  EXPECT_EQ(PasswordForm::TYPE_MANUAL, password_form->type);
}

TEST_F(PasswordFormConversionUtilsTest, InvalidWebFormElementToPasswordForm) {
  PasswordFormBuilder builder("invalid_target");
  builder.AddUsernameField("username", "johnsmith");
  builder.AddSubmitButton();
  builder.AddPasswordField("password", "secret");
  std::string html = builder.ProduceHTML();

  scoped_ptr<PasswordForm> password_form;
  ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
  ASSERT_EQ(static_cast<PasswordForm*>(NULL), password_form.get());
}

TEST_F(PasswordFormConversionUtilsTest,
       WebFormWithMultipleUseNameAndPassWordFieldsToPasswordForm) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddUsernameField("username1", "John");
  builder.AddPasswordField("password1", "oldsecret");
  builder.AddUsernameField("username2", "Smith");
  builder.AddPasswordField("password2", "secret");
  builder.AddUsernameField("username3", "JohnSmith");
  builder.AddPasswordField("password3", "secret");
  builder.AddSubmitButton();
  std::string html = builder.ProduceHTML();

  scoped_ptr<PasswordForm> password_form;
  ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
  ASSERT_NE(static_cast<PasswordForm*>(NULL), password_form.get());

  EXPECT_EQ("data:", password_form->signon_realm);
  EXPECT_EQ(GURL(kTestFormActionURL), password_form->action);
  EXPECT_EQ(base::UTF8ToUTF16("username1"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("John"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password2"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
  EXPECT_EQ(base::UTF8ToUTF16("password1"),
            password_form->old_password_element);
  EXPECT_EQ(base::UTF8ToUTF16("oldsecret"), password_form->old_password_value);
  ASSERT_EQ(2u, password_form->other_possible_usernames.size());
  EXPECT_EQ(base::UTF8ToUTF16("Smith"),
            password_form->other_possible_usernames[0]);
  EXPECT_EQ(base::UTF8ToUTF16("JohnSmith"),
            password_form->other_possible_usernames[1]);
  EXPECT_EQ(PasswordForm::SCHEME_HTML, password_form->scheme);
  EXPECT_FALSE(password_form->ssl_valid);
  EXPECT_FALSE(password_form->preferred);
  EXPECT_FALSE(password_form->blacklisted_by_user);
  EXPECT_EQ(PasswordForm::TYPE_MANUAL, password_form->type);
}

TEST_F(PasswordFormConversionUtilsTest,
       WebFormwithThreeDifferentPasswordsToPasswordForm) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddUsernameField("username1", "John");
  builder.AddPasswordField("password1", "alpha");
  builder.AddPasswordField("password2", "beta");
  builder.AddPasswordField("password3", "gamma");
  builder.AddSubmitButton();
  std::string html = builder.ProduceHTML();

  scoped_ptr<PasswordForm> password_form;
  ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
  ASSERT_EQ(static_cast<PasswordForm*>(NULL), password_form.get());
}

}  // namespace autofill
