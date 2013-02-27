// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_test_common.h"

#include "cc/animation.h"
#include "cc/animation_registrar.h"
#include "cc/content_layer.h"
#include "cc/input_handler.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/single_thread_proxy.h"
#include "cc/thread_impl.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/test/tiled_layer_test_common.h"
#include "cc/timing_function.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "ui/gfx/size_conversions.h"

using namespace WebKit;

namespace cc {

TestHooks::TestHooks()
{
  bool useSoftwareRendering = false;
  bool useDelegatingRenderer = false;
  m_fakeClient.reset(new FakeLayerImplTreeHostClient(useSoftwareRendering, useDelegatingRenderer));
}

TestHooks::~TestHooks() { }

bool TestHooks::prepareToDrawOnThread(cc::LayerTreeHostImpl*, LayerTreeHostImpl::FrameData&, bool)
{
    return true;
}

bool TestHooks::canActivatePendingTree()
{
    return true;
}

scoped_ptr<OutputSurface> TestHooks::createOutputSurface()
{
    return createFakeOutputSurface();
}

scoped_refptr<cc::ContextProvider> TestHooks::OffscreenContextProviderForMainThread()
{
    return m_fakeClient->OffscreenContextProviderForMainThread();
}

scoped_refptr<cc::ContextProvider> TestHooks::OffscreenContextProviderForCompositorThread()
{
    return m_fakeClient->OffscreenContextProviderForCompositorThread();
}

scoped_ptr<MockLayerTreeHostImpl> MockLayerTreeHostImpl::create(TestHooks* testHooks, const LayerTreeSettings& settings, LayerTreeHostImplClient* client, Proxy* proxy)
{
    return make_scoped_ptr(new MockLayerTreeHostImpl(testHooks, settings, client, proxy));
}

void MockLayerTreeHostImpl::beginCommit()
{
    LayerTreeHostImpl::beginCommit();
    m_testHooks->beginCommitOnThread(this);
}

void MockLayerTreeHostImpl::commitComplete()
{
    LayerTreeHostImpl::commitComplete();
    m_testHooks->commitCompleteOnThread(this);

    if (!settings().implSidePainting)
        m_testHooks->treeActivatedOnThread(this);

}

bool MockLayerTreeHostImpl::prepareToDraw(FrameData& frame)
{
    bool result = LayerTreeHostImpl::prepareToDraw(frame);
    if (!m_testHooks->prepareToDrawOnThread(this, frame, result))
        result = false;
    return result;
}

void MockLayerTreeHostImpl::drawLayers(FrameData& frame)
{
    LayerTreeHostImpl::drawLayers(frame);
    m_testHooks->drawLayersOnThread(this);
}

bool MockLayerTreeHostImpl::activatePendingTreeIfNeeded()
{
    if (!pendingTree())
        return false;

    if (!m_testHooks->canActivatePendingTree())
        return false;

    bool activated = LayerTreeHostImpl::activatePendingTreeIfNeeded();
    if (activated)
        m_testHooks->treeActivatedOnThread(this);
    return activated;
}

bool MockLayerTreeHostImpl::initializeRenderer(scoped_ptr<OutputSurface> outputSurface)
{
    bool success = LayerTreeHostImpl::initializeRenderer(outputSurface.Pass());
    m_testHooks->initializedRendererOnThread(this, success);
    return success;
}

void MockLayerTreeHostImpl::animateLayers(base::TimeTicks monotonicTime, base::Time wallClockTime)
{
    m_testHooks->willAnimateLayers(this, monotonicTime);
    LayerTreeHostImpl::animateLayers(monotonicTime, wallClockTime);
    m_testHooks->animateLayers(this, monotonicTime);
}

void MockLayerTreeHostImpl::updateAnimationState()
{
    LayerTreeHostImpl::updateAnimationState();
    bool hasUnfinishedAnimation = false;
    AnimationRegistrar::AnimationControllerMap::const_iterator iter = activeAnimationControllers().begin();
    for (; iter != activeAnimationControllers().end(); ++iter) {
        if (iter->second->hasActiveAnimation()) {
            hasUnfinishedAnimation = true;
            break;
        }
    }
    m_testHooks->updateAnimationState(this, hasUnfinishedAnimation);
}

base::TimeDelta MockLayerTreeHostImpl::lowFrequencyAnimationInterval() const
{
    return base::TimeDelta::FromMilliseconds(16);
}

MockLayerTreeHostImpl::MockLayerTreeHostImpl(TestHooks* testHooks, const LayerTreeSettings& settings, LayerTreeHostImplClient* client, Proxy* proxy)
    : LayerTreeHostImpl(settings, client, proxy)
    , m_testHooks(testHooks)
{
}

// Adapts LayerTreeHost for test. Injects MockLayerTreeHostImpl.
class MockLayerTreeHost : public cc::LayerTreeHost {
public:
    static scoped_ptr<MockLayerTreeHost> create(TestHooks* testHooks, cc::LayerTreeHostClient* client, const cc::LayerTreeSettings& settings, scoped_ptr<cc::Thread> implThread)
    {
        scoped_ptr<MockLayerTreeHost> layerTreeHost(new MockLayerTreeHost(testHooks, client, settings));
        bool success = layerTreeHost->initialize(implThread.Pass());
        EXPECT_TRUE(success);
        return layerTreeHost.Pass();
    }

