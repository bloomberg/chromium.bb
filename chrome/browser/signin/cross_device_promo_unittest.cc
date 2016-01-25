// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_promo.h"

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/cross_device_promo_factory.h"
#include "chrome/browser/signin/fake_gaia_cookie_manager_service.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef std::map<std::string, std::string> VariationsMap;

int64_t InTwoHours() {
  return (base::Time::Now() + base::TimeDelta::FromHours(2)).ToInternalValue();
}

}  // namespace

class CrossDevicePromoObserver : public CrossDevicePromo::Observer {
 public:
  explicit CrossDevicePromoObserver(CrossDevicePromo* promo)
      : eligible_(false),
        times_set_eligible_(0),
        times_set_ineligible_(0),
        promo_(promo) {
    promo->AddObserver(this);
  }

  ~CrossDevicePromoObserver() { promo_->RemoveObserver(this); }

  void OnPromoEligibilityChanged(bool eligible) override {
    eligible_ = eligible;
    if (eligible)
      ++times_set_eligible_;
    else
      ++times_set_ineligible_;
  }

  bool is_eligible() const { return eligible_; }
  int times_set_eligible() const { return times_set_eligible_; }
  int times_set_inactive() const { return times_set_ineligible_; }

 private:
  bool eligible_;
  int times_set_eligible_;
  int times_set_ineligible_;
  CrossDevicePromo* promo_;

  DISALLOW_COPY_AND_ASSIGN(CrossDevicePromoObserver);
};

class CrossDevicePromoTest : public ::testing::Test {
 public:
  CrossDevicePromoTest();

  void SetUp() override;

  // Destroys any variations which might be defined, and starts fresh.
  void ResetFieldTrialList();

  // Defines a default set of variation parameters for promo initialization.
  void InitPromoVariation();

  CrossDevicePromo* promo() { return cross_device_promo_; }
  TestingProfile* profile() { return profile_; }
  FakeSigninManagerForTesting* signin_manager() { return signin_manager_; }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  syncable_prefs::TestingPrefServiceSyncable* prefs() { return pref_service_; }
  FakeGaiaCookieManagerService* cookie_manager_service() {
    return cookie_manager_service_;
  }
  net::FakeURLFetcherFactory* fetcher_factory() {
    return &fake_url_fetcher_factory_;
  }

 private:
  content::TestBrowserThreadBundle bundle_;
  CrossDevicePromo* cross_device_promo_;
  TestingProfile* profile_;
  FakeSigninManagerForTesting* signin_manager_;
  FakeGaiaCookieManagerService* cookie_manager_service_;
  syncable_prefs::TestingPrefServiceSyncable* pref_service_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;
  base::HistogramTester histogram_tester_;
  scoped_ptr<base::FieldTrialList> field_trial_list_;
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrossDevicePromoTest);
};

CrossDevicePromoTest::CrossDevicePromoTest() : fake_url_fetcher_factory_(NULL) {
  ResetFieldTrialList();
}

void CrossDevicePromoTest::SetUp() {
  testing_profile_manager_.reset(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  ASSERT_TRUE(testing_profile_manager_.get()->SetUp());

  TestingProfile::TestingFactories factories;
  factories.push_back(std::make_pair(ChromeSigninClientFactory::GetInstance(),
                                     signin::BuildTestSigninClient));
  factories.push_back(
      std::make_pair(GaiaCookieManagerServiceFactory::GetInstance(),
                     FakeGaiaCookieManagerService::Build));
  factories.push_back(std::make_pair(SigninManagerFactory::GetInstance(),
                                     BuildFakeSigninManagerBase));

  pref_service_ = new syncable_prefs::TestingPrefServiceSyncable();
  chrome::RegisterUserProfilePrefs(pref_service_->registry());

  profile_ = testing_profile_manager_.get()->CreateTestingProfile(
      "name",
      make_scoped_ptr<syncable_prefs::PrefServiceSyncable>(pref_service_),
      base::UTF8ToUTF16("name"), 0, std::string(), factories);

  cookie_manager_service_ = static_cast<FakeGaiaCookieManagerService*>(
      GaiaCookieManagerServiceFactory::GetForProfile(profile()));
  cookie_manager_service_->Init(&fake_url_fetcher_factory_);

  signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
      SigninManagerFactory::GetForProfile(profile()));

  cross_device_promo_ = CrossDevicePromoFactory::GetForProfile(profile());
}

