// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/gl_renderer.h"

#include "cc/compositor_frame_metadata.h"
#include "cc/draw_quad.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/resource_provider.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/transform.h"

using namespace WebKit;

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Expectation;
using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

namespace cc {
namespace {

class FrameCountingMemoryAllocationSettingContext : public TestWebGraphicsContext3D {
public:
    FrameCountingMemoryAllocationSettingContext() : m_frame(0) { }

    // WebGraphicsContext3D methods.

    // This method would normally do a glSwapBuffers under the hood.
    virtual void prepareTexture() { m_frame++; }
    virtual void setMemoryAllocationChangedCallbackCHROMIUM(WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) { m_memoryAllocationChangedCallback = callback; }
    virtual WebString getString(WebKit::WGC3Denum name)
    {
        if (name == GL_EXTENSIONS)
            return WebString("GL_CHROMIUM_set_visibility GL_CHROMIUM_gpu_memory_manager GL_CHROMIUM_discard_backbuffer");
        return WebString();
    }

    // Methods added for test.
    int frameCount() { return m_frame; }
    void setMemoryAllocation(WebGraphicsMemoryAllocation allocation)
    {
        m_memoryAllocationChangedCallback->onMemoryAllocationChanged(allocation);
    }

private:
    int m_frame;
    WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* m_memoryAllocationChangedCallback;
};

class FakeRendererClient : public RendererClient {
public:
    FakeRendererClient()
        : m_hostImpl(&m_proxy)
        , m_setFullRootLayerDamageCount(0)
        , m_lastCallWasSetVisibility(0)
        , m_rootLayer(LayerImpl::create(m_hostImpl.activeTree(), 1))
        , m_memoryAllocationLimitBytes(PrioritizedResourceManager::defaultMemoryAllocationLimit())
    {
        m_rootLayer->createRenderSurface();
        RenderPass::Id renderPassId = m_rootLayer->renderSurface()->renderPassId();
        scoped_ptr<RenderPass> rootRenderPass = RenderPass::Create();
        rootRenderPass->SetNew(renderPassId, gfx::Rect(), gfx::Rect(), gfx::Transform());
        m_renderPassesInDrawOrder.push_back(rootRenderPass.Pass());
    }

    // RendererClient methods.
    virtual const gfx::Size& deviceViewportSize() const OVERRIDE { static gfx::Size fakeSize(1, 1); return fakeSize; }
    virtual const LayerTreeSettings& settings() const OVERRIDE { static LayerTreeSettings fakeSettings; return fakeSettings; }
    virtual void didLoseOutputSurface() OVERRIDE { }
    virtual void onSwapBuffersComplete() OVERRIDE { }
    virtual void setFullRootLayerDamage() OVERRIDE { m_setFullRootLayerDamageCount++; }
    virtual void setManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { m_memoryAllocationLimitBytes = policy.bytesLimitWhenVisible; }
    virtual void enforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { if (m_lastCallWasSetVisibility) *m_lastCallWasSetVisibility = false; }
    virtual bool hasImplThread() const OVERRIDE { return false; }
    virtual bool shouldClearRootRenderPass() const OVERRIDE { return true; }
    virtual CompositorFrameMetadata makeCompositorFrameMetadata() const
        OVERRIDE { return CompositorFrameMetadata(); }

    // Methods added for test.
    int setFullRootLayerDamageCount() const { return m_setFullRootLayerDamageCount; }
    void setLastCallWasSetVisibilityPointer(bool* lastCallWasSetVisibility) { m_lastCallWasSetVisibility = lastCallWasSetVisibility; }

    RenderPass* rootRenderPass() { return m_renderPassesInDrawOrder.back(); }
    RenderPassList& renderPassesInDrawOrder() { return m_renderPassesInDrawOrder; }

    size_t memoryAllocationLimitBytes() const { return m_memoryAllocationLimitBytes; }

private:
    FakeImplProxy m_proxy;
    FakeLayerTreeHostImpl m_hostImpl;
    int m_setFullRootLayerDamageCount;
    bool* m_lastCallWasSetVisibility;
    scoped_ptr<LayerImpl> m_rootLayer;
    RenderPassList m_renderPassesInDrawOrder;
    size_t m_memoryAllocationLimitBytes;
};

class FakeRendererGL : public GLRenderer {
public:
    FakeRendererGL(RendererClient* client, OutputSurface* outputSurface, ResourceProvider* resourceProvider) : GLRenderer(client, outputSurface, resourceProvider) { }

