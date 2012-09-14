// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTimer.h"

#include "CCSchedulerTestCommon.h"
#include <gtest/gtest.h>

using namespace cc;
using namespace WebKitTests;

namespace {

class CCTimerTest : public testing::Test, public CCTimerClient {
public:
    CCTimerTest() : m_flag(false) { }

    void onTimerFired() { m_flag = true; }

protected:
    FakeCCThread m_thread;
    bool m_flag;
};

TEST_F(CCTimerTest, OneShot)
{
    CCTimer timer(&m_thread, this);
    timer.startOneShot(0.001);
    EXPECT_TRUE(timer.isActive());
    m_thread.runPendingTask();
    EXPECT_FALSE(timer.isActive());
    EXPECT_TRUE(m_flag);
    EXPECT_FALSE(m_thread.hasPendingTask());
}

TEST_F(CCTimerTest, StopManually)
{
    CCTimer timer(&m_thread, this);
    timer.startOneShot(0.001);
    EXPECT_TRUE(timer.isActive());
    timer.stop();
    EXPECT_FALSE(timer.isActive());

    m_thread.runPendingTask();
    EXPECT_FALSE(m_flag);
    EXPECT_FALSE(m_thread.hasPendingTask());
}

TEST_F(CCTimerTest, StopByScope)
{
    {
        CCTimer timer(&m_thread, this);
        timer.startOneShot(0.001);
    }

    m_thread.runPendingTask();
    EXPECT_FALSE(m_flag);
}

}
