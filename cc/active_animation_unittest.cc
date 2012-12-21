// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/active_animation.h"

#include "cc/test/animation_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

scoped_ptr<ActiveAnimation> createActiveAnimation(int iterations, double duration)
{
    scoped_ptr<ActiveAnimation> toReturn(ActiveAnimation::create(make_scoped_ptr(new FakeFloatAnimationCurve(duration)).PassAs<AnimationCurve>(), 0, 1, ActiveAnimation::Opacity));
    toReturn->setIterations(iterations);
    return toReturn.Pass();
}

scoped_ptr<ActiveAnimation> createActiveAnimation(int iterations)
{
    return createActiveAnimation(iterations, 1);
}

TEST(ActiveAnimationTest, TrimTimeZeroIterations)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
}

TEST(ActiveAnimationTest, TrimTimeOneIteration)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(2));
}

TEST(ActiveAnimationTest, TrimTimeInfiniteIterations)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1.5));
}

TEST(ActiveAnimationTest, TrimTimeAlternating)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(-1));
    anim->setAlternatesDirection(true);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(0.75, anim->trimTimeToCurrentIteration(1.25));
}

TEST(ActiveAnimationTest, TrimTimeStartTime)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(1));
    anim->setStartTime(4);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(4));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(4.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(6));
}

TEST(ActiveAnimationTest, TrimTimeTimeOffset)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(1));
    anim->setTimeOffset(4);
    anim->setStartTime(4);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
}

TEST(ActiveAnimationTest, TrimTimePauseResume)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    anim->setRunState(ActiveAnimation::Paused, 0.5);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    anim->setRunState(ActiveAnimation::Running, 1024);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1024.5));
}

TEST(ActiveAnimationTest, TrimTimeSuspendResume)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    anim->suspend(0.5);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    anim->resume(1024);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1024.5));
}

TEST(ActiveAnimationTest, TrimTimeZeroDuration)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(0, 0));
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
}

TEST(ActiveAnimationTest, IsFinishedAtZeroIterations)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(0));
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(-1));
    EXPECT_TRUE(anim->isFinishedAt(0));
    EXPECT_TRUE(anim->isFinishedAt(1));
}

TEST(ActiveAnimationTest, IsFinishedAtOneIteration)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(-1));
    EXPECT_FALSE(anim->isFinishedAt(0));
    EXPECT_TRUE(anim->isFinishedAt(1));
    EXPECT_TRUE(anim->isFinishedAt(2));
}

TEST(ActiveAnimationTest, IsFinishedAtInfiniteIterations)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(-1));
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    EXPECT_FALSE(anim->isFinishedAt(0.5));
    EXPECT_FALSE(anim->isFinishedAt(1));
    EXPECT_FALSE(anim->isFinishedAt(1.5));
}

TEST(ActiveAnimationTest, IsFinishedAtNotRunning)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(0));
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
    anim->setRunState(ActiveAnimation::Paused, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(ActiveAnimation::WaitingForNextTick, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(ActiveAnimation::WaitingForTargetAvailability, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(ActiveAnimation::WaitingForStartTime, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(ActiveAnimation::Finished, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
    anim->setRunState(ActiveAnimation::Aborted, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
}

TEST(ActiveAnimationTest, IsFinished)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::Paused, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::WaitingForNextTick, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::WaitingForTargetAvailability, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::WaitingForStartTime, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::Finished, 0);
    EXPECT_TRUE(anim->isFinished());
    anim->setRunState(ActiveAnimation::Aborted, 0);
    EXPECT_TRUE(anim->isFinished());
}

TEST(ActiveAnimationTest, IsFinishedNeedsSynchronizedStartTime)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(ActiveAnimation::Running, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::Paused, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::WaitingForNextTick, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::WaitingForTargetAvailability, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::WaitingForStartTime, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(ActiveAnimation::Finished, 0);
    EXPECT_TRUE(anim->isFinished());
    anim->setRunState(ActiveAnimation::Aborted, 0);
    EXPECT_TRUE(anim->isFinished());
}

TEST(ActiveAnimationTest, RunStateChangesIgnoredWhileSuspended)
{
    scoped_ptr<ActiveAnimation> anim(createActiveAnimation(1));
    anim->suspend(0);
    EXPECT_EQ(ActiveAnimation::Paused, anim->runState());
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_EQ(ActiveAnimation::Paused, anim->runState());
    anim->resume(0);
    anim->setRunState(ActiveAnimation::Running, 0);
    EXPECT_EQ(ActiveAnimation::Running, anim->runState());
}

}  // namespace
}  // namespace cc
