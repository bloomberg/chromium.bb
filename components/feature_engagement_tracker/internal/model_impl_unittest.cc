// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/model_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement_tracker/internal/editable_configuration.h"
#include "components/feature_engagement_tracker/internal/in_memory_store.h"
#include "components/feature_engagement_tracker/internal/proto/event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {
const base::Feature kTestFeatureFoo{"test_foo",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBar{"test_bar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

void RegisterFeatureConfig(EditableConfiguration* configuration,
                           const base::Feature& feature,
                           bool valid) {
  FeatureConfig config;
  config.valid = valid;
  config.feature_used_event = feature.name;
  configuration->SetConfiguration(&feature, config);
}

void SetEventCountForDay(Event* event, uint32_t day, uint32_t count) {
  Event_Count* event_count = event->add_events();
  event_count->set_day(day);
  event_count->set_count(count);
}

// Verifies that the given |event| contains a |day| with the correct |count|,
// and that the day only exists a single time.
void VerifyEventCount(const Event& event, uint32_t day, uint32_t count) {
  bool found_day = false;
  for (int i = 0; i < event.events_size(); ++i) {
    Event_Count event_count = event.events(i);
    if (event_count.day() == day) {
      EXPECT_FALSE(found_day);
      found_day = true;
      EXPECT_EQ(count, event_count.count());
    }
  }
  EXPECT_TRUE(found_day);
}

// Verifies that the event |a| and |b| contain the exact same data.
void VerifyEqual(const Event& a, const Event& b) {
  EXPECT_EQ(a.name(), b.name());
  EXPECT_EQ(a.events_size(), b.events_size());
  for (int i = 0; i < a.events_size(); ++i) {
    VerifyEventCount(b, a.events(i).day(), a.events(i).count());
  }
}

// A test-only implementation of InMemoryStore that tracks calls to
// WriteEvent(...).
class TestInMemoryStore : public InMemoryStore {
 public:
  explicit TestInMemoryStore(std::unique_ptr<std::vector<Event>> events,
                             bool load_should_succeed)
      : InMemoryStore(std::move(events)),
        load_should_succeed_(load_should_succeed) {}

  void Load(const OnLoadedCallback& callback) override {
    HandleLoadResult(callback, load_should_succeed_);
  }

  void WriteEvent(const Event& event) override { last_written_event_ = event; }

  Event GetLastWrittenEvent() { return last_written_event_; }

 private:
  // Temporary store the last written event.
  Event last_written_event_;

  // Denotes whether the call to Load(...) should succeed or not. This impacts
  // both the ready-state and the result for the OnLoadedCallback.
  bool load_should_succeed_;
};

// Creates a TestInMemoryStore containing three hard coded events.
std::unique_ptr<TestInMemoryStore> CreatePrefilledStore() {
  std::unique_ptr<std::vector<Event>> events =
      base::MakeUnique<std::vector<Event>>();

  Event foo;
  foo.set_name("foo");
  SetEventCountForDay(&foo, 1, 1);
  events->push_back(foo);

  Event bar;
  bar.set_name("bar");
  SetEventCountForDay(&bar, 1, 3);
  SetEventCountForDay(&bar, 2, 3);
  SetEventCountForDay(&bar, 5, 5);
  events->push_back(bar);

  Event qux;
  qux.set_name("qux");
  SetEventCountForDay(&qux, 1, 5);
  SetEventCountForDay(&qux, 2, 1);
  SetEventCountForDay(&qux, 3, 2);
  events->push_back(qux);

  return base::MakeUnique<TestInMemoryStore>(std::move(events), true);
}

// A test-only implementation of ModelImpl to be able to change the current
// day while a test is running.
class TestModelImpl : public ModelImpl {
 public:
  TestModelImpl(std::unique_ptr<Store> store,
                std::unique_ptr<Configuration> configuration)
      : ModelImpl(std::move(store), std::move(configuration)),
        current_day_(0) {}
  ~TestModelImpl() override {}

  uint32_t GetCurrentDay() override { return current_day_; }

  void SetCurrentDay(uint32_t current_day) { current_day_ = current_day; }

 private:
  uint32_t current_day_;
};

class ModelImplTest : public ::testing::Test {
 public:
  ModelImplTest()
      : got_initialize_callback_(false), initialize_callback_result_(false) {}

  void SetUp() override {
    std::unique_ptr<EditableConfiguration> configuration =
        base::MakeUnique<EditableConfiguration>();

    RegisterFeatureConfig(configuration.get(), kTestFeatureFoo, true);
    RegisterFeatureConfig(configuration.get(), kTestFeatureBar, true);

    scoped_feature_list_.InitWithFeatures({kTestFeatureFoo, kTestFeatureBar},
                                          {});

    std::unique_ptr<TestInMemoryStore> store = CreateStore();
    store_ = store.get();

    model_.reset(new TestModelImpl(std::move(store), std::move(configuration)));
  }

  virtual std::unique_ptr<TestInMemoryStore> CreateStore() {
    return CreatePrefilledStore();
  }

  void OnModelInitializationFinished(bool success) {
    got_initialize_callback_ = true;
    initialize_callback_result_ = success;
  }

 protected:
  std::unique_ptr<TestModelImpl> model_;
  TestInMemoryStore* store_;
  bool got_initialize_callback_;
  bool initialize_callback_result_;

 private:
  base::MessageLoop message_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LoadFailingModelImplTest : public ModelImplTest {
 public:
  LoadFailingModelImplTest() : ModelImplTest() {}

  std::unique_ptr<TestInMemoryStore> CreateStore() override {
    return base::MakeUnique<TestInMemoryStore>(
        base::MakeUnique<std::vector<Event>>(), false);
  }
};

}  // namespace

TEST_F(ModelImplTest, InitializeShouldLoadEntries) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());
  EXPECT_TRUE(got_initialize_callback_);
  EXPECT_TRUE(initialize_callback_result_);

  // Verify that all the data matches what was put into the store in
  // CreateStore().
  Event foo_event = model_->GetEvent("foo");
  EXPECT_EQ("foo", foo_event.name());
  EXPECT_EQ(1, foo_event.events_size());
  VerifyEventCount(foo_event, 1u, 1u);

  Event bar_event = model_->GetEvent("bar");
  EXPECT_EQ("bar", bar_event.name());
  EXPECT_EQ(3, bar_event.events_size());
  VerifyEventCount(bar_event, 1u, 3u);
  VerifyEventCount(bar_event, 2u, 3u);
  VerifyEventCount(bar_event, 5u, 5u);

  Event qux_event = model_->GetEvent("qux");
  EXPECT_EQ("qux", qux_event.name());
  EXPECT_EQ(3, qux_event.events_size());
  VerifyEventCount(qux_event, 1u, 5u);
  VerifyEventCount(qux_event, 2u, 1u);
  VerifyEventCount(qux_event, 3u, 2u);
}

