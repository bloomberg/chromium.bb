// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler.h"

#include "base/logging.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FakeSchedulerClient : public SchedulerClient {
public:
    FakeSchedulerClient() { reset(); }
    void reset()
    {
        m_actions.clear();
        m_drawWillHappen = true;
        m_swapWillHappenIfDrawHappens = true;
        m_numDraws = 0;
    }

    int numDraws() const { return m_numDraws; }
    int numActions() const { return static_cast<int>(m_actions.size()); }
    const char* action(int i) const { return m_actions[i]; }

    bool hasAction(const char* action) const
    {
        for (size_t i = 0; i < m_actions.size(); i++)
            if (!strcmp(m_actions[i], action))
                return true;
        return false;
    }

    virtual void scheduledActionBeginFrame() OVERRIDE { m_actions.push_back("scheduledActionBeginFrame"); }
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE
    {
        m_actions.push_back("scheduledActionDrawAndSwapIfPossible");
        m_numDraws++;
        return ScheduledActionDrawAndSwapResult(m_drawWillHappen, m_drawWillHappen && m_swapWillHappenIfDrawHappens);
    }

    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE
    {
        m_actions.push_back("scheduledActionDrawAndSwapForced");
        return ScheduledActionDrawAndSwapResult(true, m_swapWillHappenIfDrawHappens);
    }

    virtual void scheduledActionCommit() OVERRIDE { m_actions.push_back("scheduledActionCommit"); }
    virtual void scheduledActionCheckForCompletedTextures() OVERRIDE { m_actions.push_back("scheduledActionCheckForCompletedTextures"); }
    virtual void scheduledActionActivatePendingTreeIfNeeded() OVERRIDE { m_actions.push_back("scheduledActionActivatePendingTreeIfNeeded"); }
    virtual void scheduledActionBeginContextRecreation() OVERRIDE { m_actions.push_back("scheduledActionBeginContextRecreation"); }
    virtual void scheduledActionAcquireLayerTexturesForMainThread() OVERRIDE { m_actions.push_back("scheduledActionAcquireLayerTexturesForMainThread"); }
    virtual void didAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE { }

    void setDrawWillHappen(bool drawWillHappen) { m_drawWillHappen = drawWillHappen; }
    void setSwapWillHappenIfDrawHappens(bool swapWillHappenIfDrawHappens) { m_swapWillHappenIfDrawHappens = swapWillHappenIfDrawHappens; }

protected:
    bool m_drawWillHappen;
    bool m_swapWillHappenIfDrawHappens;
    int m_numDraws;
    std::vector<const char*> m_actions;
};

TEST(SchedulerTest, RequestCommit)
{
    FakeSchedulerClient client;
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, make_scoped_ptr(new FrameRateController(timeSource)), defaultSchedulerSettings);
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    scheduler->setCanDraw(true);

    // SetNeedsCommit should begin the frame.
    scheduler->setNeedsCommit();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(0));
    EXPECT_FALSE(timeSource->active());
    client.reset();

    // beginFrameComplete should commit
    scheduler->beginFrameComplete();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionCommit", client.action(0));
    EXPECT_TRUE(timeSource->active());
    client.reset();

    // Tick should draw.
    timeSource->tick();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionDrawAndSwapIfPossible", client.action(0));
    EXPECT_FALSE(timeSource->active());
    client.reset();

    // Timer should be off.
    EXPECT_FALSE(timeSource->active());
}

TEST(SchedulerTest, RequestCommitAfterBeginFrame)
{
    FakeSchedulerClient client;
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, make_scoped_ptr(new FrameRateController(timeSource)), defaultSchedulerSettings);
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    scheduler->setCanDraw(true);

    // SetNedsCommit should begin the frame.
    scheduler->setNeedsCommit();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(0));
    client.reset();

    // Now setNeedsCommit again. Calling here means we need a second frame.
    scheduler->setNeedsCommit();

    // Since, another commit is needed, beginFrameComplete should commit,
    // then begin another frame.
    scheduler->beginFrameComplete();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionCommit", client.action(0));
    client.reset();

    // Tick should draw but then begin another frame.
    timeSource->tick();
    EXPECT_FALSE(timeSource->active());
    EXPECT_EQ(2, client.numActions());
    EXPECT_STREQ("scheduledActionDrawAndSwapIfPossible", client.action(0));
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(1));
    client.reset();
}

