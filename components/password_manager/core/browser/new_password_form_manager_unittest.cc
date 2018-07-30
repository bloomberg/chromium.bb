// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormStructure;
using autofill::FormFieldData;
using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using base::ASCIIToUTF16;
using base::TestMockTimeTaskRunner;
using testing::_;
using testing::Mock;
using testing::SaveArg;

namespace password_manager {

namespace {

// Indices of username and password fields in the observed form.
const int kUsernameFieldIndex = 1;
const int kPasswordFieldIndex = 2;

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() {}

  ~MockPasswordManagerDriver() override {}

  MOCK_METHOD1(FillPasswordForm, void(const PasswordFormFillData&));
  MOCK_METHOD1(AllowPasswordGenerationForForm, void(const PasswordForm&));
};

void CheckPendingCredentials(const PasswordForm& expected,
                             const PasswordForm& actual) {
  EXPECT_EQ(expected.signon_realm, actual.signon_realm);
  EXPECT_EQ(expected.origin, actual.origin);
  EXPECT_EQ(expected.action, actual.action);
  EXPECT_EQ(expected.username_value, actual.username_value);
  EXPECT_EQ(expected.password_value, actual.password_value);
  EXPECT_EQ(expected.username_element, actual.username_element);
  EXPECT_EQ(expected.password_element, actual.password_element);
  EXPECT_EQ(expected.blacklisted_by_user, actual.blacklisted_by_user);
  EXPECT_EQ(expected.form_data, actual.form_data);
}

}  // namespace

// TODO(https://crbug.com/831123): Test sending metrics.
// TODO(https://crbug.com/831123): Test create pending credentials when
// generation happened.
// TODO(https://crbug.com/831123): Test create pending credentials with
// Credential API.
class NewPasswordFormManagerTest : public testing::Test {
 public:
  NewPasswordFormManagerTest() : task_runner_(new TestMockTimeTaskRunner) {
    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");

    observed_form_.origin = origin;
    observed_form_.action = action;
    observed_form_.name = ASCIIToUTF16("sign-in");
    observed_form_.unique_renderer_id = 1;
    observed_form_.is_form_tag = true;

    observed_form_only_password_fields_ = observed_form_;

    FormFieldData field;
    field.name = ASCIIToUTF16("firstname");
    field.form_control_type = "text";
    field.unique_renderer_id = 1;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("username");
    field.id = field.name;
    field.form_control_type = "text";
    field.unique_renderer_id = 2;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("password");
    field.id = field.name;
    field.form_control_type = "password";
    field.unique_renderer_id = 3;
    observed_form_.fields.push_back(field);
    observed_form_only_password_fields_.fields.push_back(field);

    field.name = ASCIIToUTF16("password2");
    field.id = field.name;
    field.form_control_type = "password";
    field.unique_renderer_id = 5;
    observed_form_only_password_fields_.fields.push_back(field);

    saved_match_.origin = origin;
    saved_match_.action = action;
    saved_match_.signon_realm = "https://accounts.google.com/";
    saved_match_.preferred = true;
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.password_value = ASCIIToUTF16("test1");
    saved_match_.is_public_suffix_match = false;
    saved_match_.scheme = PasswordForm::SCHEME_HTML;

    parsed_form_ = saved_match_;
    parsed_form_.form_data = observed_form_;
    parsed_form_.username_element = observed_form_.fields[1].name;
    parsed_form_.password_element = observed_form_.fields[2].name;

    blacklisted_match_ = saved_match_;
    blacklisted_match_.blacklisted_by_user = true;
  }

 protected:
  FormData observed_form_;
  FormData observed_form_only_password_fields_;
  PasswordForm saved_match_;
  PasswordForm blacklisted_match_;
  PasswordForm parsed_form_;
  StubPasswordManagerClient client_;
  MockPasswordManagerDriver driver_;
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;
};

TEST_F(NewPasswordFormManagerTest, DoesManage) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  EXPECT_TRUE(form_manager.DoesManage(observed_form_, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager.DoesManage(observed_form_, nullptr));
  FormData another_form = observed_form_;
  another_form.is_form_tag = false;
  EXPECT_FALSE(form_manager.DoesManage(another_form, &driver_));

  another_form = observed_form_;
  another_form.unique_renderer_id = observed_form_.unique_renderer_id + 1;
  EXPECT_FALSE(form_manager.DoesManage(another_form, &driver_));
}

