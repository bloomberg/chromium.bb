// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCActiveAnimation.h"

#include "cc/test/animation_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace WebKitTests;
using namespace cc;

namespace {

scoped_ptr<CCActiveAnimation> createActiveAnimation(int iterations, double duration)
{
    scoped_ptr<CCActiveAnimation> toReturn(CCActiveAnimation::create(make_scoped_ptr(new FakeFloatAnimationCurve(duration)).PassAs<CCAnimationCurve>(), 0, 1, CCActiveAnimation::Opacity));
    toReturn->setIterations(iterations);
    return toReturn.Pass();
}

scoped_ptr<CCActiveAnimation> createActiveAnimation(int iterations)
{
    return createActiveAnimation(iterations, 1);
}

TEST(CCActiveAnimationTest, TrimTimeZeroIterations)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
}

TEST(CCActiveAnimationTest, TrimTimeOneIteration)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(2));
}

TEST(CCActiveAnimationTest, TrimTimeInfiniteIterations)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1.5));
}

TEST(CCActiveAnimationTest, TrimTimeAlternating)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(-1));
    anim->setAlternatesDirection(true);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(0.75, anim->trimTimeToCurrentIteration(1.25));
}

TEST(CCActiveAnimationTest, TrimTimeStartTime)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setStartTime(4);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(4));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(4.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(6));
}

TEST(CCActiveAnimationTest, TrimTimeTimeOffset)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setTimeOffset(4);
    anim->setStartTime(4);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
}

TEST(CCActiveAnimationTest, TrimTimePauseResume)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    anim->setRunState(CCActiveAnimation::Paused, 0.5);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    anim->setRunState(CCActiveAnimation::Running, 1024);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1024.5));
}

TEST(CCActiveAnimationTest, TrimTimeSuspendResume)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    anim->suspend(0.5);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    anim->resume(1024);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1024.5));
}

TEST(CCActiveAnimationTest, TrimTimeZeroDuration)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(0, 0));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
}

TEST(CCActiveAnimationTest, IsFinishedAtZeroIterations)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(0));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(-1));
    EXPECT_TRUE(anim->isFinishedAt(0));
    EXPECT_TRUE(anim->isFinishedAt(1));
}

TEST(CCActiveAnimationTest, IsFinishedAtOneIteration)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(-1));
    EXPECT_FALSE(anim->isFinishedAt(0));
    EXPECT_TRUE(anim->isFinishedAt(1));
    EXPECT_TRUE(anim->isFinishedAt(2));
}

TEST(CCActiveAnimationTest, IsFinishedAtInfiniteIterations)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(-1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    EXPECT_FALSE(anim->isFinishedAt(0.5));
    EXPECT_FALSE(anim->isFinishedAt(1));
    EXPECT_FALSE(anim->isFinishedAt(1.5));
}

TEST(CCActiveAnimationTest, IsFinishedAtNotRunning)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(0));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::Paused, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::WaitingForNextTick, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::WaitingForTargetAvailability, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::Finished, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::Aborted, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
}

TEST(CCActiveAnimationTest, IsFinished)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Paused, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForNextTick, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForTargetAvailability, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Finished, 0);
    EXPECT_TRUE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Aborted, 0);
    EXPECT_TRUE(anim->isFinished());
}

TEST(CCActiveAnimationTest, IsFinishedNeedsSynchronizedStartTime)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Paused, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForNextTick, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForTargetAvailability, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForStartTime, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Finished, 0);
    EXPECT_TRUE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Aborted, 0);
    EXPECT_TRUE(anim->isFinished());
}

TEST(CCActiveAnimationTest, RunStateChangesIgnoredWhileSuspended)
{
    scoped_ptr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->suspend(0);
    EXPECT_EQ(CCActiveAnimation::Paused, anim->runState());
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_EQ(CCActiveAnimation::Paused, anim->runState());
    anim->resume(0);
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_EQ(CCActiveAnimation::Running, anim->runState());
}

} // namespace
