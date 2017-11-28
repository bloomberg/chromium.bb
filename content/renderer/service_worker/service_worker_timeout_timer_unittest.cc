// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_timeout_timer.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockEvent {
 public:
  base::OnceCallback<void(int)> CreateAbortCallback() {
    EXPECT_FALSE(has_aborted_);
    return base::BindOnce(&MockEvent::Abort, base::Unretained(this));
  }

  int event_id() const { return event_id_; }
  void set_event_id(int event_id) { event_id_ = event_id; }
  bool has_aborted() const { return has_aborted_; }

 private:
  void Abort(int event_id) {
    EXPECT_EQ(event_id_, event_id);
    has_aborted_ = true;
  }

  bool has_aborted_ = false;
  int event_id_ = 0;
};

}  // namespace

class ServiceWorkerTimeoutTimerTest : public testing::Test {
 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    message_loop_.SetTaskRunner(task_runner_);
  }

  void EnableServicification() {
    feature_list_.InitWithFeatures(
        {features::kBrowserSideNavigation, features::kNetworkService}, {});
    ASSERT_TRUE(ServiceWorkerUtils::IsServicificationEnabled());
  }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ServiceWorkerTimeoutTimerTest, IdleTimer) {
  EnableServicification();

  const base::TimeDelta kIdleInterval =
      ServiceWorkerTimeoutTimer::kIdleDelay +
      ServiceWorkerTimeoutTimer::kUpdateInterval +
      base::TimeDelta::FromSeconds(1);

  base::RepeatingCallback<void(int)> do_nothing_callback =
      base::BindRepeating([](int) {});

  bool is_idle = false;
  ServiceWorkerTimeoutTimer timer(
      base::BindRepeating([](bool* out_is_idle) { *out_is_idle = true; },
                          &is_idle),
      task_runner()->GetMockTickClock());
  task_runner()->FastForwardBy(kIdleInterval);
  // |idle_callback| should be fired since there is no event.
  EXPECT_TRUE(is_idle);

  is_idle = false;
  int event_id_1 = timer.StartEvent(do_nothing_callback);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  int event_id_2 = timer.StartEvent(do_nothing_callback);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there are two inflight events.
  EXPECT_FALSE(is_idle);

  timer.EndEvent(event_id_2);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  timer.EndEvent(event_id_1);
  task_runner()->FastForwardBy(kIdleInterval);
  // |idle_callback| should be fired.
  EXPECT_TRUE(is_idle);
}

TEST_F(ServiceWorkerTimeoutTimerTest, EventTimer) {
  EnableServicification();

  ServiceWorkerTimeoutTimer timer(base::BindRepeating(&base::DoNothing),
                                  task_runner()->GetMockTickClock());
  MockEvent event1, event2;

  int event_id1 = timer.StartEvent(event1.CreateAbortCallback());
  int event_id2 = timer.StartEvent(event2.CreateAbortCallback());
  event1.set_event_id(event_id1);
  event2.set_event_id(event_id2);
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_FALSE(event1.has_aborted());
  EXPECT_FALSE(event2.has_aborted());
  timer.EndEvent(event1.event_id());
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kEventTimeout +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_FALSE(event1.has_aborted());
  EXPECT_TRUE(event2.has_aborted());
}

TEST_F(ServiceWorkerTimeoutTimerTest, BecomeIdleAfterAbort) {
  EnableServicification();

  bool is_idle = false;
  ServiceWorkerTimeoutTimer timer(
      base::BindRepeating([](bool* out_is_idle) { *out_is_idle = true; },
                          &is_idle),
      task_runner()->GetMockTickClock());

  MockEvent event;
  int event_id = timer.StartEvent(event.CreateAbortCallback());
  event.set_event_id(event_id);
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kEventTimeout +
                               ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(event.has_aborted());
  EXPECT_FALSE(is_idle);
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kIdleDelay +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(is_idle);
}

TEST_F(ServiceWorkerTimeoutTimerTest, AbortAllOnDestruction) {
  EnableServicification();

  MockEvent event1, event2;
  {
    ServiceWorkerTimeoutTimer timer(base::BindRepeating(&base::DoNothing),
                                    task_runner()->GetMockTickClock());

    int event_id1 = timer.StartEvent(event1.CreateAbortCallback());
    int event_id2 = timer.StartEvent(event2.CreateAbortCallback());
    event1.set_event_id(event_id1);
    event2.set_event_id(event_id2);
    task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kUpdateInterval +
                                 base::TimeDelta::FromSeconds(1));

    EXPECT_FALSE(event1.has_aborted());
    EXPECT_FALSE(event2.has_aborted());
  }

  EXPECT_TRUE(event1.has_aborted());
  EXPECT_TRUE(event2.has_aborted());
}

TEST_F(ServiceWorkerTimeoutTimerTest, NonS13nServiceWorker) {
  ASSERT_FALSE(ServiceWorkerUtils::IsServicificationEnabled());

  MockEvent event;
  {
    bool is_idle = false;
    ServiceWorkerTimeoutTimer timer(
        base::BindRepeating([](bool* out_is_idle) { *out_is_idle = true; },
                            &is_idle),
        task_runner()->GetMockTickClock());

    int event_id = timer.StartEvent(event.CreateAbortCallback());
    event.set_event_id(event_id);
    task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kEventTimeout +
                                 ServiceWorkerTimeoutTimer::kUpdateInterval +
                                 base::TimeDelta::FromSeconds(1));

    // Timed out events  should *NOT* be aborted in non-S13nServiceWorker.
    EXPECT_FALSE(event.has_aborted());
    EXPECT_FALSE(is_idle);

    task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kIdleDelay +
                                 ServiceWorkerTimeoutTimer::kUpdateInterval +
                                 base::TimeDelta::FromSeconds(1));

    // |idle_callback| should *NOT* be fired in non-S13nServiceWorker.
    EXPECT_FALSE(is_idle);
  }

  // Events should be aborted when the timer is destructed.
  EXPECT_TRUE(event.has_aborted());
}

}  // namespace content