    // GLRenderer methods.

    // Changing visibility to public.
    using GLRenderer::initialize;
    using GLRenderer::isBackbufferDiscarded;
    using GLRenderer::drawQuad;
    using GLRenderer::beginDrawingFrame;
    using GLRenderer::finishDrawingQuadList;
};

class GLRendererTest : public testing::Test {
protected:
    GLRendererTest()
        : m_suggestHaveBackbufferYes(1, true)
        , m_suggestHaveBackbufferNo(1, false)
        , m_outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new FrameCountingMemoryAllocationSettingContext())))
        , m_resourceProvider(ResourceProvider::create(m_outputSurface.get()))
        , m_renderer(&m_mockClient, m_outputSurface.get(), m_resourceProvider.get())
    {
    }

    virtual void SetUp()
    {
        m_renderer.initialize();
    }

    void swapBuffers()
    {
        m_renderer.swapBuffers();
    }

    FrameCountingMemoryAllocationSettingContext* context() { return static_cast<FrameCountingMemoryAllocationSettingContext*>(m_outputSurface->context3d()); }

    WebGraphicsMemoryAllocation m_suggestHaveBackbufferYes;
    WebGraphicsMemoryAllocation m_suggestHaveBackbufferNo;

    scoped_ptr<OutputSurface> m_outputSurface;
    FakeRendererClient m_mockClient;
    scoped_ptr<ResourceProvider> m_resourceProvider;
    FakeRendererGL m_renderer;
};

// Test GLRenderer discardBackbuffer functionality:
// Suggest recreating framebuffer when one already exists.
// Expected: it does nothing.
TEST_F(GLRendererTest, SuggestBackbufferYesWhenItAlreadyExistsShouldDoNothing)
{
    context()->setMemoryAllocation(m_suggestHaveBackbufferYes);
    EXPECT_EQ(0, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_FALSE(m_renderer.isBackbufferDiscarded());

    swapBuffers();
    EXPECT_EQ(1, context()->frameCount());
}

// Test GLRenderer discardBackbuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is not visible.
// Expected: it is discarded and damage tracker is reset.
TEST_F(GLRendererTest, SuggestBackbufferNoShouldDiscardBackbufferAndDamageRootLayerWhileNotVisible)
{
    m_renderer.setVisible(false);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_TRUE(m_renderer.isBackbufferDiscarded());
}

// Test GLRenderer discardBackbuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is visible.
// Expected: the allocation is ignored.
TEST_F(GLRendererTest, SuggestBackbufferNoDoNothingWhenVisible)
{
    m_renderer.setVisible(true);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(0, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_FALSE(m_renderer.isBackbufferDiscarded());
}


// Test GLRenderer discardBackbuffer functionality:
// Suggest discarding framebuffer when one does not exist.
// Expected: it does nothing.
TEST_F(GLRendererTest, SuggestBackbufferNoWhenItDoesntExistShouldDoNothing)
{
    m_renderer.setVisible(false);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_TRUE(m_renderer.isBackbufferDiscarded());

    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());
    EXPECT_TRUE(m_renderer.isBackbufferDiscarded());
}

// Test GLRenderer discardBackbuffer functionality:
// Begin drawing a frame while a framebuffer is discarded.
// Expected: will recreate framebuffer.
TEST_F(GLRendererTest, DiscardedBackbufferIsRecreatedForScopeDuration)
{
    m_renderer.setVisible(false);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_TRUE(m_renderer.isBackbufferDiscarded());
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());

    m_renderer.setVisible(true);
    m_renderer.drawFrame(m_mockClient.renderPassesInDrawOrder());
    EXPECT_FALSE(m_renderer.isBackbufferDiscarded());

    swapBuffers();
    EXPECT_EQ(1, context()->frameCount());
}

