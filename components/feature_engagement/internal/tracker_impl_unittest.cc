// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/tracker_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/test/user_action_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feature_engagement/internal/availability_model_impl.h"
#include "components/feature_engagement/internal/display_lock_controller.h"
#include "components/feature_engagement/internal/editable_configuration.h"
#include "components/feature_engagement/internal/event_model_impl.h"
#include "components/feature_engagement/internal/in_memory_event_store.h"
#include "components/feature_engagement/internal/never_availability_model.h"
#include "components/feature_engagement/internal/never_event_storage_validator.h"
#include "components/feature_engagement/internal/once_condition_validator.h"
#include "components/feature_engagement/internal/stats.h"
#include "components/feature_engagement/internal/time_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {
const base::Feature kTestFeatureFoo{"test_foo",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBar{"test_bar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBaz{"test_baz",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureQux{"test_qux",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

void RegisterFeatureConfig(EditableConfiguration* configuration,
                           const base::Feature& feature,
                           bool valid,
                           bool tracking_only) {
  FeatureConfig config;
  config.valid = valid;
  config.used.name = feature.name + std::string("_used");
  config.trigger.name = feature.name + std::string("_trigger");
  config.tracking_only = tracking_only;
  configuration->SetConfiguration(&feature, config);
}

// An OnInitializedCallback that stores whether it has been invoked and what
// the result was.
class StoringInitializedCallback {
 public:
  StoringInitializedCallback() : invoked_(false), success_(false) {}

  void OnInitialized(bool success) {
    DCHECK(!invoked_);
    invoked_ = true;
    success_ = success;
  }

  bool invoked() { return invoked_; }

  bool success() { return success_; }

 private:
  bool invoked_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(StoringInitializedCallback);
};

// An InMemoryEventStore that is able to fake successful and unsuccessful
// loading of state.
class TestInMemoryEventStore : public InMemoryEventStore {
 public:
  explicit TestInMemoryEventStore(bool load_should_succeed)
      : load_should_succeed_(load_should_succeed) {}

  void Load(const OnLoadedCallback& callback) override {
    HandleLoadResult(callback, load_should_succeed_);
  }

  void WriteEvent(const Event& event) override {
    events_[event.name()] = event;
  }

  Event GetEvent(const std::string& event_name) { return events_[event_name]; }

 private:
  // Denotes whether the call to Load(...) should succeed or not. This impacts
  // both the ready-state and the result for the OnLoadedCallback.
  bool load_should_succeed_;

  std::map<std::string, Event> events_;

  DISALLOW_COPY_AND_ASSIGN(TestInMemoryEventStore);
};

class StoreEverythingEventStorageValidator : public EventStorageValidator {
 public:
  StoreEverythingEventStorageValidator() = default;
  ~StoreEverythingEventStorageValidator() override = default;

  bool ShouldStore(const std::string& event_name) const override {
    return true;
  }

  bool ShouldKeep(const std::string& event_name,
                  uint32_t event_day,
                  uint32_t current_day) const override {
    return true;
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(StoreEverythingEventStorageValidator);
};

class TestTimeProvider : public TimeProvider {
 public:
  TestTimeProvider() = default;
  ~TestTimeProvider() override = default;

  // TimeProvider implementation.
  uint32_t GetCurrentDay() const override { return 1u; };

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTimeProvider);
};

class TestAvailabilityModel : public AvailabilityModel {
 public:
  TestAvailabilityModel() : ready_(true) {}
  ~TestAvailabilityModel() override = default;

  void Initialize(AvailabilityModel::OnInitializedCallback callback,
                  uint32_t current_day) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), ready_));
  }

  bool IsReady() const override { return ready_; }

  void SetIsReady(bool ready) { ready_ = ready; }

  base::Optional<uint32_t> GetAvailability(
      const base::Feature& feature) const override {
    return base::nullopt;
  }

 private:
  bool ready_;

  DISALLOW_COPY_AND_ASSIGN(TestAvailabilityModel);
};

class TestDisplayLockController : public DisplayLockController {
 public:
  TestDisplayLockController() = default;
  ~TestDisplayLockController() override = default;

  std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() override {
    return std::move(next_display_lock_handle_);
  }

