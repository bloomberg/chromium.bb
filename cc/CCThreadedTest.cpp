// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCThreadedTest.h"

#include "CCActiveAnimation.h"
#include "CCAnimationTestCommon.h"
#include "CCInputHandler.h"
#include "CCLayerAnimationController.h"
#include "CCLayerImpl.h"
#include "CCLayerTreeHostImpl.h"
#include "CCOcclusionTrackerTestCommon.h"
#include "CCScopedThreadProxy.h"
#include "CCSingleThreadProxy.h"
#include "CCTextureUpdateQueue.h"
#include "CCThreadTask.h"
#include "CCTiledLayerTestCommon.h"
#include "CCTimingFunction.h"
#include "ContentLayerChromium.h"
#include "FakeWebCompositorOutputSurface.h"
#include "FakeWebGraphicsContext3D.h"
#include "LayerChromium.h"
#include <gmock/gmock.h>
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <public/WebThread.h>
#include <wtf/Locker.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;
using namespace WebKit;

namespace WebKitTests {

PassOwnPtr<CompositorFakeWebGraphicsContext3DWithTextureTracking> CompositorFakeWebGraphicsContext3DWithTextureTracking::create(Attributes attrs)
{
    return adoptPtr(new CompositorFakeWebGraphicsContext3DWithTextureTracking(attrs));
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
    m_usedTextures.add(texture);
}

int CompositorFakeWebGraphicsContext3DWithTextureTracking::numTextures() const { return static_cast<int>(m_textures.size()); }
int CompositorFakeWebGraphicsContext3DWithTextureTracking::texture(int i) const { return m_textures[i]; }
void CompositorFakeWebGraphicsContext3DWithTextureTracking::resetTextures() { m_textures.clear(); }

int CompositorFakeWebGraphicsContext3DWithTextureTracking::numUsedTextures() const { return static_cast<int>(m_usedTextures.size()); }
bool CompositorFakeWebGraphicsContext3DWithTextureTracking::usedTexture(int texture) const { return m_usedTextures.find(texture) != m_usedTextures.end(); }
void CompositorFakeWebGraphicsContext3DWithTextureTracking::resetUsedTextures() { m_usedTextures.clear(); }

CompositorFakeWebGraphicsContext3DWithTextureTracking::CompositorFakeWebGraphicsContext3DWithTextureTracking(Attributes attrs) : CompositorFakeWebGraphicsContext3D(attrs)
{
}

PassOwnPtr<WebCompositorOutputSurface> TestHooks::createOutputSurface()
{
    return FakeWebCompositorOutputSurface::create(CompositorFakeWebGraphicsContext3DWithTextureTracking::create(WebGraphicsContext3D::Attributes()));
}

PassOwnPtr<MockLayerTreeHostImpl> MockLayerTreeHostImpl::create(TestHooks* testHooks, const CCLayerTreeSettings& settings, CCLayerTreeHostImplClient* client)
{
    return adoptPtr(new MockLayerTreeHostImpl(testHooks, settings, client));
}

void MockLayerTreeHostImpl::beginCommit()
{
    CCLayerTreeHostImpl::beginCommit();
    m_testHooks->beginCommitOnCCThread(this);
}

void MockLayerTreeHostImpl::commitComplete()
{
    CCLayerTreeHostImpl::commitComplete();
    m_testHooks->commitCompleteOnCCThread(this);
}

bool MockLayerTreeHostImpl::prepareToDraw(FrameData& frame)
{
    bool result = CCLayerTreeHostImpl::prepareToDraw(frame);
    if (!m_testHooks->prepareToDrawOnCCThread(this))
        result = false;
    return result;
}

void MockLayerTreeHostImpl::drawLayers(const FrameData& frame)
{
    CCLayerTreeHostImpl::drawLayers(frame);
    m_testHooks->drawLayersOnCCThread(this);
}

void MockLayerTreeHostImpl::animateLayers(double monotonicTime, double wallClockTime)
{
    m_testHooks->willAnimateLayers(this, monotonicTime);
    CCLayerTreeHostImpl::animateLayers(monotonicTime, wallClockTime);
    m_testHooks->animateLayers(this, monotonicTime);
}

double MockLayerTreeHostImpl::lowFrequencyAnimationInterval() const
{
    return 1.0 / 60;
}

MockLayerTreeHostImpl::MockLayerTreeHostImpl(TestHooks* testHooks, const CCLayerTreeSettings& settings, CCLayerTreeHostImplClient* client)
    : CCLayerTreeHostImpl(settings, client)
    , m_testHooks(testHooks)
{
}

// Adapts CCLayerTreeHost for test. Injects MockLayerTreeHostImpl.
class MockLayerTreeHost : public WebCore::CCLayerTreeHost {
public:
    static PassOwnPtr<MockLayerTreeHost> create(TestHooks* testHooks, WebCore::CCLayerTreeHostClient* client, PassRefPtr<WebCore::LayerChromium> rootLayer, const WebCore::CCLayerTreeSettings& settings)
    {
        OwnPtr<MockLayerTreeHost> layerTreeHost(adoptPtr(new MockLayerTreeHost(testHooks, client, settings)));
        bool success = layerTreeHost->initialize();
        EXPECT_TRUE(success);
        layerTreeHost->setRootLayer(rootLayer);

        // LayerTreeHostImpl won't draw if it has 1x1 viewport.
        layerTreeHost->setViewportSize(IntSize(1, 1), IntSize(1, 1));

        layerTreeHost->rootLayer()->setLayerAnimationDelegate(testHooks);

        return layerTreeHost.release();
    }