    virtual scoped_ptr<cc::LayerTreeHostImpl> createLayerTreeHostImpl(cc::LayerTreeHostImplClient* client) OVERRIDE
    {
        return MockLayerTreeHostImpl::create(m_testHooks, settings(), client, proxy()).PassAs<cc::LayerTreeHostImpl>();
    }

    virtual void setNeedsCommit() OVERRIDE
    {
        if (!m_testStarted)
            return;
        LayerTreeHost::setNeedsCommit();
    }

    void setTestStarted(bool started) { m_testStarted = started; }

    virtual void didDeferCommit() OVERRIDE
    {
        m_testHooks->didDeferCommit();
    }

private:
    MockLayerTreeHost(TestHooks* testHooks, cc::LayerTreeHostClient* client, const cc::LayerTreeSettings& settings)
        : LayerTreeHost(client, settings)
        , m_testHooks(testHooks)
        , m_testStarted(false)
    {
    }

    TestHooks* m_testHooks;
    bool m_testStarted;
};

// Implementation of LayerTreeHost callback interface.
class ThreadedMockLayerTreeHostClient : public MockLayerImplTreeHostClient {
public:
    static scoped_ptr<ThreadedMockLayerTreeHostClient> create(TestHooks* testHooks)
    {
        return make_scoped_ptr(new ThreadedMockLayerTreeHostClient(testHooks));
    }

    virtual void willBeginFrame() OVERRIDE
    {
    }

    virtual void didBeginFrame() OVERRIDE
    {
    }

    virtual void animate(double monotonicTime) OVERRIDE
    {
        m_testHooks->animate(base::TimeTicks::FromInternalValue(monotonicTime * base::Time::kMicrosecondsPerSecond));
    }

    virtual void layout() OVERRIDE
    {
        m_testHooks->layout();
    }

  virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float scale) OVERRIDE
    {
        m_testHooks->applyScrollAndScale(scrollDelta, scale);
    }

    virtual scoped_ptr<OutputSurface> createOutputSurface() OVERRIDE
    {
        return m_testHooks->createOutputSurface();
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        m_testHooks->didRecreateOutputSurface(succeeded);
    }

    virtual void willRetryRecreateOutputSurface() OVERRIDE
    {
        m_testHooks->willRetryRecreateOutputSurface();
    }

    virtual scoped_ptr<InputHandler> createInputHandler() OVERRIDE
    {
        return scoped_ptr<InputHandler>();
    }

    virtual void willCommit() OVERRIDE
    {
    }

