// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/ntp_snippets/status.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_params_manager.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::InSequence;
using testing::Invoke;
using testing::IsEmpty;
using testing::Mock;
using testing::MockFunction;
using testing::Not;
using testing::Return;
using testing::SaveArg;
using testing::SaveArgPointee;
using testing::SizeIs;
using testing::StartsWith;
using testing::StrictMock;
using testing::WithArgs;
using testing::_;

namespace ntp_snippets {

class RemoteSuggestionsFetcher;

namespace {

class MockPersistentScheduler : public PersistentScheduler {
 public:
  MOCK_METHOD2(Schedule,
               bool(base::TimeDelta period_wifi,
                    base::TimeDelta period_fallback));
  MOCK_METHOD0(Unschedule, bool());
  MOCK_METHOD0(IsOnUnmeteredConnection, bool());
};

// TODO(jkrcal): Move into its own library to reuse in other unit-tests?
class MockRemoteSuggestionsProvider : public RemoteSuggestionsProvider {
 public:
  MockRemoteSuggestionsProvider(Observer* observer)
      : RemoteSuggestionsProvider(observer) {}
  MOCK_METHOD1(RefetchInTheBackground,
               void(const RemoteSuggestionsProvider::FetchStatusCallback&));
  MOCK_CONST_METHOD0(suggestions_fetcher_for_debugging,
                     const RemoteSuggestionsFetcher*());
  MOCK_CONST_METHOD1(GetUrlWithFavicon,
                     GURL(const ContentSuggestion::ID& suggestion_id));
  MOCK_METHOD1(GetCategoryStatus, CategoryStatus(Category));
  MOCK_METHOD1(GetCategoryInfo, CategoryInfo(Category));
  MOCK_METHOD3(ClearHistory,
               void(base::Time begin,
                    base::Time end,
                    const base::Callback<bool(const GURL& url)>& filter));
  MOCK_METHOD3(Fetch,
               void(const Category&,
                    const std::set<std::string>&,
                    const FetchDoneCallback&));
  MOCK_METHOD0(ReloadSuggestions, void());
  MOCK_METHOD1(ClearCachedSuggestions, void(Category));
  MOCK_METHOD1(ClearDismissedSuggestionsForDebugging, void(Category));
  MOCK_METHOD1(DismissSuggestion, void(const ContentSuggestion::ID&));
  MOCK_METHOD2(FetchSuggestionImage,
               void(const ContentSuggestion::ID&, const ImageFetchedCallback&));
  MOCK_METHOD2(GetDismissedSuggestionsForDebugging,
               void(Category, const DismissedSuggestionsCallback&));
  MOCK_METHOD0(OnSignInStateChanged, void());
};

}  // namespace

class RemoteSuggestionsSchedulerImplTest : public ::testing::Test {
 public:
  RemoteSuggestionsSchedulerImplTest()
      :  // For the test we enabled all trigger types.
        default_variation_params_{{"scheduler_trigger_types",
                                   "persistent_scheduler_wake_up,ntp_opened,"
                                   "browser_foregrounded,browser_cold_start"}},
        params_manager_(ntp_snippets::kArticleSuggestionsFeature.name,
                        default_variation_params_,
                        {kArticleSuggestionsFeature.name}),
        user_classifier_(/*pref_service=*/nullptr,
                         base::MakeUnique<base::DefaultClock>()) {
    RemoteSuggestionsSchedulerImpl::RegisterProfilePrefs(
        utils_.pref_service()->registry());
    RequestThrottler::RegisterProfilePrefs(utils_.pref_service()->registry());
    // TODO(jkrcal) Create a static function in EulaAcceptedNotifier that
    // registers this pref and replace the call in browser_process_impl.cc & in
    // eula_accepted_notifier_unittest.cc with the new static function.
    local_state_.registry()->RegisterBooleanPref(::prefs::kEulaAccepted, false);
    // By default pretend we are on WiFi.
    EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
        .WillRepeatedly(Return(true));
    ResetProvider();
  }