TEST_F(GLRendererTest, FramebufferDiscardedAfterReadbackWhenNotVisible)
{
    m_renderer.setVisible(false);
    context()->setMemoryAllocation(m_suggestHaveBackbufferNo);
    EXPECT_TRUE(m_renderer.isBackbufferDiscarded());
    EXPECT_EQ(1, m_mockClient.setFullRootLayerDamageCount());

    char pixels[4];
    m_renderer.drawFrame(m_mockClient.renderPassesInDrawOrder());
    EXPECT_FALSE(m_renderer.isBackbufferDiscarded());

    m_renderer.getFramebufferPixels(pixels, gfx::Rect(0, 0, 1, 1));
    EXPECT_TRUE(m_renderer.isBackbufferDiscarded());
    EXPECT_EQ(2, m_mockClient.setFullRootLayerDamageCount());
}

class ForbidSynchronousCallContext : public TestWebGraphicsContext3D {
public:
    ForbidSynchronousCallContext() { }

    virtual bool getActiveAttrib(WebGLId program, WGC3Duint index, ActiveInfo&) { ADD_FAILURE(); return false; }
    virtual bool getActiveUniform(WebGLId program, WGC3Duint index, ActiveInfo&) { ADD_FAILURE(); return false; }
    virtual void getAttachedShaders(WebGLId program, WGC3Dsizei maxCount, WGC3Dsizei* count, WebGLId* shaders) { ADD_FAILURE(); }
    virtual WGC3Dint getAttribLocation(WebGLId program, const WGC3Dchar* name) { ADD_FAILURE(); return 0; }
    virtual void getBooleanv(WGC3Denum pname, WGC3Dboolean* value) { ADD_FAILURE(); }
    virtual void getBufferParameteriv(WGC3Denum target, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }
    virtual Attributes getContextAttributes() { ADD_FAILURE(); return attributes_; }
    virtual WGC3Denum getError() { ADD_FAILURE(); return 0; }
    virtual void getFloatv(WGC3Denum pname, WGC3Dfloat* value) { ADD_FAILURE(); }
    virtual void getFramebufferAttachmentParameteriv(WGC3Denum target, WGC3Denum attachment, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }
    virtual void getIntegerv(WGC3Denum pname, WGC3Dint* value)
    {
        if (pname == GL_MAX_TEXTURE_SIZE)
            *value = 1024; // MAX_TEXTURE_SIZE is cached client side, so it's OK to query.
        else
            ADD_FAILURE();
    }

    // We allow querying the shader compilation and program link status in debug mode, but not release.
    virtual void getProgramiv(WebGLId program, WGC3Denum pname, WGC3Dint* value)
    {
#ifndef NDEBUG
        *value = 1;
#else
        ADD_FAILURE();
#endif
    }

    virtual void getShaderiv(WebGLId shader, WGC3Denum pname, WGC3Dint* value)
    {
#ifndef NDEBUG
        *value = 1;
#else
        ADD_FAILURE();
#endif
    }

    virtual WebString getString(WGC3Denum name)
    {
        // We allow querying the extension string.
        // FIXME: It'd be better to check that we only do this before starting any other expensive work (like starting a compilation)
        if (name != GL_EXTENSIONS)
            ADD_FAILURE();
        return WebString();
    }

    virtual WebString getProgramInfoLog(WebGLId program) { ADD_FAILURE(); return WebString(); }
    virtual void getRenderbufferParameteriv(WGC3Denum target, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }

    virtual WebString getShaderInfoLog(WebGLId shader) { ADD_FAILURE(); return WebString(); }
    virtual void getShaderPrecisionFormat(WGC3Denum shadertype, WGC3Denum precisiontype, WGC3Dint* range, WGC3Dint* precision) { ADD_FAILURE(); }
    virtual WebString getShaderSource(WebGLId shader) { ADD_FAILURE(); return WebString(); }
    virtual void getTexParameterfv(WGC3Denum target, WGC3Denum pname, WGC3Dfloat* value) { ADD_FAILURE(); }
    virtual void getTexParameteriv(WGC3Denum target, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }
    virtual void getUniformfv(WebGLId program, WGC3Dint location, WGC3Dfloat* value) { ADD_FAILURE(); }
    virtual void getUniformiv(WebGLId program, WGC3Dint location, WGC3Dint* value) { ADD_FAILURE(); }
    virtual WGC3Dint getUniformLocation(WebGLId program, const WGC3Dchar* name) { ADD_FAILURE(); return 0; }
    virtual void getVertexAttribfv(WGC3Duint index, WGC3Denum pname, WGC3Dfloat* value) { ADD_FAILURE(); }
    virtual void getVertexAttribiv(WGC3Duint index, WGC3Denum pname, WGC3Dint* value) { ADD_FAILURE(); }
    virtual WGC3Dsizeiptr getVertexAttribOffset(WGC3Duint index, WGC3Denum pname) { ADD_FAILURE(); return 0; }
};