    virtual PassOwnPtr<WebCore::CCLayerTreeHostImpl> createLayerTreeHostImpl(WebCore::CCLayerTreeHostImplClient* client)
    {
        return MockLayerTreeHostImpl::create(m_testHooks, settings(), client);
    }

    virtual void didAddAnimation() OVERRIDE
    {
        CCLayerTreeHost::didAddAnimation();
        m_testHooks->didAddAnimation();
    }

private:
    MockLayerTreeHost(TestHooks* testHooks, WebCore::CCLayerTreeHostClient* client, const WebCore::CCLayerTreeSettings& settings)
        : CCLayerTreeHost(client, settings)
        , m_testHooks(testHooks)
    {
    }

    TestHooks* m_testHooks;
};

// Implementation of CCLayerTreeHost callback interface.
class MockLayerTreeHostClient : public MockCCLayerTreeHostClient {
public:
    static PassOwnPtr<MockLayerTreeHostClient> create(TestHooks* testHooks)
    {
        return adoptPtr(new MockLayerTreeHostClient(testHooks));
    }

    virtual void willBeginFrame() OVERRIDE
    {
    }

    virtual void didBeginFrame() OVERRIDE
    {
    }

    virtual void animate(double monotonicTime) OVERRIDE
    {
        m_testHooks->animate(monotonicTime);
    }

    virtual void layout() OVERRIDE
    {
        m_testHooks->layout();
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale) OVERRIDE
    {
        m_testHooks->applyScrollAndScale(scrollDelta, scale);
    }

    virtual PassOwnPtr<WebCompositorOutputSurface> createOutputSurface() OVERRIDE
    {
        return m_testHooks->createOutputSurface();
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        m_testHooks->didRecreateOutputSurface(succeeded);
    }

    virtual PassOwnPtr<CCInputHandler> createInputHandler() OVERRIDE
    {
        return nullptr;
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
    explicit MockLayerTreeHostClient(TestHooks* testHooks) : m_testHooks(testHooks) { }

    TestHooks* m_testHooks;
};

class TimeoutTask : public WebThread::Task {
public:
    explicit TimeoutTask(CCThreadedTest* test)
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
    CCThreadedTest* m_test;
};

class BeginTask : public WebThread::Task {
public:
    explicit BeginTask(CCThreadedTest* test)
        : m_test(test)
    {
    }

    virtual ~BeginTask() { }
    virtual void run()
    {
        m_test->doBeginTest();
    }
private:
    CCThreadedTest* m_test;
};

CCThreadedTest::CCThreadedTest()
    : m_beginning(false)
    , m_endWhenBeginReturns(false)
    , m_timedOut(false)
    , m_finished(false)
    , m_scheduled(false)
    , m_started(false)
{ }

void CCThreadedTest::endTest()
{
    m_finished = true;

    // For the case where we endTest during beginTest(), set a flag to indicate that
    // the test should end the second beginTest regains control.
    if (m_beginning)
        m_endWhenBeginReturns = true;
    else
        m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::realEndTest));
}

void CCThreadedTest::endTestAfterDelay(int delayMilliseconds)
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::endTest));
}

void CCThreadedTest::postSetNeedsAnimateToMainThread()
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchSetNeedsAnimate));
}

void CCThreadedTest::postAddAnimationToMainThread()
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchAddAnimation));
}

void CCThreadedTest::postAddInstantAnimationToMainThread()
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchAddInstantAnimation));
}

void CCThreadedTest::postSetNeedsCommitToMainThread()
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchSetNeedsCommit));
}

void CCThreadedTest::postAcquireLayerTextures()
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchAcquireLayerTextures));
}

void CCThreadedTest::postSetNeedsRedrawToMainThread()
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchSetNeedsRedraw));
}

void CCThreadedTest::postSetNeedsAnimateAndCommitToMainThread()
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchSetNeedsAnimateAndCommit));
}

