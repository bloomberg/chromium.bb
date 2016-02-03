// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace {

// Number of dismissals that for sure supresses the bubble.
const int kGreatDissmisalCount = 10;

class DialogPromptMock : public AccountChooserPrompt,
                         public AutoSigninFirstRunPrompt {
 public:
  DialogPromptMock() = default;

  MOCK_METHOD0(ShowAccountChooser, void());
  MOCK_METHOD0(ShowAutoSigninPrompt, void());
  MOCK_METHOD0(ControllerGone, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(DialogPromptMock);
};

class TestManagePasswordsIconView : public ManagePasswordsIconView {
 public:
  TestManagePasswordsIconView() {}

  void SetState(password_manager::ui::State state) override {
    state_ = state;
  }
  password_manager::ui::State state() { return state_; }

 private:
  password_manager::ui::State state_;

  DISALLOW_COPY_AND_ASSIGN(TestManagePasswordsIconView);
};

// This sublass is used to disable some code paths which are not essential for
// testing.
class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  TestManagePasswordsUIController(
      content::WebContents* contents,
      password_manager::PasswordManagerClient* client);
  ~TestManagePasswordsUIController() override;

  bool opened_bubble() const { return opened_bubble_; }

  MOCK_METHOD1(CreateAccountChooser,
               AccountChooserPrompt*(PasswordDialogController*));
  MOCK_METHOD1(CreateAutoSigninPrompt,
               AutoSigninFirstRunPrompt*(PasswordDialogController*));
  using ManagePasswordsUIController::DidNavigateMainFrame;

 private:
  void UpdateBubbleAndIconVisibility() override;
  void SavePasswordInternal() override {}
  void UpdatePasswordInternal(
      const autofill::PasswordForm& password_form) override {}
  void NeverSavePasswordInternal() override;

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

void TestManagePasswordsUIController::UpdateBubbleAndIconVisibility() {
  opened_bubble_ = IsAutomaticallyOpeningBubble();
  ManagePasswordsUIController::UpdateBubbleAndIconVisibility();
  if (opened_bubble_)
    OnBubbleShown();
}

void TestManagePasswordsUIController::NeverSavePasswordInternal() {
  autofill::PasswordForm blacklisted;
  blacklisted.origin = this->GetOrigin();
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.blacklisted_by_user = true;
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, blacklisted);
  password_manager::PasswordStoreChangeList list(1, change);
  OnLoginsChanged(list);
}

void CreateSmartBubbleFieldTrial() {
  using password_bubble_experiment::kSmartBubbleExperimentName;
  using password_bubble_experiment::kSmartBubbleThresholdParam;
  std::map<std::string, std::string> params;
  params[kSmartBubbleThresholdParam] =
      base::IntToString(kGreatDissmisalCount / 2);
  variations::AssociateVariationParams(kSmartBubbleExperimentName, "A", params);
  ASSERT_TRUE(
      base::FieldTrialList::CreateFieldTrial(kSmartBubbleExperimentName, "A"));
}

}  // namespace

class ManagePasswordsUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ManagePasswordsUIControllerTest()
      : password_manager_(&client_), field_trial_list_(nullptr) {}

  void SetUp() override;

  autofill::PasswordForm& test_local_form() { return test_local_form_; }
  autofill::PasswordForm& test_federated_form() { return test_federated_form_; }
  password_manager::CredentialInfo* credential_info() const {
    return credential_info_.get();
  }
  DialogPromptMock& dialog_prompt() { return dialog_prompt_; }

  TestManagePasswordsUIController* controller() {
    return static_cast<TestManagePasswordsUIController*>(
        ManagePasswordsUIController::FromWebContents(web_contents()));
  }

  void CredentialCallback(const password_manager::CredentialInfo& info) {
    credential_info_.reset(new password_manager::CredentialInfo(info));
  }

  void ExpectIconStateIs(password_manager::ui::State state);
  void ExpectIconAndControllerStateIs(password_manager::ui::State state);

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
  base::FieldTrialList field_trial_list_;
  DialogPromptMock dialog_prompt_;
};

void ManagePasswordsUIControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // Create the test UIController here so that it's bound to
  // |test_web_contents_|, and will be retrieved correctly via
  // ManagePasswordsUIController::FromWebContents in |controller()|.
  new TestManagePasswordsUIController(web_contents(), &client_);

  test_local_form_.origin = GURL("http://example.com/login");
  test_local_form_.username_value = base::ASCIIToUTF16("username");
  test_local_form_.password_value = base::ASCIIToUTF16("12345");

  test_federated_form_.origin = GURL("http://example.com/login");
  test_federated_form_.username_value = base::ASCIIToUTF16("username");
  test_federated_form_.federation_url = GURL("https://federation.test/");

  // We need to be on a "webby" URL for most tests.
  content::WebContentsTester::For(web_contents())
  ->NavigateAndCommit(GURL("http://example.com"));
}

void ManagePasswordsUIControllerTest::ExpectIconStateIs(
    password_manager::ui::State state) {
  TestManagePasswordsIconView view;
  controller()->UpdateIconAndBubbleState(&view);
  EXPECT_EQ(state, view.state());
}

void ManagePasswordsUIControllerTest::ExpectIconAndControllerStateIs(
    password_manager::ui::State state) {
  ExpectIconStateIs(state);
  EXPECT_EQ(state, controller()->GetState());
}

scoped_ptr<password_manager::PasswordFormManager>
ManagePasswordsUIControllerTest::CreateFormManagerWithBestMatches(
    const autofill::PasswordForm& observed_form,
    ScopedVector<autofill::PasswordForm> best_matches) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(&password_manager_, &client_,
                                                driver_.AsWeakPtr(),
                                                observed_form, true));
  test_form_manager->SimulateFetchMatchingLoginsFromPasswordStore();
  test_form_manager->OnGetPasswordStoreResults(std::move(best_matches));
  return test_form_manager;
}

scoped_ptr<password_manager::PasswordFormManager>
ManagePasswordsUIControllerTest::CreateFormManager() {
  ScopedVector<autofill::PasswordForm> stored_forms;
  stored_forms.push_back(new autofill::PasswordForm(test_local_form()));
  return CreateFormManagerWithBestMatches(test_local_form(),
                                          std::move(stored_forms));
}

