// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/scheduling_remote_suggestions_provider.h"

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
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/ntp_snippets/status.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Eq;
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
};

// TODO(jkrcal): Move into its own library to reuse in other unit-tests?
class MockRemoteSuggestionsProvider : public RemoteSuggestionsProvider {
 public:
  MockRemoteSuggestionsProvider(Observer* observer)
      : RemoteSuggestionsProvider(observer) {}

  // Move-only params are not supported by GMock. We want to mock out
  // RefetchInTheBackground() which takes a unique_ptr<>. Instead, we add a new
  // mock function which takes a copy of the callback and override the
  // RemoteSuggestionsProvider's method to forward the call into the new mock
  // function.
  void SetProviderStatusCallback(
      std::unique_ptr<RemoteSuggestionsProvider::ProviderStatusCallback>
          callback) override {
    SetProviderStatusCallback(*callback);
  }
  MOCK_METHOD1(SetProviderStatusCallback,
               void(RemoteSuggestionsProvider::ProviderStatusCallback));

  // Move-only params are not supported by GMock (same work-around as above).
  void RefetchInTheBackground(
      std::unique_ptr<RemoteSuggestionsProvider::FetchStatusCallback> callback)
      override {
    RefetchInTheBackground(*callback);
  }
  MOCK_METHOD1(RefetchInTheBackground,
               void(RemoteSuggestionsProvider::FetchStatusCallback));

  MOCK_CONST_METHOD0(suggestions_fetcher_for_debugging,
                     const RemoteSuggestionsFetcher*());

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

class SchedulingRemoteSuggestionsProviderTest
    : public ::testing::Test {
 public:
  SchedulingRemoteSuggestionsProviderTest()
      :  // For the test we enabled all trigger types.
        default_variation_params_{{"scheduler_trigger_types",
                                   "persistent_scheduler_wake_up,ntp_opened,"
                                   "browser_foregrounded,browser_cold_start"}},
        params_manager_(ntp_snippets::kStudyName,
                        default_variation_params_,
                        {kArticleSuggestionsFeature.name}),
        underlying_provider_(nullptr),
        scheduling_provider_(nullptr),
        user_classifier_(/*pref_service=*/nullptr) {
    SchedulingRemoteSuggestionsProvider::RegisterProfilePrefs(
        utils_.pref_service()->registry());
    ResetProvider();
  }

  void ResetProvider() {
    auto underlying_provider =
        base::MakeUnique<StrictMock<MockRemoteSuggestionsProvider>>(
            /*observer=*/nullptr);
    underlying_provider_ = underlying_provider.get();

    // SchedulingRemoteSuggestionsProvider calls SetProviderStatusCallback(_) to
    // stay in the loop of status changes.
    EXPECT_CALL(*underlying_provider_, SetProviderStatusCallback(_))
        .WillOnce(SaveArg<0>(&provider_status_callback_));

    auto test_clock = base::MakeUnique<base::SimpleTestClock>();
    test_clock_ = test_clock.get();
    test_clock_->SetNow(base::Time::Now());

    scheduling_provider_ =
        base::MakeUnique<SchedulingRemoteSuggestionsProvider>(
            /*observer=*/nullptr, std::move(underlying_provider),
            &persistent_scheduler_, &user_classifier_, utils_.pref_service(),
            std::move(test_clock));
  }

  void SetVariationParameter(const std::string& param_name,
                             const std::string& param_value) {
    std::map<std::string, std::string> params = default_variation_params_;
    params[param_name] = param_value;

    params_manager_.ClearAllVariationParams();
    params_manager_.SetVariationParamsWithFeatureAssociations(
        ntp_snippets::kStudyName, params,
        {ntp_snippets::kArticleSuggestionsFeature.name});
  }

 protected:
  std::map<std::string, std::string> default_variation_params_;
  variations::testing::VariationParamsManager params_manager_;
  StrictMock<MockPersistentScheduler> persistent_scheduler_;
  StrictMock<MockRemoteSuggestionsProvider>* underlying_provider_;
  std::unique_ptr<SchedulingRemoteSuggestionsProvider> scheduling_provider_;
  RemoteSuggestionsProvider::ProviderStatusCallback provider_status_callback_;
  base::SimpleTestClock* test_clock_;

  void ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus new_status) {
    provider_status_callback_.Run(new_status);
  }

