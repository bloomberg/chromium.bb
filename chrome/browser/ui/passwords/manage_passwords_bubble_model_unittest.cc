// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_samples.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

namespace {

constexpr char kSignInPromoCountTilNoThanksMetric[] =
    "PasswordManager.SignInPromoCountTilNoThanks";
constexpr char kSignInPromoCountTilSignInMetric[] =
    "PasswordManager.SignInPromoCountTilSignIn";
constexpr char kSignInPromoDismissalCountMetric[] =
    "PasswordManager.SignInPromoDismissalCount";
constexpr char kSignInPromoDismissalReasonMetric[] =
    "PasswordManager.SignInPromo";
constexpr char kSiteOrigin[] = "http://example.com/login";
constexpr char kUsername[] = "Admin";
constexpr char kPassword[] = "AdminPass";
constexpr char kUIDismissalReasonMetric[] = "PasswordManager.UIDismissalReason";

class TestSyncService : public browser_sync::ProfileSyncServiceMock {
 public:
  enum class SyncedTypes { ALL, NONE };

  explicit TestSyncService(Profile* profile)
      : browser_sync::ProfileSyncServiceMock(
            CreateProfileSyncServiceParamsForTest(profile)),
        synced_types_(SyncedTypes::NONE) {}
  ~TestSyncService() override {}

  // FakeSyncService:
  bool IsFirstSetupComplete() const override { return true; }
  bool IsSyncAllowed() const override { return true; }
  bool IsSyncActive() const override { return true; }
  syncer::ModelTypeSet GetActiveDataTypes() const override {
    switch (synced_types_) {
      case SyncedTypes::ALL:
        return syncer::ModelTypeSet::All();
      case SyncedTypes::NONE:
        return syncer::ModelTypeSet();
    }
    NOTREACHED();
    return syncer::ModelTypeSet();
  }
  bool CanSyncStart() const override { return true; }
  syncer::ModelTypeSet GetPreferredDataTypes() const override {
    return GetActiveDataTypes();
  }
  bool IsUsingSecondaryPassphrase() const override { return false; }

  void set_synced_types(SyncedTypes synced_types) {
    synced_types_ = synced_types;
  }

 private:
  SyncedTypes synced_types_;
};

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return base::MakeUnique<TestSyncService>(static_cast<Profile*>(context));
}

}  // namespace

class ManagePasswordsBubbleModelTest : public ::testing::Test {
 public:
  ManagePasswordsBubbleModelTest() = default;
  ~ManagePasswordsBubbleModelTest() override = default;

  void SetUp() override {
    test_web_contents_.reset(
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr));
    mock_delegate_.reset(new PasswordsModelDelegateMock);
    ON_CALL(*mock_delegate_, GetPasswordFormMetricsRecorder())
        .WillByDefault(Return(nullptr));
    PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        password_manager::BuildPasswordStore<
            content::BrowserContext,
            testing::StrictMock<password_manager::MockPasswordStore>>);
  }

  void TearDown() override {
    // Reset the delegate first. It can happen if the user closes the tab.
    mock_delegate_.reset();
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

  PasswordsModelDelegateMock* controller() {
    return mock_delegate_.get();
  }

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
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<ManagePasswordsBubbleModel> model_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
};

void ManagePasswordsBubbleModelTest::SetUpWithState(
    password_manager::ui::State state,
    ManagePasswordsBubbleModel::DisplayReason reason) {
  GURL origin(kSiteOrigin);
  EXPECT_CALL(*controller(), GetOrigin()).WillOnce(ReturnRef(origin));
  EXPECT_CALL(*controller(), GetState()).WillOnce(Return(state));
  EXPECT_CALL(*controller(), OnBubbleShown());
  EXPECT_CALL(*controller(), GetWebContents()).WillRepeatedly(
      Return(test_web_contents_.get()));
  model_.reset(
      new ManagePasswordsBubbleModel(mock_delegate_->AsWeakPtr(), reason));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(controller()));
  EXPECT_CALL(*controller(), GetWebContents()).WillRepeatedly(
      Return(test_web_contents_.get()));
}

void ManagePasswordsBubbleModelTest::PretendPasswordWaiting() {
  autofill::PasswordForm form = GetPendingPassword();
  EXPECT_CALL(*controller(), GetPendingPassword()).WillOnce(ReturnRef(form));
  password_manager::InteractionsStats stats = GetTestStats();
  EXPECT_CALL(*controller(), GetCurrentInteractionStats())
      .WillOnce(Return(&stats));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_STATE,
                 ManagePasswordsBubbleModel::AUTOMATIC);
}

