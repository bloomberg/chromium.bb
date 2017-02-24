// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display_scheduler.h"

#include "base/logging.h"
#include "base/test/null_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/begin_frame_args.h"
#include "cc/surfaces/display.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

const int kMaxPendingSwaps = 1;

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

class FakeDisplaySchedulerClient : public DisplaySchedulerClient {
 public:
  FakeDisplaySchedulerClient() : draw_and_swap_count_(0) {}

  ~FakeDisplaySchedulerClient() override {}

  bool DrawAndSwap() override {
    draw_and_swap_count_++;
    return true;
  }

  void Reset() { draw_and_swap_count_ = 0; }

  int draw_and_swap_count() const { return draw_and_swap_count_; }

 protected:
  int draw_and_swap_count_;
};

class TestDisplayScheduler : public DisplayScheduler {
 public:
  TestDisplayScheduler(BeginFrameSource* begin_frame_source,
                       base::SingleThreadTaskRunner* task_runner,
                       int max_pending_swaps)
      : DisplayScheduler(task_runner, max_pending_swaps),
        scheduler_begin_frame_deadline_count_(0) {
    SetBeginFrameSource(begin_frame_source);
  }

  base::TimeTicks DesiredBeginFrameDeadlineTimeForTest() {
    return DesiredBeginFrameDeadlineTime();
  }

  void BeginFrameDeadlineForTest() {
    // Ensure that any missed BeginFrames were handled by the scheduler. We need
    // to run the scheduled task ourselves since the NullTaskRunner won't.
    if (!missed_begin_frame_task_.IsCancelled())
      missed_begin_frame_task_.callback().Run();
    OnBeginFrameDeadline();
  }

  void ScheduleBeginFrameDeadline() override {
    scheduler_begin_frame_deadline_count_++;
    DisplayScheduler::ScheduleBeginFrameDeadline();
  }

  int scheduler_begin_frame_deadline_count() {
    return scheduler_begin_frame_deadline_count_;
  }

  bool inside_begin_frame_deadline_interval() {
    return inside_begin_frame_deadline_interval_;
  }

 protected:
  int scheduler_begin_frame_deadline_count_;
};

class DisplaySchedulerTest : public testing::Test {
 public:
  DisplaySchedulerTest()
      : fake_begin_frame_source_(0.f, false),
        task_runner_(new base::NullTaskRunner),
        scheduler_(&fake_begin_frame_source_,
                   task_runner_.get(),
                   kMaxPendingSwaps) {
    now_src_.Advance(base::TimeDelta::FromMicroseconds(10000));
    scheduler_.SetClient(&client_);
  }

  ~DisplaySchedulerTest() override {}

  void SetUp() override { scheduler_.SetRootSurfaceResourcesLocked(false); }

  void AdvanceTimeAndBeginFrameForTest() {
    now_src_.Advance(base::TimeDelta::FromMicroseconds(10000));
    // FakeBeginFrameSource deals with |source_id| and |sequence_number|.
    BeginFrameArgs args = fake_begin_frame_source_.CreateBeginFrameArgs(
        BEGINFRAME_FROM_HERE, &now_src_);
    fake_begin_frame_source_.TestOnBeginFrame(args);
  }

 protected:
  base::SimpleTestTickClock& now_src() { return now_src_; }
  FakeDisplaySchedulerClient& client() { return client_; }
  DisplayScheduler& scheduler() { return scheduler_; }

  FakeExternalBeginFrameSource fake_begin_frame_source_;

  base::SimpleTestTickClock now_src_;
  scoped_refptr<base::NullTaskRunner> task_runner_;
  FakeDisplaySchedulerClient client_;
  TestDisplayScheduler scheduler_;
};