// This test isn't using the same fixture as GLRendererTest, and you can't mix TEST() and TEST_F() with the same name, hence LRC2.
TEST(GLRendererTest2, initializationDoesNotMakeSynchronousCalls)
{
    FakeRendererClient mockClient;
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new ForbidSynchronousCallContext)));
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));
    FakeRendererGL renderer(&mockClient, outputSurface.get(), resourceProvider.get());

    EXPECT_TRUE(renderer.initialize());
}

class LoseContextOnFirstGetContext : public TestWebGraphicsContext3D {
public:
    LoseContextOnFirstGetContext()
        : m_contextLost(false)
    {
    }

    virtual bool makeContextCurrent() OVERRIDE
    {
        return !m_contextLost;
    }

    virtual void getProgramiv(WebGLId program, WGC3Denum pname, WGC3Dint* value) OVERRIDE
    {
        m_contextLost = true;
        *value = 0;
    }

    virtual void getShaderiv(WebGLId shader, WGC3Denum pname, WGC3Dint* value) OVERRIDE
    {
        m_contextLost = true;
        *value = 0;
    }

    virtual WGC3Denum getGraphicsResetStatusARB() OVERRIDE
    {
        return m_contextLost ? 1 : 0;
    }

private:
    bool m_contextLost;
};

TEST(GLRendererTest2, initializationWithQuicklyLostContextDoesNotAssert)
{
    FakeRendererClient mockClient;
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new LoseContextOnFirstGetContext)));
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));
    FakeRendererGL renderer(&mockClient, outputSurface.get(), resourceProvider.get());

    renderer.initialize();
}

class ContextThatDoesNotSupportMemoryManagmentExtensions : public TestWebGraphicsContext3D {
public:
    ContextThatDoesNotSupportMemoryManagmentExtensions() { }

    // WebGraphicsContext3D methods.

    // This method would normally do a glSwapBuffers under the hood.
    virtual void prepareTexture() { }
    virtual void setMemoryAllocationChangedCallbackCHROMIUM(WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) { }
    virtual WebString getString(WebKit::WGC3Denum name) { return WebString(); }
};

TEST(GLRendererTest2, initializationWithoutGpuMemoryManagerExtensionSupportShouldDefaultToNonZeroAllocation)
{
    FakeRendererClient mockClient;
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new ContextThatDoesNotSupportMemoryManagmentExtensions)));
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));
    FakeRendererGL renderer(&mockClient, outputSurface.get(), resourceProvider.get());

    renderer.initialize();

    EXPECT_GT(mockClient.memoryAllocationLimitBytes(), 0ul);
}

class ClearCountingContext : public TestWebGraphicsContext3D {
public:
    ClearCountingContext() : m_clear(0) { }

    virtual void clear(WGC3Dbitfield)
    {
        m_clear++;
    }

    int clearCount() const { return m_clear; }

private:
    int m_clear;
};

TEST(GLRendererTest2, opaqueBackground)
{
    FakeRendererClient mockClient;
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new ClearCountingContext)));
    ClearCountingContext* context = static_cast<ClearCountingContext*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));
    FakeRendererGL renderer(&mockClient, outputSurface.get(), resourceProvider.get());

    mockClient.rootRenderPass()->has_transparent_background = false;

    EXPECT_TRUE(renderer.initialize());

    renderer.drawFrame(mockClient.renderPassesInDrawOrder());

    // On DEBUG builds, render passes with opaque background clear to blue to
    // easily see regions that were not drawn on the screen.
#ifdef NDEBUG
    EXPECT_EQ(0, context->clearCount());
#else
    EXPECT_EQ(1, context->clearCount());
#endif
}

