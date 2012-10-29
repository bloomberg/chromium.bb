// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/test/layer_tree_test_common.h"

#include "base/stl_util.h"
#include "cc/active_animation.h"
#include "cc/content_layer.h"
#include "cc/input_handler.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/scoped_thread_proxy.h"
#include "cc/settings.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_web_compositor_output_surface.h"
#include "cc/test/fake_web_graphics_context_3d.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/test/test_common.h"
#include "cc/test/tiled_layer_test_common.h"
#include "cc/thread_task.h"
#include "cc/timing_function.h"
#include "testing/gmock/include/gmock/gmock.h"
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <public/WebThread.h>

using namespace cc;
using namespace WebKit;

namespace WebKitTests {

scoped_ptr<CompositorFakeWebGraphicsContext3DWithTextureTracking> CompositorFakeWebGraphicsContext3DWithTextureTracking::create(Attributes attrs)
{
    return make_scoped_ptr(new CompositorFakeWebGraphicsContext3DWithTextureTracking(attrs));
}

WebGLId CompositorFakeWebGraphicsContext3DWithTextureTracking::createTexture()
{
    WebGLId texture = m_textures.size() + 1;
    m_textures.append(texture);
    return texture;
}

void CompositorFakeWebGraphicsContext3DWithTextureTracking::deleteTexture(WebGLId texture)
{
    for (size_t i = 0; i < m_textures.size(); i++) {
        if (m_textures[i] == texture) {
            m_textures.remove(i);
            break;
        }
    }
}

void CompositorFakeWebGraphicsContext3DWithTextureTracking::bindTexture(WGC3Denum /* target */, WebGLId texture)
{
    m_usedTextures.insert(texture);
}

int CompositorFakeWebGraphicsContext3DWithTextureTracking::numTextures() const { return static_cast<int>(m_textures.size()); }
int CompositorFakeWebGraphicsContext3DWithTextureTracking::texture(int i) const { return m_textures[i]; }
void CompositorFakeWebGraphicsContext3DWithTextureTracking::resetTextures() { m_textures.clear(); }

int CompositorFakeWebGraphicsContext3DWithTextureTracking::numUsedTextures() const { return static_cast<int>(m_usedTextures.size()); }
bool CompositorFakeWebGraphicsContext3DWithTextureTracking::usedTexture(int texture) const
{
  return ContainsKey(m_usedTextures, texture);
}

void CompositorFakeWebGraphicsContext3DWithTextureTracking::resetUsedTextures() { m_usedTextures.clear(); }

CompositorFakeWebGraphicsContext3DWithTextureTracking::CompositorFakeWebGraphicsContext3DWithTextureTracking(Attributes attrs) : CompositorFakeWebGraphicsContext3D(attrs)
{
}

CompositorFakeWebGraphicsContext3DWithTextureTracking::~CompositorFakeWebGraphicsContext3DWithTextureTracking()
{
}

bool TestHooks::prepareToDrawOnThread(cc::LayerTreeHostImpl*)
{
    return true;
}

scoped_ptr<WebCompositorOutputSurface> TestHooks::createOutputSurface()
{
    return FakeWebCompositorOutputSurface::create(CompositorFakeWebGraphicsContext3DWithTextureTracking::create(WebGraphicsContext3D::Attributes()).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<WebKit::WebCompositorOutputSurface>();
}

scoped_ptr<MockLayerTreeHostImpl> MockLayerTreeHostImpl::create(TestHooks* testHooks, const LayerTreeSettings& settings, LayerTreeHostImplClient* client)
{
    return make_scoped_ptr(new MockLayerTreeHostImpl(testHooks, settings, client));
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
}

bool MockLayerTreeHostImpl::prepareToDraw(FrameData& frame)
{
    bool result = LayerTreeHostImpl::prepareToDraw(frame);
    if (!m_testHooks->prepareToDrawOnThread(this))
        result = false;
    return result;
}

void MockLayerTreeHostImpl::drawLayers(const FrameData& frame)
{
    LayerTreeHostImpl::drawLayers(frame);
    m_testHooks->drawLayersOnThread(this);
}

void MockLayerTreeHostImpl::animateLayers(base::TimeTicks monotonicTime, base::Time wallClockTime)
{
    m_testHooks->willAnimateLayers(this, monotonicTime);
    LayerTreeHostImpl::animateLayers(monotonicTime, wallClockTime);
    m_testHooks->animateLayers(this, monotonicTime);
}

base::TimeDelta MockLayerTreeHostImpl::lowFrequencyAnimationInterval() const
{
    return base::TimeDelta::FromMilliseconds(16);
}

MockLayerTreeHostImpl::MockLayerTreeHostImpl(TestHooks* testHooks, const LayerTreeSettings& settings, LayerTreeHostImplClient* client)
    : LayerTreeHostImpl(settings, client)
    , m_testHooks(testHooks)
{
}

// Adapts LayerTreeHost for test. Injects MockLayerTreeHostImpl.
class MockLayerTreeHost : public cc::LayerTreeHost {
public:
    static scoped_ptr<MockLayerTreeHost> create(TestHooks* testHooks, cc::LayerTreeHostClient* client, scoped_refptr<cc::Layer> rootLayer, const cc::LayerTreeSettings& settings)
    {
        scoped_ptr<MockLayerTreeHost> layerTreeHost(new MockLayerTreeHost(testHooks, client, settings));
        bool success = layerTreeHost->initialize();
        EXPECT_TRUE(success);
        layerTreeHost->setRootLayer(rootLayer);

        // LayerTreeHostImpl won't draw if it has 1x1 viewport.
        layerTreeHost->setViewportSize(IntSize(1, 1), IntSize(1, 1));

        layerTreeHost->rootLayer()->setLayerAnimationDelegate(testHooks);

        return layerTreeHost.Pass();
    }

    virtual scoped_ptr<cc::LayerTreeHostImpl> createLayerTreeHostImpl(cc::LayerTreeHostImplClient* client)
    {
        return MockLayerTreeHostImpl::create(m_testHooks, settings(), client).PassAs<cc::LayerTreeHostImpl>();
    }

    virtual void didAddAnimation() OVERRIDE
    {
        LayerTreeHost::didAddAnimation();
        m_testHooks->didAddAnimation();
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

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale) OVERRIDE
    {
        m_testHooks->applyScrollAndScale(scrollDelta, scale);
    }

    virtual scoped_ptr<WebCompositorOutputSurface> createOutputSurface() OVERRIDE
    {
        return m_testHooks->createOutputSurface();
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        m_testHooks->didRecreateOutputSurface(succeeded);
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

private:
    explicit ThreadedMockLayerTreeHostClient(TestHooks* testHooks) : m_testHooks(testHooks) { }

    TestHooks* m_testHooks;
};

class TimeoutTask : public WebThread::Task {
public:
    explicit TimeoutTask(ThreadedTest* test)
        : m_test(test)
    {
    }

    void clearTest()
    {
        m_test = 0;
    }

    virtual ~TimeoutTask()
    {
        if (m_test)
            m_test->clearTimeout();
    }

    virtual void run()
    {
        if (m_test)
            m_test->timeout();
    }

private:
    ThreadedTest* m_test;
};

class BeginTask : public WebThread::Task {
public:
    explicit BeginTask(ThreadedTest* test)
        : m_test(test)
    {
    }

    virtual ~BeginTask() { }
    virtual void run()
    {
        m_test->doBeginTest();
    }
private:
    ThreadedTest* m_test;
};

ThreadedTest::ThreadedTest()
    : m_beginning(false)
    , m_endWhenBeginReturns(false)
    , m_timedOut(false)
    , m_finished(false)
    , m_scheduled(false)
    , m_started(false)
{
}

ThreadedTest::~ThreadedTest()
{
}

void ThreadedTest::endTest()
{
    m_finished = true;

    // For the case where we endTest during beginTest(), set a flag to indicate that
    // the test should end the second beginTest regains control.
    if (m_beginning)
        m_endWhenBeginReturns = true;
    else
        m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::realEndTest));
}

void ThreadedTest::endTestAfterDelay(int delayMilliseconds)
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::endTest));
}