  bool IsDisplayLocked() const override { return false; }

  void SetNextDisplayLockHandle(
      std::unique_ptr<DisplayLockHandle> display_lock_handle) {
    next_display_lock_handle_ = std::move(display_lock_handle);
  }

 private:
  // The next DisplayLockHandle to return.
  std::unique_ptr<DisplayLockHandle> next_display_lock_handle_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayLockController);
};

class TrackerImplTest : public ::testing::Test {
 public:
  TrackerImplTest() = default;

  void SetUp() override {
    std::unique_ptr<EditableConfiguration> configuration =
        std::make_unique<EditableConfiguration>();
    configuration_ = configuration.get();

    RegisterFeatureConfig(configuration.get(), kTestFeatureFoo,
                          true /* is_valid */, false /* tracking_only */);
    RegisterFeatureConfig(configuration.get(), kTestFeatureBar,
                          true /* is_valid */, false /* tracking_only */);
    RegisterFeatureConfig(configuration.get(), kTestFeatureBaz,
                          true /* is_valid */, true /* tracking_only */);
    RegisterFeatureConfig(configuration.get(), kTestFeatureQux,
                          false /* is_valid */, false /* tracking_only */);

    std::unique_ptr<TestInMemoryEventStore> event_store = CreateEventStore();
    event_store_ = event_store.get();

    auto event_model = std::make_unique<EventModelImpl>(
        std::move(event_store),
        std::make_unique<StoreEverythingEventStorageValidator>());

    auto availability_model = std::make_unique<TestAvailabilityModel>();
    availability_model_ = availability_model.get();
    availability_model_->SetIsReady(ShouldAvailabilityStoreBeReady());

    auto display_lock_controller =
        std::make_unique<TestDisplayLockController>();
    display_lock_controller_ = display_lock_controller.get();

    tracker_.reset(new TrackerImpl(
        std::move(event_model), std::move(availability_model),
        std::move(configuration), std::move(display_lock_controller),
        std::make_unique<OnceConditionValidator>(),
        std::make_unique<TestTimeProvider>()));
  }

  void VerifyEventTriggerEvents(const base::Feature& feature, uint32_t count) {
    Event trigger_event = event_store_->GetEvent(
        configuration_->GetFeatureConfig(feature).trigger.name);
    if (count == 0) {
      EXPECT_EQ(0, trigger_event.events_size());
      return;
    }

    EXPECT_EQ(1, trigger_event.events_size());
    EXPECT_EQ(1u, trigger_event.events(0).day());
    EXPECT_EQ(count, trigger_event.events(0).count());
  }

  void VerifyHistogramsForFeature(const std::string& histogram_name,
                                  bool check,
                                  int expected_success_count,
                                  int expected_failure_count,
                                  int expected_success_tracking_only_count) {
    if (!check)
      return;

    histogram_tester_.ExpectBucketCount(
        histogram_name, static_cast<int>(stats::TriggerHelpUIResult::SUCCESS),
        expected_success_count);
    histogram_tester_.ExpectBucketCount(
        histogram_name, static_cast<int>(stats::TriggerHelpUIResult::FAILURE),
        expected_failure_count);
    histogram_tester_.ExpectBucketCount(
        histogram_name,
        static_cast<int>(stats::TriggerHelpUIResult::SUCCESS_TRACKING_ONLY),
        expected_success_tracking_only_count);
  }