void CrossDevicePromoTest::ResetFieldTrialList() {
  // Destroy the existing FieldTrialList before creating a new one to avoid
  // a DCHECK.
  field_trial_list_.reset();
  field_trial_list_.reset(
      new base::FieldTrialList(new metrics::SHA1EntropyProvider("foo")));
  variations::testing::ClearAllVariationParams();
}

void CrossDevicePromoTest::InitPromoVariation() {
  VariationsMap variations_params;
  variations_params[
      CrossDevicePromo::kParamHoursBetweenDeviceActivityChecks] = "2";
  variations_params[
      CrossDevicePromo::kParamDaysToVerifySingleUserProfile] = "0";
  variations_params[
      CrossDevicePromo::kParamMinutesBetweenBrowsingSessions] = "0";
  variations_params[
      CrossDevicePromo::kParamMinutesMaxContextSwitchDuration] = "10";
  variations_params[
      CrossDevicePromo::kParamRPCThrottle] = "0";
  EXPECT_TRUE(variations::AssociateVariationParams(
      CrossDevicePromo::kCrossDevicePromoFieldTrial, "A", variations_params));
  base::FieldTrialList::CreateFieldTrial(
      CrossDevicePromo::kCrossDevicePromoFieldTrial, "A");
}

// Tests for incrementally large portions flow that determines if the promo
// should be shown.

TEST_F(CrossDevicePromoTest, Uninitialized) {
  ASSERT_TRUE(promo());
  histogram_tester()->ExpectUniqueSample("Signin.XDevicePromo.Initialized",
                                         signin_metrics::NO_VARIATIONS_CONFIG,
                                         1);

  promo()->CheckPromoEligibilityForTesting();
  histogram_tester()->ExpectUniqueSample("Signin.XDevicePromo.Initialized",
                                         signin_metrics::NO_VARIATIONS_CONFIG,
                                         2);
  histogram_tester()->ExpectTotalCount("Signin.XDevicePromo.Eligibility", 0);
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kCrossDevicePromoOptedOut));
}

TEST_F(CrossDevicePromoTest, UnitializedOptedOut) {
  CrossDevicePromoObserver observer(promo());

  promo()->OptOut();
  // Opting out doesn't de-activate a never-active promo.
  EXPECT_EQ(0, observer.times_set_inactive());
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kCrossDevicePromoOptedOut));

  // An opted-out promo will never be initialized.
  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
  histogram_tester()->ExpectBucketCount("Signin.XDevicePromo.Initialized",
                                        signin_metrics::NO_VARIATIONS_CONFIG,
                                        1);
  histogram_tester()->ExpectBucketCount("Signin.XDevicePromo.Initialized",
                                        signin_metrics::UNINITIALIZED_OPTED_OUT,
                                        1);
  histogram_tester()->ExpectTotalCount("Signin.XDevicePromo.Eligibility", 0);
}

TEST_F(CrossDevicePromoTest, PartiallyInitialized) {
  histogram_tester()->ExpectUniqueSample("Signin.XDevicePromo.Initialized",
                                         signin_metrics::NO_VARIATIONS_CONFIG,
                                         1);

  VariationsMap variations_params;
  variations_params[
      CrossDevicePromo::kParamHoursBetweenDeviceActivityChecks] = "1";
  variations_params[
      CrossDevicePromo::kParamDaysToVerifySingleUserProfile] = "1";
  EXPECT_TRUE(variations::AssociateVariationParams(
      CrossDevicePromo::kCrossDevicePromoFieldTrial, "A", variations_params));
  base::FieldTrialList::CreateFieldTrial(
      CrossDevicePromo::kCrossDevicePromoFieldTrial, "A");

  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
  histogram_tester()->ExpectUniqueSample("Signin.XDevicePromo.Initialized",
                                         signin_metrics::NO_VARIATIONS_CONFIG,
                                         2);
  histogram_tester()->ExpectTotalCount("Signin.XDevicePromo.Eligibility", 0);
}

