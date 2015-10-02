// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_samples.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kUIDismissalReasonMetric[] = "PasswordManager.UIDismissalReason";

class TestSyncService : public ProfileSyncServiceMock {
 public:
  explicit TestSyncService(Profile* profile)
      : ProfileSyncServiceMock(profile), smartlock_enabled_(false) {}
  ~TestSyncService() override {}

  // FakeSyncService:
  bool HasSyncSetupCompleted() const override { return true; }
  bool IsSyncAllowed() const override { return true; }
  bool IsSyncActive() const override { return true; }
  syncer::ModelTypeSet GetActiveDataTypes() const override {
    return smartlock_enabled_ ? syncer::ModelTypeSet::All()
                              : syncer::ModelTypeSet();
  }
  bool CanSyncStart() const override { return true; }
  syncer::ModelTypeSet GetPreferredDataTypes() const override {
    return GetActiveDataTypes();
  }
  bool IsUsingSecondaryPassphrase() const override { return false; }

  void set_smartlock_enabled(bool smartlock_enabled) {
    smartlock_enabled_ = smartlock_enabled;
  }

 private:
  bool smartlock_enabled_;
};

scoped_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return make_scoped_ptr(new TestSyncService(static_cast<Profile*>(context)));
}

}  // namespace

class ManagePasswordsBubbleModelTest : public testing::Test {
 public:
  ManagePasswordsBubbleModelTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        test_web_contents_(
            content::WebContentsTester::CreateTestWebContents(&profile_,
                                                              nullptr)) {}

  void TearDown() override { model_.reset(); }

  PrefService* prefs() { return profile_.GetPrefs(); }

  TestingProfile* profile() { return &profile_; }

 protected:
  void SetUpWithState(password_manager::ui::State state,
                      ManagePasswordsBubbleModel::DisplayReason reason) {
    // Create the test UIController here so that it's bound to
    // |test_web_contents_| and therefore accessible to the model.
    ManagePasswordsUIControllerMock* mock =
        new ManagePasswordsUIControllerMock(test_web_contents_.get());
    mock->SetState(state);
    model_.reset(
        new ManagePasswordsBubbleModel(test_web_contents_.get(), reason));
    mock->UnsetState();
  }

  void PretendPasswordWaiting() {
    SetUpWithState(password_manager::ui::PENDING_PASSWORD_STATE,
                   ManagePasswordsBubbleModel::AUTOMATIC);
  }

  void PretendUpdatePasswordWaiting() {
    SetUpWithState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
                   ManagePasswordsBubbleModel::AUTOMATIC);
  }

  void PretendCredentialsWaiting() {
    SetUpWithState(password_manager::ui::CREDENTIAL_REQUEST_STATE,
                   ManagePasswordsBubbleModel::AUTOMATIC);
  }

  void PretendAutoSigningIn() {
    SetUpWithState(password_manager::ui::AUTO_SIGNIN_STATE,
                   ManagePasswordsBubbleModel::AUTOMATIC);
  }

  void PretendManagingPasswords() {
    SetUpWithState(password_manager::ui::MANAGE_STATE,
                   ManagePasswordsBubbleModel::USER_ACTION);
  }

  ManagePasswordsUIControllerMock* controller() {
    return static_cast<ManagePasswordsUIControllerMock*>(
        ManagePasswordsUIController::FromWebContents(
            test_web_contents_.get()));
  }

  content::WebContents* test_web_contents() { return test_web_contents_.get(); }

  scoped_ptr<ManagePasswordsBubbleModel> model_;
  autofill::PasswordForm test_form_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_ptr<content::WebContents> test_web_contents_;
};

