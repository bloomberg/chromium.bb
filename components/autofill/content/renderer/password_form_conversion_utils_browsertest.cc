// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/test/render_view_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
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
  // |name_and_id|, |value|, and |autocomplete| attributes. The |autocomplete|
  // argument can take two special values, namely:
  //  1.) NULL, causing no autocomplete attribute to be added,
  //  2.) "", causing an empty attribute (i.e. autocomplete="") to be added.
  void AddUsernameField(const char* name_and_id,
                        const char* value,
                        const char* autocomplete) {
    std::string autocomplete_attribute(autocomplete ?
        base::StringPrintf("autocomplete=\"%s\"", autocomplete) : "");
    base::StringAppendF(
        &html_,
        "<INPUT type=\"text\" name=\"%s\" id=\"%s\" value=\"%s\" %s/>",
        name_and_id, name_and_id, value, autocomplete_attribute.c_str());
  }

  // Appends a new password-type field at the end of the form, having the
  // specified |name_and_id|, |value|, and |autocomplete| attributes. Special
  // values for |autocomplete| are the same as in AddUsernameField.
  void AddPasswordField(const char* name_and_id,
                        const char* value,
                        const char* autocomplete) {
    std::string autocomplete_attribute(autocomplete ?
        base::StringPrintf("autocomplete=\"%s\"", autocomplete): "");
    base::StringAppendF(
        &html_,
        "<INPUT type=\"password\" name=\"%s\" id=\"%s\" value=\"%s\" %s/>",
        name_and_id, name_and_id, value, autocomplete_attribute.c_str());
  }

  // Appends a disabled text-type field at the end of the form.
  void AddDisabledUsernameField() {
    html_ += "<INPUT type=\"text\" disabled/>";
  }

  // Appends a disabled password-type field at the end of the form.
  void AddDisabledPasswordField() {
    html_ += "<INPUT type=\"password\" disabled/>";
  }

  // Appends a new submit-type field at the end of the form with the specified
  // |name|. If |activated| is true, the test will emulate as if this button
  // were used to submit the form.
  void AddSubmitButton(const char* name, bool activated) {
    base::StringAppendF(
        &html_,
        "<INPUT type=\"submit\" name=\"%s\" value=\"Submit\" %s/>",
        name, activated ? "set-activated-submit" : "");
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

    WebVector<WebFormControlElement> control_elements;
    forms[0].getFormControlElements(control_elements);
    for (size_t i = 0; i < control_elements.size(); ++i) {
      WebInputElement* input_element = toWebInputElement(&control_elements[i]);
      if (input_element->hasAttribute("set-activated-submit"))
        input_element->setActivatedSubmit(true);
    }

    *password_form = CreatePasswordForm(forms[0]);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordFormConversionUtilsTest);
};

}  // namespace

TEST_F(PasswordFormConversionUtilsTest, BasicFormAttributes) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddUsernameField("username", "johnsmith", NULL);
  builder.AddSubmitButton("inactive_submit", false);
  builder.AddSubmitButton("active_submit", true);
  builder.AddSubmitButton("inactive_submit2", false);
  builder.AddPasswordField("password", "secret", NULL);
  std::string html = builder.ProduceHTML();

  scoped_ptr<PasswordForm> password_form;
  ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
  ASSERT_TRUE(password_form);

  EXPECT_EQ("data:", password_form->signon_realm);
  EXPECT_EQ(GURL(kTestFormActionURL), password_form->action);
  EXPECT_EQ(base::UTF8ToUTF16("active_submit"), password_form->submit_element);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("johnsmith"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
  EXPECT_EQ(PasswordForm::SCHEME_HTML, password_form->scheme);
  EXPECT_FALSE(password_form->ssl_valid);
  EXPECT_FALSE(password_form->preferred);
  EXPECT_FALSE(password_form->blacklisted_by_user);
  EXPECT_EQ(PasswordForm::TYPE_MANUAL, password_form->type);
  EXPECT_FALSE(password_form->use_additional_authentication);
}

TEST_F(PasswordFormConversionUtilsTest, DisabledFieldsAreIgnored) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddUsernameField("username", "johnsmith", NULL);
  builder.AddDisabledUsernameField();
  builder.AddDisabledPasswordField();
  builder.AddPasswordField("password", "secret", NULL);
  builder.AddSubmitButton("submit", true);
  std::string html = builder.ProduceHTML();

  scoped_ptr<PasswordForm> password_form;
  ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
  ASSERT_TRUE(password_form);
  EXPECT_EQ(base::UTF8ToUTF16("username"), password_form->username_element);
  EXPECT_EQ(base::UTF8ToUTF16("johnsmith"), password_form->username_value);
  EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
  EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
}

