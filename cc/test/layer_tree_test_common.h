// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_TEST_COMMON_H_
#define CC_TEST_LAYER_TREE_TEST_COMMON_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "cc/layer_tree_host.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimationDelegate.h"

namespace cc {
class LayerImpl;
class LayerTreeHost;
class LayerTreeHostClient;
class LayerTreeHostImpl;

// Used by test stubs to notify the test when something interesting happens.
class TestHooks : public WebKit::WebAnimationDelegate {
public:
    virtual void beginCommitOnThread(LayerTreeHostImpl*) { }
    virtual void commitCompleteOnThread(LayerTreeHostImpl*) { }
    virtual bool prepareToDrawOnThread(LayerTreeHostImpl*);
    virtual void drawLayersOnThread(LayerTreeHostImpl*) { }
    virtual void animateLayers(LayerTreeHostImpl*, base::TimeTicks monotonicTime, bool hasUnfinishedAnimation) { }
    virtual void willAnimateLayers(LayerTreeHostImpl*, base::TimeTicks monotonicTime) { }
    virtual void applyScrollAndScale(gfx::Vector2d, float) { }
    virtual void animate(base::TimeTicks monotonicTime) { }
    virtual void layout() { }
    virtual void didRecreateOutputSurface(bool succeeded) { }
    virtual void didAddAnimation() { }
    virtual void didCommit() { }
    virtual void didCommitAndDrawFrame() { }
    virtual void scheduleComposite() { }
    virtual void didDeferCommit() { }

    // Implementation of WebAnimationDelegate
    virtual void notifyAnimationStarted(double time) OVERRIDE { }
    virtual void notifyAnimationFinished(double time) OVERRIDE { }

    virtual scoped_ptr<OutputSurface> createOutputSurface();
};

class TimeoutTask;
class BeginTask;

class MockLayerImplTreeHostClient : public LayerTreeHostClient {
};

// The ThreadedTests runs with the main loop running. It instantiates a single MockLayerTreeHost and associated
// MockLayerTreeHostImpl/ThreadedMockLayerTreeHostClient.
//
// beginTest() is called once the main message loop is running and the layer tree host is initialized.
//
// Key stages of the drawing loop, e.g. drawing or commiting, redirect to ThreadedTest methods of similar names.
// To track the commit process, override these functions.
//
// The test continues until someone calls endTest. endTest can be called on any thread, but be aware that
// ending the test is an asynchronous process.
class ThreadedTest : public testing::Test, public TestHooks {
public:
    virtual ~ThreadedTest();

    virtual void afterTest() = 0;
    virtual void beginTest() = 0;
    virtual void setupTree();

    void endTest();
    void endTestAfterDelay(int delayMilliseconds);

    void postAddAnimationToMainThread(Layer*);
    void postAddInstantAnimationToMainThread();
    void postSetNeedsCommitToMainThread();
    void postAcquireLayerTextures();
    void postSetNeedsRedrawToMainThread();
    void postSetVisibleToMainThread(bool visible);

    void doBeginTest();
    void timeout();

    LayerTreeHost* layerTreeHost() { return m_layerTreeHost.get(); }

protected:
    ThreadedTest();

    virtual void initializeSettings(LayerTreeSettings&) { }

    virtual void scheduleComposite() OVERRIDE;

    void realEndTest();

    void dispatchAddInstantAnimation();
    void dispatchAddAnimation(Layer*);
    void dispatchSetNeedsCommit();
    void dispatchAcquireLayerTextures();
    void dispatchSetNeedsRedraw();
    void dispatchSetVisible(bool);
    void dispatchComposite();
    void dispatchDidAddAnimation();

    virtual void runTest(bool threaded);

    Thread* implThread() { return proxy() ? proxy()->implThread() : 0; }
    Proxy* proxy() const { return m_layerTreeHost ? m_layerTreeHost->proxy() : 0; }

    LayerTreeSettings m_settings;
    scoped_ptr<MockLayerImplTreeHostClient> m_client;
    scoped_ptr<LayerTreeHost> m_layerTreeHost;

private:
    bool m_beginning;
    bool m_endWhenBeginReturns;
    bool m_timedOut;
    bool m_scheduled;
    bool m_started;