  // Histogram values are checked only if their respective |check_...| is true,
  // since inspecting a bucket count for a histogram that has not been recorded
  // yet leads to an error.
  void VerifyHistograms(bool check_foo,
                        int expected_foo_success_count,
                        int expected_foo_failure_count,
                        int expected_foo_success_tracking_only_count,
                        bool check_bar,
                        int expected_bar_success_count,
                        int expected_bar_failure_count,
                        int expected_bar_success_tracking_only_count,
                        bool check_baz,
                        int expected_baz_success_count,
                        int expected_baz_failure_count,
                        int expected_baz_success_tracking_only_count,
                        bool check_qux,
                        int expected_qux_success_count,
                        int expected_qux_failure_count,
                        int expected_qux_success_tracking_only_count) {
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI.test_foo",
                               check_foo, expected_foo_success_count,
                               expected_foo_failure_count,
                               expected_foo_success_tracking_only_count);
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI.test_bar",
                               check_bar, expected_bar_success_count,
                               expected_bar_failure_count,
                               expected_bar_success_tracking_only_count);
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI.test_baz",
                               check_baz, expected_baz_success_count,
                               expected_baz_failure_count,
                               expected_baz_success_tracking_only_count);
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI.test_qux",
                               check_qux, expected_qux_success_count,
                               expected_qux_failure_count,
                               expected_qux_success_tracking_only_count);

    int expected_total_successes =
        expected_foo_success_count + expected_bar_success_count +
        expected_baz_success_count + expected_qux_success_count;
    int expected_total_failures =
        expected_foo_failure_count + expected_bar_failure_count +
        expected_baz_failure_count + expected_qux_failure_count;
    int expected_total_success_tracking_onlys =
        expected_foo_success_tracking_only_count +
        expected_bar_success_tracking_only_count +
        expected_baz_success_tracking_only_count +
        expected_qux_success_tracking_only_count;
    bool should_check = check_foo || check_bar || check_baz || check_qux;
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI",
                               should_check, expected_total_successes,
                               expected_total_failures,
                               expected_total_success_tracking_onlys);
  }

  void VerifyUserActionsTriggerChecks(
      const base::UserActionTester& user_action_tester,
      int expected_foo_count,
      int expected_bar_count,
      int expected_baz_count,
      int expected_qux_count) {
    EXPECT_EQ(expected_foo_count,
              user_action_tester.GetActionCount(
                  "InProductHelp.ShouldTriggerHelpUI.test_foo"));
    EXPECT_EQ(expected_bar_count,
              user_action_tester.GetActionCount(
                  "InProductHelp.ShouldTriggerHelpUI.test_bar"));
    EXPECT_EQ(expected_baz_count,
              user_action_tester.GetActionCount(
                  "InProductHelp.ShouldTriggerHelpUI.test_baz"));
    EXPECT_EQ(expected_qux_count,
              user_action_tester.GetActionCount(
                  "InProductHelp.ShouldTriggerHelpUI.test_qux"));
  }

  void VerifyUserActionsTriggered(
      const base::UserActionTester& user_action_tester,
      int expected_foo_count,
      int expected_bar_count,
      int expected_baz_count,
      int expected_qux_count) {
    EXPECT_EQ(
        expected_foo_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.Triggered.test_foo"));
    EXPECT_EQ(
        expected_bar_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.Triggered.test_bar"));
    EXPECT_EQ(
        expected_baz_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.Triggered.test_baz"));
    EXPECT_EQ(
        expected_qux_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.Triggered.test_qux"));
  }

  void VerifyUserActionsNotTriggered(
      const base::UserActionTester& user_action_tester,
      int expected_foo_count,
      int expected_bar_count,
      int expected_baz_count,
      int expected_qux_count) {
    EXPECT_EQ(
        expected_foo_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.test_foo"));
    EXPECT_EQ(
        expected_bar_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.test_bar"));
    EXPECT_EQ(
        expected_baz_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.test_baz"));
    EXPECT_EQ(
        expected_qux_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.test_qux"));
  }

  void VerifyUserActionsWouldHaveTriggered(
      const base::UserActionTester& user_action_tester,
      int expected_foo_count,
      int expected_bar_count,
      int expected_baz_count,
      int expected_qux_count) {
    EXPECT_EQ(expected_foo_count, user_action_tester.GetActionCount(
                                      "InProductHelp.ShouldTriggerHelpUIResult."
                                      "WouldHaveTriggered.test_foo"));
    EXPECT_EQ(expected_bar_count, user_action_tester.GetActionCount(
                                      "InProductHelp.ShouldTriggerHelpUIResult."
                                      "WouldHaveTriggered.test_bar"));
    EXPECT_EQ(expected_baz_count, user_action_tester.GetActionCount(
                                      "InProductHelp.ShouldTriggerHelpUIResult."
                                      "WouldHaveTriggered.test_baz"));
    EXPECT_EQ(expected_qux_count, user_action_tester.GetActionCount(
                                      "InProductHelp.ShouldTriggerHelpUIResult."
                                      "WouldHaveTriggered.test_qux"));
  }

  void VerifyUserActionsDismissed(
      const base::UserActionTester& user_action_tester,
      int expected_dismissed_count) {
    EXPECT_EQ(expected_dismissed_count,
              user_action_tester.GetActionCount("InProductHelp.Dismissed"));
  }

 protected:
  virtual std::unique_ptr<TestInMemoryEventStore> CreateEventStore() {
    // Returns a EventStore that will successfully initialize.
    return std::make_unique<TestInMemoryEventStore>(true);
  }

  virtual bool ShouldAvailabilityStoreBeReady() { return true; }

  base::MessageLoop message_loop_;
  std::unique_ptr<TrackerImpl> tracker_;
  TestInMemoryEventStore* event_store_;
  TestAvailabilityModel* availability_model_;
  TestDisplayLockController* display_lock_controller_;
  Configuration* configuration_;
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrackerImplTest);
};

// A top-level test class where the store fails to initialize.
class FailingStoreInitTrackerImplTest : public TrackerImplTest {
 public:
  FailingStoreInitTrackerImplTest() = default;

 protected:
  std::unique_ptr<TestInMemoryEventStore> CreateEventStore() override {
    // Returns a EventStore that will fail to initialize.
    return std::make_unique<TestInMemoryEventStore>(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailingStoreInitTrackerImplTest);
};

// A top-level test class where the AvailabilityModel fails to initialize.
class FailingAvailabilityModelInitTrackerImplTest : public TrackerImplTest {
 public:
  FailingAvailabilityModelInitTrackerImplTest() = default;

 protected:
  bool ShouldAvailabilityStoreBeReady() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailingAvailabilityModelInitTrackerImplTest);
};

}  // namespace

TEST_F(TrackerImplTest, TestInitialization) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_TRUE(callback.success());
}

TEST_F(TrackerImplTest, TestInitializationMultipleCallbacks) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback1;
  StoringInitializedCallback callback2;

  tracker_->AddOnInitializedCallback(
      base::Bind(&StoringInitializedCallback::OnInitialized,
                 base::Unretained(&callback1)));
  tracker_->AddOnInitializedCallback(
      base::Bind(&StoringInitializedCallback::OnInitialized,
                 base::Unretained(&callback2)));
  EXPECT_FALSE(callback1.invoked());
  EXPECT_FALSE(callback2.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());
  EXPECT_TRUE(callback1.invoked());
  EXPECT_TRUE(callback2.invoked());
  EXPECT_TRUE(callback1.success());
  EXPECT_TRUE(callback2.success());
}

TEST_F(TrackerImplTest, TestAddingCallbackAfterInitFinished) {
  EXPECT_FALSE(tracker_->IsInitialized());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback.invoked());
}

TEST_F(TrackerImplTest, TestAddingCallbackBeforeAndAfterInitFinished) {
  EXPECT_FALSE(tracker_->IsInitialized());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());

  StoringInitializedCallback callback_before;
  tracker_->AddOnInitializedCallback(
      base::Bind(&StoringInitializedCallback::OnInitialized,
                 base::Unretained(&callback_before)));
  EXPECT_FALSE(callback_before.invoked());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_before.invoked());

  StoringInitializedCallback callback_after;
  tracker_->AddOnInitializedCallback(
      base::Bind(&StoringInitializedCallback::OnInitialized,
                 base::Unretained(&callback_after)));
  EXPECT_FALSE(callback_after.invoked());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_after.invoked());
}

