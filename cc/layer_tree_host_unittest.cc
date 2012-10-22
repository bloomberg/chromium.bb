// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCLayerTreeHost.h"

#include "CCGraphicsContext.h"
#include "CCThreadedTest.h"
#include "base/synchronization/lock.h"
#include "cc/content_layer.h"
#include "cc/content_layer_client.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/settings.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_web_compositor_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/texture_update_queue.h"
#include "cc/timing_function.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include <public/Platform.h>
#include <public/WebLayerScrollClient.h>
#include <public/WebSize.h>

using namespace cc;
using namespace WebKit;
using namespace WebKitTests;

namespace {

class CCLayerTreeHostTest : public CCThreadedTest { };

// Shortlived layerTreeHosts shouldn't die.
class CCLayerTreeHostTestShortlived1 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived1() { }

    virtual void beginTest() OVERRIDE
    {
        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

// Shortlived layerTreeHosts shouldn't die with a commit in flight.
class CCLayerTreeHostTestShortlived2 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived2() { }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestShortlived2)

// Shortlived layerTreeHosts shouldn't die with a redraw in flight.
class CCLayerTreeHostTestShortlived3 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived3() { }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsRedrawToMainThread();

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestShortlived3)

// Test interleaving of redraws and commits
class CCLayerTreeHostTestCommitingWithContinuousRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCommitingWithContinuousRedraw()
        : m_numCompleteCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        m_numCompleteCommits++;
        if (m_numCompleteCommits == 2)
            endTest();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDraws == 1)
          postSetNeedsCommitToMainThread();
        m_numDraws++;
        postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCompleteCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestCommitingWithContinuousRedraw, runMultiThread)
{
    runTest(true);
}

// Two setNeedsCommits in a row should lead to at least 1 commit and at least 1
// draw with frame 0.
class CCLayerTreeHostTestSetNeedsCommit1 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsCommit1()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDraws++;
        if (!impl->sourceFrameNumber())
            endTest();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_GE(1, m_numCommits);
        EXPECT_GE(1, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestSetNeedsCommit1, DISABLED_runMultiThread)
{
    runTest(true);
}

// A setNeedsCommit should lead to 1 commit. Issuing a second commit after that
// first committed frame draws should lead to another commit.
class CCLayerTreeHostTestSetNeedsCommit2 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsCommit2()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        if (!impl->sourceFrameNumber())
            postSetNeedsCommitToMainThread();
        else if (impl->sourceFrameNumber() == 1)
            endTest();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_numCommits);
        EXPECT_GE(2, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

#if OS(WINDOWS)
// http://webkit.org/b/74623
TEST_F(CCLayerTreeHostTestSetNeedsCommit2, FLAKY_runMultiThread)
#else
TEST_F(CCLayerTreeHostTestSetNeedsCommit2, runMultiThread)
#endif
{
    runTest(true);
}

// 1 setNeedsRedraw after the first commit has completed should lead to 1
// additional draw.
class CCLayerTreeHostTestSetNeedsRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_EQ(0, impl->sourceFrameNumber());
        if (!m_numDraws)
            postSetNeedsRedrawToMainThread(); // Redraw again to verify that the second redraw doesn't commit.
        else
            endTest();
        m_numDraws++;
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        EXPECT_EQ(0, m_numDraws);
        m_numCommits++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_GE(2, m_numDraws);
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestSetNeedsRedraw, runMultiThread)
{
    runTest(true);
}

// If the layerTreeHost says it can't draw, then we should not try to draw.
class CCLayerTreeHostTestCanDrawBlocksDrawing : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCanDrawBlocksDrawing()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        // Only the initial draw should bring us here.
        EXPECT_TRUE(impl->canDraw());
        EXPECT_EQ(0, impl->sourceFrameNumber());
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        if (m_numCommits >= 1) {
            // After the first commit, we should not be able to draw.
            EXPECT_FALSE(impl->canDraw());
        }
    }

    virtual void didCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            // Make the viewport empty so the host says it can't draw.
            m_layerTreeHost->setViewportSize(IntSize(0, 0), IntSize(0, 0));

            scoped_array<char> pixels(new char[4]);
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(pixels.get()), IntRect(0, 0, 1, 1));
        } else if (m_numCommits == 2) {
            m_layerTreeHost->setNeedsRedraw();
            m_layerTreeHost->setNeedsCommit();
        } else
            endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestCanDrawBlocksDrawing)

// beginLayerWrite should prevent draws from executing until a commit occurs
class CCLayerTreeHostTestWriteLayersRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestWriteLayersRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postAcquireLayerTextures();
        postSetNeedsRedrawToMainThread(); // should be inhibited without blocking
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDraws++;
        EXPECT_EQ(m_numDraws, m_numCommits);
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestWriteLayersRedraw, runMultiThread)
{
    runTest(true);
}

// Verify that when resuming visibility, requesting layer write permission
// will not deadlock the main thread even though there are not yet any
// scheduled redraws. This behavior is critical for reliably surviving tab
// switching. There are no failure conditions to this test, it just passes
// by not timing out.
class CCLayerTreeHostTestWriteLayersAfterVisible : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestWriteLayersAfterVisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 2)
            endTest();
        else {
            postSetVisibleToMainThread(false);
            postSetVisibleToMainThread(true);
            postAcquireLayerTextures();
            postSetNeedsCommitToMainThread();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestWriteLayersAfterVisible, runMultiThread)
{
    runTest(true);
}

// A compositeAndReadback while invisible should force a normal commit without assertion.
class CCLayerTreeHostTestCompositeAndReadbackWhileInvisible : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCompositeAndReadbackWhileInvisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            m_layerTreeHost->setVisible(false);
            m_layerTreeHost->setNeedsCommit();
            m_layerTreeHost->setNeedsCommit();
            scoped_array<char> pixels(new char[4]);
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(pixels.get()), IntRect(0, 0, 1, 1));
        } else
            endTest();

    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestCompositeAndReadbackWhileInvisible, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestAbortFrameWhenInvisible : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAbortFrameWhenInvisible()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        // Request a commit (from the main thread), which will trigger the commit flow from the impl side.
        m_layerTreeHost->setNeedsCommit();
        // Then mark ourselves as not visible before processing any more messages on the main thread.
        m_layerTreeHost->setVisible(false);
        // If we make it without kicking a frame, we pass!
        endTestAfterDelay(1);
    }

    virtual void layout() OVERRIDE
    {
        ASSERT_FALSE(true);
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
};

TEST_F(CCLayerTreeHostTestAbortFrameWhenInvisible, runMultiThread)
{
    runTest(true);
}

// Makes sure that setNedsAnimate does not cause the commitRequested() state to be set.
class CCLayerTreeHostTestSetNeedsAnimateShouldNotSetCommitRequested : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsAnimateShouldNotSetCommitRequested()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        // The tests start up with a commit pending because we give them a root layer.
        // We need to wait for the commit to happen before doing anything.
        EXPECT_TRUE(m_layerTreeHost->commitRequested());
    }

    virtual void animate(double monotonicTime) OVERRIDE
    {
        // We skip the first commit becasue its the commit that populates the
        // impl thread with a tree.
        if (!m_numCommits)
            return;

        m_layerTreeHost->setNeedsAnimate();
        // Right now, commitRequested is going to be true, because during
        // beginFrame, we force commitRequested to true to prevent requests from
        // hitting the impl thread. But, when the next didCommit happens, we should
        // verify that commitRequested has gone back to false.
    }
    virtual void didCommit() OVERRIDE
    {
        if (!m_numCommits) {
            EXPECT_FALSE(m_layerTreeHost->commitRequested());
            m_layerTreeHost->setNeedsAnimate();
            EXPECT_FALSE(m_layerTreeHost->commitRequested());
            m_numCommits++;
        }

        // Verifies that the setNeedsAnimate we made in ::animate did not
        // trigger commitRequested.
        EXPECT_FALSE(m_layerTreeHost->commitRequested());
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestSetNeedsAnimateShouldNotSetCommitRequested, runMultiThread)
{
    runTest(true);
}



// Trigger a frame with setNeedsCommit. Then, inside the resulting animate
// callback, requet another frame using setNeedsAnimate. End the test when
// animate gets called yet-again, indicating that the proxy is correctly
// handling the case where setNeedsAnimate() is called inside the begin frame
// flow.
class CCLayerTreeHostTestSetNeedsAnimateInsideAnimationCallback : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsAnimateInsideAnimationCallback()
        : m_numAnimates(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsAnimateToMainThread();
    }

    virtual void animate(double) OVERRIDE
    {
        if (!m_numAnimates) {
            m_layerTreeHost->setNeedsAnimate();
            m_numAnimates++;
            return;
        }
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numAnimates;
};

TEST_F(CCLayerTreeHostTestSetNeedsAnimateInsideAnimationCallback, runMultiThread)
{
    runTest(true);
}