TEST_F(DisplaySchedulerTest, ResizeHasLateDeadlineUntilNewRootSurface) {
  SurfaceId root_surface_id1(
      kArbitraryFrameSinkId,
      LocalSurfaceId(1, base::UnguessableToken::Create()));
  SurfaceId root_surface_id2(
      kArbitraryFrameSinkId,
      LocalSurfaceId(2, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(3, base::UnguessableToken::Create()));
  base::TimeTicks late_deadline;

  scheduler_.SetVisible(true);

  // Go trough an initial BeginFrame cycle with the root surface.
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.SetNewRootSurface(root_surface_id1);
  scheduler_.BeginFrameDeadlineForTest();

  // Resize on the next begin frame cycle should cause the deadline to wait
  // for a new root surface.
  AdvanceTimeAndBeginFrameForTest();
  late_deadline = now_src().NowTicks() + BeginFrameArgs::DefaultInterval();
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_GT(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.DisplayResized();
  EXPECT_EQ(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.SetNewRootSurface(root_surface_id2);
  EXPECT_GE(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.BeginFrameDeadlineForTest();

  // Verify deadline goes back to normal after resize.
  late_deadline = now_src().NowTicks() + BeginFrameArgs::DefaultInterval();
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_GT(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.SurfaceDamaged(root_surface_id2);
  EXPECT_GE(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.BeginFrameDeadlineForTest();
}

TEST_F(DisplaySchedulerTest, ResizeHasLateDeadlineUntilDamagedSurface) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(1, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(2, base::UnguessableToken::Create()));
  base::TimeTicks late_deadline;

  scheduler_.SetVisible(true);

  // Go trough an initial BeginFrame cycle with the root surface.
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.SetNewRootSurface(root_surface_id);
  scheduler_.BeginFrameDeadlineForTest();

  // Resize on the next begin frame cycle should cause the deadline to wait
  // for a new root surface.
  AdvanceTimeAndBeginFrameForTest();
  late_deadline = now_src().NowTicks() + BeginFrameArgs::DefaultInterval();
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_GT(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.DisplayResized();
  EXPECT_EQ(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.SurfaceDamaged(root_surface_id);
  EXPECT_GE(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.BeginFrameDeadlineForTest();

  // Verify deadline goes back to normal after resize.
  AdvanceTimeAndBeginFrameForTest();
  late_deadline = now_src().NowTicks() + BeginFrameArgs::DefaultInterval();
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_GT(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.SurfaceDamaged(root_surface_id);
  EXPECT_GE(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.BeginFrameDeadlineForTest();
}

TEST_F(DisplaySchedulerTest, SurfaceDamaged) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(0, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, base::UnguessableToken::Create()));
  SurfaceId sid2(kArbitraryFrameSinkId,
                 LocalSurfaceId(2, base::UnguessableToken::Create()));

  scheduler_.SetVisible(true);

  // Set the root surface
  scheduler_.SetNewRootSurface(root_surface_id);

  // Get scheduler to detect surface 1 as active by drawing
  // two frames in a row with damage from surface 1.
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.BeginFrameDeadlineForTest();
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.BeginFrameDeadlineForTest();

  // Damage only from surface 2 (inactive) does not trigger deadline early.
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.SurfaceDamaged(sid2);
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());

  // Damage from surface 1 triggers deadline early.
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_GE(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.BeginFrameDeadlineForTest();

  // Make both surface 1 and 2 active.
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.SurfaceDamaged(sid2);
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.BeginFrameDeadlineForTest();

  // Deadline doesn't trigger early until surface 1 and 2 are both damaged.
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.SurfaceDamaged(sid2);
  EXPECT_GE(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.BeginFrameDeadlineForTest();

  // Make the system idle
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.BeginFrameDeadlineForTest();
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_FALSE(scheduler_.inside_begin_frame_deadline_interval());

  // Deadline should trigger early if child surfaces are idle and
  // we get damage on the root surface.
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_FALSE(scheduler_.inside_begin_frame_deadline_interval());
  scheduler_.SurfaceDamaged(root_surface_id);
  EXPECT_GE(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.BeginFrameDeadlineForTest();
}

TEST_F(DisplaySchedulerTest, OutputSurfaceLost) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(0, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, base::UnguessableToken::Create()));

  scheduler_.SetVisible(true);

  // Set the root surface
  scheduler_.SetNewRootSurface(root_surface_id);

  // DrawAndSwap normally.
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  EXPECT_EQ(0, client_.draw_and_swap_count());
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(1, client_.draw_and_swap_count());

  // Deadline triggers immediately on OutputSurfaceLost.
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.OutputSurfaceLost();
  EXPECT_GE(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());

  // Deadline does not DrawAndSwap after OutputSurfaceLost.
  EXPECT_EQ(1, client_.draw_and_swap_count());
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(1, client_.draw_and_swap_count());
}

TEST_F(DisplaySchedulerTest, VisibleWithoutDamageNoTicks) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(0, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, base::UnguessableToken::Create()));

  EXPECT_EQ(0u, fake_begin_frame_source_.num_observers());
  scheduler_.SetVisible(true);

  // When becoming visible, don't start listening for begin frames until there
  // is some damage.
  EXPECT_EQ(0u, fake_begin_frame_source_.num_observers());
  scheduler_.SetNewRootSurface(root_surface_id);

  EXPECT_EQ(1u, fake_begin_frame_source_.num_observers());
}

TEST_F(DisplaySchedulerTest, VisibleWithDamageTicks) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(0, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, base::UnguessableToken::Create()));

  scheduler_.SetNewRootSurface(root_surface_id);

  // When there is damage, start listening for begin frames once becoming
  // visible.
  EXPECT_EQ(0u, fake_begin_frame_source_.num_observers());
  scheduler_.SetVisible(true);

  EXPECT_EQ(1u, fake_begin_frame_source_.num_observers());
}

TEST_F(DisplaySchedulerTest, Visibility) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(0, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, base::UnguessableToken::Create()));

  scheduler_.SetNewRootSurface(root_surface_id);
  scheduler_.SetVisible(true);
  EXPECT_EQ(1u, fake_begin_frame_source_.num_observers());

  // DrawAndSwap normally.
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  EXPECT_EQ(0, client_.draw_and_swap_count());
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(1, client_.draw_and_swap_count());

  AdvanceTimeAndBeginFrameForTest();
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());

  // Become not visible.
  scheduler_.SetVisible(false);

  // It will stop listening for begin frames after the current deadline.
  EXPECT_EQ(1u, fake_begin_frame_source_.num_observers());

  // Deadline does not DrawAndSwap when not visible.
  EXPECT_EQ(1, client_.draw_and_swap_count());
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(1, client_.draw_and_swap_count());
  // Now it stops listening for begin frames.
  EXPECT_EQ(0u, fake_begin_frame_source_.num_observers());

  // Does not start listening for begin frames when becoming visible without
  // damage.
  scheduler_.SetVisible(true);
  EXPECT_EQ(0u, fake_begin_frame_source_.num_observers());
  scheduler_.SetVisible(false);

  // Does not start listening for begin frames when damage arrives.
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_EQ(0u, fake_begin_frame_source_.num_observers());

  // But does when becoming visible with damage again.
  scheduler_.SetVisible(true);
  EXPECT_EQ(1u, fake_begin_frame_source_.num_observers());
}

TEST_F(DisplaySchedulerTest, ResizeCausesSwap) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(0, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, base::UnguessableToken::Create()));

  scheduler_.SetVisible(true);

  // Set the root surface
  scheduler_.SetNewRootSurface(root_surface_id);

  // DrawAndSwap normally.
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  EXPECT_EQ(0, client_.draw_and_swap_count());
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(1, client_.draw_and_swap_count());

  scheduler_.DisplayResized();
  AdvanceTimeAndBeginFrameForTest();
  // DisplayResized should trigger a swap to happen.
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(2, client_.draw_and_swap_count());
}