TEST_F(NewPasswordFormManagerTest, DoesManageNoFormTag) {
  observed_form_.is_form_tag = false;
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  FormData another_form = observed_form_;
  // Simulate that new input was added by JavaScript.
  another_form.fields.push_back(FormFieldData());
  EXPECT_TRUE(form_manager.DoesManage(another_form, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager.DoesManage(another_form, nullptr));
}

TEST_F(NewPasswordFormManagerTest, Autofill) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  EXPECT_CALL(driver_, AllowPasswordGenerationForForm(_));
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.origin, fill_data.origin);
  EXPECT_FALSE(fill_data.wait_for_username);
  EXPECT_EQ(observed_form_.fields[1].name, fill_data.username_field.name);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(observed_form_.fields[2].name, fill_data.password_field.name);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

// NewPasswordFormManager should always send fill data to renderer, even for
// sign-up forms (no "current-password" field, i.e., no password field to fill
// into). However, for sign-up forms, no particular password field should be
// identified for filling. That way, Chrome won't disturb the user by filling
// the sign-up form, but will be able to offer a manual fallback for filling if
// the form was misclassified.
TEST_F(NewPasswordFormManagerTest, AutofillSignUpForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  // Make |observed_form_| to be sign-up form.
  observed_form_.fields.back().autocomplete_attribute = "new-password";

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();
  constexpr uint32_t kNoID = FormFieldData::kNotSetFormControlRendererId;
  EXPECT_EQ(kNoID, fill_data.password_field.unique_renderer_id);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(NewPasswordFormManagerTest, AutofillWithBlacklistedMatch) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);

  fetcher.SetNonFederated({&saved_match_, &blacklisted_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.origin, fill_data.origin);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(NewPasswordFormManagerTest, SetSubmitted) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  EXPECT_FALSE(form_manager.is_submitted());
  EXPECT_TRUE(
      form_manager.SetSubmittedFormIfIsManaged(observed_form_, &driver_));
  EXPECT_TRUE(form_manager.is_submitted());

  FormData another_form = observed_form_;
  another_form.name += ASCIIToUTF16("1");
  // |another_form| is managed because the same |unique_renderer_id| as
  // |observed_form_|.
  EXPECT_TRUE(form_manager.SetSubmittedFormIfIsManaged(another_form, &driver_));
  EXPECT_TRUE(form_manager.is_submitted());

  form_manager.set_not_submitted();
  EXPECT_FALSE(form_manager.is_submitted());

  another_form.unique_renderer_id = observed_form_.unique_renderer_id + 1;
  EXPECT_FALSE(
      form_manager.SetSubmittedFormIfIsManaged(another_form, &driver_));
  EXPECT_FALSE(form_manager.is_submitted());

  // An identical form but in a different frame (represented here by a null
  // driver) is also not considered managed.
  EXPECT_FALSE(
      form_manager.SetSubmittedFormIfIsManaged(observed_form_, nullptr));
  EXPECT_FALSE(form_manager.is_submitted());
}

// Tests that when NewPasswordFormManager receives saved matches it waits for
// server predictions and fills on receving them.
TEST_F(NewPasswordFormManagerTest, ServerPredictionsWithinDelay) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();

  // Expects no filling on save matches receiving.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);
  Mock::VerifyAndClearExpectations(&driver_);

  FormStructure form_structure(observed_form_);
  form_structure.field(2)->set_server_type(autofill::PASSWORD);
  std::vector<FormStructure*> predictions{&form_structure};

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  form_manager.ProcessServerPredictions(predictions);
}

// Tests that NewPasswordFormManager fills after some delay even without
// server predictions.
TEST_F(NewPasswordFormManagerTest, ServerPredictionsAfterDelay) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();

  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);
  // Expect filling after passing filling delay.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  // Simulate passing filling delay.
  task_runner_->FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  FormStructure form_structure(observed_form_);
  form_structure.field(2)->set_server_type(autofill::PASSWORD);
  std::vector<FormStructure*> predictions{&form_structure};

  // Expect no filling on receiving server predictions because it was already
  // done.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  form_manager.ProcessServerPredictions(predictions);
  task_runner_->FastForwardUntilNoTasksRemain();
}