TEST(SchedulerTest, TextureAcquisitionCollision)
{
    FakeSchedulerClient client;
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, make_scoped_ptr(new FrameRateController(timeSource)), defaultSchedulerSettings);
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    scheduler->setCanDraw(true);

    scheduler->setNeedsCommit();
    scheduler->setMainThreadNeedsLayerTextures();
    EXPECT_EQ(2, client.numActions());
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(0));
    EXPECT_STREQ("scheduledActionAcquireLayerTexturesForMainThread", client.action(1));
    client.reset();

    // Compositor not scheduled to draw because textures are locked by main thread
    EXPECT_FALSE(timeSource->active());

    // Trigger the commit
    scheduler->beginFrameComplete();
    EXPECT_TRUE(timeSource->active());
    client.reset();

    // Between commit and draw, texture acquisition for main thread delayed,
    // and main thread blocks.
    scheduler->setMainThreadNeedsLayerTextures();
    EXPECT_EQ(0, client.numActions());
    client.reset();

    // Once compositor draw complete, the delayed texture acquisition fires.
    timeSource->tick();
    EXPECT_EQ(3, client.numActions());
    EXPECT_STREQ("scheduledActionDrawAndSwapIfPossible", client.action(0));
    EXPECT_STREQ("scheduledActionAcquireLayerTexturesForMainThread", client.action(1));
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(2));
    client.reset();
}

TEST(SchedulerTest, VisibilitySwitchWithTextureAcquisition)
{
    FakeSchedulerClient client;
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, make_scoped_ptr(new FrameRateController(timeSource)), defaultSchedulerSettings);
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    scheduler->setCanDraw(true);

    scheduler->setNeedsCommit();
    scheduler->beginFrameComplete();
    scheduler->setMainThreadNeedsLayerTextures();
    client.reset();
    // Verify that pending texture acquisition fires when visibility
    // is lost in order to avoid a deadlock.
    scheduler->setVisible(false);
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionAcquireLayerTexturesForMainThread", client.action(0));
    client.reset();

    // Regaining visibility with textures acquired by main thread while
    // compositor is waiting for first draw should result in a request
    // for a new frame in order to escape a deadlock.
    scheduler->setVisible(true);
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(0));
    client.reset();
}

class SchedulerClientThatSetNeedsDrawInsideDraw : public FakeSchedulerClient {
public:
    SchedulerClientThatSetNeedsDrawInsideDraw()
        : m_scheduler(0) { }

    void setScheduler(Scheduler* scheduler) { m_scheduler = scheduler; }

    virtual void scheduledActionBeginFrame() OVERRIDE { }
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE
    {
        // Only setNeedsRedraw the first time this is called
        if (!m_numDraws)
            m_scheduler->setNeedsRedraw();
        return FakeSchedulerClient::scheduledActionDrawAndSwapIfPossible();
    }

    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE
    {
        NOTREACHED();
        return ScheduledActionDrawAndSwapResult(true, true);
    }

    virtual void scheduledActionCommit() OVERRIDE { }
    virtual void scheduledActionBeginContextRecreation() OVERRIDE { }
    virtual void didAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE { }

protected:
    Scheduler* m_scheduler;
};

// Tests for two different situations:
// 1. the scheduler dropping setNeedsRedraw requests that happen inside
//    a scheduledActionDrawAndSwap
// 2. the scheduler drawing twice inside a single tick
TEST(SchedulerTest, RequestRedrawInsideDraw)
{
    SchedulerClientThatSetNeedsDrawInsideDraw client;
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, make_scoped_ptr(new FrameRateController(timeSource)), defaultSchedulerSettings);
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    scheduler->setCanDraw(true);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());
    EXPECT_EQ(0, client.numDraws());

    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_FALSE(scheduler->redrawPending());
    EXPECT_FALSE(timeSource->active());
}

// Test that requesting redraw inside a failed draw doesn't lose the request.
TEST(SchedulerTest, RequestRedrawInsideFailedDraw)
{
    SchedulerClientThatSetNeedsDrawInsideDraw client;
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, make_scoped_ptr(new FrameRateController(timeSource)), defaultSchedulerSettings);
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    scheduler->setCanDraw(true);
    client.setDrawWillHappen(false);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());
    EXPECT_EQ(0, client.numDraws());

    // Fail the draw.
    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());

    // We have a commit pending and the draw failed, and we didn't lose the redraw request.
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Fail the draw again.
    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Draw successfully.
    client.setDrawWillHappen(true);
    timeSource->tick();
    EXPECT_EQ(3, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_FALSE(scheduler->redrawPending());
    EXPECT_FALSE(timeSource->active());
}

class SchedulerClientThatSetNeedsCommitInsideDraw : public FakeSchedulerClient {
public:
    SchedulerClientThatSetNeedsCommitInsideDraw()
        : m_scheduler(0) { }