TEST_F(DisplaySchedulerTest, RootSurfaceResourcesLocked) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(0, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, base::UnguessableToken::Create()));
  base::TimeTicks late_deadline;

  scheduler_.SetVisible(true);

  // Set the root surface
  scheduler_.SetNewRootSurface(root_surface_id);

  // DrawAndSwap normally.
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  EXPECT_EQ(0, client_.draw_and_swap_count());
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(1, client_.draw_and_swap_count());

  // Deadline triggers late while root resources are locked.
  AdvanceTimeAndBeginFrameForTest();
  late_deadline = now_src().NowTicks() + BeginFrameArgs::DefaultInterval();
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_GT(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.SetRootSurfaceResourcesLocked(true);
  EXPECT_EQ(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());

  // Deadline does not DrawAndSwap while root resources are locked.
  EXPECT_EQ(1, client_.draw_and_swap_count());
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(1, client_.draw_and_swap_count());

  //  Deadline triggers normally when root resources are unlocked.
  AdvanceTimeAndBeginFrameForTest();
  late_deadline = now_src().NowTicks() + BeginFrameArgs::DefaultInterval();
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_EQ(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  scheduler_.SetRootSurfaceResourcesLocked(false);
  scheduler_.SurfaceDamaged(root_surface_id);
  EXPECT_EQ(base::TimeTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());

  EXPECT_EQ(1, client_.draw_and_swap_count());
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(2, client_.draw_and_swap_count());
}

TEST_F(DisplaySchedulerTest, DidSwapBuffers) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(0, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(1, base::UnguessableToken::Create()));
  SurfaceId sid2(kArbitraryFrameSinkId,
                 LocalSurfaceId(2, base::UnguessableToken::Create()));

  scheduler_.SetVisible(true);

  // Set the root surface
  scheduler_.SetNewRootSurface(root_surface_id);

  // Get scheduler to detect surface 1 and 2 as active.
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.SurfaceDamaged(sid2);
  scheduler_.BeginFrameDeadlineForTest();
  AdvanceTimeAndBeginFrameForTest();
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.SurfaceDamaged(sid2);
  scheduler_.BeginFrameDeadlineForTest();

  // DrawAndSwap normally.
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_LT(now_src().NowTicks(),
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  EXPECT_EQ(2, client_.draw_and_swap_count());
  scheduler_.SurfaceDamaged(sid1);
  scheduler_.SurfaceDamaged(sid2);
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(3, client_.draw_and_swap_count());
  scheduler_.DidSwapBuffers();

  // Deadline triggers late when swap throttled.
  AdvanceTimeAndBeginFrameForTest();
  base::TimeTicks late_deadline =
      now_src().NowTicks() + BeginFrameArgs::DefaultInterval();
  // Damage surface 1, but not surface 2 so we avoid triggering deadline
  // early because all surfaces are ready.
  scheduler_.SurfaceDamaged(sid1);
  EXPECT_EQ(late_deadline, scheduler_.DesiredBeginFrameDeadlineTimeForTest());

  // Don't draw and swap in deadline while swap throttled.
  EXPECT_EQ(3, client_.draw_and_swap_count());
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(3, client_.draw_and_swap_count());

  // Deadline triggers normally once not swap throttled.
  // Damage from previous BeginFrame should cary over, so don't damage again.
  scheduler_.DidReceiveSwapBuffersAck();
  AdvanceTimeAndBeginFrameForTest();
  base::TimeTicks expected_deadline =
      scheduler_.LastUsedBeginFrameArgs().deadline -
      BeginFrameArgs::DefaultEstimatedParentDrawTime();
  EXPECT_EQ(expected_deadline,
            scheduler_.DesiredBeginFrameDeadlineTimeForTest());
  // Still waiting for surface 2. Once it updates, deadline should trigger
  // immediately again.
  scheduler_.SurfaceDamaged(sid2);
  EXPECT_EQ(scheduler_.DesiredBeginFrameDeadlineTimeForTest(),
            base::TimeTicks());
  // Draw and swap now that we aren't throttled.
  EXPECT_EQ(3, client_.draw_and_swap_count());
  scheduler_.BeginFrameDeadlineForTest();
  EXPECT_EQ(4, client_.draw_and_swap_count());
}

// This test verfies that we try to reschedule the deadline
// after any event that may change what deadline we want.
TEST_F(DisplaySchedulerTest, ScheduleBeginFrameDeadline) {
  SurfaceId root_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(1, base::UnguessableToken::Create()));
  SurfaceId sid1(kArbitraryFrameSinkId,
                 LocalSurfaceId(2, base::UnguessableToken::Create()));
  int count = 1;
  EXPECT_EQ(count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.SetVisible(true);
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.SetVisible(true);
  EXPECT_EQ(count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.SetVisible(false);
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  // Set the root surface while not visible.
  scheduler_.SetNewRootSurface(root_surface_id);
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.SetVisible(true);
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  // Set the root surface while visible.
  scheduler_.SetNewRootSurface(root_surface_id);
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  AdvanceTimeAndBeginFrameForTest();
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.BeginFrameDeadlineForTest();
  scheduler_.DidSwapBuffers();
  AdvanceTimeAndBeginFrameForTest();
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.DidReceiveSwapBuffersAck();
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.DisplayResized();
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.SetNewRootSurface(root_surface_id);
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.SurfaceDamaged(sid1);
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.SetRootSurfaceResourcesLocked(true);
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());

  scheduler_.OutputSurfaceLost();
  EXPECT_EQ(++count, scheduler_.scheduler_begin_frame_deadline_count());
}

}  // namespace
}  // namespace cc
