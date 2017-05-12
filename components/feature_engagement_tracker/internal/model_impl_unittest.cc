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
#include "components/feature_engagement_tracker/internal/never_storage_validator.h"
#include "components/feature_engagement_tracker/internal/proto/event.pb.h"
#include "components/feature_engagement_tracker/internal/test/event_util.h"
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
  config.used.name = feature.name;
  configuration->SetConfiguration(&feature, config);
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

  void WriteEvent(const Event& event) override {
    last_written_event_.reset(new Event(event));
  }

  const Event* GetLastWrittenEvent() { return last_written_event_.get(); }

 private:
  // Temporary store the last written event.
  std::unique_ptr<Event> last_written_event_;

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
  test::SetEventCountForDay(&foo, 1, 1);
  events->push_back(foo);

  Event bar;
  bar.set_name("bar");
  test::SetEventCountForDay(&bar, 1, 3);
  test::SetEventCountForDay(&bar, 2, 3);
  test::SetEventCountForDay(&bar, 5, 5);
  events->push_back(bar);

  Event qux;
  qux.set_name("qux");
  test::SetEventCountForDay(&qux, 1, 5);
  test::SetEventCountForDay(&qux, 2, 1);
  test::SetEventCountForDay(&qux, 3, 2);
  events->push_back(qux);

  return base::MakeUnique<TestInMemoryStore>(std::move(events), true);
}

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

    model_.reset(new ModelImpl(std::move(store), std::move(configuration),
                               base::MakeUnique<NeverStorageValidator>()));
  }

  virtual std::unique_ptr<TestInMemoryStore> CreateStore() {
    return CreatePrefilledStore();
  }

  void OnModelInitializationFinished(bool success) {
    got_initialize_callback_ = true;
    initialize_callback_result_ = success;
  }

 protected:
  std::unique_ptr<ModelImpl> model_;
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
  const Event* foo_event = model_->GetEvent("foo");
  EXPECT_EQ("foo", foo_event->name());
  EXPECT_EQ(1, foo_event->events_size());
  test::VerifyEventCount(foo_event, 1u, 1u);

  const Event* bar_event = model_->GetEvent("bar");
  EXPECT_EQ("bar", bar_event->name());
  EXPECT_EQ(3, bar_event->events_size());
  test::VerifyEventCount(bar_event, 1u, 3u);
  test::VerifyEventCount(bar_event, 2u, 3u);
  test::VerifyEventCount(bar_event, 5u, 5u);

  const Event* qux_event = model_->GetEvent("qux");
  EXPECT_EQ("qux", qux_event->name());
  EXPECT_EQ(3, qux_event->events_size());
  test::VerifyEventCount(qux_event, 1u, 5u);
  test::VerifyEventCount(qux_event, 2u, 1u);
  test::VerifyEventCount(qux_event, 3u, 2u);
}

TEST_F(ModelImplTest, RetrievingNewEventsShouldYieldNullptr) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  const Event* no_event = model_->GetEvent("no");
  EXPECT_EQ(nullptr, no_event);
  test::VerifyEventsEqual(nullptr, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingNonExistingEvent) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // Incrementing the event should work even if it does not exist.
  model_->IncrementEvent("nonexisting", 1u);
  const Event* event1 = model_->GetEvent("nonexisting");
  EXPECT_EQ("nonexisting", event1->name());
  EXPECT_EQ(1, event1->events_size());
  test::VerifyEventCount(event1, 1u, 1u);
  test::VerifyEventsEqual(event1, store_->GetLastWrittenEvent());

  // Incrementing the event after it has been initialized to 1, it should now
  // have a count of 2 for the given day.
  model_->IncrementEvent("nonexisting", 1u);
  const Event* event2 = model_->GetEvent("nonexisting");
  Event_Count event2_count = event2->events(0);
  EXPECT_EQ(1, event2->events_size());
  test::VerifyEventCount(event2, 1u, 2u);
  test::VerifyEventsEqual(event2, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingNonExistingEventMultipleDays) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  model_->IncrementEvent("nonexisting", 1u);
  model_->IncrementEvent("nonexisting", 2u);
  model_->IncrementEvent("nonexisting", 2u);
  model_->IncrementEvent("nonexisting", 3u);
  const Event* event = model_->GetEvent("nonexisting");
  EXPECT_EQ(3, event->events_size());
  test::VerifyEventCount(event, 1u, 1u);
  test::VerifyEventCount(event, 2u, 2u);
  test::VerifyEventCount(event, 3u, 1u);
  test::VerifyEventsEqual(event, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingSingleDayExistingEvent) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |foo| is inserted into the store with a count of 1 at day 1.
  const Event* foo_event = model_->GetEvent("foo");
  EXPECT_EQ("foo", foo_event->name());
  EXPECT_EQ(1, foo_event->events_size());
  test::VerifyEventCount(foo_event, 1u, 1u);

  // Incrementing |foo| should change count to 2.
  model_->IncrementEvent("foo", 1u);
  const Event* foo_event2 = model_->GetEvent("foo");
  EXPECT_EQ(1, foo_event2->events_size());
  test::VerifyEventCount(foo_event2, 1u, 2u);
  test::VerifyEventsEqual(foo_event2, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingSingleDayExistingEventTwice) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |foo| is inserted into the store with a count of 1 at day 1, so
  // incrementing twice should lead to 3.
  model_->IncrementEvent("foo", 1u);
  model_->IncrementEvent("foo", 1u);
  const Event* foo_event = model_->GetEvent("foo");
  EXPECT_EQ(1, foo_event->events_size());
  test::VerifyEventCount(foo_event, 1u, 3u);
  test::VerifyEventsEqual(foo_event, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingExistingMultiDayEvent) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |bar| is inserted into the store with a count of 3 at day 2. Incrementing
  // that day should lead to a count of 4.
  const Event* bar_event = model_->GetEvent("bar");
  test::VerifyEventCount(bar_event, 2u, 3u);
  model_->IncrementEvent("bar", 2u);
  const Event* bar_event2 = model_->GetEvent("bar");
  test::VerifyEventCount(bar_event2, 2u, 4u);
  test::VerifyEventsEqual(bar_event2, store_->GetLastWrittenEvent());
}

TEST_F(ModelImplTest, IncrementingExistingMultiDayEventNewDay) {
  model_->Initialize(base::Bind(&ModelImplTest::OnModelInitializationFinished,
                                base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |bar| does not contain entries for day 10, so incrementing should create
  // the day.
  model_->IncrementEvent("bar", 10u);
  const Event* bar_event = model_->GetEvent("bar");
  test::VerifyEventCount(bar_event, 10u, 1u);
  test::VerifyEventsEqual(bar_event, store_->GetLastWrittenEvent());
  model_->IncrementEvent("bar", 10u);
  const Event* bar_event2 = model_->GetEvent("bar");
  test::VerifyEventCount(bar_event2, 10u, 2u);
  test::VerifyEventsEqual(bar_event2, store_->GetLastWrittenEvent());
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