TEST(GLRendererTest2, transparentBackground)
{
    FakeRendererClient mockClient;
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new ClearCountingContext)));
    ClearCountingContext* context = static_cast<ClearCountingContext*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));
    FakeRendererGL renderer(&mockClient, outputSurface.get(), resourceProvider.get());

    mockClient.rootRenderPass()->has_transparent_background = true;

    EXPECT_TRUE(renderer.initialize());

    renderer.drawFrame(mockClient.renderPassesInDrawOrder());

    EXPECT_EQ(1, context->clearCount());
}

class VisibilityChangeIsLastCallTrackingContext : public TestWebGraphicsContext3D {
public:
    VisibilityChangeIsLastCallTrackingContext()
        : m_lastCallWasSetVisibility(0)
    {
    }

    // WebGraphicsContext3D methods.
    virtual void setVisibilityCHROMIUM(bool visible) {
        if (!m_lastCallWasSetVisibility)
            return;
        DCHECK(*m_lastCallWasSetVisibility == false);
        *m_lastCallWasSetVisibility = true;
    }
    virtual void flush() { if (m_lastCallWasSetVisibility) *m_lastCallWasSetVisibility = false; }
    virtual void deleteTexture(WebGLId) { if (m_lastCallWasSetVisibility) *m_lastCallWasSetVisibility = false; }
    virtual void deleteFramebuffer(WebGLId) { if (m_lastCallWasSetVisibility) *m_lastCallWasSetVisibility = false; }
    virtual void deleteRenderbuffer(WebGLId) { if (m_lastCallWasSetVisibility) *m_lastCallWasSetVisibility = false; }

    // This method would normally do a glSwapBuffers under the hood.
    virtual WebString getString(WebKit::WGC3Denum name)
    {
        if (name == GL_EXTENSIONS)
            return WebString("GL_CHROMIUM_set_visibility GL_CHROMIUM_gpu_memory_manager GL_CHROMIUM_discard_backbuffer");
        return WebString();
    }

    // Methods added for test.
    void setLastCallWasSetVisibilityPointer(bool* lastCallWasSetVisibility) { m_lastCallWasSetVisibility = lastCallWasSetVisibility; }

private:
    bool* m_lastCallWasSetVisibility;
};

TEST(GLRendererTest2, visibilityChangeIsLastCall)
{
    FakeRendererClient mockClient;
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new VisibilityChangeIsLastCallTrackingContext)));
    VisibilityChangeIsLastCallTrackingContext* context = static_cast<VisibilityChangeIsLastCallTrackingContext*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));
    FakeRendererGL renderer(&mockClient, outputSurface.get(), resourceProvider.get());

    EXPECT_TRUE(renderer.initialize());

    bool lastCallWasSetVisiblity = false;
    // Ensure that the call to setVisibilityCHROMIUM is the last call issue to the GPU
    // process, after glFlush is called, and after the RendererClient's enforceManagedMemoryPolicy
    // is called. Plumb this tracking between both the RenderClient and the Context by giving
    // them both a pointer to a variable on the stack.
    context->setLastCallWasSetVisibilityPointer(&lastCallWasSetVisiblity);
    mockClient.setLastCallWasSetVisibilityPointer(&lastCallWasSetVisiblity);
    renderer.setVisible(true);
    renderer.drawFrame(mockClient.renderPassesInDrawOrder());
    renderer.setVisible(false);
    EXPECT_TRUE(lastCallWasSetVisiblity);
}

class TextureStateTrackingContext : public TestWebGraphicsContext3D {
public:
    TextureStateTrackingContext()
        : m_activeTexture(GL_INVALID_ENUM)
    {
    }

    virtual WebString getString(WGC3Denum name)
    {
        if (name == GL_EXTENSIONS)
            return WebString("GL_OES_EGL_image_external");
        return WebString();
    }

    MOCK_METHOD3(texParameteri, void(WGC3Denum target, WGC3Denum pname, WGC3Dint param));
    MOCK_METHOD4(drawElements, void(WGC3Denum mode, WGC3Dsizei count, WGC3Denum type, WGC3Dintptr offset));

    virtual void activeTexture(WGC3Denum texture)
    {
        EXPECT_NE(texture, m_activeTexture);
        m_activeTexture = texture;
    }

