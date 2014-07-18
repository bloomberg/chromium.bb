// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_samples.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/statistics_delta_reader.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kUIDismissalReasonMetric[] = "PasswordManager.UIDismissalReason";

class ManagePasswordsBubbleModelTest : public testing::Test {
 public:
  ManagePasswordsBubbleModelTest()
      : test_web_contents_(
            content::WebContentsTester::CreateTestWebContents(&profile_,
                                                              NULL)) {}

  virtual void SetUp() OVERRIDE {
    // Create the test UIController here so that it's bound to
    // |test_web_contents_| and therefore accessible to the model.
    new ManagePasswordsUIControllerMock(test_web_contents_.get());

    model_.reset(new ManagePasswordsBubbleModel(test_web_contents_.get()));
  }

  virtual void TearDown() OVERRIDE { model_.reset(); }

  void PretendPasswordWaiting() {
    model_->set_state(password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE);
    model_->OnBubbleShown(ManagePasswordsBubble::AUTOMATIC);
    controller()->SetState(
        password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE);
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
  base::StatisticsDeltaReader statistics_delta_reader;
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NOT_DISPLAYED);
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kUIDismissalReasonMetric));
  EXPECT_EQ(NULL, samples.get());
}

TEST_F(ManagePasswordsBubbleModelTest, CloseWithoutInteraction) {
  base::StatisticsDeltaReader statistics_delta_reader;
  PretendPasswordWaiting();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::NO_DIRECT_INTERACTION);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE,
            model_->state());
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kUIDismissalReasonMetric));
  EXPECT_EQ(
      1,
      samples->GetCount(password_manager::metrics_util::NO_DIRECT_INTERACTION));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_SAVE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_NOPE));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_NEVER));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_MANAGE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_DONE));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::CLICKED_UNBLACKLIST));
}

TEST_F(ManagePasswordsBubbleModelTest, ClickSave) {
  base::StatisticsDeltaReader statistics_delta_reader;
  PretendPasswordWaiting();
  model_->OnSaveClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_SAVE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
  EXPECT_TRUE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kUIDismissalReasonMetric));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::NO_DIRECT_INTERACTION));
  EXPECT_EQ(1, samples->GetCount(password_manager::metrics_util::CLICKED_SAVE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_NOPE));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_NEVER));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_MANAGE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_DONE));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::CLICKED_UNBLACKLIST));
}

TEST_F(ManagePasswordsBubbleModelTest, ClickNope) {
  base::StatisticsDeltaReader statistics_delta_reader;
  PretendPasswordWaiting();
  model_->OnNopeClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_NOPE);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, model_->state());
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kUIDismissalReasonMetric));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::NO_DIRECT_INTERACTION));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_SAVE));
  EXPECT_EQ(1, samples->GetCount(password_manager::metrics_util::CLICKED_NOPE));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_NEVER));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_MANAGE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_DONE));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::CLICKED_UNBLACKLIST));
}

TEST_F(ManagePasswordsBubbleModelTest, ClickNever) {
  base::StatisticsDeltaReader statistics_delta_reader;
  PretendPasswordWaiting();
  model_->OnNeverForThisSiteClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_NEVER);
  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, model_->state());
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_TRUE(controller()->never_saved_password());

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kUIDismissalReasonMetric));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::NO_DIRECT_INTERACTION));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_SAVE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_NOPE));
  EXPECT_EQ(1,
            samples->GetCount(password_manager::metrics_util::CLICKED_NEVER));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_MANAGE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_DONE));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::CLICKED_UNBLACKLIST));
}

TEST_F(ManagePasswordsBubbleModelTest, ClickManage) {
  base::StatisticsDeltaReader statistics_delta_reader;
  PretendManagingPasswords();
  model_->OnManageLinkClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_MANAGE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kUIDismissalReasonMetric));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::NO_DIRECT_INTERACTION));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_SAVE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_NOPE));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_NEVER));
  EXPECT_EQ(1,
            samples->GetCount(password_manager::metrics_util::CLICKED_MANAGE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_DONE));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::CLICKED_UNBLACKLIST));
}

TEST_F(ManagePasswordsBubbleModelTest, ClickDone) {
  base::StatisticsDeltaReader statistics_delta_reader;
  PretendManagingPasswords();
  model_->OnDoneClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_DONE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kUIDismissalReasonMetric));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::NO_DIRECT_INTERACTION));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_SAVE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_NOPE));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_NEVER));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_MANAGE));
  EXPECT_EQ(1, samples->GetCount(password_manager::metrics_util::CLICKED_DONE));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::CLICKED_UNBLACKLIST));
}

TEST_F(ManagePasswordsBubbleModelTest, ClickUnblacklist) {
  base::StatisticsDeltaReader statistics_delta_reader;
  PretendBlacklisted();
  model_->OnUnblacklistClicked();
  model_->OnBubbleHidden();
  EXPECT_EQ(model_->dismissal_reason(),
            password_manager::metrics_util::CLICKED_UNBLACKLIST);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model_->state());
  EXPECT_FALSE(controller()->saved_password());
  EXPECT_FALSE(controller()->never_saved_password());

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kUIDismissalReasonMetric));
  EXPECT_EQ(
      0,
      samples->GetCount(password_manager::metrics_util::NO_DIRECT_INTERACTION));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_SAVE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_NOPE));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_NEVER));
  EXPECT_EQ(0,
            samples->GetCount(password_manager::metrics_util::CLICKED_MANAGE));
  EXPECT_EQ(0, samples->GetCount(password_manager::metrics_util::CLICKED_DONE));
  EXPECT_EQ(
      1,
      samples->GetCount(password_manager::metrics_util::CLICKED_UNBLACKLIST));
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