TEST_F(PasswordFormConversionUtilsTest, IdentifyingUsernameFields) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below, plus the corresponding expectations.
  struct TestCase {
    const char* autocomplete[3];
    const char* expected_username_element;
    const char* expected_username_value;
    const char* expected_other_possible_usernames;
  } cases[] = {
      // When no elements are marked with autocomplete='username', the text-type
      // input field before the first password element should get selected as
      // the username, and the rest should be marked as alternatives.
      {{NULL, NULL, NULL}, "username2", "William", "John+Smith"},
      // When a sole element is marked with autocomplete='username', it should
      // be treated as the username for sure, with no other_possible_usernames.
      {{"username", NULL, NULL}, "username1", "John", ""},
      {{NULL, "username", NULL}, "username2", "William", ""},
      {{NULL, NULL, "username"}, "username3", "Smith", ""},
      // When >=2 elements have the attribute, the first should be selected as
      // the username, and the rest should go to other_possible_usernames.
      {{"username", "username", NULL}, "username1", "John", "William"},
      {{NULL, "username", "username"}, "username2", "William", "Smith"},
      {{"username", NULL, "username"}, "username1", "John", "Smith"},
      {{"username", "username", "username"}, "username1", "John",
       "William+Smith"},
      // When there is an empty autocomplete attribute (i.e. autocomplete=""),
      // it should have the same effect as having no attribute whatsoever.
      {{"", "", ""}, "username2", "William", "John+Smith"},
      {{"", "", "username"}, "username3", "Smith", ""},
      {{"username", "", "username"}, "username1", "John", "Smith"},
      // It should not matter if attribute values are upper or mixed case.
      {{"USERNAME", NULL, "uSeRNaMe"}, "username1", "John", "Smith"},
      {{"uSeRNaMe", NULL, "USERNAME"}, "username1", "John", "Smith"}};

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    for (size_t nonempty_username_fields = 0; nonempty_username_fields < 2;
         ++nonempty_username_fields) {
      SCOPED_TRACE(testing::Message()
                   << "Iteration " << i << " "
                   << (nonempty_username_fields ? "nonempty" : "empty"));

      // Repeat each test once with empty, and once with non-empty usernames.
      // In the former case, no empty other_possible_usernames should be saved.
      const char* names[3];
      if (nonempty_username_fields) {
        names[0] = "John";
        names[1] = "William";
        names[2] = "Smith";
      } else {
        names[0] = names[1] = names[2] = "";
      }

      PasswordFormBuilder builder(kTestFormActionURL);
      builder.AddUsernameField("username1", names[0], cases[i].autocomplete[0]);
      builder.AddUsernameField("username2", names[1], cases[i].autocomplete[1]);
      builder.AddPasswordField("password", "secret", NULL);
      builder.AddUsernameField("username3", names[2], cases[i].autocomplete[2]);
      builder.AddPasswordField("password2", "othersecret", NULL);
      builder.AddSubmitButton("submit", true);
      std::string html = builder.ProduceHTML();

      scoped_ptr<PasswordForm> password_form;
      ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
      ASSERT_TRUE(password_form);

      EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_username_element),
                password_form->username_element);

      if (nonempty_username_fields) {
        EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_username_value),
                  password_form->username_value);
        EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_other_possible_usernames),
                  JoinString(password_form->other_possible_usernames, '+'));
      } else {
        EXPECT_TRUE(password_form->username_value.empty());
        EXPECT_TRUE(password_form->other_possible_usernames.empty());
      }

      // Do a basic sanity check that we are still having a password field.
      EXPECT_EQ(base::UTF8ToUTF16("password"), password_form->password_element);
      EXPECT_EQ(base::UTF8ToUTF16("secret"), password_form->password_value);
    }
  }
}

