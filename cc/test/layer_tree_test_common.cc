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

bool TestHooks::prepareToDrawOnThread(cc::LayerTreeHostImpl*, LayerTreeHostImpl::FrameData*, bool)
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

void MockLayerTreeHostImpl::BeginCommit()
{
    LayerTreeHostImpl::BeginCommit();
    m_testHooks->beginCommitOnThread(this);
}

void MockLayerTreeHostImpl::CommitComplete()
{
    LayerTreeHostImpl::CommitComplete();
    m_testHooks->commitCompleteOnThread(this);

    if (!settings().implSidePainting)
        m_testHooks->treeActivatedOnThread(this);

}

bool MockLayerTreeHostImpl::PrepareToDraw(FrameData* frame)
{
    bool result = LayerTreeHostImpl::PrepareToDraw(frame);
    if (!m_testHooks->prepareToDrawOnThread(this, frame, result))
        result = false;
    return result;
}

void MockLayerTreeHostImpl::DrawLayers(FrameData* frame, base::TimeTicks frameBeginTime)
{
    LayerTreeHostImpl::DrawLayers(frame, frameBeginTime);
    m_testHooks->drawLayersOnThread(this);
}

bool MockLayerTreeHostImpl::SwapBuffers()
{
    bool result = LayerTreeHostImpl::SwapBuffers();
    m_testHooks->swapBuffersOnThread(this, result);
    return result;
}

bool MockLayerTreeHostImpl::ActivatePendingTreeIfNeeded()
{
    if (!pending_tree())
        return false;

    if (!m_testHooks->canActivatePendingTree())
        return false;

    bool activated = LayerTreeHostImpl::ActivatePendingTreeIfNeeded();
    if (activated)
        m_testHooks->treeActivatedOnThread(this);
    return activated;
}

bool MockLayerTreeHostImpl::InitializeRenderer(scoped_ptr<OutputSurface> outputSurface)
{
    bool success = LayerTreeHostImpl::InitializeRenderer(outputSurface.Pass());
    m_testHooks->initializedRendererOnThread(this, success);
    return success;
}

void MockLayerTreeHostImpl::SetVisible(bool visible)
{
    LayerTreeHostImpl::SetVisible(visible);
    m_testHooks->didSetVisibleOnImplTree(this, visible);
}

void MockLayerTreeHostImpl::AnimateLayers(base::TimeTicks monotonicTime, base::Time wallClockTime)
{
    m_testHooks->willAnimateLayers(this, monotonicTime);
    LayerTreeHostImpl::AnimateLayers(monotonicTime, wallClockTime);
    m_testHooks->animateLayers(this, monotonicTime);
}

void MockLayerTreeHostImpl::UpdateAnimationState()
{
    LayerTreeHostImpl::UpdateAnimationState();
    bool hasUnfinishedAnimation = false;
    AnimationRegistrar::AnimationControllerMap::const_iterator iter = active_animation_controllers().begin();
    for (; iter != active_animation_controllers().end(); ++iter) {
        if (iter->second->HasActiveAnimation()) {
            hasUnfinishedAnimation = true;
            break;
        }
    }
    m_testHooks->updateAnimationState(this, hasUnfinishedAnimation);
}

base::TimeDelta MockLayerTreeHostImpl::LowFrequencyAnimationInterval() const
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
        bool success = layerTreeHost->Initialize(implThread.Pass());
        EXPECT_TRUE(success);
        return layerTreeHost.Pass();
    }

    virtual scoped_ptr<cc::LayerTreeHostImpl> CreateLayerTreeHostImpl(cc::LayerTreeHostImplClient* client) OVERRIDE
    {
        return MockLayerTreeHostImpl::create(m_testHooks, settings(), client, proxy()).PassAs<cc::LayerTreeHostImpl>();
    }

    virtual void SetNeedsCommit() OVERRIDE
    {
        if (!m_testStarted)
            return;
        LayerTreeHost::SetNeedsCommit();
    }

    void setTestStarted(bool started) { m_testStarted = started; }

    virtual void DidDeferCommit() OVERRIDE
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
    static scoped_ptr<ThreadedMockLayerTreeHostClient> Create(TestHooks* testHooks)
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
    , m_scheduleWhenSetVisibleTrue(false)
    , m_started(false)
    , m_ended(false)
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
        proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::realEndTest, m_mainThreadWeakPtr));
}

void ThreadedTest::endTestAfterDelay(int delayMilliseconds)
{
    proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::endTest, m_mainThreadWeakPtr));
}

void ThreadedTest::postAddAnimationToMainThread(Layer* layerToReceiveAnimation)
{
    proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::dispatchAddAnimation, m_mainThreadWeakPtr, base::Unretained(layerToReceiveAnimation)));
}

void ThreadedTest::postAddInstantAnimationToMainThread()
{
    proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::dispatchAddInstantAnimation, m_mainThreadWeakPtr));
}

