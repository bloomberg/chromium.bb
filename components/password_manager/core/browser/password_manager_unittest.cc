// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager.h"

#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;
using testing::AnyNumber;
using testing::Exactly;
using testing::Return;
using testing::WithArg;

namespace password_manager {

namespace {

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(IsPasswordManagerEnabledForCurrentPage, bool());
  MOCK_CONST_METHOD2(IsSyncAccountCredential,
                     bool(const std::string&, const std::string&));
  MOCK_CONST_METHOD0(GetPasswordStore, PasswordStore*());
  MOCK_CONST_METHOD0(DidLastPageLoadEncounterSSLErrors, bool());
  MOCK_METHOD2(PromptUserToSavePasswordPtr,
               void(PasswordFormManager*, CredentialSourceType type));
  MOCK_METHOD1(AutomaticPasswordSavePtr, void(PasswordFormManager*));
  MOCK_METHOD0(GetPrefs, PrefService*());
  MOCK_METHOD0(GetDriver, PasswordManagerDriver*());

  // Workaround for scoped_ptr<> lacking a copy constructor.
  virtual bool PromptUserToSavePassword(
      scoped_ptr<PasswordFormManager> manager,
      password_manager::CredentialSourceType type) override {
    PromptUserToSavePasswordPtr(manager.release(), type);
    return false;
  }
  virtual void AutomaticPasswordSave(
      scoped_ptr<PasswordFormManager> manager) override {
    AutomaticPasswordSavePtr(manager.release());
  }
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD1(FillPasswordForm, void(const autofill::PasswordFormFillData&));
  MOCK_METHOD0(GetPasswordManager, PasswordManager*());
  MOCK_METHOD0(GetPasswordAutofillManager, PasswordAutofillManager*());
};

ACTION_P(InvokeConsumer, forms) {
  arg0->OnGetPasswordStoreResults(forms->Pass());
}

ACTION(InvokeEmptyConsumer) {
  arg0->OnGetPasswordStoreResults(ScopedVector<autofill::PasswordForm>());
}

ACTION_P(SaveToScopedPtr, scoped) { scoped->reset(arg0); }

class TestPasswordManager : public PasswordManager {
 public:
  explicit TestPasswordManager(PasswordManagerClient* client)
      : PasswordManager(client) {}
  ~TestPasswordManager() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPasswordManager);
};

}  // namespace

class PasswordManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(prefs::kPasswordManagerSavingEnabled,
                                           true);

    store_ = new MockPasswordStore;
    EXPECT_CALL(*store_, ReportMetrics(_, _)).Times(AnyNumber());
    CHECK(store_->Init(syncer::SyncableService::StartSyncFlare()));

    EXPECT_CALL(client_, IsPasswordManagerEnabledForCurrentPage())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(client_, IsSyncAccountCredential(_, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(client_, GetPasswordStore())
        .WillRepeatedly(Return(store_.get()));
    EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(&prefs_));
    EXPECT_CALL(client_, GetDriver()).WillRepeatedly(Return(&driver_));

    manager_.reset(new TestPasswordManager(&client_));
    password_autofill_manager_.reset(
        new PasswordAutofillManager(client_.GetDriver(), nullptr));

    EXPECT_CALL(driver_, GetPasswordManager())
        .WillRepeatedly(Return(manager_.get()));
    EXPECT_CALL(driver_, GetPasswordAutofillManager())
        .WillRepeatedly(Return(password_autofill_manager_.get()));
    EXPECT_CALL(client_, DidLastPageLoadEncounterSSLErrors())
        .WillRepeatedly(Return(false));

    ON_CALL(*store_, GetLogins(_, _, _))
        .WillByDefault(WithArg<2>(InvokeEmptyConsumer()));
  }

  void TearDown() override {
    store_->Shutdown();
    store_ = nullptr;
  }

  PasswordForm MakeSimpleForm() {
    PasswordForm form;
    form.origin = GURL("http://www.google.com/a/LoginAuth");
    form.action = GURL("http://www.google.com/a/Login");
    form.username_element = ASCIIToUTF16("Email");
    form.password_element = ASCIIToUTF16("Passwd");
    form.username_value = ASCIIToUTF16("google");
    form.password_value = ASCIIToUTF16("password");
    // Default to true so we only need to add tests in autocomplete=off cases.
    form.password_autocomplete_set = true;
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "http://www.google.com";
    return form;
  }

  // Create a sign-up form that only has a new password field.
  PasswordForm MakeFormWithOnlyNewPasswordField() {
    PasswordForm form = MakeSimpleForm();
    form.new_password_element.swap(form.password_element);
    form.new_password_value.swap(form.password_value);
    return form;
  }

  // Reproduction of the form present on twitter's login page.
  PasswordForm MakeTwitterLoginForm() {
    PasswordForm form;
    form.origin = GURL("https://twitter.com/");
    form.action = GURL("https://twitter.com/sessions");
    form.username_element = ASCIIToUTF16("Email");
    form.password_element = ASCIIToUTF16("Passwd");
    form.username_value = ASCIIToUTF16("twitter");
    form.password_value = ASCIIToUTF16("password");
    form.password_autocomplete_set = true;
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "https://twitter.com";
    return form;
  }

  // Reproduction of the form present on twitter's failed login page.
  PasswordForm MakeTwitterFailedLoginForm() {
    PasswordForm form;
    form.origin = GURL("https://twitter.com/login/error?redirect_after_login");
    form.action = GURL("https://twitter.com/sessions");
    form.username_element = ASCIIToUTF16("EmailField");
    form.password_element = ASCIIToUTF16("PasswdField");
    form.username_value = ASCIIToUTF16("twitter");
    form.password_value = ASCIIToUTF16("password");
    form.password_autocomplete_set = true;
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "https://twitter.com";
    return form;
  }

  PasswordForm MakeSimpleFormWithOnlyPasswordField() {
    PasswordForm form(MakeSimpleForm());
    form.username_element.clear();
    form.username_value.clear();
    return form;
  }

  bool FormsAreEqual(const autofill::PasswordForm& lhs,
                     const autofill::PasswordForm& rhs) {
    if (lhs.origin != rhs.origin)
      return false;
    if (lhs.action != rhs.action)
      return false;
    if (lhs.username_element != rhs.username_element)
      return false;
    if (lhs.password_element != rhs.password_element)
      return false;
    if (lhs.new_password_element != rhs.new_password_element)
      return false;
    if (lhs.username_value != rhs.username_value)
      return false;
    if (lhs.password_value != rhs.password_value)
      return false;
    if (lhs.new_password_value != rhs.new_password_value)
      return false;
    if (lhs.password_autocomplete_set != rhs.password_autocomplete_set)
      return false;
    if (lhs.submit_element != rhs.submit_element)
      return false;
    if (lhs.signon_realm != rhs.signon_realm)
      return false;
    return true;
  }

  TestPasswordManager* manager() { return manager_.get(); }

  void OnPasswordFormSubmitted(const autofill::PasswordForm& form) {
    manager()->OnPasswordFormSubmitted(&driver_, form);
  }

  PasswordManager::PasswordSubmittedCallback SubmissionCallback() {
    return base::Bind(&PasswordManagerTest::FormSubmitted,
                      base::Unretained(this));
  }

  void FormSubmitted(const autofill::PasswordForm& form) {
    submitted_form_ = form;
  }

  TestingPrefServiceSimple prefs_;
  scoped_refptr<MockPasswordStore> store_;
  MockPasswordManagerClient client_;
  MockPasswordManagerDriver driver_;
  scoped_ptr<PasswordAutofillManager> password_autofill_manager_;
  scoped_ptr<TestPasswordManager> manager_;
  PasswordForm submitted_form_;
};

MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.origin == arg.origin &&
         form.action == arg.action &&
         form.username_element == arg.username_element &&
         form.password_element == arg.password_element &&
         form.new_password_element == arg.new_password_element &&
         form.password_autocomplete_set == arg.password_autocomplete_set &&
         form.submit_element == arg.submit_element;
}