 private:
  test::RemoteSuggestionsTestUtils utils_;
  UserClassifier user_classifier_;

  DISALLOW_COPY_AND_ASSIGN(SchedulingRemoteSuggestionsProviderTest);
};

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldIgnoreSignalsWhenNotEnabled) {
  scheduling_provider_->OnPersistentSchedulerWakeUp();
  scheduling_provider_->OnNTPOpened();
  scheduling_provider_->OnBrowserForegrounded();
  scheduling_provider_->OnBrowserColdStart();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldIgnoreSignalsWhenDisabledByParam) {
  // First set an empty list of allowed trigger types.
  SetVariationParameter("scheduler_trigger_types", "-");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  scheduling_provider_->OnPersistentSchedulerWakeUp();
  scheduling_provider_->OnNTPOpened();
  scheduling_provider_->OnBrowserForegrounded();
  scheduling_provider_->OnBrowserColdStart();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldHandleEmptyParamForTriggerTypes) {
  // First set an empty param for allowed trigger types -> should result in the
  // default list.
  SetVariationParameter("scheduler_trigger_types", "");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  // For instance, persistent scheduler wake up should be enabled by default.
  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_));
  scheduling_provider_->OnPersistentSchedulerWakeUp();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldHandleIncorrentParamForTriggerTypes) {
  // First set an invalid list of allowed trigger types.
  SetVariationParameter("scheduler_trigger_types", "ntp_opened,foo;");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  // For instance, persistent scheduler wake up should be enabled by default.
  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_));
  scheduling_provider_->OnPersistentSchedulerWakeUp();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldFetchOnPersistentSchedulerWakeUp) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types",
                        "persistent_scheduler_wake_up");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_));
  scheduling_provider_->OnPersistentSchedulerWakeUp();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldFetchOnPersistentSchedulerWakeUpRepeated) {
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  {
    InSequence s;
    EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
    EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_))
        .WillOnce(SaveArg<0>(&signal_fetch_done));
    EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
    EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_));
  }
  // First enable the scheduler -- calling Schedule() for the first time.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
  // Make the first persistent fetch successful -- calling Schedule() again.
  scheduling_provider_->OnPersistentSchedulerWakeUp();
  signal_fetch_done.Run(Status::Success());
  // Make the second fetch.
  scheduling_provider_->OnPersistentSchedulerWakeUp();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldNotTriggerBackgroundFetchIfAlreadyInProgess) {
  {
    InSequence s;
    EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
    EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_));
    // RefetchInTheBackground is not called after the second trigger.
  }
  // First enable the scheduler -- calling Schedule() for the first time.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
  // Make the first persistent fetch never finish.
  scheduling_provider_->OnPersistentSchedulerWakeUp();
  // Make the second fetch.
  scheduling_provider_->OnPersistentSchedulerWakeUp();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldFetchOnNTPOpenedForTheFirstTime) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types", "ntp_opened");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_));
  scheduling_provider_->OnNTPOpened();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldFetchOnBrowserForegroundedForTheFirstTime) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types", "browser_foregrounded");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_));
  scheduling_provider_->OnBrowserForegrounded();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldFetchOnBrowserColdStartForTheFirstTime) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types", "browser_cold_start");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_));
  scheduling_provider_->OnBrowserColdStart();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldNotFetchOnNTPOpenedAfterSuccessfulSoftFetch) {
  // First enable the scheduler; the second Schedule is called after the
  // successful fetch.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _)).Times(2);
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  // Make the first soft fetch successful.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduling_provider_->OnNTPOpened();
  signal_fetch_done.Run(Status::Success());
  // The second call is ignored if it happens right after the first one.
  scheduling_provider_->OnNTPOpened();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldNotFetchOnNTPOpenedAfterSuccessfulPersistentFetch) {
  // First enable the scheduler; the second Schedule is called after the
  // successful fetch.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _)).Times(2);
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  // Make the first persistent fetch successful.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduling_provider_->OnPersistentSchedulerWakeUp();
  signal_fetch_done.Run(Status::Success());
  // The second call is ignored if it happens right after the first one.
  scheduling_provider_->OnNTPOpened();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldNotFetchOnNTPOpenedAfterFailedSoftFetch) {
  // First enable the scheduler.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  // Make the first soft fetch failed.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduling_provider_->OnNTPOpened();
  signal_fetch_done.Run(Status(StatusCode::PERMANENT_ERROR, ""));

  // The second call is ignored if it happens right after the first one.
  scheduling_provider_->OnNTPOpened();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldNotFetchOnNTPOpenedAfterFailedPersistentFetch) {
  // First enable the scheduler.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  // Make the first persistent fetch failed.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));
  scheduling_provider_->OnPersistentSchedulerWakeUp();
  signal_fetch_done.Run(Status(StatusCode::PERMANENT_ERROR, ""));

  // The second call is ignored if it happens right after the first one.
  scheduling_provider_->OnNTPOpened();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldFetchAgainOnNTPOpenedLaterAgain) {
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  {
    InSequence s;
    // Initial scheduling after being enabled.
    EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
    // The first call to NTPOpened results in a fetch.
    EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_))
        .WillOnce(SaveArg<0>(&signal_fetch_done));
    // Rescheduling after a succesful fetch.
    EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
    // The second call to NTPOpened 2hrs later again results in a fetch.
    EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_));
  }

  // First enable the scheduler.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
  // Make the first soft fetch successful.
  scheduling_provider_->OnNTPOpened();
  signal_fetch_done.Run(Status::Success());
  // Open NTP again after 2hrs.
  test_clock_->Advance(base::TimeDelta::FromHours(2));
  scheduling_provider_->OnNTPOpened();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldRescheduleOnRescheduleFetching) {
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  scheduling_provider_->RescheduleFetching();
}

