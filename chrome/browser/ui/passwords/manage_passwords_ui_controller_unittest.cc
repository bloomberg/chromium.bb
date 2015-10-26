// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#endif

using ::testing::ElementsAre;
using ::testing::Pointee;

namespace {

const int64 kSlowNavigationDelayInMS = 6000;
const int64 kQuickNavigationDelayInMS = 500;

#if !defined(OS_ANDROID)
class TestManagePasswordsIconView : public ManagePasswordsIconView {
 public:
  TestManagePasswordsIconView() {}

  void SetState(password_manager::ui::State state) override {
    state_ = state;
  }
  password_manager::ui::State state() { return state_; }
  void SetActive(bool active) override {
    active_ = active;
  }
  bool active() { return active_; }

 private:
  password_manager::ui::State state_;
  bool active_;

  DISALLOW_COPY_AND_ASSIGN(TestManagePasswordsIconView);
};
#endif

// This sublass is used to disable some code paths which are not essential for
// testing.
class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  TestManagePasswordsUIController(
      content::WebContents* contents,
      password_manager::PasswordManagerClient* client);
  ~TestManagePasswordsUIController() override;

  base::TimeDelta Elapsed() const override;
  void SetElapsed(base::TimeDelta elapsed) { elapsed_ = elapsed; }
  bool opened_bubble() const { return opened_bubble_; }

  using ManagePasswordsUIController::DidNavigateMainFrame;

 private:
  void UpdateBubbleAndIconVisibility() override;
  void UpdateAndroidAccountChooserInfoBarVisibility() override;
  void SavePasswordInternal() override {}
  void UpdatePasswordInternal(
      const autofill::PasswordForm& password_form) override {}
  void NeverSavePasswordInternal() override;

  base::TimeDelta elapsed_;
  bool opened_bubble_;
};

TestManagePasswordsUIController::TestManagePasswordsUIController(
    content::WebContents* contents,
    password_manager::PasswordManagerClient* client)
    : ManagePasswordsUIController(contents) {
  // Do not silently replace an existing ManagePasswordsUIController because it
  // unregisters itself in WebContentsDestroyed().
  EXPECT_FALSE(contents->GetUserData(UserDataKey()));
  contents->SetUserData(UserDataKey(), this);
  set_client(client);
}

TestManagePasswordsUIController::~TestManagePasswordsUIController() {
}

base::TimeDelta TestManagePasswordsUIController::Elapsed() const {
  return elapsed_;
}

void TestManagePasswordsUIController::UpdateBubbleAndIconVisibility() {
  opened_bubble_ = IsAutomaticallyOpeningBubble();
  ManagePasswordsUIController::UpdateBubbleAndIconVisibility();
  if (opened_bubble_)
    OnBubbleShown();
}

void TestManagePasswordsUIController::
    UpdateAndroidAccountChooserInfoBarVisibility() {
  OnBubbleShown();
}

void TestManagePasswordsUIController::NeverSavePasswordInternal() {
  autofill::PasswordForm blacklisted;
  blacklisted.origin = this->origin();
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.blacklisted_by_user = true;
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, blacklisted);
  password_manager::PasswordStoreChangeList list(1, change);
  OnLoginsChanged(list);
}

}  // namespace

class ManagePasswordsUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ManagePasswordsUIControllerTest() : password_manager_(&client_) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Create the test UIController here so that it's bound to
    // |test_web_contents_|, and will be retrieved correctly via
    // ManagePasswordsUIController::FromWebContents in |controller()|.
    new TestManagePasswordsUIController(web_contents(), &client_);

    test_local_form_.origin = GURL("http://example.com");
    test_local_form_.username_value = base::ASCIIToUTF16("username");
    test_local_form_.password_value = base::ASCIIToUTF16("12345");

    test_federated_form_.origin = GURL("http://example.com");
    test_federated_form_.username_value = base::ASCIIToUTF16("username");
    test_federated_form_.federation_url = GURL("https://federation.test/");

    // We need to be on a "webby" URL for most tests.
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("http://example.com"));
  }

  void ExpectIconStateIs(password_manager::ui::State state) {
// No op on Android, where there is no icon.
#if !defined(OS_ANDROID)
    TestManagePasswordsIconView view;
    controller()->UpdateIconAndBubbleState(&view);
    EXPECT_EQ(state, view.state());
#endif
  }

  void ExpectIconAndControllerStateIs(password_manager::ui::State state) {
    ExpectIconStateIs(state);
    EXPECT_EQ(state, controller()->state());
  }

  autofill::PasswordForm& test_local_form() { return test_local_form_; }
  autofill::PasswordForm& test_federated_form() { return test_federated_form_; }
  password_manager::CredentialInfo* credential_info() const {
    return credential_info_.get();
  }

  TestManagePasswordsUIController* controller() {
    return static_cast<TestManagePasswordsUIController*>(
        ManagePasswordsUIController::FromWebContents(web_contents()));
  }

  void CredentialCallback(const password_manager::CredentialInfo& info) {
    credential_info_.reset(new password_manager::CredentialInfo(info));
  }

  scoped_ptr<password_manager::PasswordFormManager>
  CreateFormManagerWithBestMatches(
      const autofill::PasswordForm& observed_form,
      ScopedVector<autofill::PasswordForm> best_matches);

  scoped_ptr<password_manager::PasswordFormManager> CreateFormManager();

 private:
  password_manager::StubPasswordManagerClient client_;
  password_manager::StubPasswordManagerDriver driver_;
  password_manager::PasswordManager password_manager_;

  autofill::PasswordForm test_local_form_;
  autofill::PasswordForm test_federated_form_;
  scoped_ptr<password_manager::CredentialInfo> credential_info_;
};

scoped_ptr<password_manager::PasswordFormManager>
ManagePasswordsUIControllerTest::CreateFormManagerWithBestMatches(
    const autofill::PasswordForm& observed_form,
    ScopedVector<autofill::PasswordForm> best_matches) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(&password_manager_, &client_,
                                                driver_.AsWeakPtr(),
                                                observed_form, true));
  test_form_manager->SimulateFetchMatchingLoginsFromPasswordStore();
  test_form_manager->OnGetPasswordStoreResults(best_matches.Pass());
  return test_form_manager.Pass();
}

scoped_ptr<password_manager::PasswordFormManager>
ManagePasswordsUIControllerTest::CreateFormManager() {
  ScopedVector<autofill::PasswordForm> stored_forms;
  stored_forms.push_back(new autofill::PasswordForm(test_local_form()));
  return CreateFormManagerWithBestMatches(test_local_form(),
                                          stored_forms.Pass());
}