TEST_F(CrossDevicePromoTest, FullyInitialized) {
  histogram_tester()->ExpectUniqueSample("Signin.XDevicePromo.Initialized",
                                         signin_metrics::NO_VARIATIONS_CONFIG,
                                         1);

  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
  histogram_tester()->ExpectUniqueSample("Signin.XDevicePromo.Initialized",
                                         signin_metrics::NO_VARIATIONS_CONFIG,
                                         2);

  InitPromoVariation();
  signin_manager()->SignIn("12345", "foo@bar.com", "password");
  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
  histogram_tester()->ExpectBucketCount("Signin.XDevicePromo.Initialized",
                                        signin_metrics::INITIALIZED, 1);
  histogram_tester()->ExpectBucketCount("Signin.XDevicePromo.Initialized",
                                        signin_metrics::NO_VARIATIONS_CONFIG,
                                        2);

  histogram_tester()->ExpectUniqueSample("Signin.XDevicePromo.Eligibility",
                                         signin_metrics::SIGNED_IN, 1);
}

TEST_F(CrossDevicePromoTest, InitializedOptOut) {
  // In a normal browser, the variations get set before the CrossDevicePromo is
  // created. Here, we need to force another Init() by calling
  // CheckPromoEligibilityForTesting().
  InitPromoVariation();
  signin_manager()->SignIn("12345", "foo@bar.com", "password");
  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());

  histogram_tester()->ExpectBucketCount("Signin.XDevicePromo.Initialized",
                                        signin_metrics::INITIALIZED, 1);
  histogram_tester()->ExpectUniqueSample("Signin.XDevicePromo.Eligibility",
                                         signin_metrics::SIGNED_IN, 1);

  // After opting out the initialized state remains; eligibility changes.
  promo()->OptOut();
  promo()->CheckPromoEligibilityForTesting();
  histogram_tester()->ExpectBucketCount("Signin.XDevicePromo.Initialized",
                                        signin_metrics::INITIALIZED, 1);
  histogram_tester()->ExpectBucketCount("Signin.XDevicePromo.Eligibility",
                                        signin_metrics::OPTED_OUT, 1);
}

TEST_F(CrossDevicePromoTest, SignedInAndOut) {
  InitPromoVariation();

  {
    base::HistogramTester test_signed_in;
    signin_manager()->SignIn("12345", "foo@bar.com", "password");
    EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
    test_signed_in.ExpectUniqueSample("Signin.XDevicePromo.Eligibility",
                                      signin_metrics::SIGNED_IN, 1);
  }

  {
    base::HistogramTester test_signed_out;
    signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                              signin_metrics::SignoutDelete::IGNORE_METRIC);
    promo()->CheckPromoEligibilityForTesting();
    test_signed_out.ExpectUniqueSample("Signin.XDevicePromo.Eligibility",
                                       signin_metrics::NOT_SINGLE_GAIA_ACCOUNT,
                                       1);
  }
}

TEST_F(CrossDevicePromoTest, TrackAccountsInCookie) {
  InitPromoVariation();
  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());

  ASSERT_FALSE(prefs()->HasPrefPath(
      prefs::kCrossDevicePromoObservedSingleAccountCookie));
  std::vector<gaia::ListedAccount> accounts;

  // Setting a single cookie sets the time.
  base::Time before_setting_cookies = base::Time::Now();
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  cookie_manager_service()->SetListAccountsResponseOneAccount("f@bar.com", "1");
  EXPECT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  base::RunLoop().RunUntilIdle();

  base::Time after_setting_cookies = base::Time::Now();
  EXPECT_TRUE(prefs()->HasPrefPath(
      prefs::kCrossDevicePromoObservedSingleAccountCookie));
  EXPECT_LE(
      before_setting_cookies.ToInternalValue(),
      prefs()->GetInt64(prefs::kCrossDevicePromoObservedSingleAccountCookie));
  EXPECT_GE(
      after_setting_cookies.ToInternalValue(),
      prefs()->GetInt64(prefs::kCrossDevicePromoObservedSingleAccountCookie));

  // A single cookie a second time doesn't change the time.
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  cookie_manager_service()->SetListAccountsResponseOneAccount("f@bar.com", "1");
  EXPECT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefs()->HasPrefPath(
      prefs::kCrossDevicePromoObservedSingleAccountCookie));
  EXPECT_GE(
      after_setting_cookies.ToInternalValue(),
      prefs()->GetInt64(prefs::kCrossDevicePromoObservedSingleAccountCookie));

  // Setting accounts with an auth error doesn't change the time.
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  cookie_manager_service()->SetListAccountsResponseWebLoginRequired();
  EXPECT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefs()->HasPrefPath(
      prefs::kCrossDevicePromoObservedSingleAccountCookie));
  EXPECT_LE(
      before_setting_cookies.ToInternalValue(),
      prefs()->GetInt64(prefs::kCrossDevicePromoObservedSingleAccountCookie));
  EXPECT_GE(
      after_setting_cookies.ToInternalValue(),
      prefs()->GetInt64(prefs::kCrossDevicePromoObservedSingleAccountCookie));

  // Seeing zero accounts clears the pref.
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  EXPECT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(prefs()->HasPrefPath(
      prefs::kCrossDevicePromoObservedSingleAccountCookie));
}