TEST_F(SchedulingRemoteSuggestionsProviderTest, ShouldScheduleOnActivation) {
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldUnscheduleOnLaterInactivation) {
  {
    InSequence s;
    EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
    EXPECT_CALL(persistent_scheduler_, Unschedule());
  }
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::INACTIVE);
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldScheduleOnLaterActivation) {
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  // There is no schedule yet, so inactivation does not trigger unschedule.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::INACTIVE);
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldRescheduleAfterSuccessfulFetch) {
  // First reschedule on becoming active.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _)).Times(2);
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));

  // Trigger a fetch.
  scheduling_provider_->OnPersistentSchedulerWakeUp();
  // Second reschedule after a successful fetch.
  signal_fetch_done.Run(Status::Success());
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ShouldNotRescheduleAfterFailedFetch) {
  // Only reschedule on becoming active.
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*underlying_provider_, RefetchInTheBackground(_))
      .WillOnce(SaveArg<0>(&signal_fetch_done));

  // Trigger a fetch.
  scheduling_provider_->OnPersistentSchedulerWakeUp();
  // No furter reschedule after a failure.
  signal_fetch_done.Run(Status(StatusCode::PERMANENT_ERROR, ""));
}

TEST_F(SchedulingRemoteSuggestionsProviderTest, ShouldScheduleOnlyOnce) {
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
  // No further call to Schedule on a second status callback.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
}

TEST_F(SchedulingRemoteSuggestionsProviderTest, ShouldUnscheduleOnlyOnce) {
  {
    InSequence s;
    EXPECT_CALL(persistent_scheduler_, Schedule(_, _));
    EXPECT_CALL(persistent_scheduler_, Unschedule());
  }
  // First schedule so that later we really unschedule.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::INACTIVE);
  // No further call to Unschedule on second status callback.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::INACTIVE);
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ReschedulesWhenWifiParamChanges) {
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _)).Times(2);
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the wifi interval for this class.
  SetVariationParameter("fetching_interval_hours-wifi-active_ntp_user", "1.5");

  // Schedule() should get called for the second time after params have changed.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ReschedulesWhenFallbackParamChanges) {
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _)).Times(2);
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the fallback interval for this class.
  SetVariationParameter("fetching_interval_hours-fallback-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
}

TEST_F(SchedulingRemoteSuggestionsProviderTest,
       ReschedulesWhenOnUsageEventParamChanges) {
  EXPECT_CALL(persistent_scheduler_, Schedule(_, _)).Times(2);
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the on usage interval for this class.
  SetVariationParameter("soft_fetching_interval_hours-active-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ChangeStatusOfUnderlyingProvider(
      RemoteSuggestionsProvider::ProviderStatus::ACTIVE);
}

}  // namespace ntp_snippets
