// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "content/public/common/password_form.h"
#include "content/public/renderer/password_form_conversion_utils.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPasswordFormData.h"

using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebPasswordFormData;
using WebKit::WebVector;

namespace content {
namespace {

class PasswordFormConversionUtilsTest : public RenderViewTest {
 public:
  PasswordFormConversionUtilsTest() : RenderViewTest() {}
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
  WebPasswordFormData web_password_form(forms[0]);
  ASSERT_TRUE(web_password_form.isValid());

  scoped_ptr<PasswordForm> password_form = CreatePasswordForm(forms[0]);
  ASSERT_NE(static_cast<PasswordForm*>(NULL), password_form.get());

  EXPECT_EQ("data:", password_form->signon_realm);
  EXPECT_EQ(GURL("http://cnn.com"), password_form->action);
  EXPECT_EQ(UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(UTF8ToUTF16("johnsmith"), password_form->username_value);
  EXPECT_EQ(UTF8ToUTF16("password"), password_form->password_element);
  EXPECT_EQ(UTF8ToUTF16("secret"), password_form->password_value);
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
  WebPasswordFormData web_password_form(forms[0]);
  ASSERT_FALSE(web_password_form.isValid());

  scoped_ptr<PasswordForm> password_form = CreatePasswordForm(forms[0]);
  EXPECT_EQ(static_cast<PasswordForm*>(NULL), password_form.get());
}

}  // namespace content