// Tests that filling happens immediately if server predictions are received
// before saved matches.
TEST_F(NewPasswordFormManagerTest, ServerPredictionsBeforeFetcher) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  // Expect no filling after receiving saved matches from |fetcher|, since
  // |form_manager| is waiting for server-side predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  FormStructure form_structure(observed_form_);
  form_structure.field(2)->set_server_type(autofill::PASSWORD);
  std::vector<FormStructure*> predictions{&form_structure};
  form_manager.ProcessServerPredictions(predictions);
  Mock::VerifyAndClearExpectations(&driver_);

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  fetcher.SetNonFederated({&saved_match_}, 0u);
}

// Tests creating pending credentials when the password store is empty.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsEmptyStore) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({}, 0u);

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value = ASCIIToUTF16("user1");
  submitted_form.fields[kPasswordFieldIndex].value = ASCIIToUTF16("pw1");

  PasswordForm expected = parsed_form_;
  expected.username_value = ASCIIToUTF16("user1");
  expected.password_value = ASCIIToUTF16("pw1");

  EXPECT_TRUE(
      form_manager.SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  CheckPendingCredentials(expected, form_manager.GetPendingCredentials());
}

// Tests creating pending credentials when new credentials are submitted and the
// store has another credentials saved.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsNewCredentials) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);

  FormData submitted_form = observed_form_;
  base::string16 username = saved_match_.username_value + ASCIIToUTF16("1");
  base::string16 password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[kUsernameFieldIndex].value = username;
  submitted_form.fields[kPasswordFieldIndex].value = password;
  PasswordForm expected = parsed_form_;
  expected.username_value = username;
  expected.password_value = password;

  EXPECT_TRUE(
      form_manager.SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  CheckPendingCredentials(expected, form_manager.GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsAlreadySaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;
  EXPECT_TRUE(
      form_manager.SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  CheckPendingCredentials(/* expected */ saved_match_,
                          form_manager.GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved PSL
// credentials.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsPSLMatchSaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  PasswordForm expected = saved_match_;

  saved_match_.origin = GURL("https://m.accounts.google.com/auth");
  saved_match_.signon_realm = "https://m.accounts.google.com/";
  saved_match_.is_public_suffix_match = true;

  fetcher.SetNonFederated({&saved_match_}, 0u);

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  EXPECT_TRUE(
      form_manager.SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  CheckPendingCredentials(expected, form_manager.GetPendingCredentials());
}

// Tests creating pending credentials when new credentials are different only in
// password with already saved one.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsPasswordOverriden) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);

  PasswordForm expected = saved_match_;
  expected.password_value += ASCIIToUTF16("1");

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form.fields[kPasswordFieldIndex].value = expected.password_value;
  EXPECT_TRUE(
      form_manager.SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  CheckPendingCredentials(expected, form_manager.GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsUpdate) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  submitted_form.fields[1].value = ASCIIToUTF16("verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = ASCIIToUTF16("verystrongpassword");

  EXPECT_TRUE(
      form_manager.SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  CheckPendingCredentials(expected, form_manager.GetPendingCredentials());
}

// Tests creating pending credentials when a change password form is submitted
// and there are multipe saved forms.
TEST_F(NewPasswordFormManagerTest,
       CreatePendingCredentialsUpdateMultipleSaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  PasswordForm another_saved_match = saved_match_;
  another_saved_match.username_value += ASCIIToUTF16("1");
  fetcher.SetNonFederated({&saved_match_, &another_saved_match}, 0u);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  submitted_form.fields[1].value = ASCIIToUTF16("verystrongpassword");

  PasswordForm expected;
  expected.origin = observed_form_.origin;
  expected.action = observed_form_.action;
  expected.password_value = ASCIIToUTF16("verystrongpassword");

  EXPECT_TRUE(
      form_manager.SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  CheckPendingCredentials(expected, form_manager.GetPendingCredentials());
}

// Tests that there is no crash even when the observed form is a not password
// form and the submitted form is password form.
TEST_F(NewPasswordFormManagerTest, NoCrashOnNonPasswordForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  FormData form_without_password_fields = observed_form_;
  // Remove the password field.
  form_without_password_fields.fields.resize(kPasswordFieldIndex);
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      form_without_password_fields, &fetcher);
  fetcher.SetNonFederated({}, 0u);

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value = ASCIIToUTF16("username");
  submitted_form.fields[kPasswordFieldIndex].value = ASCIIToUTF16("password");

  // Expect no crash.
  form_manager.SetSubmittedFormIfIsManaged(submitted_form, &driver_);
}

}  // namespace  password_manager