    void setScheduler(Scheduler* scheduler) { m_scheduler = scheduler; }

    virtual void scheduledActionBeginFrame() OVERRIDE { }
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE
    {
        // Only setNeedsCommit the first time this is called
        if (!m_numDraws)
            m_scheduler->setNeedsCommit();
        return FakeSchedulerClient::scheduledActionDrawAndSwapIfPossible();
    }

    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE
    {
        NOTREACHED();
        return ScheduledActionDrawAndSwapResult(true, true);
    }

    virtual void scheduledActionCommit() OVERRIDE { }
    virtual void scheduledActionBeginContextRecreation() OVERRIDE { }
    virtual void didAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE { }

protected:
    Scheduler* m_scheduler;
};

// Tests for the scheduler infinite-looping on setNeedsCommit requests that
// happen inside a scheduledActionDrawAndSwap
TEST(SchedulerTest, RequestCommitInsideDraw)
{
    SchedulerClientThatSetNeedsCommitInsideDraw client;
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, make_scoped_ptr(new FrameRateController(timeSource)), defaultSchedulerSettings);
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    scheduler->setCanDraw(true);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_EQ(0, client.numDraws());
    EXPECT_TRUE(timeSource->active());

    timeSource->tick();
    EXPECT_FALSE(timeSource->active());
    EXPECT_EQ(1, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    scheduler->beginFrameComplete();

    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_FALSE(timeSource->active());
    EXPECT_FALSE(scheduler->redrawPending());
}

// Tests that when a draw fails then the pending commit should not be dropped.
TEST(SchedulerTest, RequestCommitInsideFailedDraw)
{
    SchedulerClientThatSetNeedsDrawInsideDraw client;
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, make_scoped_ptr(new FrameRateController(timeSource)), defaultSchedulerSettings);
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    scheduler->setCanDraw(true);
    client.setDrawWillHappen(false);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());
    EXPECT_EQ(0, client.numDraws());

    // Fail the draw.
    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());

    // We have a commit pending and the draw failed, and we didn't lose the commit request.
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Fail the draw again.
    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Draw successfully.
    client.setDrawWillHappen(true);
    timeSource->tick();
    EXPECT_EQ(3, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_FALSE(scheduler->redrawPending());
    EXPECT_FALSE(timeSource->active());
}

TEST(SchedulerTest, NoBeginFrameWhenDrawFails)
{
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    SchedulerClientThatSetNeedsCommitInsideDraw client;
    scoped_ptr<FakeFrameRateController> controller(new FakeFrameRateController(timeSource));
    FakeFrameRateController* controllerPtr = controller.get();
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, controller.PassAs<FrameRateController>(), defaultSchedulerSettings);
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    scheduler->setCanDraw(true);

    EXPECT_EQ(0, controllerPtr->numFramesPending());

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());
    EXPECT_EQ(0, client.numDraws());

    // Draw successfully, this starts a new frame.
    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());
    EXPECT_EQ(1, controllerPtr->numFramesPending());
    scheduler->didSwapBuffersComplete();
    EXPECT_EQ(0, controllerPtr->numFramesPending());

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Fail to draw, this should not start a frame.
    client.setDrawWillHappen(false);
    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_EQ(0, controllerPtr->numFramesPending());
}

TEST(SchedulerTest, NoBeginFrameWhenSwapFailsDuringForcedCommit)
{
    scoped_refptr<FakeTimeSource> timeSource(new FakeTimeSource());
    FakeSchedulerClient client;
    scoped_ptr<FakeFrameRateController> controller(new FakeFrameRateController(timeSource));
    FakeFrameRateController* controllerPtr = controller.get();
    SchedulerSettings defaultSchedulerSettings;
    scoped_ptr<Scheduler> scheduler = Scheduler::create(&client, controller.PassAs<FrameRateController>(), defaultSchedulerSettings);

    EXPECT_EQ(0, controllerPtr->numFramesPending());

    // Tell the client that it will fail to swap.
    client.setDrawWillHappen(true);
    client.setSwapWillHappenIfDrawHappens(false);

    // Get the compositor to do a scheduledActionDrawAndSwapForced.
    scheduler->setNeedsRedraw();
    scheduler->setNeedsForcedRedraw();
    EXPECT_TRUE(client.hasAction("scheduledActionDrawAndSwapForced"));

    // We should not have told the frame rate controller that we began a frame.
    EXPECT_EQ(0, controllerPtr->numFramesPending());
}

}  // namespace
}  // namespace cc