// Add a layer animation and confirm that CCLayerTreeHostImpl::animateLayers does get
// called and continues to get called.
class CCLayerTreeHostTestAddAnimation : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAddAnimation()
        : m_numAnimates(0)
        , m_receivedAnimationStartedNotification(false)
        , m_startTime(0)
        , m_firstMonotonicTime(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postAddInstantAnimationToMainThread();
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime) OVERRIDE
    {
        if (!m_numAnimates) {
            // The animation had zero duration so layerTreeHostImpl should no
            // longer need to animate its layers.
            EXPECT_FALSE(layerTreeHostImpl->needsAnimateLayers());
            m_numAnimates++;
            m_firstMonotonicTime = monotonicTime;
            return;
        }
        EXPECT_LT(0, m_startTime);
        EXPECT_LT(0, m_firstMonotonicTime);
        EXPECT_NE(m_startTime, m_firstMonotonicTime);
        EXPECT_TRUE(m_receivedAnimationStartedNotification);
        endTest();
    }

    virtual void notifyAnimationStarted(double wallClockTime) OVERRIDE
    {
        m_receivedAnimationStartedNotification = true;
        m_startTime = wallClockTime;
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numAnimates;
    bool m_receivedAnimationStartedNotification;
    double m_startTime;
    double m_firstMonotonicTime;
};

TEST_F(CCLayerTreeHostTestAddAnimation, runMultiThread)
{
    runTest(true);
}

// Add a layer animation to a layer, but continually fail to draw. Confirm that after
// a while, we do eventually force a draw.
class CCLayerTreeHostTestCheckerboardDoesNotStarveDraws : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCheckerboardDoesNotStarveDraws()
        : m_startedAnimating(false)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postAddAnimationToMainThread();
    }

    virtual void afterTest() OVERRIDE
    {
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime) OVERRIDE
    {
        m_startedAnimating = true;
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        if (m_startedAnimating)
            endTest();
    }

    virtual bool prepareToDrawOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        return false;
    }

private:
    bool m_startedAnimating;
};

// Starvation can only be an issue with the MT compositor.
TEST_F(CCLayerTreeHostTestCheckerboardDoesNotStarveDraws, runMultiThread)
{
    runTest(true);
}

// Ensures that animations continue to be ticked when we are backgrounded.
class CCLayerTreeHostTestTickAnimationWhileBackgrounded : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestTickAnimationWhileBackgrounded()
        : m_numAnimates(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postAddAnimationToMainThread();
    }

    // Use willAnimateLayers to set visible false before the animation runs and
    // causes a commit, so we block the second visible animate in single-thread
    // mode.
    virtual void willAnimateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime) OVERRIDE
    {
        if (m_numAnimates < 2) {
            if (!m_numAnimates) {
                // We have a long animation running. It should continue to tick even if we are not visible.
                postSetVisibleToMainThread(false);
            }
            m_numAnimates++;
            return;
        }
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numAnimates;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestTickAnimationWhileBackgrounded)

// Ensures that animations continue to be ticked when we are backgrounded.
class CCLayerTreeHostTestAddAnimationWithTimingFunction : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAddAnimationWithTimingFunction()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postAddAnimationToMainThread();
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime) OVERRIDE
    {
        const CCActiveAnimation* animation = m_layerTreeHost->rootLayer()->layerAnimationController()->getActiveAnimation(0, CCActiveAnimation::Opacity);
        if (!animation)
            return;
        const CCFloatAnimationCurve* curve = animation->curve()->toFloatAnimationCurve();
        float startOpacity = curve->getValue(0);
        float endOpacity = curve->getValue(curve->duration());
        float linearlyInterpolatedOpacity = 0.25 * endOpacity + 0.75 * startOpacity;
        double time = curve->duration() * 0.25;
        // If the linear timing function associated with this animation was not picked up,
        // then the linearly interpolated opacity would be different because of the
        // default ease timing function.
        EXPECT_FLOAT_EQ(linearlyInterpolatedOpacity, curve->getValue(time));
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestAddAnimationWithTimingFunction)

// Ensures that when opacity is being animated, this value does not cause the subtree to be skipped.
class CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setDrawOpacity(1);
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));
        m_layerTreeHost->rootLayer()->setOpacity(0);
        postAddAnimationToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        // If the subtree was skipped when preparing to draw, the layer's draw opacity
        // will not have been updated. It should be set to 0 due to the animation.
        // Without the animation, the layer will be skipped since it has zero opacity.
        EXPECT_EQ(0, m_layerTreeHost->rootLayer()->drawOpacity());
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

#if OS(WINDOWS)
// http://webkit.org/b/74623
TEST_F(CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity, FLAKY_runMultiThread)
#else
TEST_F(CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity, runMultiThread)
#endif
{
    runTest(true);
}

// Ensures that main thread animations have their start times synchronized with impl thread animations.
class CCLayerTreeHostTestSynchronizeAnimationStartTimes : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSynchronizeAnimationStartTimes()
        : m_layerTreeHostImpl(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postAddAnimationToMainThread();
    }

    // This is guaranteed to be called before CCLayerTreeHostImpl::animateLayers.
    virtual void willAnimateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime) OVERRIDE
    {
        m_layerTreeHostImpl = layerTreeHostImpl;
    }

    virtual void notifyAnimationStarted(double time) OVERRIDE
    {
        EXPECT_TRUE(m_layerTreeHostImpl);

        CCLayerAnimationController* controllerImpl = m_layerTreeHostImpl->rootLayer()->layerAnimationController();
        CCLayerAnimationController* controller = m_layerTreeHost->rootLayer()->layerAnimationController();
        CCActiveAnimation* animationImpl = controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity);
        CCActiveAnimation* animation = controller->getActiveAnimation(0, CCActiveAnimation::Opacity);

        EXPECT_EQ(animationImpl->startTime(), animation->startTime());

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    CCLayerTreeHostImpl* m_layerTreeHostImpl;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestSynchronizeAnimationStartTimes)

// Ensures that main thread animations have their start times synchronized with impl thread animations.
class CCLayerTreeHostTestAnimationFinishedEvents : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAnimationFinishedEvents()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postAddInstantAnimationToMainThread();
    }

    virtual void notifyAnimationFinished(double time) OVERRIDE
    {
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestAnimationFinishedEvents)

class CCLayerTreeHostTestScrollSimple : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestScrollSimple()
        : m_initialScroll(IntPoint(10, 20))
        , m_secondScroll(IntPoint(40, 5))
        , m_scrollAmount(2, -1)
        , m_scrolls(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollPosition(m_initialScroll);
        postSetNeedsCommitToMainThread();
    }

    virtual void layout() OVERRIDE
    {
        LayerChromium* root = m_layerTreeHost->rootLayer();
        if (!m_layerTreeHost->commitNumber())
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
        else {
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount);

            // Pretend like Javascript updated the scroll position itself.
            root->setScrollPosition(m_secondScroll);
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        CCLayerImpl* root = impl->rootLayer();
        EXPECT_EQ(root->scrollDelta(), IntSize());

        root->setScrollable(true);
        root->setMaxScrollPosition(IntSize(100, 100));
        root->scrollBy(m_scrollAmount);

        if (!impl->sourceFrameNumber()) {
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            postSetNeedsCommitToMainThread();
        } else if (impl->sourceFrameNumber() == 1) {
            EXPECT_EQ(root->scrollPosition(), m_secondScroll);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale) OVERRIDE
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
        m_scrolls++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_scrolls);
    }
private:
    IntPoint m_initialScroll;
    IntPoint m_secondScroll;
    IntSize m_scrollAmount;
    int m_scrolls;
};

TEST_F(CCLayerTreeHostTestScrollSimple, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestScrollMultipleRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestScrollMultipleRedraw()
        : m_initialScroll(IntPoint(40, 10))
        , m_scrollAmount(-3, 17)
        , m_scrolls(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollPosition(m_initialScroll);
        postSetNeedsCommitToMainThread();
    }

    virtual void beginCommitOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        LayerChromium* root = m_layerTreeHost->rootLayer();
        if (!m_layerTreeHost->commitNumber())
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
        else if (m_layerTreeHost->commitNumber() == 1)
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
        else if (m_layerTreeHost->commitNumber() == 2)
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        CCLayerImpl* root = impl->rootLayer();
        root->setScrollable(true);
        root->setMaxScrollPosition(IntSize(100, 100));

        if (!impl->sourceFrameNumber() && impl->sourceAnimationFrameNumber() == 1) {
            // First draw after first commit.
            EXPECT_EQ(root->scrollDelta(), IntSize());
            root->scrollBy(m_scrollAmount);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);

            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            postSetNeedsRedrawToMainThread();
        } else if (!impl->sourceFrameNumber() && impl->sourceAnimationFrameNumber() == 2) {
            // Second draw after first commit.
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            root->scrollBy(m_scrollAmount);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount + m_scrollAmount);

            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            postSetNeedsCommitToMainThread();
        } else if (impl->sourceFrameNumber() == 1) {
            // Third or later draw after second commit.
            EXPECT_GE(impl->sourceAnimationFrameNumber(), 3);
            EXPECT_EQ(root->scrollDelta(), IntSize());
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale) OVERRIDE
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
        m_scrolls++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_scrolls);
    }
private:
    IntPoint m_initialScroll;
    IntSize m_scrollAmount;
    int m_scrolls;
};

TEST_F(CCLayerTreeHostTestScrollMultipleRedraw, runMultiThread)
{
    runTest(true);
}

// This test verifies that properties on the layer tree host are commited to the impl side.
class CCLayerTreeHostTestCommit : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestCommit() { }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(IntSize(20, 20), IntSize(20, 20));
        m_layerTreeHost->setBackgroundColor(SK_ColorGRAY);
        m_layerTreeHost->setPageScaleFactorAndLimits(5, 5, 5);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_EQ(IntSize(20, 20), impl->layoutViewportSize());
        EXPECT_EQ(SK_ColorGRAY, impl->backgroundColor());
        EXPECT_EQ(5, impl->pageScaleFactor());

        endTest();
    }

    virtual void afterTest() OVERRIDE { }
};