void ManagePasswordsBubbleModelTest::PretendUpdatePasswordWaiting() {
  autofill::PasswordForm form = GetPendingPassword();
  EXPECT_CALL(*controller(), GetPendingPassword()).WillOnce(ReturnRef(form));
  std::vector<std::unique_ptr<autofill::PasswordForm>> forms;
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
  std::vector<std::unique_ptr<autofill::PasswordForm>> forms(1);
  forms[0].reset(new autofill::PasswordForm(GetPendingPassword()));
  EXPECT_CALL(*controller(), GetCurrentForms()).WillOnce(ReturnRef(forms));
  SetUpWithState(password_manager::ui::MANAGE_STATE,
                 ManagePasswordsBubbleModel::USER_ACTION);
}

void ManagePasswordsBubbleModelTest::DestroyModel() {
  EXPECT_CALL(*controller(), OnBubbleHidden());
  model_.reset();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(controller()));
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
  form.password_value = base::ASCIIToUTF16(kPassword);
  return form;
}

TEST_F(ManagePasswordsBubbleModelTest, CloseWithoutInteraction) {
  PretendPasswordWaiting();

  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, model()->state());
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  base::Time now = base::Time::Now();
  clock->SetNow(now);
  model()->SetClockForTesting(std::move(clock));
  password_manager::InteractionsStats stats = GetTestStats();
  stats.dismissal_count++;
  stats.update_time = now;
  EXPECT_CALL(*GetStore(), AddSiteStatsImpl(stats));
  EXPECT_CALL(*controller(), OnNoInteraction());
  EXPECT_CALL(*controller(), SavePassword(_, _)).Times(0);
  EXPECT_CALL(*controller(), NeverSavePassword()).Times(0);
  DestroyModelExpectReason(
      password_manager::metrics_util::NO_DIRECT_INTERACTION);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickSave) {
  PretendPasswordWaiting();

  EXPECT_TRUE(model()->enable_editing());

  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*controller(), SavePassword(GetPendingPassword().username_value,
                                          GetPendingPassword().password_value));
  EXPECT_CALL(*controller(), NeverSavePassword()).Times(0);
  model()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_SAVE);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickNever) {
  PretendPasswordWaiting();

  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*controller(), SavePassword(_, _)).Times(0);
  EXPECT_CALL(*controller(), NeverSavePassword());
  model()->OnNeverForThisSiteClicked();
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, model()->state());
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_NEVER);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickManage) {
  PretendManagingPasswords();

  EXPECT_CALL(*controller(), NavigateToPasswordManagerSettingsPage());
  model()->OnManageLinkClicked();

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model()->state());
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_MANAGE);
}

TEST_F(ManagePasswordsBubbleModelTest, ClickDone) {
  PretendManagingPasswords();

  model()->OnDoneClicked();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, model()->state());
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_DONE);
}

TEST_F(ManagePasswordsBubbleModelTest, PopupAutoSigninToast) {
  PretendAutoSigningIn();

  model()->OnAutoSignInToastTimeout();
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

TEST_F(ManagePasswordsBubbleModelTest, EditCredential) {
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));

  const base::string16 kExpectedUsername = base::UTF8ToUTF16("new_username");
  const base::string16 kExpectedPassword = base::UTF8ToUTF16("new_password");

  model()->OnCredentialEdited(kExpectedUsername, kExpectedPassword);
  EXPECT_EQ(kExpectedUsername, model()->pending_password().username_value);
  EXPECT_EQ(kExpectedPassword, model()->pending_password().password_value);
  EXPECT_CALL(*controller(),
              SavePassword(kExpectedUsername, kExpectedPassword));
  EXPECT_CALL(*controller(), NeverSavePassword()).Times(0);
  model()->OnSaveClicked();
  DestroyModel();
}

TEST_F(ManagePasswordsBubbleModelTest, OnBrandLinkClicked) {
  PretendPasswordWaiting();

  EXPECT_CALL(*controller(), NavigateToSmartLockHelpPage());
  model()->OnBrandLinkClicked();
}

