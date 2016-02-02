// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

#include <utility>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser/profile_sync_service_mock.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_bubble_experiment::kBrandingExperimentName;
using password_bubble_experiment::kSmartLockBrandingGroupName;
using password_bubble_experiment::kSmartLockBrandingSavePromptOnlyGroupName;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

namespace {

const char kSiteOrigin[] = "http://example.com/login";
const char kUsername[] = "Admin";
const char kUIDismissalReasonMetric[] = "PasswordManager.UIDismissalReason";

class TestSyncService : public ProfileSyncServiceMock {
 public:
  explicit TestSyncService(Profile* profile)
      : ProfileSyncServiceMock(CreateProfileSyncServiceParamsForTest(profile)),
        smartlock_enabled_(false) {}
  ~TestSyncService() override {}

  // FakeSyncService:
  bool IsFirstSetupComplete() const override { return true; }
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

class ManagePasswordsBubbleModelTest : public ::testing::Test {
 public:
  ManagePasswordsBubbleModelTest() : field_trials_(nullptr) {}
  ~ManagePasswordsBubbleModelTest() override = default;

  void SetUp() override {
    test_web_contents_.reset(
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr));
    // Create the test UIController here so that it's bound to
    // |test_web_contents_| and therefore accessible to the model.
    new testing::StrictMock<ManagePasswordsUIControllerMock>(
        test_web_contents_.get());
    PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        password_manager::BuildPasswordStore<
            content::BrowserContext,
            testing::StrictMock<password_manager::MockPasswordStore>>);
  }

  void TearDown() override {
    // Reset WebContents first. It can happen if the user closes the tab.
    test_web_contents_.reset();
    model_.reset();
  }

  PrefService* prefs() { return profile_.GetPrefs(); }

  TestingProfile* profile() { return &profile_; }

  password_manager::MockPasswordStore* GetStore() {
    return static_cast<password_manager::MockPasswordStore*>(
        PasswordStoreFactory::GetInstance()
            ->GetForProfile(profile(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  ManagePasswordsUIControllerMock* controller() {
    return static_cast<ManagePasswordsUIControllerMock*>(
        PasswordsModelDelegateFromWebContents(test_web_contents_.get()));
  }

  content::WebContents* test_web_contents() { return test_web_contents_.get(); }

  ManagePasswordsBubbleModel* model() { return model_.get(); }

  void SetUpWithState(password_manager::ui::State state,
                      ManagePasswordsBubbleModel::DisplayReason reason);
  void PretendPasswordWaiting();
  void PretendUpdatePasswordWaiting();
  void PretendAutoSigningIn();
  void PretendManagingPasswords();

  void DestroyModel();
  void DestroyModelExpectReason(
      password_manager::metrics_util::UIDismissalReason dismissal_reason);

  static password_manager::InteractionsStats GetTestStats();
  static autofill::PasswordForm GetPendingPassword();

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_ptr<content::WebContents> test_web_contents_;
  base::FieldTrialList field_trials_;
  scoped_ptr<ManagePasswordsBubbleModel> model_;
};

void ManagePasswordsBubbleModelTest::SetUpWithState(
    password_manager::ui::State state,
    ManagePasswordsBubbleModel::DisplayReason reason) {
  ManagePasswordsUIControllerMock* mock = controller();
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*mock, GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*mock, GetState()).WillOnce(Return(state));
  EXPECT_CALL(*mock, OnBubbleShown());
  model_.reset(
      new ManagePasswordsBubbleModel(test_web_contents_.get(), reason));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(mock));
}

void ManagePasswordsBubbleModelTest::PretendPasswordWaiting() {
  autofill::PasswordForm form = GetPendingPassword();
  EXPECT_CALL(*controller(), GetPendingPassword()).WillOnce(ReturnRef(form));
  std::vector<const autofill::PasswordForm*> forms;
  EXPECT_CALL(*controller(), GetCurrentForms()).WillOnce(ReturnRef(forms));
  password_manager::InteractionsStats stats = GetTestStats();
  EXPECT_CALL(*controller(), GetCurrentInteractionStats())
      .WillOnce(Return(&stats));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_STATE,
                 ManagePasswordsBubbleModel::AUTOMATIC);
}

void ManagePasswordsBubbleModelTest::PretendUpdatePasswordWaiting() {
  autofill::PasswordForm form = GetPendingPassword();
  EXPECT_CALL(*controller(), GetPendingPassword()).WillOnce(ReturnRef(form));
  std::vector<const autofill::PasswordForm*> forms;
  EXPECT_CALL(*controller(), GetCurrentForms()).WillOnce(ReturnRef(forms));
  EXPECT_CALL(*controller(), IsPasswordOverridden()).WillOnce(Return(false));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
                 ManagePasswordsBubbleModel::AUTOMATIC);
}