    virtual void didCommit() OVERRIDE
    {
        m_testHooks->didCommit();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_testHooks->didCommitAndDrawFrame();
    }

    virtual void didCompleteSwapBuffers() OVERRIDE
    {
    }

    virtual void scheduleComposite() OVERRIDE
    {
        m_testHooks->scheduleComposite();
    }

    virtual scoped_refptr<cc::ContextProvider> OffscreenContextProviderForMainThread() OVERRIDE
    {
        return m_testHooks->OffscreenContextProviderForMainThread();
    }

    virtual scoped_refptr<cc::ContextProvider> OffscreenContextProviderForCompositorThread() OVERRIDE
    {
        return m_testHooks->OffscreenContextProviderForCompositorThread();
    }

private:
    explicit ThreadedMockLayerTreeHostClient(TestHooks* testHooks) : m_testHooks(testHooks) { }

    TestHooks* m_testHooks;
};

ThreadedTest::ThreadedTest()
    : m_beginning(false)
    , m_endWhenBeginReturns(false)
    , m_timedOut(false)
    , m_scheduled(false)
    , m_started(false)
    , m_implThread(0)
    , m_weakFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this))
{
    m_mainThreadWeakPtr = m_weakFactory.GetWeakPtr();
}

ThreadedTest::~ThreadedTest()
{
}

void ThreadedTest::endTest()
{
    // For the case where we endTest during beginTest(), set a flag to indicate that
    // the test should end the second beginTest regains control.
    if (m_beginning)
        m_endWhenBeginReturns = true;
    else
        proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::realEndTest, m_mainThreadWeakPtr));
}

void ThreadedTest::endTestAfterDelay(int delayMilliseconds)
{
    proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::endTest, m_mainThreadWeakPtr));
}

void ThreadedTest::postAddAnimationToMainThread(Layer* layerToReceiveAnimation)
{
    proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::dispatchAddAnimation, m_mainThreadWeakPtr, base::Unretained(layerToReceiveAnimation)));
}

void ThreadedTest::postAddInstantAnimationToMainThread()
{
    proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::dispatchAddInstantAnimation, m_mainThreadWeakPtr));
}

void ThreadedTest::postSetNeedsCommitToMainThread()
{
    proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::dispatchSetNeedsCommit, m_mainThreadWeakPtr));
}

void ThreadedTest::postAcquireLayerTextures()
{
    proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::dispatchAcquireLayerTextures, m_mainThreadWeakPtr));
}

void ThreadedTest::postSetNeedsRedrawToMainThread()
{
    proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::dispatchSetNeedsRedraw, m_mainThreadWeakPtr));
}

void ThreadedTest::postSetVisibleToMainThread(bool visible)
{
    proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::dispatchSetVisible, m_mainThreadWeakPtr, visible));
}

void ThreadedTest::doBeginTest()
{
    m_client = ThreadedMockLayerTreeHostClient::create(this);

    scoped_ptr<cc::Thread> implCCThread(NULL);
    if (m_implThread)
        implCCThread = cc::ThreadImpl::createForDifferentThread(m_implThread->message_loop_proxy());
    m_layerTreeHost = MockLayerTreeHost::create(this, m_client.get(), m_settings, implCCThread.Pass());
    ASSERT_TRUE(m_layerTreeHost.get());

    m_started = true;
    m_beginning = true;
    setupTree();
    m_layerTreeHost->setSurfaceReady();
    beginTest();
    m_beginning = false;
    if (m_endWhenBeginReturns)
        realEndTest();

    // Allow commits to happen once beginTest() has had a chance to post tasks
    // so that those tasks will happen before the first commit.
    if (m_layerTreeHost)
        static_cast<MockLayerTreeHost*>(m_layerTreeHost.get())->setTestStarted(true);
}

