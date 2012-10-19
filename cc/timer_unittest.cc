// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTimer.h"

#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;
using namespace WebKitTests;

namespace {

class TimerTest : public testing::Test, public TimerClient {
public:
    TimerTest() : m_flag(false) { }

    void onTimerFired() { m_flag = true; }

protected:
    FakeThread m_thread;
    bool m_flag;
};

TEST_F(TimerTest, OneShot)
{
    Timer timer(&m_thread, this);
    timer.startOneShot(0.001);
    EXPECT_TRUE(timer.isActive());
    m_thread.runPendingTask();
    EXPECT_FALSE(timer.isActive());
    EXPECT_TRUE(m_flag);
    EXPECT_FALSE(m_thread.hasPendingTask());
}

TEST_F(TimerTest, StopManually)
{
    Timer timer(&m_thread, this);
    timer.startOneShot(0.001);
    EXPECT_TRUE(timer.isActive());
    timer.stop();
    EXPECT_FALSE(timer.isActive());

    m_thread.runPendingTask();
    EXPECT_FALSE(m_flag);
    EXPECT_FALSE(m_thread.hasPendingTask());
}

TEST_F(TimerTest, StopByScope)
{
    {
        Timer timer(&m_thread, this);
        timer.startOneShot(0.001);
    }

    m_thread.runPendingTask();
    EXPECT_FALSE(m_flag);
}

}