TEST_F(ManagePasswordsUIControllerTest, DefaultState) {
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordAutofilled) {
  scoped_ptr<autofill::PasswordForm> test_form(
      new autofill::PasswordForm(test_local_form()));
  autofill::PasswordForm* test_form_ptr = test_form.get();
  base::string16 kTestUsername = test_form->username_value;
  autofill::PasswordFormMap map;
  map.insert(kTestUsername, test_form.Pass());
  controller()->OnPasswordAutofilled(map, map.begin()->second->origin);

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_form_ptr->origin, controller()->origin());
  ASSERT_EQ(1u, controller()->GetCurrentForms().size());
  EXPECT_EQ(kTestUsername, controller()->GetCurrentForms()[0]->username_value);

  // Controller should store a separate copy of the form as it doesn't own it.
  EXPECT_NE(test_form_ptr, controller()->GetCurrentForms()[0]);

  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmitted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  controller()->OnPasswordSubmitted(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->state());
  EXPECT_TRUE(controller()->PasswordPendingUserDecision());
  EXPECT_TRUE(controller()->opened_bubble());

  // TODO(mkwst): This should be the value of test_local_form().origin, but
  // it's being masked by the stub implementation of
  // ManagePasswordsUIControllerMock::PendingCredentials.
  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, BlacklistedFormPasswordSubmitted) {
  autofill::PasswordForm blacklisted;
  blacklisted.origin = test_local_form().origin;
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.blacklisted_by_user = true;
  ScopedVector<autofill::PasswordForm> stored_forms;
  stored_forms.push_back(new autofill::PasswordForm(blacklisted));
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager =
      CreateFormManagerWithBestMatches(test_local_form(), stored_forms.Pass());

  controller()->OnPasswordSubmitted(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->state());
  EXPECT_TRUE(controller()->PasswordPendingUserDecision());
  EXPECT_FALSE(controller()->opened_bubble());

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSaved) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnPasswordSubmitted(test_form_manager.Pass());

  controller()->SavePassword();
  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordBlacklisted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnPasswordSubmitted(test_form_manager.Pass());

  controller()->NeverSavePassword();
  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, QuickNavigations) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  controller()->OnPasswordSubmitted(test_form_manager.Pass());
  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);

  // Fake-navigate within a second. We expect the bubble's state to persist
  // if a navigation occurs too quickly for a user to reasonably have been
  // able to interact with the bubble. This happens on `accounts.google.com`,
  // for instance.
  controller()->SetElapsed(
      base::TimeDelta::FromMilliseconds(kQuickNavigationDelayInMS));
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                     content::FrameNavigateParams());

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, SlowNavigations) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  controller()->OnPasswordSubmitted(test_form_manager.Pass());
  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);

  // Fake-navigate after a second. We expect the bubble's state to be reset
  // if a navigation occurs after this limit.
  controller()->SetElapsed(
      base::TimeDelta::FromMilliseconds(kSlowNavigationDelayInMS));
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                     content::FrameNavigateParams());

  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedToNonWebbyURL) {
  // Navigate to a non-webby URL, then see what happens!
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("chrome://sign-in"));

  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  controller()->OnPasswordSubmitted(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());

  // TODO(mkwst): This should be the value of test_local_form().origin, but
  // it's being masked by the stub implementation of
  // ManagePasswordsUIControllerMock::PendingCredentials.
  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, BlacklistedElsewhere) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map.insert(kTestUsername,
             make_scoped_ptr(new autofill::PasswordForm(test_local_form())));
  controller()->OnPasswordAutofilled(map, map.begin()->second->origin);

  test_local_form().blacklisted_by_user = true;
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, test_local_form());
  password_manager::PasswordStoreChangeList list(1, change);
  controller()->OnLoginsChanged(list);

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  EXPECT_EQ(test_local_form().origin, controller()->origin());

  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, AutomaticPasswordSave) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());

  controller()->OnAutomaticPasswordSave(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::CONFIRMATION_STATE, controller()->state());

  controller()->OnBubbleHidden();
  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialLocal) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  GURL origin("http://example.com");
  EXPECT_TRUE(controller()->OnChooseCredentials(
      local_credentials.Pass(), federated_credentials.Pass(), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(origin, controller()->origin());
  EXPECT_THAT(controller()->GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));

  ExpectIconStateIs(password_manager::ui::CREDENTIAL_REQUEST_STATE);

  controller()->ManagePasswordsUIController::ChooseCredential(
      test_local_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  ASSERT_TRUE(credential_info());
  EXPECT_EQ(test_local_form().username_value, credential_info()->id);
  EXPECT_EQ(test_local_form().password_value, credential_info()->password);
  EXPECT_TRUE(credential_info()->federation.is_empty());
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
            credential_info()->type);
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialLocalButFederated) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(
      new autofill::PasswordForm(test_federated_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  GURL origin("http://example.com");
  EXPECT_TRUE(controller()->OnChooseCredentials(
      local_credentials.Pass(), federated_credentials.Pass(), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(origin, controller()->origin());
  EXPECT_THAT(controller()->GetCurrentForms(),
              ElementsAre(Pointee(test_federated_form())));

  ExpectIconStateIs(password_manager::ui::CREDENTIAL_REQUEST_STATE);

  controller()->ManagePasswordsUIController::ChooseCredential(
      test_federated_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  ASSERT_TRUE(credential_info());
  EXPECT_EQ(test_federated_form().username_value, credential_info()->id);
  EXPECT_EQ(test_federated_form().federation_url,
            credential_info()->federation);
  EXPECT_TRUE(credential_info()->password.empty());
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED,
            credential_info()->type);
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialFederated) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  ScopedVector<autofill::PasswordForm> federated_credentials;
  federated_credentials.push_back(
      new autofill::PasswordForm(test_local_form()));
  GURL origin("http://example.com");
  EXPECT_TRUE(controller()->OnChooseCredentials(
      local_credentials.Pass(), federated_credentials.Pass(), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(0u, controller()->GetCurrentForms().size());
  EXPECT_EQ(origin, controller()->origin());

  ExpectIconStateIs(password_manager::ui::CREDENTIAL_REQUEST_STATE);

  controller()->ManagePasswordsUIController::ChooseCredential(
     test_local_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED);
  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  ASSERT_TRUE(credential_info());
  EXPECT_EQ(test_local_form().username_value, credential_info()->id);
  EXPECT_TRUE(credential_info()->password.empty());
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED,
            credential_info()->type);
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialCancel) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  GURL origin("http://example.com");
  EXPECT_TRUE(controller()->OnChooseCredentials(
      local_credentials.Pass(), federated_credentials.Pass(), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->state());
  EXPECT_EQ(origin, controller()->origin());
  controller()->ManagePasswordsUIController::ChooseCredential(
      test_local_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY);
  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  ASSERT_TRUE(credential_info());
  EXPECT_TRUE(credential_info()->federation.is_empty());
  EXPECT_TRUE(credential_info()->password.empty());
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY,
            credential_info()->type);
}

TEST_F(ManagePasswordsUIControllerTest, AutoSignin) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  controller()->OnAutoSignin(local_credentials.Pass());
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, controller()->state());
  EXPECT_EQ(test_local_form().origin, controller()->origin());
  ASSERT_FALSE(controller()->GetCurrentForms().empty());
  EXPECT_EQ(test_local_form(), *controller()->GetCurrentForms()[0]);
  ExpectIconStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
  controller()->OnBubbleHidden();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, AutoSigninClickThrough) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  controller()->OnAutoSignin(local_credentials.Pass());
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, controller()->state());
  EXPECT_EQ(test_local_form().origin, controller()->origin());
  ASSERT_FALSE(controller()->GetCurrentForms().empty());
  EXPECT_EQ(test_local_form(), *controller()->GetCurrentForms()[0]);
  ExpectIconStateIs(password_manager::ui::AUTO_SIGNIN_STATE);

  controller()->ManageAccounts();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, AutofillDuringAutoSignin) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  controller()->OnAutoSignin(local_credentials.Pass());
  ExpectIconAndControllerStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
  scoped_ptr<autofill::PasswordForm> test_form(
      new autofill::PasswordForm(test_local_form()));
  autofill::PasswordFormMap map;
  base::string16 kTestUsername = test_form->username_value;
  map.insert(kTestUsername, test_form.Pass());
  controller()->OnPasswordAutofilled(map, map.begin()->second->origin);

  ExpectIconAndControllerStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, InactiveOnPSLMatched) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  scoped_ptr<autofill::PasswordForm> psl_matched_test_form(
      new autofill::PasswordForm(test_local_form()));
  psl_matched_test_form->is_public_suffix_match = true;
  map.insert(kTestUsername, psl_matched_test_form.Pass());
  controller()->OnPasswordAutofilled(map, map.begin()->second->origin);

  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
}

TEST_F(ManagePasswordsUIControllerTest, UpdatePasswordSubmitted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  controller()->OnUpdatePasswordSubmitted(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            controller()->state());

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordUpdated) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnUpdatePasswordSubmitted(test_form_manager.Pass());

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  controller()->UpdatePassword(autofill::PasswordForm());
  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}