    scoped_ptr<Thread> m_mainCCThread;
    scoped_ptr<base::Thread> m_implThread;
    base::CancelableClosure m_timeout;
    base::WeakPtr<ThreadedTest> m_mainThreadWeakPtr;
    base::WeakPtrFactory<ThreadedTest> m_weakFactory;
};

class ThreadedTestThreadOnly : public ThreadedTest {
public:
    void runTestThreaded()
    {
        ThreadedTest::runTest(true);
    }
};

// Adapts LayerTreeHostImpl for test. Runs real code, then invokes test hooks.
class MockLayerTreeHostImpl : public LayerTreeHostImpl {
public:
    static scoped_ptr<MockLayerTreeHostImpl> create(TestHooks*, const LayerTreeSettings&, LayerTreeHostImplClient*, Proxy*);

    virtual void beginCommit() OVERRIDE;
    virtual void commitComplete() OVERRIDE;
    virtual bool prepareToDraw(FrameData&) OVERRIDE;
    virtual void drawLayers(FrameData&) OVERRIDE;

    // Make these public.
    typedef std::vector<LayerImpl*> LayerList;

protected:
    virtual void animateLayers(base::TimeTicks monotonicTime, base::Time wallClockTime) OVERRIDE;
    virtual base::TimeDelta lowFrequencyAnimationInterval() const OVERRIDE;

private:
    MockLayerTreeHostImpl(TestHooks*, const LayerTreeSettings&, LayerTreeHostImplClient*, Proxy*);

    TestHooks* m_testHooks;
};

} // namespace cc

#define SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME) \
    TEST_F(TEST_FIXTURE_NAME, runSingleThread) \
    { \
        runTest(false); \
    }

#define MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME) \
    TEST_F(TEST_FIXTURE_NAME, runMultiThread) \
    { \
        runTest(true); \
    }

#define SINGLE_AND_MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME) \
    SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME) \
    MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)

#define MULTI_THREAD_TEST_P1(TEST_FIXTURE_NAME, P1_NAME, P1) \
    class TEST_FIXTURE_NAME##_##P1_NAME : public TEST_FIXTURE_NAME { \
    public: \
        TEST_FIXTURE_NAME##_##P1_NAME() \
            : TEST_FIXTURE_NAME(P1) {} \
    }; \
    MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME##_##P1)

#define SINGLE_THREAD_TEST_P1(TEST_FIXTURE_NAME, P1_NAME, P1) \
    class TEST_FIXTURE_NAME##_##P1_NAME : public TEST_FIXTURE_NAME { \
    public: \
        TEST_FIXTURE_NAME##_##P1_NAME() \
            : TEST_FIXTURE_NAME(P1) {} \
    }; \
    SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME##_##P1_NAME)

#define SINGLE_AND_MULTI_THREAD_TEST_P1(TEST_FIXTURE_NAME, P1_NAME, P1) \
    SINGLE_THREAD_TEST_P1(TEST_FIXTURE_NAME, P1) \
    MULTI_THREAD_TEST_P1(TEST_FIXTURE_NAME, P1)

#define MULTI_THREAD_TEST_P2(TEST_FIXTURE_NAME, P1_NAME, P1, P2_NAME, P2) \
    class TEST_FIXTURE_NAME##_##P1_NAME##_##P2_NAME : public TEST_FIXTURE_NAME { \
    public: \
        TEST_FIXTURE_NAME##_##P1_NAME##_##P2_NAME() \
            : TEST_FIXTURE_NAME(P1, P2) {} \
    }; \
    MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME##_##P1_NAME##_##P2_NAME)

#define SINGLE_THREAD_TEST_P2(TEST_FIXTURE_NAME, P1_NAME, P1, P2_NAME, P2) \
    class TEST_FIXTURE_NAME##_##P1_NAME##_##P2_NAME : public TEST_FIXTURE_NAME { \
    public: \
        TEST_FIXTURE_NAME##_##P1_NAME##_##P2_NAME() \
            : TEST_FIXTURE_NAME(P1, P2) {} \
    }; \
    SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME##_##P1_NAME##_##P2_NAME)

#define SINGLE_AND_MULTI_THREAD_TEST_P2(TEST_FIXTURE_NAME, P1_NAME, P1, P2_NAME, P2) \
    SINGLE_THREAD_TEST_P1(TEST_FIXTURE_NAME, P1, P2) \
    MULTI_THREAD_TEST_P1(TEST_FIXTURE_NAME, P1, P2)

#endif  // CC_TEST_LAYER_TREE_TEST_COMMON_H_
