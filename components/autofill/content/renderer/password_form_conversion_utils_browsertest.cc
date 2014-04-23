// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
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

class PasswordFormConversionUtilsTest : public content::RenderViewTest {
 public:
  PasswordFormConversionUtilsTest() : content::RenderViewTest() {}
  virtual ~PasswordFormConversionUtilsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordFormConversionUtilsTest);
};

}  // namespace

TEST_F(PasswordFormConversionUtilsTest, ValidWebFormElementToPasswordForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           " <INPUT type=\"text\" name=\"username\" "
               " id=\"username\" value=\"johnsmith\"/>"
           " <INPUT type=\"submit\" name=\"submit\" value=\"Submit\"/>"
           " <INPUT type=\"password\" name=\"password\" id=\"password\" "
               " value=\"secret\"/>"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->document().forms(forms);
  ASSERT_EQ(1U, forms.size());

  scoped_ptr<PasswordForm> password_form = CreatePasswordForm(forms[0]);
  ASSERT_NE(static_cast<PasswordForm*>(NULL), password_form.get());

  EXPECT_EQ("data:", password_form->signon_realm);
  EXPECT_EQ(GURL("http://cnn.com"), password_form->action);
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
  LoadHTML("<FORM name=\"TestForm\" action=\"invalid\" method=\"post\">"
           " <INPUT type=\"text\" name=\"username\" "
               " id=\"username\" value=\"johnsmith\"/>"
           " <INPUT type=\"submit\" name=\"submit\" value=\"Submit\"/>"
           " <INPUT type=\"password\" name=\"password\" id=\"password\" "
               " value=\"secret\"/>"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->document().forms(forms);
  ASSERT_EQ(1U, forms.size());

  scoped_ptr<PasswordForm> password_form = CreatePasswordForm(forms[0]);
  EXPECT_EQ(static_cast<PasswordForm*>(NULL), password_form.get());
}

TEST_F(PasswordFormConversionUtilsTest,
       WebFormWithMultipleUseNameAndPassWordFieldsToPasswordForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           " <INPUT type=\"text\" name=\"username1\" "
               " id=\"username1\" value=\"John\"/>"
           " <INPUT type=\"password\" name=\"password1\" id=\"password1\" "
               " value=\"encrypted\"/>"
           " <INPUT type=\"text\" name=\"username2\" "
               " id=\"username2\" value=\"Smith\"/>"
           " <INPUT type=\"password\" name=\"password2\" id=\"password2\" "
               " value=\"secret\"/>"
           " <INPUT type=\"text\" name=\"username3\" "
               " id=\"username3\" value=\"JohnSmith\"/>"
           " <INPUT type=\"password\" name=\"password3\" id=\"password3\" "
               " value=\"secret\"/>"
           " <INPUT type=\"submit\" name=\"submit\" value=\"Submit\"/>"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->document().forms(forms);
  ASSERT_EQ(1U, forms.size());

  scoped_ptr<PasswordForm> password_form = CreatePasswordForm(forms[0]);
  ASSERT_NE(static_cast<PasswordForm*>(NULL), password_form.get());

  EXPECT_EQ("data:", password_form->signon_realm);
  EXPECT_EQ(GURL("http://cnn.com"), password_form->action);
  EXPECT_EQ(base::UTF8ToUTF16("username1"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("John"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password2"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
  EXPECT_EQ(base::UTF8ToUTF16("password1"),
            password_form->old_password_element);
  EXPECT_EQ(base::UTF8ToUTF16("encrypted"), password_form->old_password_value);
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
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           " <INPUT type=\"text\" name=\"username\" "
               " id=\"username\" value=\"John\"/>"
           " <INPUT type=\"password\" name=\"password1\" id=\"password1\" "
               " value=\"secret\"/>"
           " <INPUT type=\"password\" name=\"password2\" id=\"password2\" "
               " value=\"encrypted\"/>"
           " <INPUT type=\"password\" name=\"password3\" id=\"password3\" "
               " value=\"key\"/>"
           " <INPUT type=\"submit\" name=\"submit\" value=\"Submit\"/>"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->document().forms(forms);
  ASSERT_EQ(1U, forms.size());

  scoped_ptr<PasswordForm> password_form = CreatePasswordForm(forms[0]);
  ASSERT_EQ(static_cast<PasswordForm*>(NULL), password_form.get());
}

}  // namespace autofill