TEST_F(CCLayerTreeHostTestCommit, runTest)
{
    runTest(true);
}

// Verifies that startPageScaleAnimation events propagate correctly from CCLayerTreeHost to
// CCLayerTreeHostImpl in the MT compositor.
class CCLayerTreeHostTestStartPageScaleAnimation : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestStartPageScaleAnimation()
        : m_animationRequested(false)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollPosition(IntPoint());
        postSetNeedsRedrawToMainThread();
    }

    void requestStartPageScaleAnimation()
    {
        layerTreeHost()->startPageScaleAnimation(IntSize(), false, 1.25, 0);
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        impl->rootLayer()->setScrollable(true);
        impl->rootLayer()->setScrollPosition(IntPoint());
        impl->setPageScaleFactorAndLimits(impl->pageScaleFactor(), 0.5, 2);

        // We request animation only once.
        if (!m_animationRequested) {
            m_mainThreadProxy->postTask(createCCThreadTask(this, &CCLayerTreeHostTestStartPageScaleAnimation::requestStartPageScaleAnimation));
            m_animationRequested = true;
        }
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale) OVERRIDE
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
        m_layerTreeHost->setPageScaleFactorAndLimits(scale, 0.5, 2);
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        impl->processScrollDeltas();
        // We get one commit before the first draw, and the animation doesn't happen until the second draw.
        if (impl->sourceFrameNumber() == 1) {
            EXPECT_EQ(1.25, impl->pageScaleFactor());
            endTest();
        } else
            postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    bool m_animationRequested;
};

TEST_F(CCLayerTreeHostTestStartPageScaleAnimation, runTest)
{
    runTest(true);
}

class CCLayerTreeHostTestSetVisible : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestSetVisible()
        : m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetVisibleToMainThread(false);
        postSetNeedsRedrawToMainThread(); // This is suppressed while we're invisible.
        postSetVisibleToMainThread(true); // Triggers the redraw.
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_TRUE(impl->visible());
        ++m_numDraws;
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numDraws);
    }

private:
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestSetVisible, runMultiThread)
{
    runTest(true);
}

class TestOpacityChangeLayerDelegate : public ContentLayerChromiumClient {
public:
    TestOpacityChangeLayerDelegate(CCLayerTreeHostTest* test)
        : m_test(test)
    {
    }

    virtual void paintContents(SkCanvas*, const IntRect&, FloatRect&) OVERRIDE
    {
        // Set layer opacity to 0.
        m_test->layerTreeHost()->rootLayer()->setOpacity(0);
    }

private:
    CCLayerTreeHostTest* m_test;
};

class ContentLayerChromiumWithUpdateTracking : public ContentLayerChromium {
public:
    static scoped_refptr<ContentLayerChromiumWithUpdateTracking> create(ContentLayerChromiumClient* client) { return make_scoped_refptr(new ContentLayerChromiumWithUpdateTracking(client)); }

    int paintContentsCount() { return m_paintContentsCount; }
    void resetPaintContentsCount() { m_paintContentsCount = 0; }

    virtual void update(CCTextureUpdateQueue& queue, const CCOcclusionTracker* occlusion, CCRenderingStats& stats) OVERRIDE
    {
        ContentLayerChromium::update(queue, occlusion, stats);
        m_paintContentsCount++;
    }

private:
    explicit ContentLayerChromiumWithUpdateTracking(ContentLayerChromiumClient* client)
        : ContentLayerChromium(client)
        , m_paintContentsCount(0)
    {
        setAnchorPoint(FloatPoint(0, 0));
        setBounds(IntSize(10, 10));
        setIsDrawable(true);
    }
    virtual ~ContentLayerChromiumWithUpdateTracking()
    {
    }

    int m_paintContentsCount;
};

// Layer opacity change during paint should not prevent compositor resources from being updated during commit.
class CCLayerTreeHostTestOpacityChange : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestOpacityChange()
        : m_testOpacityChangeDelegate(this)
        , m_updateCheckLayer(ContentLayerChromiumWithUpdateTracking::create(&m_testOpacityChangeDelegate))
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_updateCheckLayer);
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // update() should have been called once.
        EXPECT_EQ(1, m_updateCheckLayer->paintContentsCount());

        // clear m_updateCheckLayer so CCLayerTreeHost dies.
        m_updateCheckLayer = NULL;
    }

private:
    TestOpacityChangeLayerDelegate m_testOpacityChangeDelegate;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_updateCheckLayer;
};

TEST_F(CCLayerTreeHostTestOpacityChange, runMultiThread)
{
    runTest(true);
}

class MockContentLayerChromiumClient : public ContentLayerChromiumClient {
public:
    bool drawsContent() const { return true; }
    MOCK_CONST_METHOD0(preserves3D, bool());
    void paintContents(SkCanvas*, const IntRect&, FloatRect&) OVERRIDE { }
    void notifySyncRequired() { }
};

class NoScaleContentLayerChromium : public ContentLayerChromium {
public:
    static scoped_refptr<NoScaleContentLayerChromium> create(ContentLayerChromiumClient* client) { return make_scoped_refptr(new NoScaleContentLayerChromium(client)); }

    virtual bool needsContentsScale() const OVERRIDE { return false; }

private:
    explicit NoScaleContentLayerChromium(ContentLayerChromiumClient* client)
        : ContentLayerChromium(client) { }
    virtual ~NoScaleContentLayerChromium() { }
};

class CCLayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers()
        : m_rootLayer(NoScaleContentLayerChromium::create(&m_client))
        , m_childLayer(ContentLayerChromium::create(&m_client))
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(IntSize(40, 40), IntSize(60, 60));
        m_layerTreeHost->setDeviceScaleFactor(1.5);
        EXPECT_EQ(IntSize(40, 40), m_layerTreeHost->layoutViewportSize());
        EXPECT_EQ(IntSize(60, 60), m_layerTreeHost->deviceViewportSize());

        m_rootLayer->addChild(m_childLayer);

        m_rootLayer->setIsDrawable(true);
        m_rootLayer->setBounds(IntSize(30, 30));
        m_rootLayer->setAnchorPoint(FloatPoint(0, 0));

        m_childLayer->setIsDrawable(true);
        m_childLayer->setPosition(IntPoint(2, 2));
        m_childLayer->setBounds(IntSize(10, 10));
        m_childLayer->setAnchorPoint(FloatPoint(0, 0));

        m_layerTreeHost->setRootLayer(m_rootLayer);
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        // Get access to protected methods.
        MockLayerTreeHostImpl* mockImpl = static_cast<MockLayerTreeHostImpl*>(impl);

        // Should only do one commit.
        EXPECT_EQ(0, impl->sourceFrameNumber());
        // Device scale factor should come over to impl.
        EXPECT_NEAR(impl->deviceScaleFactor(), 1.5, 0.00001);

        // Both layers are on impl.
        ASSERT_EQ(1u, impl->rootLayer()->children().size());

        // Device viewport is scaled.
        EXPECT_EQ(IntSize(40, 40), impl->layoutViewportSize());
        EXPECT_EQ(IntSize(60, 60), impl->deviceViewportSize());

        CCLayerImpl* root = impl->rootLayer();
        CCLayerImpl* child = impl->rootLayer()->children()[0];

        // Positions remain in layout pixels.
        EXPECT_EQ(IntPoint(0, 0), root->position());
        EXPECT_EQ(IntPoint(2, 2), child->position());

        // Compute all the layer transforms for the frame.
        MockLayerTreeHostImpl::CCLayerList renderSurfaceLayerList;
        mockImpl->calculateRenderSurfaceLayerList(renderSurfaceLayerList);

        // Both layers should be drawing into the root render surface.
        ASSERT_EQ(1u, renderSurfaceLayerList.size());
        ASSERT_EQ(root->renderSurface(), renderSurfaceLayerList[0]->renderSurface());
        ASSERT_EQ(2u, root->renderSurface()->layerList().size());

        // The root render surface is the size of the viewport.
        EXPECT_RECT_EQ(IntRect(0, 0, 60, 60), root->renderSurface()->contentRect());

        // The content bounds of the child should be scaled.
        IntSize childBoundsScaled = child->bounds();
        childBoundsScaled.scale(1.5);
        EXPECT_EQ(childBoundsScaled, child->contentBounds());

        WebTransformationMatrix scaleTransform;
        scaleTransform.scale(impl->deviceScaleFactor());

        // The root layer is scaled by 2x.
        WebTransformationMatrix rootScreenSpaceTransform = scaleTransform;
        WebTransformationMatrix rootDrawTransform = scaleTransform;

        EXPECT_EQ(rootDrawTransform, root->drawTransform());
        EXPECT_EQ(rootScreenSpaceTransform, root->screenSpaceTransform());

        // The child is at position 2,2, which is transformed to 3,3 after the scale
        WebTransformationMatrix childScreenSpaceTransform;
        childScreenSpaceTransform.translate(3, 3);
        WebTransformationMatrix childDrawTransform = childScreenSpaceTransform;

        EXPECT_EQ(childDrawTransform, child->drawTransform());
        EXPECT_EQ(childScreenSpaceTransform, child->screenSpaceTransform());

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        m_rootLayer = NULL;
        m_childLayer = NULL;
    }

private:
    MockContentLayerChromiumClient m_client;
    scoped_refptr<NoScaleContentLayerChromium> m_rootLayer;
    scoped_refptr<ContentLayerChromium> m_childLayer;
};