    WGC3Denum activeTexture() const { return m_activeTexture; }

private:
    WGC3Denum m_activeTexture;
};

TEST(GLRendererTest2, activeTextureState)
{
    FakeRendererClient fakeClient;
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new TextureStateTrackingContext)));
    TextureStateTrackingContext* context = static_cast<TextureStateTrackingContext*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));
    FakeRendererGL renderer(&fakeClient, outputSurface.get(), resourceProvider.get());

    // During initialization we are allowed to set any texture parameters.
    EXPECT_CALL(*context, texParameteri(_, _, _)).Times(AnyNumber());
    EXPECT_TRUE(renderer.initialize());

    cc::RenderPass::Id id(1, 1);
    scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
    pass->SetNew(id, gfx::Rect(0, 0, 100, 100), gfx::Rect(0, 0, 100, 100), gfx::Transform());
    pass->AppendOneOfEveryQuadType(resourceProvider.get(), RenderPass::Id(2, 1));

    // Set up expected texture filter state transitions that match the quads
    // created in AppendOneOfEveryQuadType().
    Mock::VerifyAndClearExpectations(context);
    {
        InSequence sequence;

        // yuv_quad is drawn with the default linear filter.
        EXPECT_CALL(*context, drawElements(_, _, _, _));

        // tile_quad is drawn with GL_NEAREST because it is not transformed or
        // scaled.
        EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        EXPECT_CALL(*context, drawElements(_, _, _, _));

        // transformed_tile_quad uses GL_LINEAR.
        EXPECT_CALL(*context, drawElements(_, _, _, _));

        // scaled_tile_quad also uses GL_LINEAR.
        EXPECT_CALL(*context, drawElements(_, _, _, _));

        // The remaining quads also use GL_LINEAR because nearest neighbor
        // filtering is currently only used with tile quads.
        EXPECT_CALL(*context, drawElements(_, _, _, _)).Times(6);
    }

    cc::DirectRenderer::DrawingFrame drawingFrame;
    renderer.beginDrawingFrame(drawingFrame);
    EXPECT_EQ(context->activeTexture(), GL_TEXTURE0);

    for (cc::QuadList::backToFrontIterator it = pass->quad_list.backToFrontBegin();
         it != pass->quad_list.backToFrontEnd(); ++it) {
        renderer.drawQuad(drawingFrame, *it);
    }
    renderer.finishDrawingQuadList();
    EXPECT_EQ(context->activeTexture(), GL_TEXTURE0);
    Mock::VerifyAndClearExpectations(context);
}

class NoClearRootRenderPassFakeClient : public FakeRendererClient {
public:
    virtual bool shouldClearRootRenderPass() const OVERRIDE { return false; }
};

class NoClearRootRenderPassMockContext : public TestWebGraphicsContext3D {
public:
    MOCK_METHOD1(clear, void(WGC3Dbitfield mask));
    MOCK_METHOD4(drawElements, void(WGC3Denum mode, WGC3Dsizei count, WGC3Denum type, WGC3Dintptr offset));
};

TEST(GLRendererTest2, shouldClearRootRenderPass)
{
    NoClearRootRenderPassFakeClient mockClient;
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new NoClearRootRenderPassMockContext)));
    NoClearRootRenderPassMockContext* mockContext = static_cast<NoClearRootRenderPassMockContext*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));
    FakeRendererGL renderer(&mockClient, outputSurface.get(), resourceProvider.get());
    EXPECT_TRUE(renderer.initialize());

    gfx::Rect viewportRect(mockClient.deviceViewportSize());
    ScopedPtrVector<RenderPass>& renderPasses = mockClient.renderPassesInDrawOrder();
    renderPasses.clear();

    RenderPass::Id rootPassId(1, 0);
    TestRenderPass* rootPass = addRenderPass(renderPasses, rootPassId, viewportRect, gfx::Transform());
    addQuad(rootPass, viewportRect, SK_ColorGREEN);

    RenderPass::Id childPassId(2, 0);
    TestRenderPass* childPass = addRenderPass(renderPasses, childPassId, viewportRect, gfx::Transform());
    addQuad(childPass, viewportRect, SK_ColorBLUE);

    addRenderPassQuad(rootPass, childPass);

    // First render pass is not the root one, clearing should happen.
    EXPECT_CALL(*mockContext, clear(GL_COLOR_BUFFER_BIT))
        .Times(AtLeast(1));

    Expectation firstRenderPass = EXPECT_CALL(*mockContext, drawElements(_, _, _, _))
        .Times(1);

    // The second render pass is the root one, clearing should be prevented.
    EXPECT_CALL(*mockContext, clear(GL_COLOR_BUFFER_BIT))
        .Times(0)
        .After(firstRenderPass);

    EXPECT_CALL(*mockContext, drawElements(_, _, _, _))
        .Times(AnyNumber())
        .After(firstRenderPass);

    renderer.decideRenderPassAllocationsForFrame(mockClient.renderPassesInDrawOrder());
    renderer.drawFrame(mockClient.renderPassesInDrawOrder());

    // In multiple render passes all but the root pass should clear the framebuffer.
    Mock::VerifyAndClearExpectations(&mockContext);
}