TEST_F(FailingStoreInitTrackerImplTest, TestFailingInitialization) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tracker_->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_FALSE(callback.success());
}

TEST_F(FailingStoreInitTrackerImplTest,
       TestFailingInitializationMultipleCallbacks) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback1;
  StoringInitializedCallback callback2;
  tracker_->AddOnInitializedCallback(
      base::Bind(&StoringInitializedCallback::OnInitialized,
                 base::Unretained(&callback1)));
  tracker_->AddOnInitializedCallback(
      base::Bind(&StoringInitializedCallback::OnInitialized,
                 base::Unretained(&callback2)));
  EXPECT_FALSE(callback1.invoked());
  EXPECT_FALSE(callback2.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tracker_->IsInitialized());
  EXPECT_TRUE(callback1.invoked());
  EXPECT_TRUE(callback2.invoked());
  EXPECT_FALSE(callback1.success());
  EXPECT_FALSE(callback2.success());
}

TEST_F(FailingAvailabilityModelInitTrackerImplTest, AvailabilityModelNotReady) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tracker_->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_FALSE(callback.success());
}

TEST_F(TrackerImplTest, TestTriggering) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // The first time a feature triggers it should be shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureQux, 0);
  VerifyUserActionsTriggerChecks(user_action_tester, 2, 0, 0, 1);
  VerifyUserActionsTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 1, 0, 0, 1);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(true, 1, 1, 0, false, 0, 0, 0, false, 0, 0, 0, true, 0, 1,
                   0);

  // While in-product help is currently showing, no other features should be
  // shown.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  VerifyEventTriggerEvents(kTestFeatureBar, 0);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureQux, 0);
  VerifyUserActionsTriggerChecks(user_action_tester, 2, 1, 0, 2);
  VerifyUserActionsTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 1, 1, 0, 2);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(true, 1, 1, 0, true, 0, 1, 0, false, 0, 0, 0, true, 0, 2, 0);

  // After dismissing the current in-product help, that feature can not be shown
  // again, but a different feature should.
  tracker_->Dismissed(kTestFeatureFoo);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  VerifyEventTriggerEvents(kTestFeatureBar, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureQux, 0);
  VerifyUserActionsTriggerChecks(user_action_tester, 3, 2, 0, 3);
  VerifyUserActionsTriggered(user_action_tester, 1, 1, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 2, 1, 0, 3);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 1);
  VerifyHistograms(true, 1, 2, 0, true, 1, 1, 0, false, 0, 0, 0, true, 0, 3, 0);

  // After dismissing the second registered feature, no more in-product help
  // should be shown, since kTestFeatureQux is invalid.
  tracker_->Dismissed(kTestFeatureBar);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  VerifyEventTriggerEvents(kTestFeatureBar, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureQux, 0);
  VerifyUserActionsTriggerChecks(user_action_tester, 4, 3, 0, 4);
  VerifyUserActionsTriggered(user_action_tester, 1, 1, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 3, 2, 0, 4);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 2);
  VerifyHistograms(true, 1, 3, 0, true, 1, 2, 0, false, 0, 0, 0, true, 0, 4, 0);
}

