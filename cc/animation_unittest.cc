// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation.h"

#include "cc/test/animation_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

scoped_ptr<Animation> createAnimation(int iterations, double duration)
{
    scoped_ptr<Animation> toReturn(Animation::create(make_scoped_ptr(new FakeFloatAnimationCurve(duration)).PassAs<AnimationCurve>(), 0, 1, Animation::Opacity));
    toReturn->setIterations(iterations);
    return toReturn.Pass();
}

scoped_ptr<Animation> createAnimation(int iterations)
{
    return createAnimation(iterations, 1);
}

TEST(AnimationTest, TrimTimeZeroIterations)
{
    scoped_ptr<Animation> anim(createAnimation(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
}

TEST(AnimationTest, TrimTimeOneIteration)
{
    scoped_ptr<Animation> anim(createAnimation(1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(2));
}

TEST(AnimationTest, TrimTimeInfiniteIterations)
{
    scoped_ptr<Animation> anim(createAnimation(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1.5));
}

TEST(AnimationTest, TrimTimeAlternating)
{
    scoped_ptr<Animation> anim(createAnimation(-1));
    anim->setAlternatesDirection(true);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(0.75, anim->trimTimeToCurrentIteration(1.25));
}

TEST(AnimationTest, TrimTimeStartTime)
{
    scoped_ptr<Animation> anim(createAnimation(1));
    anim->setStartTime(4);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(4));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(4.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(6));
}

TEST(AnimationTest, TrimTimeTimeOffset)
{
    scoped_ptr<Animation> anim(createAnimation(1));
    anim->setTimeOffset(4);
    anim->setStartTime(4);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
}

TEST(AnimationTest, TrimTimePauseResume)
{
    scoped_ptr<Animation> anim(createAnimation(1));
    anim->setRunState(Animation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    anim->setRunState(Animation::Paused, 0.5);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    anim->setRunState(Animation::Running, 1024);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1024.5));
}

TEST(AnimationTest, TrimTimeSuspendResume)
{
    scoped_ptr<Animation> anim(createAnimation(1));
    anim->setRunState(Animation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    anim->suspend(0.5);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    anim->resume(1024);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1024.5));
}

TEST(AnimationTest, TrimTimeZeroDuration)
{
    scoped_ptr<Animation> anim(createAnimation(0, 0));
    anim->setRunState(Animation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
}

TEST(AnimationTest, IsFinishedAtZeroIterations)
{
    scoped_ptr<Animation> anim(createAnimation(0));
    anim->setRunState(Animation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(-1));
    EXPECT_TRUE(anim->isFinishedAt(0));
    EXPECT_TRUE(anim->isFinishedAt(1));
}

TEST(AnimationTest, IsFinishedAtOneIteration)
{
    scoped_ptr<Animation> anim(createAnimation(1));
    anim->setRunState(Animation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(-1));
    EXPECT_FALSE(anim->isFinishedAt(0));
    EXPECT_TRUE(anim->isFinishedAt(1));
    EXPECT_TRUE(anim->isFinishedAt(2));
}

TEST(AnimationTest, IsFinishedAtInfiniteIterations)
{
    scoped_ptr<Animation> anim(createAnimation(-1));
    anim->setRunState(Animation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    EXPECT_FALSE(anim->isFinishedAt(0.5));
    EXPECT_FALSE(anim->isFinishedAt(1));
    EXPECT_FALSE(anim->isFinishedAt(1.5));
}

TEST(AnimationTest, IsFinishedAtNotRunning)
{
    scoped_ptr<Animation> anim(createAnimation(0));
    anim->setRunState(Animation::Running, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
    anim->setRunState(Animation::Paused, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(Animation::WaitingForNextTick, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(Animation::WaitingForTargetAvailability, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(Animation::WaitingForStartTime, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(Animation::Finished, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
    anim->setRunState(Animation::Aborted, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
}

TEST(AnimationTest, IsFinished)
{
    scoped_ptr<Animation> anim(createAnimation(1));
    anim->setRunState(Animation::Running, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::Paused, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::WaitingForNextTick, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::WaitingForTargetAvailability, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::WaitingForStartTime, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::Finished, 0);
    EXPECT_TRUE(anim->isFinished());
    anim->setRunState(Animation::Aborted, 0);
    EXPECT_TRUE(anim->isFinished());
}

TEST(AnimationTest, IsFinishedNeedsSynchronizedStartTime)
{
    scoped_ptr<Animation> anim(createAnimation(1));
    anim->setRunState(Animation::Running, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::Paused, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::WaitingForNextTick, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::WaitingForTargetAvailability, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::WaitingForStartTime, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(Animation::Finished, 0);
    EXPECT_TRUE(anim->isFinished());
    anim->setRunState(Animation::Aborted, 0);
    EXPECT_TRUE(anim->isFinished());
}

TEST(AnimationTest, RunStateChangesIgnoredWhileSuspended)
{
    scoped_ptr<Animation> anim(createAnimation(1));
    anim->suspend(0);
    EXPECT_EQ(Animation::Paused, anim->runState());
    anim->setRunState(Animation::Running, 0);
    EXPECT_EQ(Animation::Paused, anim->runState());
    anim->resume(0);
    anim->setRunState(Animation::Running, 0);
    EXPECT_EQ(Animation::Running, anim->runState());
}

}  // namespace
}  // namespace cc