TEST_F(ManagePasswordsUIControllerTest, DefaultState) {
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  EXPECT_EQ(GURL::EmptyGURL(), controller()->GetOrigin());

  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordAutofilled) {
  scoped_ptr<autofill::PasswordForm> test_form(
      new autofill::PasswordForm(test_local_form()));
  autofill::PasswordForm* test_form_ptr = test_form.get();
  base::string16 kTestUsername = test_form->username_value;
  autofill::PasswordFormMap map;
  map.insert(std::make_pair(kTestUsername, std::move(test_form)));
  controller()->OnPasswordAutofilled(map, map.begin()->second->origin);

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
  EXPECT_EQ(test_form_ptr->origin, controller()->GetOrigin());
  ASSERT_EQ(1u, controller()->GetCurrentForms().size());
  EXPECT_EQ(kTestUsername, controller()->GetCurrentForms()[0]->username_value);

  // Controller should store a separate copy of the form as it doesn't own it.
  EXPECT_NE(test_form_ptr, controller()->GetCurrentForms()[0]);

  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmitted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->GetState());
  EXPECT_TRUE(controller()->opened_bubble());
  EXPECT_EQ(test_local_form().origin, controller()->GetOrigin());

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
      CreateFormManagerWithBestMatches(test_local_form(),
                                       std::move(stored_forms));

  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->GetState());
  EXPECT_FALSE(controller()->opened_bubble());

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedBubbleSuppressed) {
  CreateSmartBubbleFieldTrial();
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  password_manager::InteractionsStats stats;
  stats.origin_domain = test_local_form().origin.GetOrigin();
  stats.username_value = test_local_form().username_value;
  stats.dismissal_count = kGreatDissmisalCount;
  auto interactions(make_scoped_ptr(
      new std::vector<scoped_ptr<password_manager::InteractionsStats>>));
  interactions->push_back(
      make_scoped_ptr(new password_manager::InteractionsStats(stats)));
  test_form_manager->OnGetSiteStatistics(std::move(interactions));
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->GetState());
  EXPECT_FALSE(controller()->opened_bubble());
  ASSERT_TRUE(controller()->GetCurrentInteractionStats());
  EXPECT_EQ(stats, *controller()->GetCurrentInteractionStats());

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
  variations::testing::ClearAllVariationParams();
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedBubbleNotSuppressed) {
  CreateSmartBubbleFieldTrial();
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  password_manager::InteractionsStats stats;
  stats.origin_domain = test_local_form().origin.GetOrigin();
  stats.username_value = base::ASCIIToUTF16("not my username");
  stats.dismissal_count = kGreatDissmisalCount;
  auto interactions(make_scoped_ptr(
      new std::vector<scoped_ptr<password_manager::InteractionsStats>>));
  interactions->push_back(
      make_scoped_ptr(new password_manager::InteractionsStats(stats)));
  test_form_manager->OnGetSiteStatistics(std::move(interactions));
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->GetState());
  EXPECT_TRUE(controller()->opened_bubble());
  EXPECT_FALSE(controller()->GetCurrentInteractionStats());

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
  variations::testing::ClearAllVariationParams();
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSaved) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  controller()->SavePassword();
  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordBlacklisted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  controller()->NeverSavePassword();
  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, RedirectNavigations) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);

  // Fake-redirect. We expect the bubble's state to persist so a user reasonably
  // have been able to interact with the bubble. This happens on
  // `accounts.google.com`, for instance.
  content::FrameNavigateParams params;
  params.transition = ui::PAGE_TRANSITION_SERVER_REDIRECT;
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(), params);

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, NormalNavigations) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_STATE);

  // Fake-navigate. We expect the bubble's state to be reset.
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
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  EXPECT_EQ(GURL::EmptyGURL(), controller()->GetOrigin());

  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, BlacklistedElsewhere) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map.insert(std::make_pair(
      kTestUsername,
      make_scoped_ptr(new autofill::PasswordForm(test_local_form()))));
  controller()->OnPasswordAutofilled(map, map.begin()->second->origin);

  test_local_form().blacklisted_by_user = true;
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, test_local_form());
  password_manager::PasswordStoreChangeList list(1, change);
  controller()->OnLoginsChanged(list);

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
  EXPECT_EQ(test_local_form().origin, controller()->GetOrigin());

  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, AutomaticPasswordSave) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());

  controller()->OnAutomaticPasswordSave(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::CONFIRMATION_STATE, controller()->GetState());

  controller()->OnBubbleHidden();
  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialLocal) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  GURL origin("http://example.com");
  PasswordDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_)).WillOnce(
      DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_TRUE(controller()->OnChooseCredentials(
      std::move(local_credentials), std::move(federated_credentials), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_EQ(origin, controller()->GetOrigin());
  EXPECT_THAT(controller()->GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));

  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);

  EXPECT_CALL(dialog_prompt(), ControllerGone());
  dialog_controller->OnChooseCredentials(
      test_local_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
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
  PasswordDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_)).WillOnce(
      DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_TRUE(controller()->OnChooseCredentials(
      std::move(local_credentials), std::move(federated_credentials), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_EQ(origin, controller()->GetOrigin());
  EXPECT_THAT(controller()->GetCurrentForms(),
              ElementsAre(Pointee(test_federated_form())));

  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);

  EXPECT_CALL(dialog_prompt(), ControllerGone());
  dialog_controller->OnChooseCredentials(
      test_federated_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
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
  PasswordDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_)).WillOnce(
      DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_TRUE(controller()->OnChooseCredentials(
      std::move(local_credentials), std::move(federated_credentials), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_EQ(0u, controller()->GetCurrentForms().size());
  EXPECT_EQ(origin, controller()->GetOrigin());

  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);

  EXPECT_CALL(dialog_prompt(), ControllerGone());
  dialog_controller->OnChooseCredentials(
     test_local_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED);
  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
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
  PasswordDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_)).WillOnce(
      DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_TRUE(controller()->OnChooseCredentials(
      std::move(local_credentials), std::move(federated_credentials), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_EQ(origin, controller()->GetOrigin());

  EXPECT_CALL(dialog_prompt(), ControllerGone()).Times(0);
  dialog_controller->OnCloseDialog();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
  ASSERT_TRUE(credential_info());
  EXPECT_TRUE(credential_info()->federation.is_empty());
  EXPECT_TRUE(credential_info()->password.empty());
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY,
            credential_info()->type);
}

TEST_F(ManagePasswordsUIControllerTest, AutoSignin) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  controller()->OnAutoSignin(std::move(local_credentials));
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, controller()->GetState());
  EXPECT_EQ(test_local_form().origin, controller()->GetOrigin());
  ASSERT_FALSE(controller()->GetCurrentForms().empty());
  EXPECT_EQ(test_local_form(), *controller()->GetCurrentForms()[0]);
  ExpectIconStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
  controller()->OnBubbleHidden();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, AutoSigninFirstRun) {
  EXPECT_CALL(*controller(), CreateAutoSigninPrompt(_)).WillOnce(
      Return(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ShowAutoSigninPrompt());
  controller()->OnPromptEnableAutoSignin();

  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  EXPECT_CALL(dialog_prompt(), ControllerGone());
}

TEST_F(ManagePasswordsUIControllerTest, AutoSigninFirstRunAfterAutofill) {
  // Setup the managed state first.
  scoped_ptr<autofill::PasswordForm> test_form(
      new autofill::PasswordForm(test_local_form()));
  autofill::PasswordForm* test_form_ptr = test_form.get();
  const base::string16 kTestUsername = test_form->username_value;
  autofill::PasswordFormMap map;
  map.insert(std::make_pair(kTestUsername, std::move(test_form)));
  controller()->OnPasswordAutofilled(map, test_form_ptr->origin);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());

  // Pop up the autosignin promo. The state should stay intact.
  EXPECT_CALL(*controller(), CreateAutoSigninPrompt(_)).WillOnce(
      Return(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ShowAutoSigninPrompt());
  controller()->OnPromptEnableAutoSignin();

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
  EXPECT_EQ(test_form_ptr->origin, controller()->GetOrigin());
  EXPECT_THAT(controller()->GetCurrentForms(),
              ElementsAre(Pointee(*test_form_ptr)));
  EXPECT_CALL(dialog_prompt(), ControllerGone());
}

TEST_F(ManagePasswordsUIControllerTest, AutoSigninFirstRunAfterNavigation) {
  // Pop up the autosignin promo.
  EXPECT_CALL(*controller(), CreateAutoSigninPrompt(_)).WillOnce(
      Return(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ShowAutoSigninPrompt());
  controller()->OnPromptEnableAutoSignin();

  EXPECT_CALL(dialog_prompt(), ControllerGone()).Times(0);
  content::FrameNavigateParams params;
  params.transition = ui::PAGE_TRANSITION_LINK;
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(), params);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ControllerGone());
}