TEST_F(ManagePasswordsBubbleModelTest, SuppressSignInPromo) {
  prefs()->SetBoolean(password_manager::prefs::kWasSignInPasswordPromoClicked,
                      true);
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*controller(), SavePassword(GetPendingPassword().username_value,
                                          GetPendingPassword().password_value));
  model()->OnSaveClicked();

  EXPECT_FALSE(model()->ReplaceToShowPromotionIfNeeded());
  DestroyModel();
  histogram_tester.ExpectTotalCount(kSignInPromoDismissalReasonMetric, 0);
  histogram_tester.ExpectTotalCount(kSignInPromoCountTilSignInMetric, 0);
  histogram_tester.ExpectTotalCount(kSignInPromoCountTilNoThanksMetric, 0);
  histogram_tester.ExpectTotalCount(kSignInPromoDismissalCountMetric, 0);
}

TEST_F(ManagePasswordsBubbleModelTest, SignInPromoOK) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*controller(), SavePassword(GetPendingPassword().username_value,
                                          GetPendingPassword().password_value));
  model()->OnSaveClicked();

  EXPECT_TRUE(model()->ReplaceToShowPromotionIfNeeded());
  EXPECT_CALL(*controller(), NavigateToChromeSignIn());
  model()->OnSignInToChromeClicked();
  DestroyModel();
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_SAVE, 1);
  histogram_tester.ExpectUniqueSample(
      kSignInPromoDismissalReasonMetric,
      password_manager::metrics_util::CHROME_SIGNIN_OK, 1);
  histogram_tester.ExpectUniqueSample(kSignInPromoCountTilSignInMetric, 1, 1);
  histogram_tester.ExpectTotalCount(kSignInPromoCountTilNoThanksMetric, 0);
  histogram_tester.ExpectTotalCount(kSignInPromoDismissalCountMetric, 0);
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked));
}

TEST_F(ManagePasswordsBubbleModelTest, SignInPromoCancel) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*controller(), SavePassword(GetPendingPassword().username_value,
                                          GetPendingPassword().password_value));
  model()->OnSaveClicked();

  EXPECT_TRUE(model()->ReplaceToShowPromotionIfNeeded());
  model()->OnSkipSignInClicked();
  DestroyModel();
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_SAVE, 1);
  histogram_tester.ExpectUniqueSample(
      kSignInPromoDismissalReasonMetric,
      password_manager::metrics_util::CHROME_SIGNIN_CANCEL, 1);
  histogram_tester.ExpectUniqueSample(kSignInPromoCountTilNoThanksMetric, 1, 1);
  histogram_tester.ExpectTotalCount(kSignInPromoCountTilSignInMetric, 0);
  histogram_tester.ExpectTotalCount(kSignInPromoDismissalCountMetric, 0);
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked));
}

TEST_F(ManagePasswordsBubbleModelTest, SignInPromoDismiss) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*controller(), SavePassword(GetPendingPassword().username_value,
                                          GetPendingPassword().password_value));
  model()->OnSaveClicked();

  EXPECT_TRUE(model()->ReplaceToShowPromotionIfNeeded());
  DestroyModel();
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric,
      password_manager::metrics_util::CLICKED_SAVE, 1);
  histogram_tester.ExpectUniqueSample(
      kSignInPromoDismissalReasonMetric,
      password_manager::metrics_util::CHROME_SIGNIN_DISMISSED, 1);
  histogram_tester.ExpectTotalCount(kSignInPromoCountTilSignInMetric, 0);
  histogram_tester.ExpectTotalCount(kSignInPromoCountTilNoThanksMetric, 0);
  histogram_tester.ExpectUniqueSample(kSignInPromoDismissalCountMetric, 1, 1);
  EXPECT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked));
}

namespace {

struct TitleTestCase {
  TestSyncService::SyncedTypes synced_types;
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
  sync_service->set_synced_types(test_case.synced_types);

  PretendPasswordWaiting();
  EXPECT_THAT(base::UTF16ToUTF8(model()->title()),
              testing::HasSubstr(test_case.expected_title));
}

namespace {

// Below, "Chrom" is the common prefix of Chromium and Google Chrome. Ideally,
// we would use the localised strings, but ResourceBundle does not get
// initialised for this unittest.
constexpr TitleTestCase kTitleTestCases[] = {
    {TestSyncService::SyncedTypes::ALL, "Google Smart Lock"},
    {TestSyncService::SyncedTypes::NONE, "Chrom"},
};

}  // namespace

INSTANTIATE_TEST_CASE_P(Default,
                        ManagePasswordsBubbleModelTitleTest,
                        ::testing::ValuesIn(kTitleTestCases));

class ManagePasswordsBubbleModelManageLinkTest
    : public ManagePasswordsBubbleModelTest,
      public ::testing::WithParamInterface<TestSyncService::SyncedTypes> {};

TEST_P(ManagePasswordsBubbleModelManageLinkTest, OnManageLinkClicked) {
  TestSyncService* sync_service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), &TestingSyncFactoryFunction));
  sync_service->set_synced_types(GetParam());

  PretendManagingPasswords();

  EXPECT_CALL(*controller(), NavigateToPasswordManagerSettingsPage());

  model()->OnManageLinkClicked();
}