void CCThreadedTest::postSetVisibleToMainThread(bool visible)
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchSetVisible, visible));
}

void CCThreadedTest::postDidAddAnimationToMainThread()
{
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchDidAddAnimation));
}

void CCThreadedTest::doBeginTest()
{
    ASSERT(CCProxy::isMainThread());
    m_client = MockLayerTreeHostClient::create(this);

    RefPtr<LayerChromium> rootLayer = LayerChromium::create();
    m_layerTreeHost = MockLayerTreeHost::create(this, m_client.get(), rootLayer, m_settings);
    ASSERT_TRUE(m_layerTreeHost);
    rootLayer->setLayerTreeHost(m_layerTreeHost.get());
    m_layerTreeHost->setSurfaceReady();

    m_started = true;
    m_beginning = true;
    beginTest();
    m_beginning = false;
    if (m_endWhenBeginReturns)
        realEndTest();
}

void CCThreadedTest::timeout()
{
    m_timedOut = true;
    endTest();
}

void CCThreadedTest::scheduleComposite()
{
    if (!m_started || m_scheduled || m_finished)
        return;
    m_scheduled = true;
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::dispatchComposite));
}

void CCThreadedTest::realEndTest()
{
    ASSERT(CCProxy::isMainThread());
    WebKit::Platform::current()->currentThread()->exitRunLoop();
}

void CCThreadedTest::dispatchSetNeedsAnimate()
{
    ASSERT(CCProxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->setNeedsAnimate();
}

void CCThreadedTest::dispatchAddInstantAnimation()
{
    ASSERT(CCProxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost && m_layerTreeHost->rootLayer())
        addOpacityTransitionToLayer(*m_layerTreeHost->rootLayer(), 0, 0, 0.5, false);
}

void CCThreadedTest::dispatchAddAnimation()
{
    ASSERT(CCProxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost && m_layerTreeHost->rootLayer())
        addOpacityTransitionToLayer(*m_layerTreeHost->rootLayer(), 10, 0, 0.5, true);
}

void CCThreadedTest::dispatchSetNeedsAnimateAndCommit()
{
    ASSERT(CCProxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost) {
        m_layerTreeHost->setNeedsAnimate();
        m_layerTreeHost->setNeedsCommit();
    }
}

void CCThreadedTest::dispatchSetNeedsCommit()
{
    ASSERT(CCProxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->setNeedsCommit();
}

void CCThreadedTest::dispatchAcquireLayerTextures()
{
    ASSERT(CCProxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->acquireLayerTextures();
}

void CCThreadedTest::dispatchSetNeedsRedraw()
{
    ASSERT(CCProxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->setNeedsRedraw();
}

void CCThreadedTest::dispatchSetVisible(bool visible)
{
    ASSERT(CCProxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->setVisible(visible);
}

void CCThreadedTest::dispatchComposite()
{
    m_scheduled = false;
    if (m_layerTreeHost && !m_finished)
        m_layerTreeHost->composite();
}

void CCThreadedTest::dispatchDidAddAnimation()
{
    ASSERT(CCProxy::isMainThread());

    if (m_finished)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->didAddAnimation();
}

void CCThreadedTest::runTest(bool threaded)
{
    // For these tests, we will enable threaded animations.
    Platform::current()->compositorSupport()->setAcceleratedAnimationEnabled(true);

    if (threaded) {
        m_webThread = adoptPtr(WebKit::Platform::current()->createThread("CCThreadedTest"));
        Platform::current()->compositorSupport()->initialize(m_webThread.get());
    } else
        Platform::current()->compositorSupport()->initialize(0);

    ASSERT(CCProxy::isMainThread());
    m_mainThreadProxy = CCScopedThreadProxy::create(CCProxy::mainThread());

    initializeSettings(m_settings);

    m_beginTask = new BeginTask(this);
    WebKit::Platform::current()->currentThread()->postDelayedTask(m_beginTask, 0); // postDelayedTask takes ownership of the task
    m_timeoutTask = new TimeoutTask(this);
    WebKit::Platform::current()->currentThread()->postDelayedTask(m_timeoutTask, 5000);
    WebKit::Platform::current()->currentThread()->enterRunLoop();

    if (m_layerTreeHost && m_layerTreeHost->rootLayer())
        m_layerTreeHost->rootLayer()->setLayerTreeHost(0);
    m_layerTreeHost.clear();

    if (m_timeoutTask)
        m_timeoutTask->clearTest();

    ASSERT_FALSE(m_layerTreeHost.get());
    m_client.clear();
    if (m_timedOut) {
        FAIL() << "Test timed out";
        Platform::current()->compositorSupport()->shutdown();
        return;
    }
    afterTest();
    Platform::current()->compositorSupport()->shutdown();
}

} // namespace WebKitTests