void ManagePasswordsBubbleModelTest::PretendAutoSigningIn() {
  autofill::PasswordForm form = GetPendingPassword();
  EXPECT_CALL(*controller(), GetPendingPassword()).WillOnce(ReturnRef(form));
  SetUpWithState(password_manager::ui::AUTO_SIGNIN_STATE,
                 ManagePasswordsBubbleModel::AUTOMATIC);
}

void ManagePasswordsBubbleModelTest::PretendManagingPasswords() {
  autofill::PasswordForm form = GetPendingPassword();
  std::vector<const autofill::PasswordForm*> forms(1, &form);
  EXPECT_CALL(*controller(), GetCurrentForms()).WillOnce(ReturnRef(forms));
  SetUpWithState(password_manager::ui::MANAGE_STATE,
                 ManagePasswordsBubbleModel::USER_ACTION);
}

void ManagePasswordsBubbleModelTest::DestroyModel() {
  ManagePasswordsUIControllerMock* mock =
      test_web_contents_ ? controller() : nullptr;
  if (mock)
    EXPECT_CALL(*mock, OnBubbleHidden());
  model_.reset();
  if (mock)
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(mock));
}

void ManagePasswordsBubbleModelTest::DestroyModelExpectReason(
    password_manager::metrics_util::UIDismissalReason dismissal_reason) {
  base::HistogramTester histogram_tester;
  DestroyModel();
  histogram_tester.ExpectUniqueSample(kUIDismissalReasonMetric,
                                      dismissal_reason, 1);
}

// static
password_manager::InteractionsStats
ManagePasswordsBubbleModelTest::GetTestStats() {
  password_manager::InteractionsStats result;
  result.origin_domain = GURL(kSiteOrigin).GetOrigin();
  result.username_value = base::ASCIIToUTF16(kUsername);
  result.dismissal_count = 5;
  result.update_time = base::Time::FromTimeT(1);
  return result;
}

// static
autofill::PasswordForm ManagePasswordsBubbleModelTest::GetPendingPassword() {
  autofill::PasswordForm form;
  form.origin = GURL(kSiteOrigin);
  form.signon_realm = kSiteOrigin;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16("12345");
  return form;
}

TEST_F(ManagePasswordsBubbleModelTest, CloseWithoutInteraction) {
  PretendPasswordWaiting();

  EXPECT_EQ(model()->dismissal_reason(),
            password_manager::metrics_util::NO_DIRECT_INTERACTION);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, model()->state());
  scoped_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  base::Time now = base::Time::Now();
  clock->SetNow(now);
  model()->set_clock(std::move(clock));
  password_manager::InteractionsStats stats = GetTestStats();
  stats.dismissal_count++;
  stats.update_time = now;
  EXPECT_CALL(*GetStore(), AddSiteStatsImpl(stats));
  EXPECT_CALL(*controller(), SavePassword()).Times(0);
  EXPECT_CALL(*controller(), NeverSavePassword()).Times(0);
  DestroyModelExpectReason(
      password_manager::metrics_util::NO_DIRECT_INTERACTION);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickSave) {
  PretendPasswordWaiting();

  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*controller(), SavePassword());
  EXPECT_CALL(*controller(), NeverSavePassword()).Times(0);
  model()->OnSaveClicked();
  EXPECT_EQ(model()->dismissal_reason(),
            password_manager::metrics_util::CLICKED_SAVE);
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_SAVE);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickNever) {
  PretendPasswordWaiting();

  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*controller(), SavePassword()).Times(0);
  EXPECT_CALL(*controller(), NeverSavePassword());
  model()->OnNeverForThisSiteClicked();
  EXPECT_EQ(model()->dismissal_reason(),
            password_manager::metrics_util::CLICKED_NEVER);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, model()->state());
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_NEVER);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickManage) {
  PretendManagingPasswords();

  EXPECT_CALL(*controller(), NavigateToPasswordManagerSettingsPage());
  model()->OnManageLinkClicked();

  EXPECT_EQ(model()->dismissal_reason(),
            password_manager::metrics_util::CLICKED_MANAGE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model()->state());
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_MANAGE);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickDone) {
  PretendManagingPasswords();

  model()->OnDoneClicked();
  EXPECT_EQ(model()->dismissal_reason(),
            password_manager::metrics_util::CLICKED_DONE);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model()->state());
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_DONE);
}