class ScissorTestOnClearCheckingContext : public TestWebGraphicsContext3D {
public:
    ScissorTestOnClearCheckingContext() : m_scissorEnabled(false) { }

    virtual void clear(WGC3Dbitfield)
    {
        EXPECT_FALSE(m_scissorEnabled);
    }

    virtual void enable(WGC3Denum cap)
    {
        if (cap == GL_SCISSOR_TEST)
            m_scissorEnabled = true;
    }

    virtual void disable(WGC3Denum cap)
    {
        if (cap == GL_SCISSOR_TEST)
            m_scissorEnabled = false;
    }

private:
    bool m_scissorEnabled;
};

TEST(GLRendererTest2, scissorTestWhenClearing) {
    FakeRendererClient mockClient;
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new ScissorTestOnClearCheckingContext)));
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(outputSurface.get()));
    FakeRendererGL renderer(&mockClient, outputSurface.get(), resourceProvider.get());
    EXPECT_TRUE(renderer.initialize());
    EXPECT_FALSE(renderer.capabilities().usingPartialSwap);

    gfx::Rect viewportRect(mockClient.deviceViewportSize());
    ScopedPtrVector<RenderPass>& renderPasses = mockClient.renderPassesInDrawOrder();
    renderPasses.clear();

    gfx::Rect grandChildRect(25, 25);
    RenderPass::Id grandChildPassId(3, 0);
    TestRenderPass* grandChildPass = addRenderPass(renderPasses, grandChildPassId, grandChildRect, gfx::Transform());
    addClippedQuad(grandChildPass, grandChildRect, SK_ColorYELLOW);

    gfx::Rect childRect(50, 50);
    RenderPass::Id childPassId(2, 0);
    TestRenderPass* childPass = addRenderPass(renderPasses, childPassId, childRect, gfx::Transform());
    addQuad(childPass, childRect, SK_ColorBLUE);

    RenderPass::Id rootPassId(1, 0);
    TestRenderPass* rootPass = addRenderPass(renderPasses, rootPassId, viewportRect, gfx::Transform());
    addQuad(rootPass, viewportRect, SK_ColorGREEN);

    addRenderPassQuad(rootPass, childPass);
    addRenderPassQuad(childPass, grandChildPass);

    renderer.decideRenderPassAllocationsForFrame(mockClient.renderPassesInDrawOrder());
    renderer.drawFrame(mockClient.renderPassesInDrawOrder());
}

class OutputSurfaceMockContext : public TestWebGraphicsContext3D {
public:
    // Specifically override methods even if they are unused (used in conjunction with StrictMock).
    // We need to make sure that GLRenderer does not issue framebuffer-related GL calls directly. Instead these
    // are supposed to go through the OutputSurface abstraction.
    MOCK_METHOD0(ensureBackbufferCHROMIUM, void());
    MOCK_METHOD0(discardBackbufferCHROMIUM, void());
    MOCK_METHOD2(bindFramebuffer, void(WGC3Denum target, WebGLId framebuffer));
    MOCK_METHOD0(prepareTexture, void());
    MOCK_METHOD2(reshape, void(int width, int height));
    MOCK_METHOD4(drawElements, void(WGC3Denum mode, WGC3Dsizei count, WGC3Denum type, WGC3Dintptr offset));

