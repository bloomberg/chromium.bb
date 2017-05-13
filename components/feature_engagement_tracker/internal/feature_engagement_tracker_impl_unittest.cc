// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/feature_engagement_tracker_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/feature_engagement_tracker/internal/editable_configuration.h"
#include "components/feature_engagement_tracker/internal/in_memory_store.h"
#include "components/feature_engagement_tracker/internal/model_impl.h"
#include "components/feature_engagement_tracker/internal/never_storage_validator.h"
#include "components/feature_engagement_tracker/internal/once_condition_validator.h"
#include "components/feature_engagement_tracker/internal/time_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {
const base::Feature kTestFeatureFoo{"test_foo",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBar{"test_bar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureQux{"test_qux",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

void RegisterFeatureConfig(EditableConfiguration* configuration,
                           const base::Feature& feature,
                           bool valid) {
  FeatureConfig config;
  config.valid = valid;
  config.used.name = feature.name + std::string("_used");
  config.trigger.name = feature.name + std::string("_trigger");
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

// An InMemoryStore that is able to fake successful and unsuccessful
// loading of state.
class TestInMemoryStore : public InMemoryStore {
 public:
  explicit TestInMemoryStore(bool load_should_succeed)
      : InMemoryStore(), load_should_succeed_(load_should_succeed) {}

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

  DISALLOW_COPY_AND_ASSIGN(TestInMemoryStore);
};

class StoreEverythingStorageValidator : public StorageValidator {
 public:
  StoreEverythingStorageValidator() = default;
  ~StoreEverythingStorageValidator() override = default;

  bool ShouldStore(const std::string& event_name) const override {
    return true;
  }

  bool ShouldKeep(const std::string& event_name,
                  uint32_t event_day,
                  uint32_t current_day) const override {
    return true;
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(StoreEverythingStorageValidator);
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

class FeatureEngagementTrackerImplTest : public ::testing::Test {
 public:
  FeatureEngagementTrackerImplTest() = default;

  void SetUp() override {
    std::unique_ptr<EditableConfiguration> configuration =
        base::MakeUnique<EditableConfiguration>();
    configuration_ = configuration.get();

    RegisterFeatureConfig(configuration.get(), kTestFeatureFoo, true);
    RegisterFeatureConfig(configuration.get(), kTestFeatureBar, true);
    RegisterFeatureConfig(configuration.get(), kTestFeatureQux, false);

    std::unique_ptr<TestInMemoryStore> store = CreateStore();
    store_ = store.get();

    auto model = base::MakeUnique<ModelImpl>(
        std::move(store), base::MakeUnique<StoreEverythingStorageValidator>());

    tracker_.reset(new FeatureEngagementTrackerImpl(
        std::move(model), std::move(configuration),
        base::MakeUnique<OnceConditionValidator>(),
        base::MakeUnique<TestTimeProvider>()));
  }

  void VerifyEventTriggerEvents(const base::Feature& feature, uint32_t count) {
    Event trigger_event = store_->GetEvent(
        configuration_->GetFeatureConfig(feature).trigger.name);
    if (count == 0) {
      EXPECT_EQ(0, trigger_event.events_size());
      return;
    }

    EXPECT_EQ(1, trigger_event.events_size());
    EXPECT_EQ(1u, trigger_event.events(0).day());
    EXPECT_EQ(count, trigger_event.events(0).count());
  }

 protected:
  virtual std::unique_ptr<TestInMemoryStore> CreateStore() {
    // Returns a Store that will successfully initialize.
    return base::MakeUnique<TestInMemoryStore>(true);
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<FeatureEngagementTrackerImpl> tracker_;
  TestInMemoryStore* store_;
  Configuration* configuration_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeatureEngagementTrackerImplTest);
};

// A top-level test class where the store fails to initialize.
class FailingInitFeatureEngagementTrackerImplTest
    : public FeatureEngagementTrackerImplTest {
 public:
  FailingInitFeatureEngagementTrackerImplTest() = default;

 protected:
  std::unique_ptr<TestInMemoryStore> CreateStore() override {
    // Returns a Store that will fail to initialize.
    return base::MakeUnique<TestInMemoryStore>(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailingInitFeatureEngagementTrackerImplTest);
};

}  // namespace

TEST_F(FeatureEngagementTrackerImplTest, TestInitialization) {
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

TEST_F(FeatureEngagementTrackerImplTest, TestInitializationMultipleCallbacks) {
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

TEST_F(FeatureEngagementTrackerImplTest, TestAddingCallbackAfterInitFinished) {
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

TEST_F(FeatureEngagementTrackerImplTest,
       TestAddingCallbackBeforeAndAfterInitFinished) {
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

TEST_F(FailingInitFeatureEngagementTrackerImplTest, TestFailingInitialization) {
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

TEST_F(FailingInitFeatureEngagementTrackerImplTest,
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

TEST_F(FeatureEngagementTrackerImplTest, TestTriggering) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();

  // The first time a feature triggers it should be shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureQux, 0);

  // While in-product help is currently showing, no other features should be
  // shown.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  VerifyEventTriggerEvents(kTestFeatureBar, 0);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureQux, 0);

  // After dismissing the current in-product help, that feature can not be shown
  // again, but a different feature should.
  tracker_->Dismissed(kTestFeatureFoo);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  VerifyEventTriggerEvents(kTestFeatureBar, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureQux, 0);

  // After dismissing the second registered feature, no more in-product help
  // should be shown, since kTestFeatureQux is invalid.
  tracker_->Dismissed(kTestFeatureBar);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  VerifyEventTriggerEvents(kTestFeatureFoo, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  VerifyEventTriggerEvents(kTestFeatureBar, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
  VerifyEventTriggerEvents(kTestFeatureQux, 0);
}

TEST_F(FeatureEngagementTrackerImplTest, TestNotifyEvent) {
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::Bind(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();

  tracker_->NotifyEvent("foo");
  tracker_->NotifyEvent("foo");
  tracker_->NotifyEvent("bar");

  Event foo_event = store_->GetEvent("foo");
  ASSERT_EQ(1, foo_event.events_size());
  EXPECT_EQ(1u, foo_event.events(0).day());
  EXPECT_EQ(2u, foo_event.events(0).count());

  Event bar_event = store_->GetEvent("bar");
  ASSERT_EQ(1, bar_event.events_size());
  EXPECT_EQ(1u, bar_event.events(0).day());
  EXPECT_EQ(1u, bar_event.events(0).count());
}

}  // namespace feature_engagement_tracker