TEST_F(PasswordManagerTest, FormSubmitEmptyStore) {
  // Test that observing a newly submitted form shows the save password bar.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  // The initial load.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // The initial layout.
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // And the form submit contract is to call ProvisionallySavePassword.
  manager()->ProvisionallySavePassword(form);

  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  // The post-navigation load.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // The post-navigation layout.
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  ASSERT_TRUE(form_to_save.get());
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));

  // Simulate saving the form, as if the info bar was accepted.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, FormSubmitWithOnlyNewPasswordField) {
  // This test is the same as FormSubmitEmptyStore, except that it simulates the
  // user entering credentials into a sign-up form that only has a new password
  // field.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // And the form submit contract is to call ProvisionallySavePassword.
  manager()->ProvisionallySavePassword(form);

  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  ASSERT_TRUE(form_to_save.get());

  // Simulate saving the form, as if the info bar was accepted.
  PasswordForm saved_form;
  EXPECT_CALL(*store_, AddLogin(_)).WillOnce(testing::SaveArg<0>(&saved_form));
  form_to_save->Save();

  // The value of the new password field should have been promoted to, and saved
  // to the password store as the current password, and no password element name
  // should have been saved.
  PasswordForm expected_form(form);
  expected_form.password_value.swap(expected_form.new_password_value);
  expected_form.new_password_element.clear();
  EXPECT_THAT(saved_form, FormMatches(expected_form));
  EXPECT_EQ(expected_form.password_value, saved_form.password_value);
  EXPECT_EQ(expected_form.new_password_value, saved_form.new_password_value);
}

TEST_F(PasswordManagerTest, GeneratedPasswordFormSubmitEmptyStore) {
  // This test is the same as FormSubmitEmptyStore, except that it simulates the
  // user generating the password through the browser.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  // The initial load.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // The initial layout.
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate the user generating the password and submitting the form.
  manager()->SetFormHasGeneratedPassword(&driver_, form);
  manager()->ProvisionallySavePassword(form);

  // The user should not be presented with an infobar as they have already given
  // consent by using the generated password. The form should be saved once
  // navigation occurs. The client will be informed that automatic saving has
  // occured.
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .Times(Exactly(0));
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  scoped_ptr<PasswordFormManager> saved_form_manager;
  EXPECT_CALL(client_, AutomaticPasswordSavePtr(_))
      .Times(Exactly(1))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&saved_form_manager)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.
}

TEST_F(PasswordManagerTest, FormSubmitNoGoodMatch) {
  // Same as above, except with an existing form for the same signon realm,
  // but different origin.  Detailed cases like this are covered by
  // PasswordFormManagerTest.
  ScopedVector<PasswordForm> result;
  scoped_ptr<PasswordForm> existing_different(
      new PasswordForm(MakeSimpleForm()));
  existing_different->username_value = ASCIIToUTF16("google2");
  result.push_back(existing_different.release());
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  EXPECT_CALL(*store_, GetLogins(_, _, _))
      .WillOnce(WithArg<2>(InvokeConsumer(&result)));

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.
  manager()->ProvisionallySavePassword(form);

  // We still expect an add, since we didn't have a good match.
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.

  ASSERT_TRUE(form_to_save.get());
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));

  // Simulate saving the form.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, FormSeenThenLeftPage) {
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  // No message from the renderer that a password was submitted. No
  // expected calls.
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .Times(0);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.
}

TEST_F(PasswordManagerTest, FormSubmitAfterNavigateInPage) {
  // Test that navigating in the page does not prevent us from showing the save
  // password infobar.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  // Simulate submitting the password.
  OnPasswordFormSubmitted(form);

  // Now the password manager waits for the navigation to complete.
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.

  ASSERT_TRUE(form_to_save);
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));

  // Simulate saving the form, as if the info bar was accepted.
  form_to_save->Save();
}

