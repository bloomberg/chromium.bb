// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_util.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browser_sync/test_profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"

typedef GoogleServiceAuthError AuthError;

namespace {

class TestSyncService : public browser_sync::TestProfileSyncService {
 public:
  explicit TestSyncService(Profile* profile)
      : browser_sync::TestProfileSyncService(
            CreateProfileSyncServiceParamsForTest(profile)) {}

  bool IsSyncAllowed() const override { return is_sync_allowed_; }

  void set_sync_allowed(bool sync_allowed) { is_sync_allowed_ = sync_allowed; }

 private:
  bool is_sync_allowed_ = true;
  DISALLOW_COPY_AND_ASSIGN(TestSyncService);
};

std::unique_ptr<KeyedService> BuildFakeSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSyncService>(
      static_cast<TestingProfile*>(context));
}

constexpr int kSMSEntrypointSavePasswordBubble =
    1
    << static_cast<int>(desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE);

constexpr int kSMSEntrypointBookmarksBubble =
    1
    << static_cast<int>(desktop_ios_promotion::PromotionEntryPoint::BOOKMARKS_BUBBLE);

}  // namespace

class DesktopIOSPromotionUtilTest : public testing::Test {
 public:
  DesktopIOSPromotionUtilTest() {}
  ~DesktopIOSPromotionUtilTest() override {}

  void SetUp() override {
    local_state_.reset(new TestingPrefServiceSimple);
    TestingBrowserProcess::GetGlobal()->SetLocalState(local_state_.get());
    desktop_ios_promotion::RegisterLocalPrefs(local_state_->registry());
    auto pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPrefService(std::move(pref_service));
    profile_builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                                      &BuildFakeSyncService);
    profile_ = profile_builder.Build();
    sync_service_ = static_cast<TestSyncService*>(
        ProfileSyncServiceFactory::GetForProfile(profile_.get()));
    mock_signin_ = SigninManagerFactory::GetForProfile(profile_.get());
    mock_signin_->SetAuthenticatedAccountInfo("test", "test");
  }

  void TearDown() override {
    profile_.reset();
    // Ensure that g_accept_requests gets set back to true after test execution.
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
  }

  PrefService* local_state() { return local_state_.get(); }

  TestSyncService* sync_service() { return sync_service_; }

  PrefService* prefs() { return profile_->GetPrefs(); }

  Profile* profile() { return profile_.get(); }

  double GetDoubleNDayOldDate(int days) {
    base::Time time_result =
        base::Time::NowFromSystemTime() - base::TimeDelta::FromDays(days);
    return time_result.ToDoubleT();
  }

 private:
  TestSyncService* sync_service_ = nullptr;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  SigninManagerBase* mock_signin_ = nullptr;
  std::unique_ptr<TestingProfile> profile_;
  DISALLOW_COPY_AND_ASSIGN(DesktopIOSPromotionUtilTest);
};

TEST_F(DesktopIOSPromotionUtilTest, IsEligibleForIOSPromotionForSavePassword) {
  desktop_ios_promotion::PromotionEntryPoint entry_point =
      desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE;
  // By default the promo is off.
  EXPECT_FALSE(
      desktop_ios_promotion::IsEligibleForIOSPromotion(profile(), entry_point));

  // Enable the promotion and assign the entry_point finch parameter.
  base::FieldTrialList field_trial_list(nullptr);
  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> params;
  params["entry_point"] = "SavePasswordsBubblePromotion";
  base::AssociateFieldTrialParams("DesktopIOSPromotion",
                                  "SavePasswordBubblePromotion", params);
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "DesktopIOSPromotion", 100, "SavePasswordBubblePromotion",
          base::FieldTrialList::kNoExpirationYear, 1, 1,
          base::FieldTrial::SESSION_RANDOMIZED, nullptr));
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->RegisterFieldTrialOverride(
      features::kDesktopIOSPromotion.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  std::string locales[] = {"en-US", "en-CA", "en-AU", "es-US"};
  constexpr struct {
    bool is_sync_allowed;
    bool signin_error;
    int locale_index;
    bool is_dismissed;
    int show_count;
    int last_impression_days;
    int sms_entrypoint;
    bool is_user_eligible;
    bool promo_done;
    bool result;
  } kTestData[] = {
      // {sync allowed, signin error exist, locale, dismissed before, impression
      // count, seen days
      // ago, bitmask with entry points seen, is user eligible, flow was
      // completed before, expected result }
      {false, false, 0, false, 0, 1, 0, false, false, false},
      {false, false, 1, false, 0, 3, 0, true, false, false},
      {true, false, 3, false, 0, 4, 0, true, false, false},
      {true, false, 2, false, 0, 10, 0, true, false, false},
      {true, false, 0, true, 1, 3, 0, true, false, false},
      {true, false, 0, false, 3, 1, 0, true, false, false},
      {true, false, 0, false, 1, 3, kSMSEntrypointSavePasswordBubble, true,
       false, false},
      {true, false, 0, false, 0, 4, kSMSEntrypointBookmarksBubble, true, false,
       false},
      {true, false, 0, false, 1, 10, 0, false, false, false},
      {true, false, 0, false, 0, 1, 0, true, true, false},
      {true, true, 1, false, 1, 1, 0, true, false, false},
      {true, false, 1, false, 1, 1, 0, true, false, true},
      {true, false, 1, false, 0, 2, 0, true, false, true},
      {true, true, 0, false, 0, 8, kSMSEntrypointSavePasswordBubble, true,
       false, false},
      {true, false, 0, false, 0, 8, kSMSEntrypointSavePasswordBubble, true,
       false, true},
  };
  std::string locale = base::i18n::GetConfiguredLocale();
  FakeAuthStatusProvider auth_provider(
      SigninErrorControllerFactory::GetForProfile(profile()));

  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("#test_case = ") << (&test_case - kTestData));
    sync_service()->set_sync_allowed(test_case.is_sync_allowed);
    const GoogleServiceAuthError error(
        test_case.signin_error
            ? GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS
            : GoogleServiceAuthError::NONE);

    auth_provider.SetAuthError("test", error);

    local_state()->SetBoolean(prefs::kSavePasswordsBubbleIOSPromoDismissed,
                              test_case.is_dismissed);
    local_state()->SetInteger(prefs::kNumberSavePasswordsBubbleIOSPromoShown,
                              test_case.show_count);
    base::i18n::SetICUDefaultLocale(locales[test_case.locale_index]);
    prefs()->SetDouble(prefs::kIOSPromotionLastImpression,
                       GetDoubleNDayOldDate(test_case.last_impression_days));
    prefs()->SetInteger(prefs::kIOSPromotionSMSEntryPoint,
                        test_case.sms_entrypoint);
    prefs()->SetBoolean(prefs::kIOSPromotionEligible,
                        test_case.is_user_eligible);
    prefs()->SetBoolean(prefs::kIOSPromotionDone, test_case.promo_done);
    EXPECT_EQ(test_case.result,
              IsEligibleForIOSPromotion(profile(), entry_point));
  }
  base::i18n::SetICUDefaultLocale(locale);
}