TEST_F(ManagePasswordsBubbleModelTest, PopupAutoSigninToast) {
  PretendAutoSigningIn();

  model()->OnAutoSignInToastTimeout();
  EXPECT_EQ(model()->dismissal_reason(),
            password_manager::metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT);
  DestroyModelExpectReason(
      password_manager::metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickUpdate) {
  PretendUpdatePasswordWaiting();

  autofill::PasswordForm form;
  EXPECT_CALL(*controller(), UpdatePassword(form));
  model()->OnUpdateClicked(form);
  DestroyModel();
}

TEST_F(ManagePasswordsBubbleModelTest, ShowSmartLockWarmWelcome) {
  TestSyncService* sync_service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), &TestingSyncFactoryFunction));
  sync_service->set_smartlock_enabled(true);
  base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName,
                                         kSmartLockBrandingGroupName);

  PretendPasswordWaiting();

  EXPECT_TRUE(model()->ShouldShowGoogleSmartLockWelcome());
  EXPECT_CALL(*GetStore(), AddSiteStatsImpl(_));
  DestroyModel();
  PretendPasswordWaiting();

  EXPECT_FALSE(model()->ShouldShowGoogleSmartLockWelcome());
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kWasSavePrompFirstRunExperienceShown));
}

TEST_F(ManagePasswordsBubbleModelTest, OmitSmartLockWarmWelcome) {
  TestSyncService* sync_service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), &TestingSyncFactoryFunction));
  sync_service->set_smartlock_enabled(false);
  base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName,
                                         kSmartLockBrandingGroupName);

  PretendPasswordWaiting();

  EXPECT_FALSE(model()->ShouldShowGoogleSmartLockWelcome());
  EXPECT_CALL(*GetStore(), AddSiteStatsImpl(_));
  DestroyModel();
  PretendPasswordWaiting();

  EXPECT_FALSE(model()->ShouldShowGoogleSmartLockWelcome());
  EXPECT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::kWasSavePrompFirstRunExperienceShown));
}

namespace {

enum class SmartLockStatus { ENABLE, DISABLE };

struct TitleTestCase {
  const char* experiment_group;
  SmartLockStatus smartlock_status;
  const char* expected_title;
};

}  // namespace

class ManagePasswordsBubbleModelTitleTest
    : public ManagePasswordsBubbleModelTest,
      public ::testing::WithParamInterface<TitleTestCase> {};

TEST_P(ManagePasswordsBubbleModelTitleTest, BrandedTitleOnSaving) {
  TitleTestCase test_case = GetParam();
  TestSyncService* sync_service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), &TestingSyncFactoryFunction));
  sync_service->set_smartlock_enabled(test_case.smartlock_status ==
                                      SmartLockStatus::ENABLE);
  if (test_case.experiment_group) {
    base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName,
                                           test_case.experiment_group);
  }

  PretendPasswordWaiting();
  EXPECT_THAT(base::UTF16ToUTF8(model()->title()),
              testing::HasSubstr(test_case.expected_title));
}

namespace {

// Below, "Chrom" is the common prefix of Chromium and Google Chrome. Ideally,
// we would use the localised strings, but ResourceBundle does not get
// initialised for this unittest.
const TitleTestCase kTitleTestCases[] = {
    {kSmartLockBrandingGroupName, SmartLockStatus::ENABLE, "Google Smart Lock"},
    {kSmartLockBrandingSavePromptOnlyGroupName, SmartLockStatus::ENABLE,
     "Google Smart Lock"},
    {nullptr, SmartLockStatus::ENABLE, "Chrom"},
    {"Default", SmartLockStatus::ENABLE, "Chrom"},
    {kSmartLockBrandingGroupName, SmartLockStatus::DISABLE, "Chrom"},
    {kSmartLockBrandingSavePromptOnlyGroupName, SmartLockStatus::DISABLE,
     "Chrom"},
    {"Default", SmartLockStatus::DISABLE, "Chrom"},
    {nullptr, SmartLockStatus::DISABLE, "Chrom"},
};

}  // namespace

INSTANTIATE_TEST_CASE_P(Default,
                        ManagePasswordsBubbleModelTitleTest,
                        ::testing::ValuesIn(kTitleTestCases));

namespace {

enum class ManageLinkTarget { EXTERNAL_PASSWORD_MANAGER, SETTINGS_PAGE };

struct ManageLinkTestCase {
  const char* experiment_group;
  SmartLockStatus smartlock_status;
  ManageLinkTarget expected_target;
};

}  // namespace