// This test verifies a fix for http://crbug.com/236673
TEST_F(PasswordManagerTest, FormSubmitWithFormOnPreviousPage) {
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  PasswordForm first_form(MakeSimpleForm());
  first_form.origin = GURL("http://www.nytimes.com/");
  first_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  first_form.signon_realm = "http://www.nytimes.com/";
  PasswordForm second_form(MakeSimpleForm());
  second_form.origin = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.signon_realm = "https://myaccount.nytimes.com/";

  // Pretend that the form is hidden on the first page.
  std::vector<PasswordForm> observed;
  observed.push_back(first_form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Now navigate to a second page.
  manager()->DidNavigateMainFrame();

  // This page contains a form with the same markup, but on a different
  // URL.
  observed.push_back(second_form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Now submit this form
  OnPasswordFormSubmitted(second_form);

  // Navigation after form submit.
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Make sure that the saved form matches the second form, not the first.
  ASSERT_TRUE(form_to_save.get());
  EXPECT_CALL(*store_, AddLogin(FormMatches(second_form)));

  // Simulate saving the form, as if the info bar was accepted.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, FormSubmitFailedLogin) {
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  manager()->ProvisionallySavePassword(form);

  // The form reappears, and is visible in the layout:
  // No expected calls to the PasswordStore...
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, FormSubmitInvisibleLogin) {
  // Tests fix of issue 28911: if the login form reappears on the subsequent
  // page, but is invisible, it shouldn't count as a failed login.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  manager()->ProvisionallySavePassword(form);

  // Expect info bar to appear:
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // The form reappears, but is not visible in the layout:
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  ASSERT_TRUE(form_to_save.get());
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));

  // Simulate saving the form.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, InitiallyInvisibleForm) {
  // Make sure an invisible login form still gets autofilled.
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(MakeSimpleForm()));
  EXPECT_CALL(driver_, FillPasswordForm(_));
  EXPECT_CALL(*store_, GetLogins(_, _, _))
      .WillOnce(WithArg<2>(InvokeConsumer(&result)));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.
}

TEST_F(PasswordManagerTest, SavingDependsOnManagerEnabledPreference) {
  // Test that saving passwords depends on the password manager enabled
  // preference.
  prefs_.SetUserPref(prefs::kPasswordManagerSavingEnabled,
                     new base::FundamentalValue(true));
  EXPECT_TRUE(manager()->IsSavingEnabledForCurrentPage());
  prefs_.SetUserPref(prefs::kPasswordManagerSavingEnabled,
                     new base::FundamentalValue(false));
  EXPECT_FALSE(manager()->IsSavingEnabledForCurrentPage());
}

TEST_F(PasswordManagerTest, FillPasswordsOnDisabledManager) {
  // Test fix for issue 158296: Passwords must be filled even if the password
  // manager is disabled.
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(MakeSimpleForm()));
  prefs_.SetUserPref(prefs::kPasswordManagerSavingEnabled,
                     new base::FundamentalValue(false));
  EXPECT_CALL(driver_, FillPasswordForm(_));
  EXPECT_CALL(*store_,
              GetLogins(_, testing::Eq(PasswordStore::DISALLOW_PROMPT), _))
      .WillOnce(WithArg<2>(InvokeConsumer(&result)));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
}

TEST_F(PasswordManagerTest, FormSavedWithAutocompleteOff) {
  // Test password form with non-generated password will be saved even if
  // autocomplete=off.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  form.password_autocomplete_set = false;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  // And the form submit contract is to call ProvisionallySavePassword.
  manager()->ProvisionallySavePassword(form);

  // Password form should be saved.
  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .Times(Exactly(1))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));
  EXPECT_CALL(*store_, AddLogin(FormMatches(form))).Times(Exactly(0));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.

  ASSERT_TRUE(form_to_save.get());
}

TEST_F(PasswordManagerTest, GeneratedPasswordFormSavedAutocompleteOff) {
  // Test password form with generated password will still be saved if
  // autocomplete=off.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  form.password_autocomplete_set = false;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  // Simulate the user generating the password and submitting the form.
  manager()->SetFormHasGeneratedPassword(&driver_, form);
  manager()->ProvisionallySavePassword(form);

  // The user should not be presented with an infobar as they have already given
  // consent by using the generated password. The form should be saved once
  // navigation occurs. The client will be informed that automatic saving has
  // occured.
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .Times(Exactly(0));
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  scoped_ptr<PasswordFormManager> saved_form_manager;
  EXPECT_CALL(client_, AutomaticPasswordSavePtr(_))
      .Times(Exactly(1))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&saved_form_manager)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.
}

TEST_F(PasswordManagerTest, SubmissionCallbackTest) {
  manager()->AddSubmissionCallback(SubmissionCallback());
  PasswordForm form = MakeSimpleForm();
  OnPasswordFormSubmitted(form);
  EXPECT_TRUE(FormsAreEqual(form, submitted_form_));
}