    virtual WebString getString(WebKit::WGC3Denum name)
    {
        if (name == GL_EXTENSIONS)
            return WebString("GL_CHROMIUM_post_sub_buffer GL_CHROMIUM_discard_backbuffer");
        return WebString();
    }
};

class MockOutputSurface : public OutputSurface {
 public:
    MockOutputSurface()
        : OutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D>(new StrictMock<OutputSurfaceMockContext>)) { }
    virtual ~MockOutputSurface() { }

    MOCK_METHOD1(SendFrameToParentCompositor, void(CompositorFrame* frame));
    MOCK_METHOD0(EnsureBackbuffer, void());
    MOCK_METHOD0(DiscardBackbuffer, void());
    MOCK_METHOD1(Reshape, void(gfx::Size size));
    MOCK_METHOD0(BindFramebuffer, void());
    MOCK_METHOD1(PostSubBuffer, void(gfx::Rect rect));
    MOCK_METHOD0(SwapBuffers, void());
};

class MockOutputSurfaceTest : public testing::Test,
                              public FakeRendererClient {
protected:
    MockOutputSurfaceTest()
        : m_resourceProvider(ResourceProvider::create(&m_outputSurface))
        , m_renderer(this, &m_outputSurface, m_resourceProvider.get())
    {
    }

    virtual void SetUp()
    {
        EXPECT_TRUE(m_renderer.initialize());
    }

    void swapBuffers()
    {
        m_renderer.swapBuffers();
    }

    void drawFrame()
    {
        gfx::Rect viewportRect(deviceViewportSize());
        ScopedPtrVector<RenderPass>& renderPasses = renderPassesInDrawOrder();
        renderPasses.clear();

        RenderPass::Id renderPassId(1, 0);
        TestRenderPass* renderPass = addRenderPass(renderPasses, renderPassId, viewportRect, gfx::Transform());
        addQuad(renderPass, viewportRect, SK_ColorGREEN);

        EXPECT_CALL(m_outputSurface, EnsureBackbuffer())
            .WillRepeatedly(Return());

        EXPECT_CALL(m_outputSurface, Reshape(_))
            .Times(1);

        EXPECT_CALL(m_outputSurface, BindFramebuffer())
            .Times(1);

        EXPECT_CALL(*context(), drawElements(_, _, _, _))
            .Times(1);

        m_renderer.decideRenderPassAllocationsForFrame(renderPassesInDrawOrder());
        m_renderer.drawFrame(renderPassesInDrawOrder());
    }

    OutputSurfaceMockContext* context() { return static_cast<OutputSurfaceMockContext*>(m_outputSurface.context3d()); }

    StrictMock<MockOutputSurface> m_outputSurface;
    scoped_ptr<ResourceProvider> m_resourceProvider;
    FakeRendererGL m_renderer;
};

TEST_F(MockOutputSurfaceTest, DrawFrameAndSwap)
{
    drawFrame();

    EXPECT_CALL(m_outputSurface, SwapBuffers())
        .Times(1);
    m_renderer.swapBuffers();
}

class MockOutputSurfaceTestWithPartialSwap : public MockOutputSurfaceTest {
public:
    virtual const LayerTreeSettings& settings() const OVERRIDE
    {
        static LayerTreeSettings fakeSettings;
        fakeSettings.partialSwapEnabled = true;
        return fakeSettings;
    }
};

TEST_F(MockOutputSurfaceTestWithPartialSwap, DrawFrameAndSwap)
{
    drawFrame();

    EXPECT_CALL(m_outputSurface, PostSubBuffer(_))
        .Times(1);
    m_renderer.swapBuffers();
}

class MockOutputSurfaceTestWithSendCompositorFrame : public MockOutputSurfaceTest {
public:
    virtual const LayerTreeSettings& settings() const OVERRIDE
    {
        static LayerTreeSettings fakeSettings;
        fakeSettings.compositorFrameMessage = true;
        return fakeSettings;
    }
};

TEST_F(MockOutputSurfaceTestWithSendCompositorFrame, DrawFrame)
{
    EXPECT_CALL(m_outputSurface, SendFrameToParentCompositor(_))
        .Times(1);
    drawFrame();
}

}  // namespace
}  // namespace cc