TEST_F(CrossDevicePromoTest, SingleAccountEligibility) {
  InitPromoVariation();

  {
    base::HistogramTester test_single_account;
    EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
    test_single_account.ExpectUniqueSample(
        "Signin.XDevicePromo.Eligibility",
        signin_metrics::NOT_SINGLE_GAIA_ACCOUNT, 1);
  }

  // Notice a single account.
  {
    base::HistogramTester test_single_account;
    std::vector<gaia::ListedAccount> accounts;
    cookie_manager_service()->set_list_accounts_stale_for_testing(true);
    cookie_manager_service()->SetListAccountsResponseOneAccount("a@b.com", "1");
    EXPECT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
    test_single_account.ExpectUniqueSample(
        "Signin.XDevicePromo.Eligibility",
        signin_metrics::UNKNOWN_COUNT_DEVICES, 1);
  }

  // Set a single account that hasn't been around for "long enough".
  {
    base::HistogramTester test_single_account;
    prefs()->SetInt64(prefs::kCrossDevicePromoObservedSingleAccountCookie,
                      InTwoHours());
    EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
    test_single_account.ExpectBucketCount(
        "Signin.XDevicePromo.Eligibility",
        signin_metrics::NOT_SINGLE_GAIA_ACCOUNT, 1);
  }
}

TEST_F(CrossDevicePromoTest, NumDevicesEligibility) {
  // Start with a variation, signed in, and one account in the cookie jar.
  InitPromoVariation();
  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  cookie_manager_service()->SetListAccountsResponseOneAccount("f@bar.com", "1");
  std::vector<gaia::ListedAccount> accounts;
  EXPECT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  base::RunLoop().RunUntilIdle();

  // Ensure we appropriate schedule a check for device activity.
  {
    base::HistogramTester test_missing_list_devices;
    int64_t earliest_time_to_check_list_devices =
        base::Time::Now().ToInternalValue();
    EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
    int64_t latest_time_to_check_list_devices = InTwoHours();
    test_missing_list_devices.ExpectUniqueSample(
        "Signin.XDevicePromo.Eligibility",
        signin_metrics::UNKNOWN_COUNT_DEVICES, 1);
    EXPECT_TRUE(
        prefs()->HasPrefPath(prefs::kCrossDevicePromoNextFetchListDevicesTime));
    int64_t when_to_check_list_devices =
        prefs()->GetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime);
    EXPECT_LT(earliest_time_to_check_list_devices, when_to_check_list_devices);
    EXPECT_GT(latest_time_to_check_list_devices, when_to_check_list_devices);
  }

  // Don't reschedule the device activity check if there's one pending.
  {
    base::HistogramTester test_unknown_devices;
    int64_t list_devices_time = InTwoHours();
    prefs()->SetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime,
                      list_devices_time);
    EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
    test_unknown_devices.ExpectUniqueSample(
        "Signin.XDevicePromo.Eligibility",
        signin_metrics::UNKNOWN_COUNT_DEVICES, 1);
    // The scheduled time to fetch device activity should not have changed.
    EXPECT_EQ(
        list_devices_time,
        prefs()->GetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime));
  }

  // Execute the device activity fetch if it's time.
  {
    base::HistogramTester test_unknown_devices;
    prefs()->SetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime,
                      base::Time::Now().ToInternalValue());
    // The DeviceActivityFetcher will return an error to the promo service.
    fetcher_factory()->SetFakeResponse(
        GaiaUrls::GetInstance()->oauth2_iframe_url(), "not json", net::HTTP_OK,
        net::URLRequestStatus::SUCCESS);
    EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
    base::RunLoop().RunUntilIdle();
    test_unknown_devices.ExpectUniqueSample(
        "Signin.XDevicePromo.Eligibility",
        signin_metrics::ERROR_FETCHING_DEVICE_ACTIVITY, 1);
  }
}

