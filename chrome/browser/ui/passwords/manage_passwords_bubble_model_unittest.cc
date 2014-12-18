// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_samples.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_url_collection_experiment.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kUIDismissalReasonMetric[] = "PasswordManager.UIDismissalReason";

class ManagePasswordsBubbleModelTest : public testing::Test {
 public:
  ManagePasswordsBubbleModelTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        test_web_contents_(
            content::WebContentsTester::CreateTestWebContents(&profile_,
                                                              NULL)) {}

  void SetUp() override {
    // Create the test UIController here so that it's bound to
    // |test_web_contents_| and therefore accessible to the model.
    new ManagePasswordsUIControllerMock(test_web_contents_.get());

    model_.reset(new ManagePasswordsBubbleModel(test_web_contents_.get()));
  }

  void TearDown() override { model_.reset(); }

  PrefService* prefs() { return profile_.GetPrefs(); }

  void PretendNeedToAskUserToSubmitURL() {
    model_->set_state(password_manager::ui::ASK_USER_REPORT_URL_STATE);
    EXPECT_FALSE(prefs()->GetBoolean(
        password_manager::prefs::kAllowToCollectURLBubbleWasShown));
    model_->OnBubbleShown(ManagePasswordsBubble::AUTOMATIC);
    EXPECT_TRUE(prefs()->GetBoolean(
        password_manager::prefs::kAllowToCollectURLBubbleWasShown));
    controller()->SetState(
        password_manager::ui::ASK_USER_REPORT_URL_BUBBLE_SHOWN_STATE);
  }

  void PretendUserInteractedWithAllowToSubmitBubbleBeforeNavigation() {
    // TODO(melandory) This method should be removed after solution where
    // "Ask to collect URL?" doesn't appear before navigation is implemented.
    model_->OnBubbleShown(ManagePasswordsBubble::AUTOMATIC);
    model_->set_state(
        password_manager::ui::
            ASK_USER_REPORT_URL_BUBBLE_SHOWN_BEFORE_TRANSITION_STATE);
    controller()->SetState(
        password_manager::ui::ASK_USER_REPORT_URL_BUBBLE_SHOWN_STATE);
  }

  void PretendPasswordWaiting() {
    model_->set_state(password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE);
    model_->OnBubbleShown(ManagePasswordsBubble::AUTOMATIC);
    controller()->SetState(
        password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE);
  }

  void PretendCredentialsWaiting() {
    model_->set_state(password_manager::ui::CREDENTIAL_REQUEST_STATE);
    model_->OnBubbleShown(ManagePasswordsBubble::AUTOMATIC);
    controller()->SetState(password_manager::ui::CREDENTIAL_REQUEST_STATE);
  }

  void PretendManagingPasswords() {
    model_->set_state(password_manager::ui::MANAGE_STATE);
    model_->OnBubbleShown(ManagePasswordsBubble::USER_ACTION);
    controller()->SetState(password_manager::ui::MANAGE_STATE);
  }

  void PretendBlacklisted() {
    model_->set_state(password_manager::ui::BLACKLIST_STATE);
    model_->OnBubbleShown(ManagePasswordsBubble::USER_ACTION);

    base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
    autofill::ConstPasswordFormMap map;
    map[kTestUsername] = &test_form_;
    controller()->SetPasswordFormMap(map);
    controller()->SetState(password_manager::ui::BLACKLIST_STATE);
  }

  ManagePasswordsUIControllerMock* controller() {
    return static_cast<ManagePasswordsUIControllerMock*>(
        ManagePasswordsUIController::FromWebContents(
            test_web_contents_.get()));
  }

 protected:
  scoped_ptr<ManagePasswordsBubbleModel> model_;
  autofill::PasswordForm test_form_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_ptr<content::WebContents> test_web_contents_;
};

TEST_F(ManagePasswordsBubbleModelTest, DefaultValues) {
  EXPECT_EQ(model_->display_disposition(),
            password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NOT_DISPLAYED);
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());
}

TEST_F(ManagePasswordsBubbleModelTest, CloseWithoutLogging) {
  base::HistogramTester histogram_tester;
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NOT_DISPLAYED);
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  scoped_ptr<base::HistogramSamples> samples(
      histogram_tester.GetHistogramSamplesSinceCreation(
          kUIDismissalReasonMetric));
  EXPECT_EQ(NULL, samples.get());
}