class ManagePasswordsBubbleModelManageLinkTest
    : public ManagePasswordsBubbleModelTest,
      public ::testing::WithParamInterface<ManageLinkTestCase> {};

TEST_P(ManagePasswordsBubbleModelManageLinkTest, OnManageLinkClicked) {
  ManageLinkTestCase test_case = GetParam();
  TestSyncService* sync_service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), &TestingSyncFactoryFunction));
  sync_service->set_smartlock_enabled(test_case.smartlock_status ==
                                      SmartLockStatus::ENABLE);
  if (test_case.experiment_group) {
    base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName,
                                           test_case.experiment_group);
  }

  PretendManagingPasswords();

  switch (test_case.expected_target) {
    case ManageLinkTarget::EXTERNAL_PASSWORD_MANAGER:
      EXPECT_CALL(*controller(), NavigateToExternalPasswordManager());
      break;
    case ManageLinkTarget::SETTINGS_PAGE:
      EXPECT_CALL(*controller(), NavigateToPasswordManagerSettingsPage());
      break;
  }

  model()->OnManageLinkClicked();
}

namespace {

const ManageLinkTestCase kManageLinkTestCases[] = {
    {kSmartLockBrandingGroupName, SmartLockStatus::ENABLE,
     ManageLinkTarget::EXTERNAL_PASSWORD_MANAGER},
    {kSmartLockBrandingSavePromptOnlyGroupName, SmartLockStatus::ENABLE,
     ManageLinkTarget::SETTINGS_PAGE},
    {nullptr, SmartLockStatus::ENABLE, ManageLinkTarget::SETTINGS_PAGE},
    {"Default", SmartLockStatus::ENABLE, ManageLinkTarget::SETTINGS_PAGE},
    {kSmartLockBrandingGroupName, SmartLockStatus::DISABLE,
     ManageLinkTarget::SETTINGS_PAGE},
    {kSmartLockBrandingSavePromptOnlyGroupName, SmartLockStatus::DISABLE,
     ManageLinkTarget::SETTINGS_PAGE},
    {nullptr, SmartLockStatus::DISABLE, ManageLinkTarget::SETTINGS_PAGE},
    {"Default", SmartLockStatus::DISABLE, ManageLinkTarget::SETTINGS_PAGE},
};

}  // namespace

INSTANTIATE_TEST_CASE_P(Default,
                        ManagePasswordsBubbleModelManageLinkTest,
                        ::testing::ValuesIn(kManageLinkTestCases));

enum class BrandLinkTarget { SMART_LOCK_HOME, SMART_LOCK_HELP };

struct BrandLinkTestCase {
  const char* experiment_group;
  SmartLockStatus smartlock_status;
  BrandLinkTarget expected_target;
};

class ManagePasswordsBubbleModelBrandLinkTest
    : public ManagePasswordsBubbleModelTest,
      public ::testing::WithParamInterface<BrandLinkTestCase> {};

TEST_P(ManagePasswordsBubbleModelBrandLinkTest, OnBrandLinkClicked) {
  BrandLinkTestCase test_case = GetParam();
  TestSyncService* sync_service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), &TestingSyncFactoryFunction));
  sync_service->set_smartlock_enabled(test_case.smartlock_status ==
                                      SmartLockStatus::ENABLE);
  if (test_case.experiment_group) {
    base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName,
                                           test_case.experiment_group);
  }

  PretendManagingPasswords();

  switch (test_case.expected_target) {
    case BrandLinkTarget::SMART_LOCK_HOME:
      EXPECT_CALL(*controller(), NavigateToExternalPasswordManager());
      break;
    case BrandLinkTarget::SMART_LOCK_HELP:
      EXPECT_CALL(*controller(), NavigateToSmartLockHelpPage());
      break;
  }

  model()->OnBrandLinkClicked();
}

namespace {

const BrandLinkTestCase kBrandLinkTestCases[] = {
    {kSmartLockBrandingGroupName, SmartLockStatus::ENABLE,
     BrandLinkTarget::SMART_LOCK_HOME},
    {kSmartLockBrandingSavePromptOnlyGroupName, SmartLockStatus::ENABLE,
     BrandLinkTarget::SMART_LOCK_HELP},
};

}  // namespace

INSTANTIATE_TEST_CASE_P(Default,
                        ManagePasswordsBubbleModelBrandLinkTest,
                        ::testing::ValuesIn(kBrandLinkTestCases));