// Test is flaky - http://crbug.com/148490
TEST_F(CCLayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers, DISABLED_runMultiThread)
{
    runTest(true);
}

// Verify atomicity of commits and reuse of textures.
class CCLayerTreeHostTestAtomicCommit : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAtomicCommit()
        : m_layer(ContentLayerChromiumWithUpdateTracking::create(&m_client))
    {
        // Make sure partial texture updates are turned off.
        m_settings.maxPartialTextureUpdates = 0;
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));

        postSetNeedsCommitToMainThread();
        postSetNeedsRedrawToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(impl->context()->context3D());

        switch (impl->sourceFrameNumber()) {
        case 0:
            // Number of textures should be one.
            ASSERT_EQ(1, context->numTextures());
            // Number of textures used for commit should be one.
            EXPECT_EQ(1, context->numUsedTextures());
            // Verify that used texture is correct.
            EXPECT_TRUE(context->usedTexture(context->texture(0)));

            context->resetUsedTextures();
            break;
        case 1:
            // Number of textures should be two as the first texture
            // is used by impl thread and cannot by used for update.
            ASSERT_EQ(2, context->numTextures());
            // Number of textures used for commit should still be one.
            EXPECT_EQ(1, context->numUsedTextures());
            // First texture should not have been used.
            EXPECT_FALSE(context->usedTexture(context->texture(0)));
            // New texture should have been used.
            EXPECT_TRUE(context->usedTexture(context->texture(1)));

            context->resetUsedTextures();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(impl->context()->context3D());

        // Number of textures used for draw should always be one.
        EXPECT_EQ(1, context->numUsedTextures());

        if (impl->sourceFrameNumber() < 1) {
            context->resetUsedTextures();
            postSetNeedsAnimateAndCommitToMainThread();
            postSetNeedsRedrawToMainThread();
        } else
            endTest();
    }

    virtual void layout() OVERRIDE
    {
        m_layer->setNeedsDisplay();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    MockContentLayerChromiumClient m_client;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_layer;
};

TEST_F(CCLayerTreeHostTestAtomicCommit, runMultiThread)
{
    runTest(true);
}

static void setLayerPropertiesForTesting(LayerChromium* layer, LayerChromium* parent, const WebTransformationMatrix& transform, const FloatPoint& anchor, const FloatPoint& position, const IntSize& bounds, bool opaque)
{
    layer->removeAllChildren();
    if (parent)
        parent->addChild(layer);
    layer->setTransform(transform);
    layer->setAnchorPoint(anchor);
    layer->setPosition(position);
    layer->setBounds(bounds);
    layer->setContentsOpaque(opaque);
}

class CCLayerTreeHostTestAtomicCommitWithPartialUpdate : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAtomicCommitWithPartialUpdate()
        : m_parent(ContentLayerChromiumWithUpdateTracking::create(&m_client))
        , m_child(ContentLayerChromiumWithUpdateTracking::create(&m_client))
        , m_numCommits(0)
    {
        // Allow one partial texture update.
        m_settings.maxPartialTextureUpdates = 1;
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_parent);
        m_layerTreeHost->setViewportSize(IntSize(10, 20), IntSize(10, 20));

        WebTransformationMatrix identityMatrix;
        setLayerPropertiesForTesting(m_parent.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 20), true);
        setLayerPropertiesForTesting(m_child.get(), m_parent.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(0, 10), IntSize(10, 10), false);

        postSetNeedsCommitToMainThread();
        postSetNeedsRedrawToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(impl->context()->context3D());

        switch (impl->sourceFrameNumber()) {
        case 0:
            // Number of textures should be two.
            ASSERT_EQ(2, context->numTextures());
            // Number of textures used for commit should be two.
            EXPECT_EQ(2, context->numUsedTextures());
            // Verify that used textures are correct.
            EXPECT_TRUE(context->usedTexture(context->texture(0)));
            EXPECT_TRUE(context->usedTexture(context->texture(1)));

            context->resetUsedTextures();
            break;
        case 1:
            // Number of textures used for commit should still be two.
            EXPECT_EQ(2, context->numUsedTextures());
            // First two textures should not have been used.
            EXPECT_FALSE(context->usedTexture(context->texture(0)));
            EXPECT_FALSE(context->usedTexture(context->texture(1)));
            // New textures should have been used.
            EXPECT_TRUE(context->usedTexture(context->texture(2)));
            EXPECT_TRUE(context->usedTexture(context->texture(3)));

            context->resetUsedTextures();
            break;
        case 2:
            // Number of textures used for commit should still be two.
            EXPECT_EQ(2, context->numUsedTextures());

            context->resetUsedTextures();
            break;
        case 3:
            // No textures should be used for commit.
            EXPECT_EQ(0, context->numUsedTextures());

            context->resetUsedTextures();
            break;
        case 4:
            // Number of textures used for commit should be one.
            EXPECT_EQ(1, context->numUsedTextures());

            context->resetUsedTextures();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(impl->context()->context3D());

        // Number of textures used for drawing should two except for frame 4
        // where the viewport only contains one layer.
        if (impl->sourceFrameNumber() == 3)
            EXPECT_EQ(1, context->numUsedTextures());
        else
            EXPECT_EQ(2, context->numUsedTextures());

        if (impl->sourceFrameNumber() < 4) {
            context->resetUsedTextures();
            postSetNeedsAnimateAndCommitToMainThread();
            postSetNeedsRedrawToMainThread();
        } else
            endTest();
    }

    virtual void layout() OVERRIDE
    {
        switch (m_numCommits++) {
        case 0:
        case 1:
            m_parent->setNeedsDisplay();
            m_child->setNeedsDisplay();
            break;
        case 2:
            // Damage part of layers.
            m_parent->setNeedsDisplayRect(FloatRect(0, 0, 5, 5));
            m_child->setNeedsDisplayRect(FloatRect(0, 0, 5, 5));
            break;
        case 3:
            m_child->setNeedsDisplay();
            m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));
            break;
        case 4:
            m_layerTreeHost->setViewportSize(IntSize(10, 20), IntSize(10, 20));
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    MockContentLayerChromiumClient m_client;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_parent;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_child;
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestAtomicCommitWithPartialUpdate, runMultiThread)
{
    runTest(true);
}

class TestLayerChromium : public LayerChromium {
public:
    static scoped_refptr<TestLayerChromium> create() { return make_scoped_refptr(new TestLayerChromium()); }

    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker* occlusion, CCRenderingStats&) OVERRIDE
    {
        // Gain access to internals of the CCOcclusionTracker.
        const TestCCOcclusionTracker* testOcclusion = static_cast<const TestCCOcclusionTracker*>(occlusion);
        m_occludedScreenSpace = testOcclusion ? testOcclusion->occlusionInScreenSpace() : Region();
    }

    virtual bool drawsContent() const OVERRIDE { return true; }

    const Region& occludedScreenSpace() const { return m_occludedScreenSpace; }
    void clearOccludedScreenSpace() { m_occludedScreenSpace = Region(); }

private:
    TestLayerChromium() : LayerChromium() { }
    virtual ~TestLayerChromium() { }

    Region m_occludedScreenSpace;
};

static void setTestLayerPropertiesForTesting(TestLayerChromium* layer, LayerChromium* parent, const WebTransformationMatrix& transform, const FloatPoint& anchor, const FloatPoint& position, const IntSize& bounds, bool opaque)
{
    setLayerPropertiesForTesting(layer, parent, transform, anchor, position, bounds, opaque);
    layer->clearOccludedScreenSpace();
}

class CCLayerTreeHostTestLayerOcclusion : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLayerOcclusion() { }

    virtual void beginTest() OVERRIDE
    {
        scoped_refptr<TestLayerChromium> rootLayer = TestLayerChromium::create();
        scoped_refptr<TestLayerChromium> child = TestLayerChromium::create();
        scoped_refptr<TestLayerChromium> child2 = TestLayerChromium::create();
        scoped_refptr<TestLayerChromium> grandChild = TestLayerChromium::create();
        scoped_refptr<TestLayerChromium> mask = TestLayerChromium::create();

        WebTransformationMatrix identityMatrix;
        WebTransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(90);
        childTransform.translate(-250, -250);

        child->setMasksToBounds(true);

        // See CCLayerTreeHostCommonTest.layerAddsSelfToOccludedRegionWithRotatedSurface for a nice visual of these layers and how they end up
        // positioned on the screen.

        // The child layer is rotated and the grandChild is opaque, but clipped to the child and rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), false);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        ASSERT_TRUE(m_layerTreeHost->initializeRendererIfNeeded());
        CCTextureUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 170, 160), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer is opaque, then it adds to the occlusion seen by the rootLayer.
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 30, 170, 170), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Add a second child to the root layer and the regions should merge
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(70, 20), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 30, 170, 170), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child2->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 20, 170, 180), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, rootLayer->occludedScreenSpace().rects().size());

        // Move the second child to be sure.
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 30, 170, 170), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child2->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 30, 190, 170), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a mask on it, then it shouldn't contribute to occlusion on stuff below it
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        child->setMaskLayer(mask.get());

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer with a mask is below child2, then child2 should contribute to occlusion on everything, and child shouldn't contribute to the rootLayer
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        child->setMaskLayer(mask.get());

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 70, 190, 130), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 40, 190, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a non-opaque drawOpacity, then it shouldn't contribute to occlusion on stuff below it
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        child->setMaskLayer(0);
        child->setOpacity(0.5);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer with non-opaque drawOpacity is below child2, then child2 should contribute to occlusion on everything, and child shouldn't contribute to the rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        child->setMaskLayer(0);
        child->setOpacity(0.5);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 70, 190, 130), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 40, 190, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestLayerOcclusion)