TEST_F(PasswordFormConversionUtilsTest, IdentifyingTwoPasswordFields) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below, plus the corresponding expectations.
  struct TestCase {
    const char* password_values[2];
    const char* expected_password_element;
    const char* expected_password_value;
    const char* expected_new_password_element;
    const char* expected_new_password_value;
  } cases[] = {
      // Twp non-empty fields with the same value should be treated as a new
      // password field plus a confirmation field for the new password.
      {{"alpha", "alpha"}, "", "", "password1", "alpha"},
      // The same goes if the fields are yet empty: we speculate that we will
      // identify them as new password fields once they are filled out, and we
      // want to keep our abstract interpretation of the form less flaky.
      {{"", ""}, "", "", "password1", ""},
      // Two different values should be treated as a password change form, one
      // that also asks for the current password, but only once for the new.
      {{"alpha", ""}, "password1", "alpha", "password2", ""},
      {{"", "beta"}, "password1", "", "password2", "beta"},
      {{"alpha", "beta"}, "password1", "alpha", "password2", "beta"}};

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    PasswordFormBuilder builder(kTestFormActionURL);
    builder.AddPasswordField("password1", cases[i].password_values[0], NULL);
    builder.AddUsernameField("username1", "William", NULL);
    builder.AddPasswordField("password2", cases[i].password_values[1], NULL);
    builder.AddUsernameField("username2", "Smith", NULL);
    builder.AddSubmitButton("submit", true);
    std::string html = builder.ProduceHTML();

    scoped_ptr<PasswordForm> password_form;
    ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
    ASSERT_TRUE(password_form);

    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_element),
              password_form->password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_value),
              password_form->password_value);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_element),
              password_form->new_password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_value),
              password_form->new_password_value);

    // Do a basic sanity check that we are still selecting the right username.
    EXPECT_EQ(base::UTF8ToUTF16("username1"), password_form->username_element);
    EXPECT_EQ(base::UTF8ToUTF16("William"), password_form->username_value);
    EXPECT_THAT(password_form->other_possible_usernames,
                testing::ElementsAre(base::UTF8ToUTF16("Smith")));
  }
}

TEST_F(PasswordFormConversionUtilsTest, IdentifyingThreePasswordFields) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below, plus the corresponding expectations.
  struct TestCase {
    const char* password_values[3];
    const char* expected_password_element;
    const char* expected_password_value;
    const char* expected_new_password_element;
    const char* expected_new_password_value;
  } cases[] = {
      // Two fields with the same value, and one different: we should treat this
      // as a password change form with confirmation for the new password. Note
      // that we only recognize (current + new + new) and (new + new + current)
      // without autocomplete attributes.
      {{"alpha", "", ""}, "password1", "alpha", "password2", ""},
      {{"", "beta", "beta"}, "password1", "", "password2", "beta"},
      {{"alpha", "beta", "beta"}, "password1", "alpha", "password2", "beta"},
      {{"beta", "beta", "alpha"}, "password3", "alpha", "password1", "beta"},
      // If the fields are yet empty, we speculate that we will identify them as
      // (current + new + new) once they are filled out, so we should classify
      // them the same for now to keep our abstract interpretation less flaky.
      {{"", "", ""}, "password1", "", "password2", ""}};
      // Note: In all other cases, we give up and consider the form invalid.
      // This is tested in InvalidFormDueToConfusingPasswordFields.

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    PasswordFormBuilder builder(kTestFormActionURL);
    builder.AddPasswordField("password1", cases[i].password_values[0], NULL);
    builder.AddUsernameField("username1", "William", NULL);
    builder.AddPasswordField("password2", cases[i].password_values[1], NULL);
    builder.AddUsernameField("username2", "Smith", NULL);
    builder.AddPasswordField("password3", cases[i].password_values[2], NULL);
    builder.AddSubmitButton("submit", true);
    std::string html = builder.ProduceHTML();

    scoped_ptr<PasswordForm> password_form;
    ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
    ASSERT_TRUE(password_form);

    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_element),
              password_form->password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_value),
              password_form->password_value);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_element),
              password_form->new_password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_value),
              password_form->new_password_value);

    // Do a basic sanity check that we are still selecting the right username.
    EXPECT_EQ(base::UTF8ToUTF16("username1"), password_form->username_element);
    EXPECT_EQ(base::UTF8ToUTF16("William"), password_form->username_value);
    EXPECT_THAT(password_form->other_possible_usernames,
                testing::ElementsAre(base::UTF8ToUTF16("Smith")));
  }
}