TEST_F(ManagePasswordsBubbleModelTest, CloseWithoutInteraction) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NO_DIRECT_INTERACTION);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE,
            model_->state());
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
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_SAVE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
  EXPECT_TRUE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_SAVE,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickNope) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  model_->OnNopeClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_NOPE);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, model_->state());
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_NOPE,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickNever) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  model_->OnNeverForThisSiteClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_NEVER);
  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, model_->state());
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
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_MANAGE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
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
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_DONE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_DONE,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickUnblacklist) {
  base::HistogramTester histogram_tester;
  PretendBlacklisted();
  model_->OnUnblacklistClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_UNBLACKLIST);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_UNBLACKLIST,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickCredential) {
  base::HistogramTester histogram_tester;
  PretendCredentialsWaiting();
  EXPECT_FALSE(controller()->choose_credential());
  autofill::PasswordForm form;
  model_->OnChooseCredentials(form);
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_CREDENTIAL);
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
  model_->OnNopeClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_NOPE);
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());
  EXPECT_TRUE(controller()->choose_credential());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_NOPE,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickCollectURL) {
  base::HistogramTester histogram_tester;
  PretendNeedToAskUserToSubmitURL();
  model_->OnCollectURLClicked("http://example.com");
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_COLLECT_URL);
  EXPECT_EQ(password_manager::ui::ASK_USER_REPORT_URL_BUBBLE_SHOWN_STATE,
            model_->state());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_COLLECT_URL, 1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickDoNotCollectURL) {
  base::HistogramTester histogram_tester;
  PretendNeedToAskUserToSubmitURL();
  model_->OnDoNotCollectURLClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_DO_NOT_COLLECT_URL);
  EXPECT_EQ(password_manager::ui::ASK_USER_REPORT_URL_BUBBLE_SHOWN_STATE,
            model_->state());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_DO_NOT_COLLECT_URL, 1);
}

TEST_F(ManagePasswordsBubbleModelTest,
       CollectURLBubbleCloseWithoutInteraction) {
  base::HistogramTester histogram_tester;
  PretendNeedToAskUserToSubmitURL();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NO_DIRECT_INTERACTION);
  EXPECT_EQ(password_manager::ui::ASK_USER_REPORT_URL_BUBBLE_SHOWN_STATE,
            model_->state());
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickCollectURLBeforeNavigation) {
  // TODO(melandory) This test case should be removed after solution where
  // "Ask to collect URL?" doesn't appear before navigation is implemented.
  base::HistogramTester histogram_tester;
  PretendUserInteractedWithAllowToSubmitBubbleBeforeNavigation();
  model_->OnCollectURLClicked("http://example.com");
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_COLLECT_URL);
  EXPECT_EQ(password_manager::ui::ASK_USER_REPORT_URL_BUBBLE_SHOWN_STATE,
            model_->state());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_COLLECT_URL, 1);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickDoNotCollectURLBeforeNavigation) {
  // TODO(melandory) This test case should be removed after solution where
  // "Ask to collect URL?" doesn't appear before navigation is implemented.
  base::HistogramTester histogram_tester;
  PretendUserInteractedWithAllowToSubmitBubbleBeforeNavigation();
  model_->OnDoNotCollectURLClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_DO_NOT_COLLECT_URL);
  EXPECT_EQ(password_manager::ui::ASK_USER_REPORT_URL_BUBBLE_SHOWN_STATE,
            model_->state());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_DO_NOT_COLLECT_URL, 1);
}

TEST_F(ManagePasswordsBubbleModelTest,
       CollectURLBubbleCloseWithoutInteractionBeforeNavigation) {
  // TODO(melandory) This test case should be removed after solution where
  // "Ask to collect URL?" doesn't appear before navigation is implemented.
  base::HistogramTester histogram_tester;
  PretendUserInteractedWithAllowToSubmitBubbleBeforeNavigation();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NO_DIRECT_INTERACTION);
  EXPECT_EQ(password_manager::ui::ASK_USER_REPORT_URL_BUBBLE_SHOWN_STATE,
            model_->state());
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
}

TEST_F(ManagePasswordsBubbleModelTest, DismissCredential) {
  base::HistogramTester histogram_tester;
  PretendCredentialsWaiting();
  EXPECT_FALSE(controller()->choose_credential());
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NO_DIRECT_INTERACTION);
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());
  EXPECT_TRUE(controller()->choose_credential());

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::NO_DIRECT_INTERACTION,
      1);
}

TEST_F(ManagePasswordsBubbleModelTest, PasswordPendingUserDecision) {
  EXPECT_FALSE(password_manager::ui::IsPendingState(model_->state()));

  model_->set_state(password_manager::ui::INACTIVE_STATE);
  EXPECT_FALSE(password_manager::ui::IsPendingState(model_->state()));
  model_->set_state(password_manager::ui::MANAGE_STATE);
  EXPECT_FALSE(password_manager::ui::IsPendingState(model_->state()));
  model_->set_state(password_manager::ui::BLACKLIST_STATE);
  EXPECT_FALSE(password_manager::ui::IsPendingState(model_->state()));

  model_->set_state(password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE);
  EXPECT_TRUE(password_manager::ui::IsPendingState(model_->state()));
  model_->set_state(password_manager::ui::PENDING_PASSWORD_STATE);
  EXPECT_TRUE(password_manager::ui::IsPendingState(model_->state()));
}