class CCLayerTreeHostTestLayerOcclusionWithFilters : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLayerOcclusionWithFilters() { }

    virtual void beginTest() OVERRIDE
    {
        scoped_refptr<TestLayerChromium> rootLayer = TestLayerChromium::create();
        scoped_refptr<TestLayerChromium> child = TestLayerChromium::create();
        scoped_refptr<TestLayerChromium> child2 = TestLayerChromium::create();
        scoped_refptr<TestLayerChromium> grandChild = TestLayerChromium::create();
        scoped_refptr<TestLayerChromium> mask = TestLayerChromium::create();

        WebTransformationMatrix identityMatrix;
        WebTransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(90);
        childTransform.translate(-250, -250);

        child->setMasksToBounds(true);

        // If the child layer has a filter that changes alpha values, and is below child2, then child2 should contribute to occlusion on everything,
        // and child shouldn't contribute to the rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        {
            WebFilterOperations filters;
            filters.append(WebFilterOperation::createOpacityFilter(0.5));
            child->setFilters(filters);
        }

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        ASSERT_TRUE(m_layerTreeHost->initializeRendererIfNeeded());
        CCTextureUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 70, 190, 130), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 40, 190, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a filter that moves pixels/changes alpha, and is below child2, then child should not inherit occlusion from outside its subtree,
        // and should not contribute to the rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        {
            WebFilterOperations filters;
            filters.append(WebFilterOperation::createBlurFilter(10));
            child->setFilters(filters);
        }

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds(), rootLayer->bounds());
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        EXPECT_RECT_EQ(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        CCLayerTreeHost::setNeedsFilterContext(false);
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestLayerOcclusionWithFilters)

class CCLayerTreeHostTestManySurfaces : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestManySurfaces() { }

    virtual void beginTest() OVERRIDE
    {
        // We create enough RenderSurfaces that it will trigger Vector reallocation while computing occlusion.
        Region occluded;
        const WebTransformationMatrix identityMatrix;
        std::vector<scoped_refptr<TestLayerChromium> > layers;
        std::vector<scoped_refptr<TestLayerChromium> > children;
        int numSurfaces = 20;
        scoped_refptr<TestLayerChromium> replica = TestLayerChromium::create();

        for (int i = 0; i < numSurfaces; ++i) {
            layers.push_back(TestLayerChromium::create());
            if (!i) {
                setTestLayerPropertiesForTesting(layers.back().get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
                layers.back()->createRenderSurface();
            } else {
                setTestLayerPropertiesForTesting(layers.back().get(), layers[layers.size()-2].get(), identityMatrix, FloatPoint(0, 0), FloatPoint(1, 1), IntSize(200-i, 200-i), true);
                layers.back()->setMasksToBounds(true);
                layers.back()->setReplicaLayer(replica.get()); // Make it have a RenderSurface
            }
        }

        for (int i = 1; i < numSurfaces; ++i) {
            children.push_back(TestLayerChromium::create());
            setTestLayerPropertiesForTesting(children.back().get(), layers[i].get(), identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), false);
        }

        m_layerTreeHost->setRootLayer(layers[0].get());
        m_layerTreeHost->setViewportSize(layers[0]->bounds(), layers[0]->bounds());
        ASSERT_TRUE(m_layerTreeHost->initializeRendererIfNeeded());
        CCTextureUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        m_layerTreeHost->commitComplete();

        for (int i = 0; i < numSurfaces-1; ++i) {
            IntRect expectedOcclusion(i+1, i+1, 200-i-1, 200-i-1);

            EXPECT_RECT_EQ(expectedOcclusion, layers[i]->occludedScreenSpace().bounds());
            EXPECT_EQ(1u, layers[i]->occludedScreenSpace().rects().size());
        }

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestManySurfaces)

// A loseContext(1) should lead to a didRecreateOutputSurface(true)
class CCLayerTreeHostTestSetSingleLostContext : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetSingleLostContext()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_layerTreeHost->loseContext(1);
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        EXPECT_TRUE(succeeded);
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

TEST_F(CCLayerTreeHostTestSetSingleLostContext, runMultiThread)
{
    runTest(true);
}

// A loseContext(10) should lead to a didRecreateOutputSurface(false), and
// a finishAllRendering() should not hang.
class CCLayerTreeHostTestSetRepeatedLostContext : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetRepeatedLostContext()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_layerTreeHost->loseContext(10);
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        EXPECT_FALSE(succeeded);
        m_layerTreeHost->finishAllRendering();
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

TEST_F(CCLayerTreeHostTestSetRepeatedLostContext, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestFractionalScroll : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestFractionalScroll()
        : m_scrollAmount(1.75, 0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        CCLayerImpl* root = impl->rootLayer();
        root->setMaxScrollPosition(IntSize(100, 100));

        // Check that a fractional scroll delta is correctly accumulated over multiple commits.
        if (!impl->sourceFrameNumber()) {
            EXPECT_EQ(root->scrollPosition(), IntPoint(0, 0));
            EXPECT_EQ(root->scrollDelta(), FloatSize(0, 0));
            postSetNeedsCommitToMainThread();
        } else if (impl->sourceFrameNumber() == 1) {
            EXPECT_EQ(root->scrollPosition(), flooredIntPoint(m_scrollAmount));
            EXPECT_EQ(root->scrollDelta(), FloatSize(fmod(m_scrollAmount.width(), 1), 0));
            postSetNeedsCommitToMainThread();
        } else if (impl->sourceFrameNumber() == 2) {
            EXPECT_EQ(root->scrollPosition(), flooredIntPoint(m_scrollAmount + m_scrollAmount));
            EXPECT_EQ(root->scrollDelta(), FloatSize(fmod(2 * m_scrollAmount.width(), 1), 0));
            endTest();
        }
        root->scrollBy(m_scrollAmount);
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale) OVERRIDE
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
    }

    virtual void afterTest() OVERRIDE
    {
    }
private:
    FloatSize m_scrollAmount;
};

TEST_F(CCLayerTreeHostTestFractionalScroll, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestFinishAllRendering : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestFinishAllRendering()
        : m_once(false)
        , m_drawCount(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setNeedsRedraw();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        if (m_once)
            return;
        m_once = true;
        m_layerTreeHost->setNeedsRedraw();
        m_layerTreeHost->acquireLayerTextures();
        {
            base::AutoLock lock(m_lock);
            m_drawCount = 0;
        }
        m_layerTreeHost->finishAllRendering();
        {
            base::AutoLock lock(m_lock);
            EXPECT_EQ(0, m_drawCount);
        }
        endTest();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        base::AutoLock lock(m_lock);
        ++m_drawCount;
    }

    virtual void afterTest() OVERRIDE
    {
    }
private:

    bool m_once;
    base::Lock m_lock;
    int m_drawCount;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestFinishAllRendering)

// Layers added to tree with existing active animations should have the animation
// correctly recognized.
class CCLayerTreeHostTestLayerAddedWithAnimation : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLayerAddedWithAnimation()
        : m_addedAnimation(false)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        EXPECT_FALSE(m_addedAnimation);

        scoped_refptr<LayerChromium> layer = LayerChromium::create();
        layer->setLayerAnimationDelegate(this);

        // Any valid CCAnimationCurve will do here.
        scoped_ptr<CCAnimationCurve> curve(CCEaseTimingFunction::create());
        scoped_ptr<CCActiveAnimation> animation(CCActiveAnimation::create(curve.Pass(), 1, 1, CCActiveAnimation::Opacity));
        layer->layerAnimationController()->addAnimation(animation.Pass());

        // We add the animation *before* attaching the layer to the tree.
        m_layerTreeHost->rootLayer()->addChild(layer);
        EXPECT_TRUE(m_addedAnimation);

        endTest();
    }

    virtual void didAddAnimation() OVERRIDE
    {
        m_addedAnimation = true;
    }

    virtual void afterTest() OVERRIDE { }

private:
    bool m_addedAnimation;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestLayerAddedWithAnimation)

class CCLayerTreeHostTestScrollChildLayer : public CCLayerTreeHostTest, public WebLayerScrollClient {
public:
    CCLayerTreeHostTestScrollChildLayer(float deviceScaleFactor)
        : m_deviceScaleFactor(deviceScaleFactor)
        , m_initialScroll(IntPoint(10, 20))
        , m_secondScroll(IntPoint(40, 5))
        , m_scrollAmount(2, -1)
        , m_rootScrolls(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        IntSize viewportSize(10, 10);
        IntSize deviceViewportSize = viewportSize;
        deviceViewportSize.scale(m_deviceScaleFactor, m_deviceScaleFactor);
        m_layerTreeHost->setViewportSize(viewportSize, deviceViewportSize);

        m_layerTreeHost->setDeviceScaleFactor(m_deviceScaleFactor);

        m_rootScrollLayer = ContentLayerChromium::create(&m_mockDelegate);
        m_rootScrollLayer->setBounds(IntSize(110, 110));

        m_rootScrollLayer->setPosition(FloatPoint(0, 0));
        m_rootScrollLayer->setAnchorPoint(FloatPoint(0, 0));

        m_rootScrollLayer->setIsDrawable(true);
        m_rootScrollLayer->setScrollable(true);
        m_rootScrollLayer->setMaxScrollPosition(IntSize(100, 100));
        m_layerTreeHost->rootLayer()->addChild(m_rootScrollLayer);

        m_childLayer = ContentLayerChromium::create(&m_mockDelegate);
        m_childLayer->setLayerScrollClient(this);
        m_childLayer->setBounds(IntSize(110, 110));

        // The scrolls will happen at 5, 5. If they are treated like device pixels, then
        // they will be at 2.5, 2.5 in logical pixels, and will miss this layer.
        m_childLayer->setPosition(FloatPoint(5, 5));
        m_childLayer->setAnchorPoint(FloatPoint(0, 0));

        m_childLayer->setIsDrawable(true);
        m_childLayer->setScrollable(true);
        m_childLayer->setMaxScrollPosition(IntSize(100, 100));
        m_rootScrollLayer->addChild(m_childLayer);

        m_childLayer->setScrollPosition(m_initialScroll);

        postSetNeedsCommitToMainThread();
    }