TEST_F(ManagePasswordsBubbleModelTest, CloseWithoutInteraction) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NO_DIRECT_INTERACTION);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            model_->state());
  model_.reset();
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::NO_DIRECT_INTERACTION,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickSave) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  model_->OnSaveClicked();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_SAVE);
  model_.reset();

  EXPECT_TRUE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_SAVE,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickNever) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  model_->OnNeverForThisSiteClicked();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_NEVER);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, model_->state());
  model_.reset();

  EXPECT_FALSE(controller()->saved_password());
  EXPECT_TRUE(controller()->never_saved_password());
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_NEVER,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickManage) {
  base::HistogramTester histogram_tester;
  PretendManagingPasswords();
  model_->OnManageLinkClicked();

  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_MANAGE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
  model_.reset();
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_MANAGE,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickDone) {
  base::HistogramTester histogram_tester;
  PretendManagingPasswords();
  model_->OnDoneClicked();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_DONE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
  model_.reset();
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_DONE,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickCredential) {
  base::HistogramTester histogram_tester;
  PretendCredentialsWaiting();
  EXPECT_FALSE(controller()->choose_credential());
  autofill::PasswordForm form;
  model_->OnChooseCredentials(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_CREDENTIAL);
  model_.reset();
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());
  EXPECT_TRUE(controller()->choose_credential());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_CREDENTIAL,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickCancelCredential) {
  base::HistogramTester histogram_tester;
  PretendCredentialsWaiting();
  EXPECT_FALSE(controller()->choose_credential());
  model_->OnCancelClicked();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_CANCEL);
  model_.reset();
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());
  EXPECT_FALSE(controller()->choose_credential());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric, password_manager::metrics_util::CLICKED_CANCEL,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, DismissCredential) {
  base::HistogramTester histogram_tester;
  PretendCredentialsWaiting();
  EXPECT_FALSE(controller()->choose_credential());
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NO_DIRECT_INTERACTION);
  model_.reset();
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());
  EXPECT_FALSE(controller()->choose_credential());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::NO_DIRECT_INTERACTION,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, PopupAutoSigninToast) {
  base::HistogramTester histogram_tester;
  PretendAutoSigningIn();
  model_->OnAutoSignInToastTimeout();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT);
  model_.reset();

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, PopupAutoSigninAndManagedBubble) {
  base::HistogramTester histogram_tester;
  PretendAutoSigningIn();
  model_->OnAutoSignInToastTimeout();
  model_->OnAutoSignInClicked();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::AUTO_SIGNIN_TOAST_CLICKED);
  model_.reset();

  EXPECT_TRUE(controller()->manage_accounts());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::AUTO_SIGNIN_TOAST_CLICKED,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickUpdate) {
  PretendUpdatePasswordWaiting();
  model_->OnUpdateClicked(autofill::PasswordForm());
  model_.reset();
  EXPECT_TRUE(controller()->updated_password());
  EXPECT_FALSE(controller()->never_saved_password());
}

TEST_F(ManagePasswordsBubbleModelTest, ShowSmartLockWarmWelcome) {
  TestSyncService* sync_service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), &TestingSyncFactoryFunction));
  sync_service->set_smartlock_enabled(true);
  base::FieldTrialList field_trials(nullptr);
  base::FieldTrialList::CreateFieldTrial(
      password_bubble_experiment::kBrandingExperimentName,
      password_bubble_experiment::kSmartLockBrandingGroupName);

  PretendPasswordWaiting();
  EXPECT_TRUE(model_->ShouldShowGoogleSmartLockWelcome());
  model_.reset();
  model_.reset(new ManagePasswordsBubbleModel(
      test_web_contents(), ManagePasswordsBubbleModel::AUTOMATIC));
  EXPECT_FALSE(model_->ShouldShowGoogleSmartLockWelcome());
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kWasSavePrompFirstRunExperienceShown));
}

TEST_F(ManagePasswordsBubbleModelTest, OmitSmartLockWarmWelcome) {
  TestSyncService* sync_service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), &TestingSyncFactoryFunction));
  sync_service->set_smartlock_enabled(false);
  base::FieldTrialList field_trials(nullptr);
  base::FieldTrialList::CreateFieldTrial(
      password_bubble_experiment::kBrandingExperimentName,
      password_bubble_experiment::kSmartLockBrandingGroupName);

  PretendPasswordWaiting();
  EXPECT_FALSE(model_->ShouldShowGoogleSmartLockWelcome());
  model_.reset();
  model_.reset(new ManagePasswordsBubbleModel(
      test_web_contents(), ManagePasswordsBubbleModel::AUTOMATIC));
  EXPECT_FALSE(model_->ShouldShowGoogleSmartLockWelcome());
  EXPECT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::kWasSavePrompFirstRunExperienceShown));
}