TEST_F(PasswordFormConversionUtilsTest,
       IdentifyingPasswordFieldsWithAutocompleteAttributes) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below, plus the corresponding expectations.
  struct TestCase {
    const char* autocomplete[3];
    const char* expected_password_element;
    const char* expected_password_value;
    const char* expected_new_password_element;
    const char* expected_new_password_value;
  } cases[] = {
      // When there are elements marked with autocomplete='current-password',
      // but no elements with 'new-password', we should treat the first of the
      // former kind as the current password, and ignore all other password
      // fields, assuming they are not intentionally not marked. They might be
      // for other purposes, such as PINs, OTPs, and the like. Actual values in
      // the password fields should be ignored in all cases below.
      {{"current-password", NULL, NULL},
       "password1", "alpha", "", ""},
      {{NULL, "current-password", NULL},
       "password2", "beta", "", ""},
      {{NULL, NULL, "current-password"},
       "password3", "gamma", "", ""},
      {{NULL, "current-password", "current-password"},
       "password2", "beta", "", ""},
      {{"current-password", NULL, "current-password"},
       "password1", "alpha", "", ""},
      {{"current-password", "current-password", NULL},
       "password1", "alpha", "", ""},
      {{"current-password", "current-password", "current-password"},
       "password1", "alpha", "", ""},
      // The same goes vice versa for autocomplete='new-password'.
      {{"new-password", NULL, NULL},
       "", "", "password1", "alpha"},
      {{NULL, "new-password", NULL},
       "", "", "password2", "beta"},
      {{NULL, NULL, "new-password"},
       "", "", "password3", "gamma"},
      {{NULL, "new-password", "new-password"},
       "", "", "password2", "beta"},
      {{"new-password", NULL, "new-password"},
       "", "", "password1", "alpha"},
      {{"new-password", "new-password", NULL},
       "", "", "password1", "alpha"},
      {{"new-password", "new-password", "new-password"},
       "", "", "password1", "alpha"},
      // When there is one element marked with autocomplete='current-password',
      // and one with 'new-password', just comply, regardless of their order.
      // Ignore the unmarked password field(s) for the same reason as above.
      {{"current-password", "new-password", NULL},
       "password1", "alpha", "password2", "beta"},
      {{"current-password", NULL, "new-password"},
       "password1", "alpha", "password3", "gamma"},
      {{NULL, "current-password", "new-password"},
       "password2", "beta", "password3", "gamma"},
      {{"new-password", "current-password", NULL},
       "password2", "beta", "password1", "alpha"},
      {{"new-password", NULL, "current-password"},
       "password3", "gamma", "password1", "alpha"},
      {{NULL, "new-password", "current-password"},
       "password3", "gamma", "password2", "beta"},
      // In case of duplicated elements of either kind, go with the first one of
      // its kind.
      {{"current-password", "current-password", "new-password"},
       "password1", "alpha", "password3", "gamma"},
      {{"current-password", "new-password", "current-password"},
       "password1", "alpha", "password2", "beta"},
      {{"new-password", "current-password", "current-password"},
       "password2", "beta", "password1", "alpha"},
      {{"current-password", "new-password", "new-password"},
       "password1", "alpha", "password2", "beta"},
      {{"new-password", "current-password", "new-password"},
       "password2", "beta", "password1", "alpha"},
      {{"new-password", "new-password", "current-password"},
       "password3", "gamma", "password1", "alpha"},
      // When there is an empty autocomplete attribute (i.e. autocomplete=""),
      // it should have the same effect as having no attribute whatsoever.
      {{"current-password", "", ""},
       "password1", "alpha", "", ""},
      {{"", "", "new-password"},
       "", "", "password3", "gamma"},
      {{"", "new-password", ""},
       "", "", "password2", "beta"},
      {{"", "current-password", "current-password"},
       "password2", "beta", "", ""},
      {{"new-password", "", "new-password"},
       "", "", "password1", "alpha"},
      {{"new-password", "", "current-password"},
       "password3", "gamma", "password1", "alpha"},
      // It should not matter if attribute values are upper or mixed case.
      {{NULL, "current-password", NULL},
       "password2", "beta", "", ""},
      {{NULL, "CURRENT-PASSWORD", NULL},
       "password2", "beta", "", ""},
      {{NULL, "new-password", NULL},
       "", "", "password2", "beta"},
      {{NULL, "nEw-PaSsWoRd", NULL},
       "", "", "password2", "beta"}};

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    PasswordFormBuilder builder(kTestFormActionURL);
    builder.AddPasswordField("pin1", "123456", NULL);
    builder.AddPasswordField("pin2", "789101", NULL);
    builder.AddPasswordField("password1", "alpha", cases[i].autocomplete[0]);
    builder.AddUsernameField("username1", "William", NULL);
    builder.AddPasswordField("password2", "beta", cases[i].autocomplete[1]);
    builder.AddUsernameField("username2", "Smith", NULL);
    builder.AddPasswordField("password3", "gamma", cases[i].autocomplete[2]);
    builder.AddSubmitButton("submit", true);
    std::string html = builder.ProduceHTML();

    scoped_ptr<PasswordForm> password_form;
    ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
    ASSERT_TRUE(password_form);

    // In the absence of username autocomplete attributes, the username should
    // be the text input field before the first password element.
    // No constellation of password autocomplete attributes should change that.
    EXPECT_EQ(base::UTF8ToUTF16("username1"), password_form->username_element);
    EXPECT_EQ(base::UTF8ToUTF16("William"), password_form->username_value);
    EXPECT_THAT(password_form->other_possible_usernames,
                testing::ElementsAre(base::UTF8ToUTF16("Smith")));
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_element),
              password_form->password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_password_value),
              password_form->password_value);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_element),
              password_form->new_password_element);
    EXPECT_EQ(base::UTF8ToUTF16(cases[i].expected_new_password_value),
              password_form->new_password_value);
  }
}