  void ResetProvider() {
    provider_ = base::MakeUnique<StrictMock<MockRemoteSuggestionsProvider>>(
        /*observer=*/nullptr);

    auto test_clock = base::MakeUnique<base::SimpleTestClock>();
    test_clock_ = test_clock.get();
    test_clock_->SetNow(base::Time::Now());

    scheduler_ = base::MakeUnique<RemoteSuggestionsSchedulerImpl>(
        &persistent_scheduler_, &user_classifier_, utils_.pref_service(),
        &local_state_, std::move(test_clock));
    scheduler_->SetProvider(provider_.get());
  }

  void SetVariationParameter(const std::string& param_name,
                             const std::string& param_value) {
    std::map<std::string, std::string> params = default_variation_params_;
    params[param_name] = param_value;

    params_manager_.ClearAllVariationParams();
    params_manager_.SetVariationParamsWithFeatureAssociations(
        ntp_snippets::kArticleSuggestionsFeature.name, params,
        {ntp_snippets::kArticleSuggestionsFeature.name});
  }

  bool IsEulaNotifierAvailable() {
    // Create() returns a unique_ptr, so this is no leak.
    return web_resource::EulaAcceptedNotifier::Create(&local_state_) != nullptr;
  }

  void SetEulaAcceptedPref() {
    local_state_.SetBoolean(::prefs::kEulaAccepted, true);
  }

  // GMock cannot deal with move-only types. We need to pass the vector to the
  // mock function as const ref using this wrapper callback.
  void FetchDoneWrapper(
      MockFunction<void(Status status_code,
                        const std::vector<ContentSuggestion>& suggestions)>*
          fetch_done,
      Status status_code,
      std::vector<ContentSuggestion> suggestions) {
    fetch_done->Call(status_code, suggestions);
  }

 protected:
  std::map<std::string, std::string> default_variation_params_;
  variations::testing::VariationParamsManager params_manager_;

  void ActivateProvider() {
    SetEulaAcceptedPref();
    scheduler_->OnProviderActivated();
  }

  void DeactivateProvider() { scheduler_->OnProviderDeactivated(); }

  MockPersistentScheduler* persistent_scheduler() {
    return &persistent_scheduler_;
  }
  base::SimpleTestClock* test_clock() { return test_clock_; }
  MockRemoteSuggestionsProvider* provider() { return provider_.get(); }
  RemoteSuggestionsSchedulerImpl* scheduler() { return scheduler_.get(); }