TEST_F(TrackerImplTest, TestTrackingOnlyTriggering) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // Tracking only kTestFeatureBaz should never be shown, but should be counted.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBaz));
  VerifyEventTriggerEvents(kTestFeatureBaz, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 0u);
  VerifyUserActionsTriggerChecks(user_action_tester, 1, 0, 1, 0);
  VerifyUserActionsTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 1, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(true, 0, 1, 0, false, 0, 0, 0, true, 0, 0, 1, false, 0, 0,
                   0);

  // While in-product help is currently showing, even in a tracking only
  // setting, no other features should be shown.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 0);
  VerifyUserActionsTriggerChecks(user_action_tester, 2, 0, 1, 0);
  VerifyUserActionsTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 2, 0, 0, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 1, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(true, 0, 2, 0, false, 0, 0, 0, true, 0, 0, 1, false, 0, 0,
                   0);

  // After dismissing the current in-product help, that feature can not be shown
  // again, but a different feature should.
  tracker_->Dismissed(kTestFeatureBaz);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBaz));
  VerifyEventTriggerEvents(kTestFeatureBaz, 1u);
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  VerifyUserActionsTriggerChecks(user_action_tester, 3, 0, 2, 0);
  VerifyUserActionsTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 2, 0, 1, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 1, 0);
  VerifyUserActionsDismissed(user_action_tester, 1);
  VerifyHistograms(true, 1, 2, 0, false, 0, 0, 0, true, 0, 1, 1, false, 0, 0,
                   0);
}