TEST_F(CrossDevicePromoTest, ThrottleDeviceActivityCall) {
  // Start with a variation (fully throttled), signed in, one account in cookie.
  VariationsMap variations_params;
  variations_params[
      CrossDevicePromo::kParamHoursBetweenDeviceActivityChecks] = "1";
  variations_params[
      CrossDevicePromo::kParamDaysToVerifySingleUserProfile] = "0";
  variations_params[
      CrossDevicePromo::kParamMinutesBetweenBrowsingSessions] = "0";
  variations_params[
      CrossDevicePromo::kParamMinutesMaxContextSwitchDuration] = "10";
  variations_params[
      CrossDevicePromo::kParamRPCThrottle] = "100";
  EXPECT_TRUE(variations::AssociateVariationParams(
      CrossDevicePromo::kCrossDevicePromoFieldTrial, "A", variations_params));
  base::FieldTrialList::CreateFieldTrial(
      CrossDevicePromo::kCrossDevicePromoFieldTrial, "A");

  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  cookie_manager_service()->SetListAccountsResponseOneAccount("f@bar.com", "1");
  std::vector<gaia::ListedAccount> accounts;
  EXPECT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  base::RunLoop().RunUntilIdle();

  // Ensure device activity fetches get throttled.
  {
    base::HistogramTester test_throttle_rpc;
    prefs()->SetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime,
                      base::Time::Now().ToInternalValue());
    EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
    test_throttle_rpc.ExpectUniqueSample(
        "Signin.XDevicePromo.Eligibility",
        signin_metrics::THROTTLED_FETCHING_DEVICE_ACTIVITY, 1);
  }
}

TEST_F(CrossDevicePromoTest, NumDevicesKnown) {
  // Start with a variation, signed in, and one account, fetch device activity
  // in two hours.
  InitPromoVariation();
  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  cookie_manager_service()->SetListAccountsResponseOneAccount("f@bar.com", "1");
  std::vector<gaia::ListedAccount> accounts;
  EXPECT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  base::RunLoop().RunUntilIdle();
  prefs()->SetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime,
                    InTwoHours());

  // Verify that knowing there are no devices for this account logs the
  // appropriate metric for ineligibility.
  {
    base::HistogramTester test_no_devices;
    prefs()->SetInteger(prefs::kCrossDevicePromoNumDevices, 0);
    EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
    test_no_devices.ExpectUniqueSample("Signin.XDevicePromo.Eligibility",
                                       signin_metrics::ZERO_DEVICES, 1);
  }

  // Verify that knowing there is another device for this account results in the
  // promo being eligible to be shown.
  {
    prefs()->SetInteger(prefs::kCrossDevicePromoNumDevices, 1);
    EXPECT_TRUE(promo()->CheckPromoEligibilityForTesting());
  }
}