void ThreadedTest::postSetNeedsAnimateToMainThread()
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchSetNeedsAnimate));
}

void ThreadedTest::postAddAnimationToMainThread()
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchAddAnimation));
}

void ThreadedTest::postAddInstantAnimationToMainThread()
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchAddInstantAnimation));
}

void ThreadedTest::postSetNeedsCommitToMainThread()
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchSetNeedsCommit));
}

void ThreadedTest::postAcquireLayerTextures()
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchAcquireLayerTextures));
}

void ThreadedTest::postSetNeedsRedrawToMainThread()
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchSetNeedsRedraw));
}

void ThreadedTest::postSetNeedsAnimateAndCommitToMainThread()
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchSetNeedsAnimateAndCommit));
}

void ThreadedTest::postSetVisibleToMainThread(bool visible)
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchSetVisible, visible));
}

void ThreadedTest::postDidAddAnimationToMainThread()
{
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchDidAddAnimation));
}

void ThreadedTest::doBeginTest()
{
    DCHECK(Proxy::isMainThread());
    m_client = ThreadedMockLayerTreeHostClient::create(this);

    scoped_refptr<Layer> rootLayer = Layer::create();
    m_layerTreeHost = MockLayerTreeHost::create(this, m_client.get(), rootLayer, m_settings);
    ASSERT_TRUE(m_layerTreeHost.get());
    rootLayer->setLayerTreeHost(m_layerTreeHost.get());
    m_layerTreeHost->setSurfaceReady();

    m_started = true;
    m_beginning = true;
    beginTest();
    m_beginning = false;
    if (m_endWhenBeginReturns)
        realEndTest();

    // Allow commits to happen once beginTest() has had a chance to post tasks
    // so that those tasks will happen before the first commit.
    if (m_layerTreeHost)
        static_cast<MockLayerTreeHost*>(m_layerTreeHost.get())->setTestStarted(true);
}