void ThreadedTest::setupTree()
{
    if (!m_layerTreeHost->rootLayer()) {
        scoped_refptr<Layer> rootLayer = Layer::create();
        rootLayer->setBounds(gfx::Size(1, 1));
        m_layerTreeHost->setRootLayer(rootLayer);
    }

    gfx::Size rootBounds = m_layerTreeHost->rootLayer()->bounds();
    gfx::Size deviceRootBounds = gfx::ToCeiledSize(
        gfx::ScaleSize(rootBounds, m_layerTreeHost->deviceScaleFactor()));
    m_layerTreeHost->setViewportSize(rootBounds, deviceRootBounds);
}

void ThreadedTest::timeout()
{
    m_timedOut = true;
    endTest();
}

void ThreadedTest::scheduleComposite()
{
    if (!m_started || m_scheduled)
        return;
    m_scheduled = true;
    proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::dispatchComposite, m_mainThreadWeakPtr));
}

void ThreadedTest::realEndTest()
{
    if (m_layerTreeHost && proxy()->commitPendingForTesting()) {
        proxy()->mainThread()->postTask(base::Bind(&ThreadedTest::realEndTest, m_mainThreadWeakPtr));
        return;
    }
        
    MessageLoop::current()->Quit();
}

void ThreadedTest::dispatchAddInstantAnimation()
{
    DCHECK(!proxy() || proxy()->isMainThread());

    if (m_layerTreeHost.get() && m_layerTreeHost->rootLayer())
        addOpacityTransitionToLayer(*m_layerTreeHost->rootLayer(), 0, 0, 0.5, false);
}

void ThreadedTest::dispatchAddAnimation(Layer* layerToReceiveAnimation)
{
    DCHECK(!proxy() || proxy()->isMainThread());

    if (layerToReceiveAnimation)
        addOpacityTransitionToLayer(*layerToReceiveAnimation, 10, 0, 0.5, true);
}

void ThreadedTest::dispatchSetNeedsCommit()
{
    DCHECK(!proxy() || proxy()->isMainThread());

    if (m_layerTreeHost.get())
        m_layerTreeHost->setNeedsCommit();
}

void ThreadedTest::dispatchAcquireLayerTextures()
{
    DCHECK(!proxy() || proxy()->isMainThread());

    if (m_layerTreeHost.get())
        m_layerTreeHost->acquireLayerTextures();
}

void ThreadedTest::dispatchSetNeedsRedraw()
{
    DCHECK(!proxy() || proxy()->isMainThread());

    if (m_layerTreeHost.get())
        m_layerTreeHost->setNeedsRedraw();
}

void ThreadedTest::dispatchSetVisible(bool visible)
{
    DCHECK(!proxy() || proxy()->isMainThread());

    if (m_layerTreeHost.get())
        m_layerTreeHost->setVisible(visible);
}

void ThreadedTest::dispatchComposite()
{
    m_scheduled = false;
    if (m_layerTreeHost.get())
        m_layerTreeHost->composite();
}

void ThreadedTest::runTest(bool threaded)
{
    if (threaded) {
        m_implThread.reset(new base::Thread("ThreadedTest"));
        ASSERT_TRUE(m_implThread->Start());
    }

    m_mainCCThread = cc::ThreadImpl::createForCurrentThread();

    initializeSettings(m_settings);

    m_mainCCThread->postTask(base::Bind(&ThreadedTest::doBeginTest, base::Unretained(this)));
    m_timeout.Reset(base::Bind(&ThreadedTest::timeout, base::Unretained(this)));
    m_mainCCThread->postDelayedTask(m_timeout.callback(), 5000);
    MessageLoop::current()->Run();
    if (m_layerTreeHost.get() && m_layerTreeHost->rootLayer())
        m_layerTreeHost->rootLayer()->setLayerTreeHost(0);
    m_layerTreeHost.reset();

    m_timeout.Cancel();

    ASSERT_FALSE(m_layerTreeHost.get());
    m_client.reset();
    if (m_timedOut) {
        FAIL() << "Test timed out";
        return;
    }
    afterTest();
}

}  // namespace cc