    virtual void didScroll() OVERRIDE
    {
        m_finalScrollPosition = m_childLayer->scrollPosition();
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale) OVERRIDE
    {
        IntPoint position = m_rootScrollLayer->scrollPosition();
        m_rootScrollLayer->setScrollPosition(position + scrollDelta);
        m_rootScrolls++;
    }

    virtual void layout() OVERRIDE
    {
        EXPECT_EQ(IntPoint(), m_rootScrollLayer->scrollPosition());

        switch (m_layerTreeHost->commitNumber()) {
        case 0:
            EXPECT_POINT_EQ(m_initialScroll, m_childLayer->scrollPosition());
            break;
        case 1:
            EXPECT_POINT_EQ(m_initialScroll + m_scrollAmount, m_childLayer->scrollPosition());

            // Pretend like Javascript updated the scroll position itself.
            m_childLayer->setScrollPosition(m_secondScroll);
            break;
        case 2:
            EXPECT_POINT_EQ(m_secondScroll + m_scrollAmount, m_childLayer->scrollPosition());
            break;
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        CCLayerImpl* root = impl->rootLayer();
        CCLayerImpl* rootScrollLayer = root->children()[0];
        CCLayerImpl* childLayer = rootScrollLayer->children()[0];

        EXPECT_EQ(root->scrollDelta(), IntSize());
        EXPECT_EQ(rootScrollLayer->scrollDelta(), IntSize());
        EXPECT_EQ(rootScrollLayer->bounds().width() * m_deviceScaleFactor, rootScrollLayer->contentBounds().width());
        EXPECT_EQ(rootScrollLayer->bounds().height() * m_deviceScaleFactor, rootScrollLayer->contentBounds().height());
        EXPECT_EQ(childLayer->bounds().width() * m_deviceScaleFactor, childLayer->contentBounds().width());
        EXPECT_EQ(childLayer->bounds().height() * m_deviceScaleFactor, childLayer->contentBounds().height());

        switch (impl->sourceFrameNumber()) {
        case 0:
            // Gesture scroll on impl thread.
            EXPECT_EQ(impl->scrollBegin(IntPoint(5, 5), CCInputHandlerClient::Gesture), CCInputHandlerClient::ScrollStarted);
            impl->scrollBy(IntPoint(), m_scrollAmount);
            impl->scrollEnd();

            EXPECT_POINT_EQ(m_initialScroll, childLayer->scrollPosition());
            EXPECT_SIZE_EQ(m_scrollAmount, childLayer->scrollDelta());
            postSetNeedsCommitToMainThread();
            break;
        case 1:
            // Wheel scroll on impl thread.
            EXPECT_EQ(impl->scrollBegin(IntPoint(5, 5), CCInputHandlerClient::Wheel), CCInputHandlerClient::ScrollStarted);
            impl->scrollBy(IntPoint(), m_scrollAmount);
            impl->scrollEnd();

            EXPECT_POINT_EQ(m_secondScroll, childLayer->scrollPosition());
            EXPECT_SIZE_EQ(m_scrollAmount, childLayer->scrollDelta());
            break;
        case 2:
            EXPECT_POINT_EQ(m_secondScroll + m_scrollAmount, childLayer->scrollPosition());
            EXPECT_SIZE_EQ(IntSize(0, 0), childLayer->scrollDelta());

            endTest();
        }
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(0, m_rootScrolls);
        EXPECT_POINT_EQ(m_secondScroll + m_scrollAmount, m_finalScrollPosition);
    }

private:
    float m_deviceScaleFactor;
    IntPoint m_initialScroll;
    IntPoint m_secondScroll;
    IntSize m_scrollAmount;
    int m_rootScrolls;
    IntPoint m_finalScrollPosition;

    MockContentLayerChromiumClient m_mockDelegate;
    scoped_refptr<LayerChromium> m_rootScrollLayer;
    scoped_refptr<LayerChromium> m_childLayer;
};

class CCLayerTreeHostTestScrollChildLayerNormalDpi : public CCLayerTreeHostTestScrollChildLayer {
public:
    CCLayerTreeHostTestScrollChildLayerNormalDpi() : CCLayerTreeHostTestScrollChildLayer(1) { }
};

TEST_F(CCLayerTreeHostTestScrollChildLayerNormalDpi, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestScrollChildLayerHighDpi : public CCLayerTreeHostTestScrollChildLayer {
public:
    CCLayerTreeHostTestScrollChildLayerHighDpi() : CCLayerTreeHostTestScrollChildLayer(2) { }
};

TEST_F(CCLayerTreeHostTestScrollChildLayerHighDpi, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestScrollRootScrollLayer : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestScrollRootScrollLayer(float deviceScaleFactor)
        : m_deviceScaleFactor(deviceScaleFactor)
        , m_initialScroll(IntPoint(10, 20))
        , m_secondScroll(IntPoint(40, 5))
        , m_scrollAmount(2, -1)
        , m_rootScrolls(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        IntSize viewportSize(10, 10);
        IntSize deviceViewportSize = viewportSize;
        deviceViewportSize.scale(m_deviceScaleFactor, m_deviceScaleFactor);
        m_layerTreeHost->setViewportSize(viewportSize, deviceViewportSize);

        m_layerTreeHost->setDeviceScaleFactor(m_deviceScaleFactor);

        m_rootScrollLayer = ContentLayerChromium::create(&m_mockDelegate);
        m_rootScrollLayer->setBounds(IntSize(110, 110));

        m_rootScrollLayer->setPosition(FloatPoint(0, 0));
        m_rootScrollLayer->setAnchorPoint(FloatPoint(0, 0));

        m_rootScrollLayer->setIsDrawable(true);
        m_rootScrollLayer->setScrollable(true);
        m_rootScrollLayer->setMaxScrollPosition(IntSize(100, 100));
        m_layerTreeHost->rootLayer()->addChild(m_rootScrollLayer);

        m_rootScrollLayer->setScrollPosition(m_initialScroll);

        postSetNeedsCommitToMainThread();
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale) OVERRIDE
    {
        IntPoint position = m_rootScrollLayer->scrollPosition();
        m_rootScrollLayer->setScrollPosition(position + scrollDelta);
        m_rootScrolls++;
    }

    virtual void layout() OVERRIDE
    {
        switch (m_layerTreeHost->commitNumber()) {
        case 0:
            EXPECT_POINT_EQ(m_initialScroll, m_rootScrollLayer->scrollPosition());
            break;
        case 1:
            EXPECT_POINT_EQ(m_initialScroll + m_scrollAmount, m_rootScrollLayer->scrollPosition());

            // Pretend like Javascript updated the scroll position itself.
            m_rootScrollLayer->setScrollPosition(m_secondScroll);
            break;
        case 2:
            EXPECT_POINT_EQ(m_secondScroll + m_scrollAmount, m_rootScrollLayer->scrollPosition());
            break;
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        CCLayerImpl* root = impl->rootLayer();
        CCLayerImpl* rootScrollLayer = root->children()[0];

        EXPECT_EQ(root->scrollDelta(), IntSize());
        EXPECT_EQ(rootScrollLayer->bounds().width() * m_deviceScaleFactor, rootScrollLayer->contentBounds().width());
        EXPECT_EQ(rootScrollLayer->bounds().height() * m_deviceScaleFactor, rootScrollLayer->contentBounds().height());

        switch (impl->sourceFrameNumber()) {
        case 0:
            // Gesture scroll on impl thread.
            EXPECT_EQ(impl->scrollBegin(IntPoint(5, 5), CCInputHandlerClient::Gesture), CCInputHandlerClient::ScrollStarted);
            impl->scrollBy(IntPoint(), m_scrollAmount);
            impl->scrollEnd();

            EXPECT_POINT_EQ(m_initialScroll, rootScrollLayer->scrollPosition());
            EXPECT_SIZE_EQ(m_scrollAmount, rootScrollLayer->scrollDelta());
            postSetNeedsCommitToMainThread();
            break;
        case 1:
            // Wheel scroll on impl thread.
            EXPECT_EQ(impl->scrollBegin(IntPoint(5, 5), CCInputHandlerClient::Wheel), CCInputHandlerClient::ScrollStarted);
            impl->scrollBy(IntPoint(), m_scrollAmount);
            impl->scrollEnd();

            EXPECT_POINT_EQ(m_secondScroll, rootScrollLayer->scrollPosition());
            EXPECT_SIZE_EQ(m_scrollAmount, rootScrollLayer->scrollDelta());
            break;
        case 2:
            EXPECT_POINT_EQ(m_secondScroll + m_scrollAmount, rootScrollLayer->scrollPosition());
            EXPECT_SIZE_EQ(IntSize(0, 0), rootScrollLayer->scrollDelta());

            endTest();
        }
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_rootScrolls);
    }

private:
    float m_deviceScaleFactor;
    IntPoint m_initialScroll;
    IntPoint m_secondScroll;
    IntSize m_scrollAmount;
    int m_rootScrolls;

    MockContentLayerChromiumClient m_mockDelegate;
    scoped_refptr<LayerChromium> m_rootScrollLayer;
};

class CCLayerTreeHostTestScrollRootScrollLayerNormalDpi : public CCLayerTreeHostTestScrollRootScrollLayer {
public:
    CCLayerTreeHostTestScrollRootScrollLayerNormalDpi() : CCLayerTreeHostTestScrollRootScrollLayer(1) { }
};

TEST_F(CCLayerTreeHostTestScrollRootScrollLayerNormalDpi, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestScrollRootScrollLayerHighDpi : public CCLayerTreeHostTestScrollRootScrollLayer {
public:
    CCLayerTreeHostTestScrollRootScrollLayerHighDpi() : CCLayerTreeHostTestScrollRootScrollLayer(2) { }
};

TEST_F(CCLayerTreeHostTestScrollRootScrollLayerHighDpi, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestCompositeAndReadbackCleanup : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCompositeAndReadbackCleanup() { }

    virtual void beginTest() OVERRIDE
    {
        LayerChromium* rootLayer = m_layerTreeHost->rootLayer();

        scoped_array<char> pixels(new char[4]);
        m_layerTreeHost->compositeAndReadback(static_cast<void*>(pixels.get()), IntRect(0, 0, 1, 1));
        EXPECT_FALSE(rootLayer->renderSurface());

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestCompositeAndReadbackCleanup)

class CCLayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit()
        : m_rootLayer(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
        , m_surfaceLayer1(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
        , m_replicaLayer1(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
        , m_surfaceLayer2(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
        , m_replicaLayer2(ContentLayerChromiumWithUpdateTracking::create(&m_mockDelegate))
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(IntSize(100, 100), IntSize(100, 100));

        m_rootLayer->setBounds(IntSize(100, 100));
        m_surfaceLayer1->setBounds(IntSize(100, 100));
        m_surfaceLayer1->setForceRenderSurface(true);
        m_surfaceLayer1->setOpacity(0.5);
        m_surfaceLayer2->setBounds(IntSize(100, 100));
        m_surfaceLayer2->setForceRenderSurface(true);
        m_surfaceLayer2->setOpacity(0.5);

        m_surfaceLayer1->setReplicaLayer(m_replicaLayer1.get());
        m_surfaceLayer2->setReplicaLayer(m_replicaLayer2.get());

        m_rootLayer->addChild(m_surfaceLayer1);
        m_surfaceLayer1->addChild(m_surfaceLayer2);
        m_layerTreeHost->setRootLayer(m_rootLayer);
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* hostImpl) OVERRIDE
    {
        CCRenderer* renderer = hostImpl->renderer();
        CCRenderPass::Id surface1RenderPassId = hostImpl->rootLayer()->children()[0]->renderSurface()->renderPassId();
        CCRenderPass::Id surface2RenderPassId = hostImpl->rootLayer()->children()[0]->children()[0]->renderSurface()->renderPassId();

        switch (hostImpl->sourceFrameNumber()) {
        case 0:
            EXPECT_TRUE(renderer->haveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_TRUE(renderer->haveCachedResourcesForRenderPassId(surface2RenderPassId));

            // Reduce the memory limit to only fit the root layer and one render surface. This
            // prevents any contents drawing into surfaces from being allocated.
            hostImpl->setMemoryAllocationLimitBytes(100 * 100 * 4 * 2);
            break;
        case 1:
            EXPECT_FALSE(renderer->haveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_FALSE(renderer->haveCachedResourcesForRenderPassId(surface2RenderPassId));

            endTest();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_rootLayer->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer1->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer2->paintContentsCount());

        // Clear layer references so CCLayerTreeHost dies.
        m_rootLayer = NULL;
        m_surfaceLayer1 = NULL;
        m_replicaLayer1 = NULL;
        m_surfaceLayer2 = NULL;
        m_replicaLayer2 = NULL;
    }

private:
    MockContentLayerChromiumClient m_mockDelegate;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_rootLayer;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_surfaceLayer1;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_replicaLayer1;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_surfaceLayer2;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_replicaLayer2;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit)

class EvictionTestLayer : public LayerChromium {
public:
    static scoped_refptr<EvictionTestLayer> create() { return make_scoped_refptr(new EvictionTestLayer()); }

    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE { return true; }

    virtual scoped_ptr<CCLayerImpl> createCCLayerImpl() OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;
    virtual void setTexturePriorities(const CCPriorityCalculator&) OVERRIDE;

    bool haveBackingTexture() const { return m_texture.get() ? m_texture->haveBackingTexture() : false; }

private:
    EvictionTestLayer() : LayerChromium() { }
    virtual ~EvictionTestLayer() { }

    void createTextureIfNeeded()
    {
        if (m_texture.get())
            return;
        m_texture = CCPrioritizedTexture::create(layerTreeHost()->contentsTextureManager());
        m_texture->setDimensions(IntSize(10, 10), GL_RGBA);
        m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    }

    scoped_ptr<CCPrioritizedTexture> m_texture;
    SkBitmap m_bitmap;
};

class EvictionTestLayerImpl : public CCLayerImpl {
public:
    static scoped_ptr<EvictionTestLayerImpl> create(int id)
    {
        return make_scoped_ptr(new EvictionTestLayerImpl(id));
    }
    virtual ~EvictionTestLayerImpl() { }

    virtual void appendQuads(CCQuadSink& quadSink, CCAppendQuadsData&) OVERRIDE
    {
        ASSERT_TRUE(m_hasTexture);
        ASSERT_NE(0u, layerTreeHostImpl()->resourceProvider()->numResources());
    }

    void setHasTexture(bool hasTexture) { m_hasTexture = hasTexture; }

private:
    explicit EvictionTestLayerImpl(int id)
        : CCLayerImpl(id)
        , m_hasTexture(false) { }

    bool m_hasTexture;
};

void EvictionTestLayer::setTexturePriorities(const CCPriorityCalculator&)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;
    m_texture->setRequestPriority(CCPriorityCalculator::uiPriority(true));
}

void EvictionTestLayer::update(CCTextureUpdateQueue& queue, const CCOcclusionTracker*, CCRenderingStats&)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;
    IntRect fullRect(0, 0, 10, 10);

    ResourceUpdate upload = ResourceUpdate::Create(
        m_texture.get(), &m_bitmap, fullRect, fullRect, IntSize());
    queue.appendFullUpload(upload);
}

scoped_ptr<CCLayerImpl> EvictionTestLayer::createCCLayerImpl()
{
    return EvictionTestLayerImpl::create(m_layerId).PassAs<CCLayerImpl>();
}

void EvictionTestLayer::pushPropertiesTo(CCLayerImpl* layerImpl)
{
    LayerChromium::pushPropertiesTo(layerImpl);

    EvictionTestLayerImpl* testLayerImpl = static_cast<EvictionTestLayerImpl*>(layerImpl);
    testLayerImpl->setHasTexture(m_texture->haveBackingTexture());
}

class CCLayerTreeHostTestEvictTextures : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestEvictTextures()
        : m_layer(EvictionTestLayer::create())
        , m_implForEvictTextures(0)
        , m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(IntSize(10, 20), IntSize(10, 20));

        WebTransformationMatrix identityMatrix;
        setLayerPropertiesForTesting(m_layer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 20), true);
    }

    class EvictTexturesTask : public WebKit::WebThread::Task {
    public:
        EvictTexturesTask(CCLayerTreeHostTestEvictTextures* test) : m_test(test) { }
        virtual ~EvictTexturesTask() { }
        virtual void run() OVERRIDE
        {
            DCHECK(m_test->m_implForEvictTextures);
            m_test->m_implForEvictTextures->reduceContentsTextureMemoryOnImplThread(0);
        }

    private:
        CCLayerTreeHostTestEvictTextures* m_test;
    };

    void postEvictTextures()
    {
        DCHECK(webThread());
        webThread()->postTask(new EvictTexturesTask(this));
    }

    // Commit 1: Just commit and draw normally, then post an eviction at the end
    // that will trigger a commit.
    // Commit 2: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 3: Triggered by the setNeedsCommit. In layout(), post an eviction
    // task, which will be handled before the commit. Don't set needsCommit, it
    // should have been posted. A frame should not be drawn (note,
    // didCommitAndDrawFrame may be called anyway).
    // Commit 4: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 5: Triggered by the setNeedsCommit, post an eviction task in
    // layout(), a frame should not be drawn but a commit will be posted.
    // Commit 6: Triggered by the eviction, post an eviction task in
    // layout(), which will be a noop, letting the commit (which recreates the
    // textures) go through and draw a frame, then end the test.
    //
    // Commits 1+2 test the eviction recovery path where eviction happens outside
    // of the beginFrame/commit pair.
    // Commits 3+4 test the eviction recovery path where eviction happens inside
    // the beginFrame/commit pair.
    // Commits 5+6 test the path where an eviction happens during the eviction
    // recovery path.
    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        switch (m_numCommits) {
        case 1:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            postEvictTextures();
            break;
        case 2:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            m_layerTreeHost->setNeedsCommit();
            break;
        case 3:
            break;
        case 4:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            m_layerTreeHost->setNeedsCommit();
            break;
        case 5:
            break;
        case 6:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        m_implForEvictTextures = impl;
    }

    virtual void layout() OVERRIDE
    {
        ++m_numCommits;
        switch (m_numCommits) {
        case 1:
        case 2:
            break;
        case 3:
            postEvictTextures();
            break;
        case 4:
            // We couldn't check in didCommitAndDrawFrame on commit 3, so check here.
            EXPECT_FALSE(m_layer->haveBackingTexture());
            break;
        case 5:
            postEvictTextures();
            break;
        case 6:
            // We couldn't check in didCommitAndDrawFrame on commit 5, so check here.
            EXPECT_FALSE(m_layer->haveBackingTexture());
            postEvictTextures();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    MockContentLayerChromiumClient m_client;
    scoped_refptr<EvictionTestLayer> m_layer;
    CCLayerTreeHostImpl* m_implForEvictTextures;
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestEvictTextures, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestLostContextAfterEvictTextures : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLostContextAfterEvictTextures()
        : m_layer(EvictionTestLayer::create())
        , m_implForEvictTextures(0)
        , m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(IntSize(10, 20), IntSize(10, 20));

        WebTransformationMatrix identityMatrix;
        setLayerPropertiesForTesting(m_layer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 20), true);
    }

    class EvictTexturesTask : public WebKit::WebThread::Task {
    public:
        EvictTexturesTask(CCLayerTreeHostTestLostContextAfterEvictTextures* test) : m_test(test) { }
        virtual ~EvictTexturesTask() { }
        virtual void run() OVERRIDE
        {
            m_test->evictTexturesOnImplThread();
        }

    private:
        CCLayerTreeHostTestLostContextAfterEvictTextures* m_test;
    };

    void postEvictTextures()
    {
        if (webThread())
            webThread()->postTask(new EvictTexturesTask(this));
        else {
            DebugScopedSetImplThread impl;
            evictTexturesOnImplThread();
        }
    }

    void evictTexturesOnImplThread()
    {
        DCHECK(m_implForEvictTextures);
        m_implForEvictTextures->reduceContentsTextureMemoryOnImplThread(0);
    }

    // Commit 1: Just commit and draw normally, then at the end, set ourselves
    // invisible (to prevent a commit that would recreate textures after
    // eviction, before the context recovery), and post a task that will evict
    // textures, then cause the context to be lost, and then set ourselves
    // visible again (to allow commits, since that's what causes context
    // recovery in single thread).
    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        ++m_numCommits;
        switch (m_numCommits) {
        case 1:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            m_layerTreeHost->setVisible(false);
            postEvictTextures();
            m_layerTreeHost->loseContext(1);
            m_layerTreeHost->setVisible(true);
            break;
        default:
            break;
        }
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        m_implForEvictTextures = impl;
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        EXPECT_TRUE(succeeded);
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    MockContentLayerChromiumClient m_client;
    scoped_refptr<EvictionTestLayer> m_layer;
    CCLayerTreeHostImpl* m_implForEvictTextures;
    int m_numCommits;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestLostContextAfterEvictTextures)

class CompositorFakeWebGraphicsContext3DWithEndQueryCausingLostContext : public WebKit::CompositorFakeWebGraphicsContext3D {
public:
    static scoped_ptr<CompositorFakeWebGraphicsContext3DWithEndQueryCausingLostContext> create(Attributes attrs)
    {
        return make_scoped_ptr(new CompositorFakeWebGraphicsContext3DWithEndQueryCausingLostContext(attrs));
    }

    virtual void setContextLostCallback(WebGraphicsContextLostCallback* callback) { m_contextLostCallback = callback; }
    virtual bool isContextLost() { return m_isContextLost; }

    virtual void beginQueryEXT(WGC3Denum, WebGLId) { }
    virtual void endQueryEXT(WGC3Denum)
    {
        // Lose context.
        if (!m_isContextLost) {
            m_contextLostCallback->onContextLost();
            m_isContextLost = true;
        }
    }
    virtual void getQueryObjectuivEXT(WebGLId, WGC3Denum pname, WGC3Duint* params)
    {
        // Context is lost. We need to behave as if result is available.
        if (pname == GL_QUERY_RESULT_AVAILABLE_EXT)
            *params = 1;
    }

private:
    explicit CompositorFakeWebGraphicsContext3DWithEndQueryCausingLostContext(Attributes attrs)
        : CompositorFakeWebGraphicsContext3D(attrs)
        , m_contextLostCallback(0)
        , m_isContextLost(false) { }

    WebGraphicsContextLostCallback* m_contextLostCallback;
    bool m_isContextLost;
};

class CCLayerTreeHostTestLostContextWhileUpdatingResources : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLostContextWhileUpdatingResources()
        : m_parent(ContentLayerChromiumWithUpdateTracking::create(&m_client))
        , m_numChildren(50)
    {
        for (int i = 0; i < m_numChildren; i++)
            m_children.push_back(ContentLayerChromiumWithUpdateTracking::create(&m_client));
    }

    virtual scoped_ptr<WebKit::WebCompositorOutputSurface> createOutputSurface()
    {
        return FakeWebCompositorOutputSurface::create(CompositorFakeWebGraphicsContext3DWithEndQueryCausingLostContext::create(WebGraphicsContext3D::Attributes()).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<WebKit::WebCompositorOutputSurface>();
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_parent);
        m_layerTreeHost->setViewportSize(IntSize(m_numChildren, 1), IntSize(m_numChildren, 1));

        WebTransformationMatrix identityMatrix;
        setLayerPropertiesForTesting(m_parent.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(m_numChildren, 1), true);
        for (int i = 0; i < m_numChildren; i++)
            setLayerPropertiesForTesting(m_children[i].get(), m_parent.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(i, 0), IntSize(1, 1), false);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        endTest();
    }

    virtual void layout()
    {
        m_parent->setNeedsDisplay();
        for (int i = 0; i < m_numChildren; i++)
            m_children[i]->setNeedsDisplay();
    }

    virtual void afterTest()
    {
    }

private:
    MockContentLayerChromiumClient m_client;
    scoped_refptr<ContentLayerChromiumWithUpdateTracking> m_parent;
    int m_numChildren;
    std::vector<scoped_refptr<ContentLayerChromiumWithUpdateTracking> > m_children;
};

TEST_F(CCLayerTreeHostTestLostContextWhileUpdatingResources, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestContinuousCommit : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestContinuousCommit()
        : m_numCommitComplete(0)
        , m_numDrawLayers(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));
        m_layerTreeHost->rootLayer()->setBounds(IntSize(10, 10));

        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDrawLayers == 1)
            m_numCommitComplete++;
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDrawLayers++;
        if (m_numDrawLayers == 2)
            endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // Check that we didn't commit twice between first and second draw.
        EXPECT_EQ(1, m_numCommitComplete);
    }