INSTANTIATE_TEST_CASE_P(Default,
                        ManagePasswordsBubbleModelManageLinkTest,
                        ::testing::Values(TestSyncService::SyncedTypes::ALL,
                                          TestSyncService::SyncedTypes::NONE));

// Verify that URL keyed metrics are properly recorded.
TEST_F(ManagePasswordsBubbleModelTest, RecordUKMs) {
  using BubbleDismissalReason =
      password_manager::PasswordFormMetricsRecorder::BubbleDismissalReason;
  using BubbleTrigger =
      password_manager::PasswordFormMetricsRecorder::BubbleTrigger;
  using password_manager::metrics_util::CredentialSourceType;
  using UkmEntry = ukm::builders::PasswordForm;

  // |credential_management_api| defines whether credentials originate from the
  // credential management API.
  for (const bool credential_management_api : {false, true}) {
    // |update| defines whether this is an update or a save bubble.
    for (const bool update : {false, true}) {
      for (const auto interaction :
           {BubbleDismissalReason::kAccepted, BubbleDismissalReason::kDeclined,
            BubbleDismissalReason::kIgnored}) {
        SCOPED_TRACE(testing::Message()
                     << "update = " << update
                     << ", interaction = " << static_cast<int64_t>(interaction)
                     << ", credential management api ="
                     << credential_management_api);
        ukm::TestAutoSetUkmRecorder test_ukm_recorder;
        {
          // Setup metrics recorder
          auto recorder = base::MakeRefCounted<
              password_manager::PasswordFormMetricsRecorder>(
              true /*is_main_frame_secure*/, &test_ukm_recorder,
              test_ukm_recorder.GetNewSourceID(),
              GURL("https://www.example.com/"));

          // Exercise bubble.
          ON_CALL(*controller(), GetPasswordFormMetricsRecorder())
              .WillByDefault(Return(recorder.get()));
          ON_CALL(*controller(), GetCredentialSource())
              .WillByDefault(
                  Return(credential_management_api
                             ? CredentialSourceType::kCredentialManagementAPI
                             : CredentialSourceType::kPasswordManager));

          if (update)
            PretendUpdatePasswordWaiting();
          else
            PretendPasswordWaiting();

          if (interaction == BubbleDismissalReason::kAccepted && update) {
            autofill::PasswordForm form;
            EXPECT_CALL(*controller(), UpdatePassword(form));
            model()->OnUpdateClicked(form);
          } else if (interaction == BubbleDismissalReason::kAccepted &&
                     !update) {
            EXPECT_CALL(*GetStore(),
                        RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
            EXPECT_CALL(*controller(),
                        SavePassword(GetPendingPassword().username_value,
                                     GetPendingPassword().password_value));
            model()->OnSaveClicked();
          } else if (interaction == BubbleDismissalReason::kDeclined &&
                     update) {
            EXPECT_CALL(*controller(), SavePassword(_, _)).Times(0);
            model()->OnNopeUpdateClicked();
          } else if (interaction == BubbleDismissalReason::kDeclined &&
                     !update) {
            EXPECT_CALL(*GetStore(),
                        RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
            EXPECT_CALL(*controller(), SavePassword(_, _)).Times(0);
            EXPECT_CALL(*controller(), NeverSavePassword());
            model()->OnNeverForThisSiteClicked();
          } else if (interaction == BubbleDismissalReason::kIgnored && update) {
            EXPECT_CALL(*controller(), SavePassword(_, _)).Times(0);
            EXPECT_CALL(*controller(), NeverSavePassword()).Times(0);
          } else if (interaction == BubbleDismissalReason::kIgnored &&
                     !update) {
            EXPECT_CALL(*GetStore(), AddSiteStatsImpl(testing::_));
            EXPECT_CALL(*controller(), OnNoInteraction());
            EXPECT_CALL(*controller(), SavePassword(_, _)).Times(0);
            EXPECT_CALL(*controller(), NeverSavePassword()).Times(0);
          } else {
            NOTREACHED();
          }
          DestroyModel();
        }

        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(controller()));
        // Flush async calls on password store.
        base::RunLoop().RunUntilIdle();
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(GetStore()));

        // Verify metrics.
        const ukm::UkmSource* source =
            test_ukm_recorder.GetSourceForUrl("https://www.example.com/");
        ASSERT_TRUE(source);
        test_ukm_recorder.ExpectMetric(
            *source, UkmEntry::kEntryName,
            update ? UkmEntry::kUpdating_Prompt_ShownName
                   : UkmEntry::kSaving_Prompt_ShownName,
            1);
        test_ukm_recorder.ExpectMetric(
            *source, UkmEntry::kEntryName,
            update ? UkmEntry::kUpdating_Prompt_TriggerName
                   : UkmEntry::kSaving_Prompt_TriggerName,
            static_cast<int64_t>(
                credential_management_api
                    ? BubbleTrigger::kCredentialManagementAPIAutomatic
                    : BubbleTrigger::kPasswordManagerSuggestionAutomatic));
        test_ukm_recorder.ExpectMetric(
            *source, UkmEntry::kEntryName,
            update ? UkmEntry::kUpdating_Prompt_InteractionName
                   : UkmEntry::kSaving_Prompt_InteractionName,
            static_cast<int64_t>(interaction));
      }
    }
  }
}