 private:
  test::RemoteSuggestionsTestUtils utils_;
  UserClassifier user_classifier_;
  TestingPrefServiceSimple local_state_;
  StrictMock<MockPersistentScheduler> persistent_scheduler_;
  base::SimpleTestClock* test_clock_;
  std::unique_ptr<MockRemoteSuggestionsProvider> provider_;
  std::unique_ptr<RemoteSuggestionsSchedulerImpl> scheduler_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsSchedulerImplTest);
};

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldIgnoreSignalsWhenNotEnabled) {
  scheduler()->OnPersistentSchedulerWakeUp();
  scheduler()->OnNTPOpened();
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldIgnoreEulaStateOnPlatformsWhereNotAvaiable) {
  // Only run this tests on platforms that don't support Eula.
  if (IsEulaNotifierAvailable()) {
    return;
  }

  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  scheduler()->OnProviderActivated();

  // Verify fetches get triggered.
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldIgnoreSignalsWhenEulaNotAccepted) {
  // Only run this tests on platforms supporting Eula.
  if (!IsEulaNotifierAvailable()) {
    return;
  }
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  scheduler()->OnProviderActivated();

  // All signals are ignored because of Eula not being accepted.
  scheduler()->OnPersistentSchedulerWakeUp();
  scheduler()->OnNTPOpened();
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldFetchWhenEulaGetsAccepted) {
  // Only run this tests on platforms supporting Eula.
  if (!IsEulaNotifierAvailable()) {
    return;
  }
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  scheduler()->OnProviderActivated();

  // Make one (ignored) call to make sure we are interested in eula state.
  scheduler()->OnPersistentSchedulerWakeUp();

  // Accepting Eula afterwards results in a background fetch.
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  SetEulaAcceptedPref();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldIgnoreSignalsWhenDisabledByParam) {
  // First set an empty list of allowed trigger types.
  SetVariationParameter("scheduler_trigger_types", "-");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  scheduler()->OnPersistentSchedulerWakeUp();
  scheduler()->OnNTPOpened();
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldHandleEmptyParamForTriggerTypes) {
  // First set an empty param for allowed trigger types -> should result in the
  // default list.
  SetVariationParameter("scheduler_trigger_types", "");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // For instance, persistent scheduler wake up should be enabled by default.
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldHandleIncorrentParamForTriggerTypes) {
  // First set an invalid list of allowed trigger types.
  SetVariationParameter("scheduler_trigger_types", "ntp_opened,foo;");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // For instance, persistent scheduler wake up should be enabled by default.
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnPersistentSchedulerWakeUp) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types",
                        "persistent_scheduler_wake_up");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnPersistentSchedulerWakeUpRepeated) {
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  {
    InSequence s;
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    EXPECT_CALL(*provider(), RefetchInTheBackground(_))
        .WillOnce(SaveArg<0>(&signal_fetch_done));
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  }
  // First enable the scheduler -- calling Schedule() for the first time.
  ActivateProvider();
  // Make the first persistent fetch successful -- calling Schedule() again.
  scheduler()->OnPersistentSchedulerWakeUp();
  signal_fetch_done.Run(Status::Success());
  // Make the second fetch.
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotTriggerBackgroundFetchIfAlreadyInProgess) {
  {
    InSequence s;
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    EXPECT_CALL(*provider(), RefetchInTheBackground(_));
    // RefetchInTheBackground is not called after the second trigger.
  }
  // First enable the scheduler -- calling Schedule() for the first time.
  ActivateProvider();
  // Make the first persistent fetch never finish.
  scheduler()->OnPersistentSchedulerWakeUp();
  // Make the second fetch.
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnNTPOpenedForTheFirstTime) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types", "ntp_opened");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnNTPOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnBrowserForegroundedForTheFirstTime) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types", "browser_foregrounded");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnBrowserForegrounded();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnBrowserColdStartForTheFirstTime) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types", "browser_cold_start");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotFetchOnNTPOpenedAfterSuccessfulSoftFetch) {
  // First enable the scheduler; the second Schedule is called after the
  // successful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProvider();

  // Make the first soft fetch successful.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduler()->OnNTPOpened();
  signal_fetch_done.Run(Status::Success());
  // The second call is ignored if it happens right after the first one.
  scheduler()->OnNTPOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotFetchOnNTPOpenedAfterSuccessfulPersistentFetch) {
  // First enable the scheduler; the second Schedule is called after the
  // successful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProvider();

  // Make the first persistent fetch successful.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduler()->OnPersistentSchedulerWakeUp();
  signal_fetch_done.Run(Status::Success());
  // The second call is ignored if it happens right after the first one.
  scheduler()->OnNTPOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotFetchOnNTPOpenedAfterFailedSoftFetch) {
  // First enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // Make the first soft fetch failed.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduler()->OnNTPOpened();
  signal_fetch_done.Run(Status(StatusCode::PERMANENT_ERROR, ""));

  // The second call is ignored if it happens right after the first one.
  scheduler()->OnNTPOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotFetchOnNTPOpenedAfterFailedPersistentFetch) {
  // First enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // Make the first persistent fetch failed.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduler()->OnPersistentSchedulerWakeUp();
  signal_fetch_done.Run(Status(StatusCode::PERMANENT_ERROR, ""));

  // The second call is ignored if it happens right after the first one.
  scheduler()->OnNTPOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchAgainOnBrowserForgroundLaterAgain) {
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  {
    InSequence s;
    // Initial scheduling after being enabled.
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    // The first call to NTPOpened results in a fetch.
    EXPECT_CALL(*provider(), RefetchInTheBackground(_))
        .WillOnce(SaveArg<0>(&signal_fetch_done));
    // Rescheduling after a succesful fetch.
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    // The second call to NTPOpened 4hrs later again results in a fetch.
    EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  }

  // First enable the scheduler.
  ActivateProvider();
  // Make the first soft fetch successful.
  scheduler()->OnBrowserForegrounded();
  signal_fetch_done.Run(Status::Success());
  // Open NTP again after 9hrs.
  test_clock()->Advance(base::TimeDelta::FromHours(24));
  scheduler()->OnBrowserForegrounded();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldRescheduleOnRescheduleFetching) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  scheduler()->RescheduleFetching();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldScheduleOnActivation) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldUnscheduleOnLaterInactivation) {
  {
    InSequence s;
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    EXPECT_CALL(*persistent_scheduler(), Unschedule());
  }
  ActivateProvider();
  DeactivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldScheduleOnLaterActivation) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  // There is no schedule yet, so inactivation does not trigger unschedule.
  DeactivateProvider();
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldRescheduleAfterSuccessfulFetch) {
  // First reschedule on becoming active.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProvider();

  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));

  // Trigger a fetch.
  scheduler()->OnPersistentSchedulerWakeUp();
  // Second reschedule after a successful fetch.
  signal_fetch_done.Run(Status::Success());
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotRescheduleAfterFailedFetch) {
  // Only reschedule on becoming active.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));

  // Trigger a fetch.
  scheduler()->OnPersistentSchedulerWakeUp();
  // No furter reschedule after a failure.
  signal_fetch_done.Run(Status(StatusCode::PERMANENT_ERROR, ""));
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldScheduleOnlyOnce) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();
  // No further call to Schedule on a second status callback.
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldUnscheduleOnlyOnce) {
  {
    InSequence s;
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    EXPECT_CALL(*persistent_scheduler(), Unschedule());
  }
  // First schedule so that later we really unschedule.
  ActivateProvider();
  DeactivateProvider();
  // No further call to Unschedule on second status callback.
  DeactivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenPersistentWifiParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProvider();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the wifi interval for this class.
  SetVariationParameter("fetching_interval_hours-wifi-active_ntp_user", "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenPersistentFallbackParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProvider();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the fallback interval for this class.
  SetVariationParameter("fetching_interval_hours-fallback-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenShownWifiParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProvider();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the on usage interval for this class.
  SetVariationParameter("soft_fetching_interval_hours-wifi-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenShownFallbackParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProvider();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the fallback interval for this class.
  SetVariationParameter("soft_fetching_interval_hours-fallback-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenStartupWifiParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProvider();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the on usage interval for this class.
  SetVariationParameter("startup_fetching_interval_hours-wifi-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenStartupFallbackParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProvider();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the fallback interval for this class.
  SetVariationParameter(
      "startup_fetching_interval_hours-fallback-active_ntp_user", "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, FetchIntervalForShownTriggerOnWifi) {
  // Pretend we are on WiFi (already done in ctor, we make it explicit here).
  EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
      .WillRepeatedly(Return(true));
  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER which uses a 8h time
  // interval by default for shown trigger on WiFi.

  // Initial scheduling after being enabled.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // The first call to NTPOpened results in a fetch.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduler()->OnNTPOpened();
  // Rescheduling after a succesful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  signal_fetch_done.Run(Status::Success());

  // Open NTP again after too short delay. This time no fetch is executed.
  test_clock()->Advance(base::TimeDelta::FromHours(1));
  scheduler()->OnNTPOpened();

  // Open NTP after another delay, now together long enough to issue a fetch.
  test_clock()->Advance(base::TimeDelta::FromHours(7));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnNTPOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       OverrideFetchIntervalForShownTriggerOnWifi) {
  // Pretend we are on WiFi (already done in ctor, we make it explicit here).
  EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
      .WillRepeatedly(Return(true));
  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the interval for this class from 4h to 30min.
  SetVariationParameter("soft_fetching_interval_hours-wifi-active_ntp_user",
                        "0.5");

  // Initial scheduling after being enabled.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // The first call to NTPOpened results in a fetch.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduler()->OnNTPOpened();
  // Rescheduling after a succesful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  signal_fetch_done.Run(Status::Success());

  // Open NTP again after too short delay. This time no fetch is executed.
  test_clock()->Advance(base::TimeDelta::FromMinutes(20));
  scheduler()->OnNTPOpened();

  // Open NTP after another delay, now together long enough to issue a fetch.
  test_clock()->Advance(base::TimeDelta::FromMinutes(10));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnNTPOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       FetchIntervalForShownTriggerOnFallback) {
  // Pretend we are not on wifi -> fallback connection.
  EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
      .WillRepeatedly(Return(false));
  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER which uses a 12h time
  // interval by default for shown trigger not on WiFi.

  // Initial scheduling after being enabled.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // The first call to NTPOpened results in a fetch.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduler()->OnNTPOpened();
  // Rescheduling after a succesful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  signal_fetch_done.Run(Status::Success());

  // Open NTP again after too short delay. This time no fetch is executed.
  test_clock()->Advance(base::TimeDelta::FromHours(5));
  scheduler()->OnNTPOpened();

  // Open NTP after another delay, now together long enough to issue a fetch.
  test_clock()->Advance(base::TimeDelta::FromHours(7));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnNTPOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       OverrideFetchIntervalForShownTriggerOnFallback) {
  // Pretend we are not on wifi -> fallback connection.
  EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
      .WillRepeatedly(Return(false));
  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the interval for this class from 4h to 30min.
  SetVariationParameter("soft_fetching_interval_hours-fallback-active_ntp_user",
                        "0.5");

  // Initial scheduling after being enabled.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // The first call to NTPOpened results in a fetch.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduler()->OnNTPOpened();
  // Rescheduling after a succesful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  signal_fetch_done.Run(Status::Success());

  // Open NTP again after too short delay. This time no fetch is executed.
  test_clock()->Advance(base::TimeDelta::FromMinutes(20));
  scheduler()->OnNTPOpened();

  // Open NTP after another delay, now together long enough to issue a fetch.
  test_clock()->Advance(base::TimeDelta::FromMinutes(10));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnNTPOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldBlockFetchingForSomeTimeAfterHistoryCleared) {
  // First enable the scheduler -- this will trigger the persistent scheduling.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();
  // Clear the history.
  scheduler()->OnHistoryCleared();

  // A trigger after 15 minutes is ignored.
  test_clock()->Advance(base::TimeDelta::FromMinutes(15));
  scheduler()->OnBrowserForegrounded();

  // A trigger after another 16 minutes is performed (more than 30m after
  // clearing the history).
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  test_clock()->Advance(base::TimeDelta::FromMinutes(16));
  scheduler()->OnBrowserForegrounded();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldImmediatelyFetchAfterSuggestionsCleared) {
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;

  // First enable the scheduler -- this will trigger the persistent scheduling.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // The first trigger results in a fetch.
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduler()->OnBrowserForegrounded();
  // Make the fetch successful -- this results in rescheduling.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  signal_fetch_done.Run(Status::Success());

  // Clear the suggestions - results in an immediate fetch.
  EXPECT_CALL(*provider(), ReloadSuggestions());
  scheduler()->OnSuggestionsCleared();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldThrottleInteractiveRequests) {
  // Change the quota for interactive requests ("active NTP user" is the default
  // class in tests).
  SetVariationParameter("interactive_quota_SuggestionFetcherActiveNTPUser",
                        "10");
  ResetProvider();

  for (int x = 0; x < 10; ++x) {
    EXPECT_THAT(scheduler()->AcquireQuotaForInteractiveFetch(), Eq(true));
  }

  // Now the quota is over.
  EXPECT_THAT(scheduler()->AcquireQuotaForInteractiveFetch(), Eq(false));
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldThrottleNonInteractiveRequests) {
  // Change the quota for interactive requests ("active NTP user" is the default
  // class in tests).
  SetVariationParameter("quota_SuggestionFetcherActiveNTPUser", "5");
  ResetProvider();

  // One scheduling on start, 5 times after successful fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(6);

  // First enable the scheduler -- this will trigger the persistent scheduling.
  ActivateProvider();

  // As long as the quota suffices, the call gets through.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .Times(5)
      .WillRepeatedly(SaveArg<0>(&signal_fetch_done));
  for (int x = 0; x < 5; ++x) {
    scheduler()->OnPersistentSchedulerWakeUp();
    signal_fetch_done.Run(Status::Success());
  }

  // For the 6th time, it is blocked by the scheduling provider.
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldIgnoreSubsequentStartupSignalsForM58) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kRemoteSuggestionsEmulateM58FetchingSchedule);
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;

  // First enable the scheduler -- this will trigger the persistent scheduling.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // The startup triggers are ignored.
  EXPECT_CALL(*provider(), RefetchInTheBackground(_)).Times(0);
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();

  // Foreground the browser again after a very long delay. Again, no fetch is
  // executed for neither Foregrounded, nor ColdStart.
  test_clock()->Advance(base::TimeDelta::FromHours(100000));
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();
}

}  // namespace ntp_snippets