TEST_F(TrackerImplTest, TestWouldTriggerInspection) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // Initially, both foo and bar would have been shown.
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureFoo, 0u);
  VerifyEventTriggerEvents(kTestFeatureBar, 0u);
  VerifyEventTriggerEvents(kTestFeatureQux, 0u);
  VerifyUserActionsTriggerChecks(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(false, 0, 0, 0, false, 0, 0, 0, false, 0, 0, 0, false, 0, 0,
                   0);

  // While foo shows, nothing else would have been shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1);
  VerifyUserActionsTriggerChecks(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(true, 1, 0, 0, false, 0, 0, 0, false, 0, 0, 0, false, 0, 0,
                   0);

  // After foo has been dismissed, it would not have triggered again, but bar
  // would have.
  tracker_->Dismissed(kTestFeatureFoo);
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1);
  VerifyEventTriggerEvents(kTestFeatureBar, 1);
  VerifyUserActionsTriggerChecks(user_action_tester, 2, 1, 0, 0);
  VerifyUserActionsTriggered(user_action_tester, 1, 1, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 1);
  VerifyHistograms(true, 1, 1, 0, true, 1, 0, 0, false, 0, 0, 0, false, 0, 0,
                   0);
}

TEST_F(TrackerImplTest, TestTriggerStateInspection) {
  // Before initialization has finished, NOT_READY should always be returned.
  EXPECT_EQ(Tracker::TriggerState::NOT_READY,
            tracker_->GetTriggerState(kTestFeatureFoo));
  EXPECT_EQ(Tracker::TriggerState::NOT_READY,
            tracker_->GetTriggerState(kTestFeatureQux));

  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  EXPECT_EQ(Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTestFeatureFoo));
  EXPECT_EQ(Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTestFeatureBar));

  // The first time a feature triggers it should be shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_EQ(Tracker::TriggerState::HAS_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTestFeatureFoo));

  // Trying to show again should keep state as displayed.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_EQ(Tracker::TriggerState::HAS_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTestFeatureFoo));

  // Other features should also be kept at not having been displayed.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  VerifyEventTriggerEvents(kTestFeatureBar, 0);
  EXPECT_EQ(Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTestFeatureBar));

  // Dismiss foo and show qux, which should update TriggerState of bar, and keep
  // TriggerState for foo.
  tracker_->Dismissed(kTestFeatureFoo);
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  VerifyEventTriggerEvents(kTestFeatureBar, 1);
  EXPECT_EQ(Tracker::TriggerState::HAS_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTestFeatureFoo));
  EXPECT_EQ(Tracker::TriggerState::HAS_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTestFeatureBar));
}

TEST_F(TrackerImplTest, TestNotifyEvent) {
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  tracker_->NotifyEvent("foo");
  tracker_->NotifyEvent("foo");
  tracker_->NotifyEvent("bar");
  tracker_->NotifyEvent(kTestFeatureFoo.name + std::string("_used"));
  tracker_->NotifyEvent(kTestFeatureFoo.name + std::string("_trigger"));

  // Used event will record both NotifyEvent and NotifyUsedEvent. Explicitly
  // specify the whole user action string here.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyUsedEvent.test_foo"));
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyEvent.test_foo"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyUsedEvent.test_bar"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyEvent.test_bar"));

  Event foo_event = event_store_->GetEvent("foo");
  ASSERT_EQ(1, foo_event.events_size());
  EXPECT_EQ(1u, foo_event.events(0).day());
  EXPECT_EQ(2u, foo_event.events(0).count());

  Event bar_event = event_store_->GetEvent("bar");
  ASSERT_EQ(1, bar_event.events_size());
  EXPECT_EQ(1u, bar_event.events(0).day());
  EXPECT_EQ(1u, bar_event.events(0).count());
}

TEST_F(TrackerImplTest, ShouldPassThroughAcquireDisplayLock) {
  auto lock_handle = std::make_unique<DisplayLockHandle>(base::DoNothing());
  DisplayLockHandle* lock_handle_ptr = lock_handle.get();
  display_lock_controller_->SetNextDisplayLockHandle(std::move(lock_handle));
  EXPECT_EQ(lock_handle_ptr, tracker_->AcquireDisplayLock().get());
}

}  // namespace feature_engagement