TEST_F(ManagePasswordsBubbleModelTest, EyeIcon) {
  for (bool is_manual_fallback_for_saving : {false, true}) {
    for (bool form_has_autofilled_value : {false, true}) {
      for (ManagePasswordsBubbleModel::DisplayReason display_reason :
           {ManagePasswordsBubbleModel::AUTOMATIC,
            ManagePasswordsBubbleModel::USER_ACTION}) {
        if (is_manual_fallback_for_saving &&
            (display_reason == ManagePasswordsBubbleModel::AUTOMATIC))
          continue;

        SCOPED_TRACE(testing::Message()
                     << "is_manual_fallback_for_saving = "
                     << is_manual_fallback_for_saving
                     << " form_has_autofilled_value = "
                     << form_has_autofilled_value << " display_reason = "
                     << (display_reason == ManagePasswordsBubbleModel::AUTOMATIC
                             ? "AUTOMATIC"
                             : "USER_ACTION"));

        autofill::PasswordForm form = GetPendingPassword();
        form.form_has_autofilled_value = form_has_autofilled_value;
        EXPECT_CALL(*controller(), GetPendingPassword())
            .WillOnce(ReturnRef(form));
        password_manager::InteractionsStats stats = GetTestStats();
        EXPECT_CALL(*controller(), GetCurrentInteractionStats())
            .WillOnce(Return(&stats));
        if (display_reason == ManagePasswordsBubbleModel::AUTOMATIC)
          EXPECT_CALL(*GetStore(), AddSiteStatsImpl(_));

        EXPECT_CALL(*controller(), BubbleIsManualFallbackForSaving())
            .WillOnce(Return(is_manual_fallback_for_saving));
        SetUpWithState(password_manager::ui::PENDING_PASSWORD_STATE,
                       display_reason);
        EXPECT_EQ(
            is_manual_fallback_for_saving
                ? form_has_autofilled_value
                : display_reason == ManagePasswordsBubbleModel::USER_ACTION,
            model()->hide_eye_icon());

        DestroyModel();
        // Flush async calls on password store.
        base::RunLoop().RunUntilIdle();
        testing::Mock::VerifyAndClearExpectations(GetStore());
      }
    }
  }
}

TEST_F(ManagePasswordsBubbleModelTest, DisableEditing) {
  autofill::PasswordForm form = GetPendingPassword();
  EXPECT_CALL(*controller(), GetPendingPassword()).WillOnce(ReturnRef(form));
  password_manager::InteractionsStats stats = GetTestStats();
  EXPECT_CALL(*controller(), GetCurrentInteractionStats())
      .WillOnce(Return(&stats));
  EXPECT_CALL(*controller(), BubbleIsManualFallbackForSaving())
      .WillOnce(Return(false));

  EXPECT_CALL(*controller(), GetCredentialSource())
      .WillOnce(Return(password_manager::metrics_util::CredentialSourceType::
                           kCredentialManagementAPI));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_STATE,
                 ManagePasswordsBubbleModel::AUTOMATIC);
  EXPECT_FALSE(model()->enable_editing());
}