TEST_F(CrossDevicePromoTest, FetchDeviceResults) {
  // Start with a variation, signed in, and one account, fetch device activity
  // in 2 hours.
  InitPromoVariation();
  EXPECT_FALSE(promo()->CheckPromoEligibilityForTesting());
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  cookie_manager_service()->SetListAccountsResponseOneAccount("f@bar.com", "1");
  std::vector<gaia::ListedAccount> accounts;
  EXPECT_FALSE(cookie_manager_service()->ListAccounts(&accounts));
  base::RunLoop().RunUntilIdle();
  prefs()->SetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime,
                    base::Time::Now().ToInternalValue());
  prefs()->SetInteger(prefs::kCrossDevicePromoNumDevices, 1);

  // Verify that if the device activity fetcher returns zero devices the
  // eligibility metric will report a ZERO_DEVICES event, and will not report
  // the promo as eligible to be shown.
  {
    base::HistogramTester test_no_devices;
    std::vector<DeviceActivityFetcher::DeviceActivity> devices;
    int64_t in_two_hours = InTwoHours();
    promo()->OnFetchDeviceActivitySuccess(devices);
    EXPECT_LE(
        in_two_hours,
        prefs()->GetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime));
    EXPECT_EQ(0, prefs()->GetInteger(prefs::kCrossDevicePromoNumDevices));
    test_no_devices.ExpectUniqueSample("Signin.XDevicePromo.Eligibility",
                                       signin_metrics::ZERO_DEVICES, 1);
  }

  // Verify that if the device activity fetcher returns one device that was
  // recently active, the promo is marked as eligible and the eligibility
  // metric reports an ELIGIBLE event.
  {
    CrossDevicePromoObserver observer(promo());
    EXPECT_FALSE(observer.is_eligible());
    base::HistogramTester test_one_device;
    std::vector<DeviceActivityFetcher::DeviceActivity> devices;
    base::Time device_last_active =
        base::Time::Now() - base::TimeDelta::FromMinutes(4);
    DeviceActivityFetcher::DeviceActivity device;
    device.last_active = device_last_active;
    device.name = "Aslan";
    devices.push_back(device);

    int64_t in_two_hours = InTwoHours();
    promo()->OnFetchDeviceActivitySuccess(devices);
    EXPECT_LE(
        in_two_hours,
        prefs()->GetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime));
    EXPECT_EQ(1, prefs()->GetInteger(prefs::kCrossDevicePromoNumDevices));
    EXPECT_EQ(device_last_active.ToInternalValue(),
              prefs()->GetInt64(prefs::kCrossDevicePromoLastDeviceActiveTime));
    EXPECT_TRUE(prefs()->GetBoolean(prefs::kCrossDevicePromoShouldBeShown));
    test_one_device.ExpectUniqueSample("Signin.XDevicePromo.Eligibility",
                                       signin_metrics::ELIGIBLE, 1);
    EXPECT_TRUE(observer.is_eligible());
    EXPECT_EQ(1, observer.times_set_eligible());
  }

  // Verify that if the device activity fetcher returns one device that was not
  // recently active then the eligibility metric will report a NO_ACTIVE_DEVICES
  // event, and will not report the promo as eligible to be shown.
  {
    CrossDevicePromoObserver observer(promo());
    EXPECT_FALSE(observer.is_eligible());
    base::HistogramTester test_one_device;
    std::vector<DeviceActivityFetcher::DeviceActivity> devices;
    base::Time device_last_active =
        base::Time::Now() - base::TimeDelta::FromMinutes(30);
    DeviceActivityFetcher::DeviceActivity device;
    device.last_active = device_last_active;
    device.name = "Aslan";
    devices.push_back(device);

    int64_t in_two_hours = InTwoHours();
    promo()->OnFetchDeviceActivitySuccess(devices);
    EXPECT_LE(
        in_two_hours,
        prefs()->GetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime));
    EXPECT_EQ(1, prefs()->GetInteger(prefs::kCrossDevicePromoNumDevices));
    EXPECT_EQ(device_last_active.ToInternalValue(),
              prefs()->GetInt64(prefs::kCrossDevicePromoLastDeviceActiveTime));
    EXPECT_FALSE(prefs()->GetBoolean(prefs::kCrossDevicePromoShouldBeShown));
    test_one_device.ExpectUniqueSample("Signin.XDevicePromo.Eligibility",
                                       signin_metrics::NO_ACTIVE_DEVICES, 1);
    EXPECT_FALSE(observer.is_eligible());
  }

  // Verify that if the device activity fetcher returns two devices and one was
  // recently active, that the promo is eligible to be shown and the last active
  // time is stored properly.
  {
    CrossDevicePromoObserver observer(promo());
    EXPECT_FALSE(observer.is_eligible());
    base::HistogramTester test_two_devices;
    std::vector<DeviceActivityFetcher::DeviceActivity> devices;
    base::Time device1_last_active =
        base::Time::Now() - base::TimeDelta::FromMinutes(30);
    base::Time device2_last_active =
        base::Time::Now() - base::TimeDelta::FromMinutes(3);
    DeviceActivityFetcher::DeviceActivity device1;
    device1.last_active = device1_last_active;
    device1.name = "Aslan";
    devices.push_back(device1);
    DeviceActivityFetcher::DeviceActivity device2;
    device2.last_active = device2_last_active;
    device2.name = "Balrog";
    devices.push_back(device2);

    int64_t in_two_hours = InTwoHours();
    promo()->OnFetchDeviceActivitySuccess(devices);
    EXPECT_LE(
        in_two_hours,
        prefs()->GetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime));
    EXPECT_EQ(2, prefs()->GetInteger(prefs::kCrossDevicePromoNumDevices));
    EXPECT_EQ(device2_last_active.ToInternalValue(),
              prefs()->GetInt64(prefs::kCrossDevicePromoLastDeviceActiveTime));
    EXPECT_TRUE(prefs()->GetBoolean(prefs::kCrossDevicePromoShouldBeShown));
    test_two_devices.ExpectUniqueSample("Signin.XDevicePromo.Eligibility",
                                        signin_metrics::ELIGIBLE, 1);
    EXPECT_TRUE(observer.is_eligible());
    EXPECT_EQ(1, observer.times_set_eligible());
  }
}