TEST_F(PasswordManagerTest, PasswordFormReappearance) {
  // Test the heuristic to know if a password form reappears.
  // We assume that if we send our credentials and there
  // is at least one visible password form in the next page that
  // means that our previous login attempt failed.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  std::vector<PasswordForm> observed;
  PasswordForm login_form(MakeTwitterLoginForm());
  observed.push_back(login_form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  manager()->ProvisionallySavePassword(login_form);

  PasswordForm failed_login_form(MakeTwitterFailedLoginForm());
  observed.clear();
  observed.push_back(failed_login_form);
  // A PasswordForm appears, and is visible in the layout:
  // No expected calls to the PasswordStore...
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, SavingNotEnabledOnSSLErrors) {
  EXPECT_CALL(client_, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(manager()->IsSavingEnabledForCurrentPage());
}

TEST_F(PasswordManagerTest, AutofillingNotEnabledOnSSLErrors) {
  // Test that in the presence of SSL errors, the password manager does not
  // attempt to autofill forms found on a website.
  EXPECT_CALL(client_, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(true));

  // Let us pretend some forms were found on a website.
  std::vector<PasswordForm> forms;
  forms.push_back(MakeSimpleForm());

  // Feed those forms to |manager()| and check that it does not try to find
  // matching saved credentials for the forms.
  EXPECT_CALL(*store_, GetLogins(_, _, _)).Times(Exactly(0));
  manager()->OnPasswordFormsParsed(&driver_, forms);
}

TEST_F(PasswordManagerTest, SavingDisabledIfManagerDisabled) {
  EXPECT_CALL(client_, IsPasswordManagerEnabledForCurrentPage())
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(manager()->IsSavingEnabledForCurrentPage());
}

TEST_F(PasswordManagerTest, AutofillingDisabledIfManagerDisabled) {
  EXPECT_CALL(client_, IsPasswordManagerEnabledForCurrentPage())
      .WillRepeatedly(Return(false));

  // Let us pretend some forms were found on a website.
  std::vector<PasswordForm> forms;
  forms.push_back(MakeSimpleForm());

  // Feed those forms to |manager()| and check that it does not try to find
  // matching saved credentials for the forms.
  EXPECT_CALL(*store_, GetLogins(_, _, _)).Times(Exactly(0));
  manager()->OnPasswordFormsParsed(&driver_, forms);
}

TEST_F(PasswordManagerTest, SyncCredentialsNotSaved) {
  EXPECT_CALL(client_, IsSyncAccountCredential(_, _))
      .WillRepeatedly(Return(true));

  // Simulate loading a simple form with no existing stored password.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  form.password_autocomplete_set = false;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  // User should not be prompted and password should not be saved.
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .Times(Exactly(0));
  EXPECT_CALL(*store_, AddLogin(FormMatches(form))).Times(Exactly(0));

  // Submit form and finish navigation.
  manager()->ProvisionallySavePassword(form);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// On failed login attempts, the retry-form can have action scheme changed from
// HTTP to HTTPS (see http://crbug.com/400769). Check that such retry-form is
// considered equal to the original login form, and the attempt recognised as a
// failure.
TEST_F(PasswordManagerTest,
       SeeingFormActionWithOnlyHttpHttpsChangeIsLoginFailure) {
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));

  PasswordForm first_form(MakeSimpleForm());
  first_form.origin = GURL("http://www.xda-developers.com/");
  first_form.action = GURL("http://forum.xda-developers.com/login.php");

  // |second_form|'s action differs only with it's scheme i.e. *https://*.
  PasswordForm second_form(first_form);
  second_form.action = GURL("https://forum.xda-developers.com/login.php");

  std::vector<PasswordForm> observed;
  observed.push_back(first_form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  observed.clear();

  // Now submit the |first_form|.
  OnPasswordFormSubmitted(first_form);

  // Simulate loading a page, which contains |second_form| instead of
  // |first_form|.
  observed.push_back(second_form);

  // Verify that no prompt to save the password is shown.
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .Times(Exactly(0));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  observed.clear();
}

// Create a form with a new_password_element. Submit the form with the empty
// new password value. It shouldn't overwrite the existing password.
TEST_F(PasswordManagerTest, DoNotUpdateWithEmptyPassword) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  form.new_password_element = ASCIIToUTF16("new_password_element");
  form.new_password_value.clear();
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  // And the form submit contract is to call ProvisionallySavePassword.
  OnPasswordFormSubmitted(form);

  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .Times(0);

  // Now the password manager waits for the login to complete successfully.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.
}

TEST_F(PasswordManagerTest, FormSubmitWithOnlyPassowrdField) {
  // Test to verify that on submitting the HTML password form without having
  // username input filed shows password save promt and saves the password to
  // store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;

  // Loads passsword form without username input field.
  PasswordForm form(MakeSimpleFormWithOnlyPasswordField());
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);  // The initial load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The initial layout.

  // And the form submit contract is to call ProvisionallySavePassword.
  manager()->ProvisionallySavePassword(form);

  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.

  ASSERT_TRUE(form_to_save.get());
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));

  // Simulate saving the form, as if the info bar was accepted.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, FillPasswordOnManyFrames) {
  // If one password form appears in more frames, it should be filled in all of
  // them.
  PasswordForm form(MakeSimpleForm());  // The observed and saved form.

  // "Save" the form.
  ScopedVector<PasswordForm> result;
  result.push_back(new PasswordForm(form));
  EXPECT_CALL(*store_,
              GetLogins(_, testing::Eq(PasswordStore::DISALLOW_PROMPT), _))
      .WillOnce(WithArg<2>(InvokeConsumer(&result)));

  // The form will be seen the first time.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);

  // Now the form will be seen the second time, in a different frame. The driver
  // for that frame should be told to fill it, but the store should not be asked
  // for it again.
  MockPasswordManagerDriver driver_b;
  EXPECT_CALL(driver_b, FillPasswordForm(_)).Times(1);
  manager()->OnPasswordFormsParsed(&driver_b, observed);
}