void ThreadedTest::timeout()
{
    m_timedOut = true;
    endTest();
}

void ThreadedTest::scheduleComposite()
{
    if (!m_started || m_scheduled || m_finished)
        return;
    m_scheduled = true;
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadedTest::dispatchComposite));
}

void ThreadedTest::realEndTest()
{
    DCHECK(Proxy::isMainThread());
    WebKit::Platform::current()->currentThread()->exitRunLoop();
}

void ThreadedTest::dispatchSetNeedsAnimate()
{
    DCHECK(Proxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost.get())
        m_layerTreeHost->setNeedsAnimate();
}

void ThreadedTest::dispatchAddInstantAnimation()
{
    DCHECK(Proxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost.get() && m_layerTreeHost->rootLayer())
        addOpacityTransitionToLayer(*m_layerTreeHost->rootLayer(), 0, 0, 0.5, false);
}

void ThreadedTest::dispatchAddAnimation()
{
    DCHECK(Proxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost.get() && m_layerTreeHost->rootLayer())
        addOpacityTransitionToLayer(*m_layerTreeHost->rootLayer(), 10, 0, 0.5, true);
}

void ThreadedTest::dispatchSetNeedsAnimateAndCommit()
{
    DCHECK(Proxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost.get()) {
        m_layerTreeHost->setNeedsAnimate();
        m_layerTreeHost->setNeedsCommit();
    }
}

void ThreadedTest::dispatchSetNeedsCommit()
{
    DCHECK(Proxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost.get())
        m_layerTreeHost->setNeedsCommit();
}

void ThreadedTest::dispatchAcquireLayerTextures()
{
    DCHECK(Proxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost.get())
        m_layerTreeHost->acquireLayerTextures();
}

void ThreadedTest::dispatchSetNeedsRedraw()
{
    DCHECK(Proxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost.get())
        m_layerTreeHost->setNeedsRedraw();
}

void ThreadedTest::dispatchSetVisible(bool visible)
{
    DCHECK(Proxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost.get())
        m_layerTreeHost->setVisible(visible);
}

void ThreadedTest::dispatchComposite()
{
    m_scheduled = false;
    if (m_layerTreeHost.get() && !m_finished)
        m_layerTreeHost->composite();
}

void ThreadedTest::dispatchDidAddAnimation()
{
    DCHECK(Proxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost.get())
        m_layerTreeHost->didAddAnimation();
}

void ThreadedTest::runTest(bool threaded)
{
    // For these tests, we will enable threaded animations.
    ScopedSettings scopedSettings;
    Settings::setAcceleratedAnimationEnabled(true);

    if (threaded) {
        m_webThread.reset(WebKit::Platform::current()->createThread("ThreadedTest"));
        Platform::current()->compositorSupport()->initialize(m_webThread.get());
    } else
        Platform::current()->compositorSupport()->initialize(0);

    DCHECK(Proxy::isMainThread());
    m_mainThreadProxy = ScopedThreadProxy::create(Proxy::mainThread());

    initializeSettings(m_settings);

    m_beginTask = new BeginTask(this);
    WebKit::Platform::current()->currentThread()->postDelayedTask(m_beginTask, 0); // postDelayedTask takes ownership of the task
    m_timeoutTask = new TimeoutTask(this);
    WebKit::Platform::current()->currentThread()->postDelayedTask(m_timeoutTask, 5000);
    WebKit::Platform::current()->currentThread()->enterRunLoop();

    if (m_layerTreeHost.get() && m_layerTreeHost->rootLayer())
        m_layerTreeHost->rootLayer()->setLayerTreeHost(0);
    m_layerTreeHost.reset();

    if (m_timeoutTask)
        m_timeoutTask->clearTest();

    ASSERT_FALSE(m_layerTreeHost.get());
    m_client.reset();
    if (m_timedOut) {
        FAIL() << "Test timed out";
        Platform::current()->compositorSupport()->shutdown();
        return;
    }
    afterTest();
    Platform::current()->compositorSupport()->shutdown();
}

}  // namespace WebKitTests
