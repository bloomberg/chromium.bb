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
  config.used.name = feature.name;
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

 private:
  // Denotes whether the call to Load(...) should succeed or not. This impacts
  // both the ready-state and the result for the OnLoadedCallback.
  bool load_should_succeed_;

  DISALLOW_COPY_AND_ASSIGN(TestInMemoryStore);
};

class TestTimeProvider : public TimeProvider {
 public:
  TestTimeProvider() = default;
  ~TestTimeProvider() override = default;

  // TimeProvider implementation.
  uint32_t GetCurrentDay() const override { return 0u; };

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTimeProvider);
};

class FeatureEngagementTrackerImplTest : public ::testing::Test {
 public:
  FeatureEngagementTrackerImplTest() = default;

  void SetUp() override {
    std::unique_ptr<EditableConfiguration> configuration =
        base::MakeUnique<EditableConfiguration>();

    RegisterFeatureConfig(configuration.get(), kTestFeatureFoo, true);
    RegisterFeatureConfig(configuration.get(), kTestFeatureBar, true);
    RegisterFeatureConfig(configuration.get(), kTestFeatureQux, false);

    auto model = base::MakeUnique<ModelImpl>(
        CreateStore(), base::MakeUnique<NeverStorageValidator>());

    tracker_.reset(new FeatureEngagementTrackerImpl(
        std::move(model), std::move(configuration),
        base::MakeUnique<OnceConditionValidator>(),
        base::MakeUnique<TestTimeProvider>()));
  }

 protected:
  virtual std::unique_ptr<Store> CreateStore() {
    // Returns a Store that will successfully initialize.
    return base::MakeUnique<TestInMemoryStore>(true);
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<FeatureEngagementTrackerImpl> tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeatureEngagementTrackerImplTest);
};

// A top-level test class where the store fails to initialize.
class FailingInitFeatureEngagementTrackerImplTest
    : public FeatureEngagementTrackerImplTest {
 public:
  FailingInitFeatureEngagementTrackerImplTest() = default;

 protected:
  std::unique_ptr<Store> CreateStore() override {
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
  base::RunLoop().RunUntilIdle();

  // The first time a feature triggers it should be shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));

  // While in-product help is currently showing, no other features should be
  // shown.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));

  // After dismissing the current in-product help, that feature can not be shown
  // again, but a different feature should.
  tracker_->Dismissed(kTestFeatureFoo);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));

  // After dismissing the second registered feature, no more in-product help
  // should be shown, since kTestFeatureQux is invalid.
  tracker_->Dismissed(kTestFeatureBar);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
}

}  // namespace feature_engagement_tracker