TEST_F(ModelImplTest, RetrievingNewEventsShouldYieldEmpty) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  Event no_event = model_->GetEvent("no");
  EXPECT_EQ("no", no_event.name());
  EXPECT_EQ(0, no_event.events_size());
  VerifyEqual(no_event, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingNonExistingEvent) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  model_->SetCurrentDay(1u);

  // Incrementing the event should work even if it does not exist.
  model_->IncrementEvent("nonexisting");
  Event event1 = model_->GetEvent("nonexisting");
  EXPECT_EQ("nonexisting", event1.name());
  EXPECT_EQ(1, event1.events_size());
  VerifyEventCount(event1, 1u, 1u);
  VerifyEqual(event1, store_->GetLastWrittenEvent());

  // Incrementing the event after it has been initialized to 1, it should now
  // have a count of 2 for the given day.
  model_->IncrementEvent("nonexisting");
  Event event2 = model_->GetEvent("nonexisting");
  Event_Count event2_count = event2.events(0);
  EXPECT_EQ(1, event2.events_size());
  VerifyEventCount(event2, 1u, 2u);
  VerifyEqual(event2, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingNonExistingEventMultipleDays) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  model_->SetCurrentDay(1u);
  model_->IncrementEvent("nonexisting");
  model_->SetCurrentDay(2u);
  model_->IncrementEvent("nonexisting");
  model_->IncrementEvent("nonexisting");
  model_->SetCurrentDay(3u);
  model_->IncrementEvent("nonexisting");
  Event event = model_->GetEvent("nonexisting");
  EXPECT_EQ(3, event.events_size());
  VerifyEventCount(event, 1u, 1u);
  VerifyEventCount(event, 2u, 2u);
  VerifyEventCount(event, 3u, 1u);
  VerifyEqual(event, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingSingleDayExistingEvent) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  model_->SetCurrentDay(1u);

  // |foo| is inserted into the store with a count of 1 at day 1.
  Event foo_event = model_->GetEvent("foo");
  EXPECT_EQ("foo", foo_event.name());
  EXPECT_EQ(1, foo_event.events_size());
  VerifyEventCount(foo_event, 1u, 1u);

  // Incrementing |foo| should change count to 2.
  model_->IncrementEvent("foo");
  Event foo_event2 = model_->GetEvent("foo");
  EXPECT_EQ(1, foo_event2.events_size());
  VerifyEventCount(foo_event2, 1u, 2u);
  VerifyEqual(foo_event2, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingSingleDayExistingEventTwice) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  model_->SetCurrentDay(1u);

  // |foo| is inserted into the store with a count of 1 at day 1, so
  // incrementing twice should lead to 3.
  model_->IncrementEvent("foo");
  model_->IncrementEvent("foo");
  Event foo_event = model_->GetEvent("foo");
  EXPECT_EQ(1, foo_event.events_size());
  VerifyEventCount(foo_event, 1u, 3u);
  VerifyEqual(foo_event, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingExistingMultiDayEvent) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |bar| is inserted into the store with a count of 3 at day 2. Incrementing
  // that day should lead to a count of 4.
  model_->SetCurrentDay(2u);
  Event bar_event = model_->GetEvent("bar");
  VerifyEventCount(bar_event, 2u, 3u);
  model_->IncrementEvent("bar");
  Event bar_event2 = model_->GetEvent("bar");
  VerifyEventCount(bar_event2, 2u, 4u);
  VerifyEqual(bar_event2, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingExistingMultiDayEventNewDay) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |bar| does not contain entries for day 10, so incrementing should create
  // the day.
  model_->SetCurrentDay(10u);
  model_->IncrementEvent("bar");
  Event bar_event = model_->GetEvent("bar");
  VerifyEventCount(bar_event, 10u, 1u);
  VerifyEqual(bar_event, store_->GetLastWrittenEvent());
  model_->IncrementEvent("bar");
  Event bar_event2 = model_->GetEvent("bar");
  VerifyEventCount(bar_event2, 10u, 2u);
  VerifyEqual(bar_event2, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, ShowState) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  EXPECT_FALSE(model_->IsCurrentlyShowing());

  model_->SetIsCurrentlyShowing(false);
  EXPECT_FALSE(model_->IsCurrentlyShowing());

  model_->SetIsCurrentlyShowing(true);
  EXPECT_TRUE(model_->IsCurrentlyShowing());

  model_->SetIsCurrentlyShowing(false);
  EXPECT_FALSE(model_->IsCurrentlyShowing());
}

TEST_F(LoadFailingModelImplTest, FailedInitializeInformsCaller) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(model_->IsReady());
  EXPECT_TRUE(got_initialize_callback_);
  EXPECT_FALSE(initialize_callback_result_);
}

}  // namespace feature_engagement_tracker