TEST_F(PasswordFormConversionUtilsTest, InvalidFormDueToBadActionURL) {
  PasswordFormBuilder builder("invalid_target");
  builder.AddUsernameField("username", "JohnSmith", NULL);
  builder.AddSubmitButton("submit", true);
  builder.AddPasswordField("password", "secret", NULL);
  std::string html = builder.ProduceHTML();

  scoped_ptr<PasswordForm> password_form;
  ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
  EXPECT_FALSE(password_form);
}

TEST_F(PasswordFormConversionUtilsTest, InvalidFormDueToNoPasswordFields) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddUsernameField("username1", "John", NULL);
  builder.AddUsernameField("username2", "Smith", NULL);
  builder.AddSubmitButton("submit", true);
  std::string html = builder.ProduceHTML();

  scoped_ptr<PasswordForm> password_form;
  ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
  EXPECT_FALSE(password_form);
}

TEST_F(PasswordFormConversionUtilsTest,
       InvalidFormsDueToConfusingPasswordFields) {
  // Each test case consists of a set of parameters to be plugged into the
  // PasswordFormBuilder below.
  const char* cases[][3] = {
      // No autocomplete attributes to guide us, and we see:
      //  * three password values that are all different,
      //  * three password values that are all the same;
      //  * three password values with the first and last matching.
      // In any case, we should just give up on this form.
      {"alpha", "beta", "gamma"},
      {"alpha", "alpha", "alpha"},
      {"alpha", "beta", "alpha"}};

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);

    PasswordFormBuilder builder(kTestFormActionURL);
    builder.AddUsernameField("username1", "John", NULL);
    builder.AddPasswordField("password1", cases[i][0], NULL);
    builder.AddPasswordField("password2", cases[i][1], NULL);
    builder.AddPasswordField("password3", cases[i][2], NULL);
    builder.AddSubmitButton("submit", true);
    std::string html = builder.ProduceHTML();

    scoped_ptr<PasswordForm> password_form;
    ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
    EXPECT_FALSE(password_form);
  }
}

TEST_F(PasswordFormConversionUtilsTest,
       InvalidFormDueToTooManyPasswordFieldsWithoutAutocompleteAttributes) {
  PasswordFormBuilder builder(kTestFormActionURL);
  builder.AddUsernameField("username1", "John", NULL);
  builder.AddPasswordField("password1", "alpha", NULL);
  builder.AddPasswordField("password2", "alpha", NULL);
  builder.AddPasswordField("password3", "alpha", NULL);
  builder.AddPasswordField("password4", "alpha", NULL);
  builder.AddSubmitButton("submit", true);
  std::string html = builder.ProduceHTML();

  scoped_ptr<PasswordForm> password_form;
  ASSERT_NO_FATAL_FAILURE(LoadHTMLAndConvertForm(html, &password_form));
  EXPECT_FALSE(password_form);
}

}  // namespace autofill