void ThreadedTest::postSetNeedsCommitToMainThread()
{
    proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::dispatchSetNeedsCommit, m_mainThreadWeakPtr));
}

void ThreadedTest::postAcquireLayerTextures()
{
    proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::dispatchAcquireLayerTextures, m_mainThreadWeakPtr));
}

void ThreadedTest::postSetNeedsRedrawToMainThread()
{
    proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::dispatchSetNeedsRedraw, m_mainThreadWeakPtr));
}

void ThreadedTest::postSetVisibleToMainThread(bool visible)
{
    proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::dispatchSetVisible, m_mainThreadWeakPtr, visible));
}

void ThreadedTest::doBeginTest()
{
    m_client = ThreadedMockLayerTreeHostClient::Create(this);

    scoped_ptr<cc::Thread> implCCThread(NULL);
    if (m_implThread)
        implCCThread = cc::ThreadImpl::createForDifferentThread(m_implThread->message_loop_proxy());
    m_layerTreeHost = MockLayerTreeHost::create(this, m_client.get(), m_settings, implCCThread.Pass());
    ASSERT_TRUE(m_layerTreeHost);

    m_started = true;
    m_beginning = true;
    setupTree();
    m_layerTreeHost->SetSurfaceReady();
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
    if (!m_layerTreeHost->root_layer()) {
        scoped_refptr<Layer> rootLayer = Layer::Create();
        rootLayer->SetBounds(gfx::Size(1, 1));
        m_layerTreeHost->SetRootLayer(rootLayer);
    }

    gfx::Size rootBounds = m_layerTreeHost->root_layer()->bounds();
    gfx::Size deviceRootBounds = gfx::ToCeiledSize(
        gfx::ScaleSize(rootBounds, m_layerTreeHost->device_scale_factor()));
    m_layerTreeHost->SetViewportSize(rootBounds, deviceRootBounds);
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
    proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::dispatchComposite, m_mainThreadWeakPtr));
}

void ThreadedTest::realEndTest()
{
    m_ended = true;

    if (m_layerTreeHost && proxy()->CommitPendingForTesting()) {
        proxy()->MainThread()->postTask(base::Bind(&ThreadedTest::realEndTest, m_mainThreadWeakPtr));
        return;
    }
        
    MessageLoop::current()->Quit();
}

void ThreadedTest::dispatchAddInstantAnimation()
{
    DCHECK(!proxy() || proxy()->IsMainThread());

    if (m_layerTreeHost.get() && m_layerTreeHost->root_layer())
        addOpacityTransitionToLayer(*m_layerTreeHost->root_layer(), 0, 0, 0.5, false);
}

void ThreadedTest::dispatchAddAnimation(Layer* layerToReceiveAnimation)
{
    DCHECK(!proxy() || proxy()->IsMainThread());

    if (layerToReceiveAnimation)
        addOpacityTransitionToLayer(*layerToReceiveAnimation, 10, 0, 0.5, true);
}

void ThreadedTest::dispatchSetNeedsCommit()
{
    DCHECK(!proxy() || proxy()->IsMainThread());

    if (m_layerTreeHost.get())
        m_layerTreeHost->SetNeedsCommit();
}

void ThreadedTest::dispatchAcquireLayerTextures()
{
    DCHECK(!proxy() || proxy()->IsMainThread());

    if (m_layerTreeHost.get())
        m_layerTreeHost->AcquireLayerTextures();
}

void ThreadedTest::dispatchSetNeedsRedraw()
{
    DCHECK(!proxy() || proxy()->IsMainThread());

    if (m_layerTreeHost.get())
        m_layerTreeHost->SetNeedsRedraw();
}

void ThreadedTest::dispatchSetVisible(bool visible)
{
    DCHECK(!proxy() || proxy()->IsMainThread());

    if (!m_layerTreeHost)
        return;

    m_layerTreeHost->SetVisible(visible);

    // If the LTH is being made visible and a previous scheduleComposite() was
    // deferred because the LTH was not visible, re-schedule the composite now.
    if (m_layerTreeHost->visible() && m_scheduleWhenSetVisibleTrue)
        scheduleComposite();
}

void ThreadedTest::dispatchComposite()
{
    m_scheduled = false;

    if (!m_layerTreeHost)
        return;

    // If the LTH is not visible, defer the composite until the LTH is made
    // visible.
    if (!m_layerTreeHost->visible()) {
        m_scheduleWhenSetVisibleTrue = true;
        return;
    }

    m_scheduleWhenSetVisibleTrue = false;
    m_layerTreeHost->Composite(base::TimeTicks::Now());
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
    if (m_layerTreeHost.get() && m_layerTreeHost->root_layer())
        m_layerTreeHost->root_layer()->SetLayerTreeHost(NULL);
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