TEST_F(ManagePasswordsUIControllerTest, AutofillDuringAutoSignin) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  controller()->OnAutoSignin(std::move(local_credentials));
  ExpectIconAndControllerStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
  scoped_ptr<autofill::PasswordForm> test_form(
      new autofill::PasswordForm(test_local_form()));
  autofill::PasswordFormMap map;
  base::string16 kTestUsername = test_form->username_value;
  map.insert(std::make_pair(kTestUsername, std::move(test_form)));
  controller()->OnPasswordAutofilled(map, map.begin()->second->origin);

  ExpectIconAndControllerStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, InactiveOnPSLMatched) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  scoped_ptr<autofill::PasswordForm> psl_matched_test_form(
      new autofill::PasswordForm(test_local_form()));
  psl_matched_test_form->is_public_suffix_match = true;
  map.insert(std::make_pair(kTestUsername, std::move(psl_matched_test_form)));
  controller()->OnPasswordAutofilled(map, map.begin()->second->origin);

  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
}

TEST_F(ManagePasswordsUIControllerTest, UpdatePasswordSubmitted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            controller()->GetState());

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordUpdated) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_local_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));

  ExpectIconStateIs(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  controller()->UpdatePassword(autofill::PasswordForm());
  ExpectIconStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, NavigationWhenUpdateBubbleActive) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            controller()->GetState());
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                     content::FrameNavigateParams());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  // The following line shouldn't crash browser.
  controller()->OnNoInteractionOnUpdate();
}