private:
    int m_numCommitComplete;
    int m_numDrawLayers;
};

TEST_F(CCLayerTreeHostTestContinuousCommit, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestContinuousInvalidate : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestContinuousInvalidate()
        : m_numCommitComplete(0)
        , m_numDrawLayers(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));
        m_layerTreeHost->rootLayer()->setBounds(IntSize(10, 10));

        m_contentLayer = ContentLayerChromium::create(&m_mockDelegate);
        m_contentLayer->setBounds(IntSize(10, 10));
        m_contentLayer->setPosition(FloatPoint(0, 0));
        m_contentLayer->setAnchorPoint(FloatPoint(0, 0));
        m_contentLayer->setIsDrawable(true);
        m_layerTreeHost->rootLayer()->addChild(m_contentLayer);

        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        m_contentLayer->setNeedsDisplay();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDrawLayers == 1)
            m_numCommitComplete++;
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDrawLayers++;
        if (m_numDrawLayers == 2)
            endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // Check that we didn't commit twice between first and second draw.
        EXPECT_EQ(1, m_numCommitComplete);

        // Clear layer references so CCLayerTreeHost dies.
        m_contentLayer = NULL;
    }

private:
    MockContentLayerChromiumClient m_mockDelegate;
    scoped_refptr<LayerChromium> m_contentLayer;
    int m_numCommitComplete;
    int m_numDrawLayers;
};

TEST_F(CCLayerTreeHostTestContinuousInvalidate, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestContinuousAnimate : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestContinuousAnimate()
        : m_numCommitComplete(0)
        , m_numDrawLayers(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(IntSize(10, 10), IntSize(10, 10));
        m_layerTreeHost->rootLayer()->setBounds(IntSize(10, 10));

        postSetNeedsCommitToMainThread();
    }

    virtual void animate(double) OVERRIDE
    {
        m_layerTreeHost->setNeedsAnimate();
    }

    virtual void layout() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setNeedsDisplay();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDrawLayers == 1)
            m_numCommitComplete++;
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDrawLayers++;
        if (m_numDrawLayers == 2)
            endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // Check that we didn't commit twice between first and second draw.
        EXPECT_EQ(1, m_numCommitComplete);
    }

private:
    int m_numCommitComplete;
    int m_numDrawLayers;
};

TEST_F(CCLayerTreeHostTestContinuousAnimate, runMultiThread)
{
    runTest(true);
}

} // namespace