TEST_F(PasswordManagerTest, InPageNavigation) {
  // Test that observing a newly submitted form shows the save password bar on
  // call in page navigation.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(Exactly(0));
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  // The initial load.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // The initial layout.
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // And the form submit contract is to call ProvisionallySavePassword.
  manager()->ProvisionallySavePassword(form);

  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  manager()->OnInPageNavigation(&driver_, form);
}

TEST_F(PasswordManagerTest, SavingSignupForms_NoHTMLMatch) {
  // Signup forms don't require HTML attributes match in order to save.
  // Verify that we prefer a better match (action + origin vs. origin).
  std::vector<PasswordForm> observed;
  PasswordForm expected_form;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  form.action = GURL("http://www.google.com/other/action");
  observed.push_back(form);
  expected_form = form;

  // The initial load.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // The initial layout.
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate either form changing or heuristics choosing other fields
  // after the user has entered their information.
  form.new_password_element = ASCIIToUTF16("new_password");
  form.new_password_value = form.password_value;
  form.password_element.clear();
  form.password_value.clear();

  // Saved signup forms don't have password set.
  expected_form.password_element.clear();

  manager()->ProvisionallySavePassword(form);

  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.

  ASSERT_TRUE(form_to_save);
  EXPECT_CALL(*store_, AddLogin(FormMatches(expected_form)));

  // Simulate saving the form, as if the info bar was accepted.
  form_to_save->Save();
}

TEST_F(PasswordManagerTest, SavingSignupForms_NoActionMatch) {
  // Signup forms don't require HTML attributes match in order to save.
  // Verify that we prefer a better match (HTML attributes + origin vs. origin).
  std::vector<PasswordForm> observed;
  PasswordForm expected_form;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  // Change the submit element so we can track which of the two forms is
  // chosen as a better match.
  form.submit_element = ASCIIToUTF16("different_signin");
  expected_form = form;
  form.new_password_element = ASCIIToUTF16("new_password");
  form.new_password_value = form.password_value;
  form.password_element.clear();
  form.password_value.clear();
  observed.push_back(form);

  // The initial load.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // The initial layout.
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate form changing it's action. Update expectation as well since
  // the action is copied during saving.
  form.action = GURL("http://www.google.com/other/action");
  expected_form.action = form.action;

  // Signup forms clear password element when saving.
  expected_form.password_element.clear();

  manager()->ProvisionallySavePassword(form);

  scoped_ptr<PasswordFormManager> form_to_save;
  EXPECT_CALL(client_,
              PromptUserToSavePasswordPtr(
                  _, CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_,
                                   observed);  // The post-navigation load.
  manager()->OnPasswordFormsRendered(&driver_, observed,
                                     true);  // The post-navigation layout.

  ASSERT_TRUE(form_to_save);
  EXPECT_CALL(*store_, AddLogin(FormMatches(expected_form)));

  // Simulate saving the form, as if the info bar was accepted.
  form_to_save->Save();
}

}  // namespace password_manager
