// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/software_renderer.h"

#include "cc/compositor_frame_metadata.h"
#include "cc/quad_sink.h"
#include "cc/render_pass.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_software_output_device.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/tile_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace WebKit;

namespace cc {
namespace {

class SoftwareRendererTest : public testing::Test, public RendererClient {
public:
    SoftwareRendererTest()
        : m_shouldClearRootRenderPass(true)
    {
    }

    void initializeRenderer() {
        m_outputSurface = FakeOutputSurface::CreateSoftware(scoped_ptr<SoftwareOutputDevice>(new FakeSoftwareOutputDevice));
        m_resourceProvider = ResourceProvider::create(m_outputSurface.get());
        m_renderer = SoftwareRenderer::create(this, resourceProvider(), softwareDevice());
    }

    SoftwareOutputDevice* softwareDevice() const { return m_outputSurface->SoftwareDevice(); }
    FakeOutputSurface* outputSurface() const { return m_outputSurface.get(); }
    ResourceProvider* resourceProvider() const { return m_resourceProvider.get(); }
    SoftwareRenderer* renderer() const { return m_renderer.get(); }
    void setViewportSize(const gfx::Size& viewportSize) { m_viewportSize = viewportSize; }
    void setShouldClearRootRenderPass(bool clearRootRenderPass) { m_shouldClearRootRenderPass = clearRootRenderPass; }

    // RendererClient implementation.
    virtual const gfx::Size& deviceViewportSize() const OVERRIDE { return m_viewportSize; }
    virtual const LayerTreeSettings& settings() const OVERRIDE { return m_settings; }
    virtual void didLoseOutputSurface() OVERRIDE { }
    virtual void onSwapBuffersComplete() OVERRIDE { }
    virtual void setFullRootLayerDamage() OVERRIDE { }
    virtual void setManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { };
    virtual void enforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { };
    virtual bool hasImplThread() const OVERRIDE { return false; }
    virtual bool shouldClearRootRenderPass() const OVERRIDE { return m_shouldClearRootRenderPass; }
    virtual CompositorFrameMetadata makeCompositorFrameMetadata() const
        OVERRIDE { return CompositorFrameMetadata(); }

protected:
    scoped_ptr<FakeOutputSurface> m_outputSurface;
    scoped_ptr<ResourceProvider> m_resourceProvider;
    scoped_ptr<SoftwareRenderer> m_renderer;
    gfx::Size m_viewportSize;
    LayerTreeSettings m_settings;
    bool m_shouldClearRootRenderPass;
};

TEST_F(SoftwareRendererTest, solidColorQuad)
{
    gfx::Size outerSize(100, 100);
    int outerPixels = outerSize.width() * outerSize.height();
    gfx::Size innerSize(98, 98);
    gfx::Rect outerRect(gfx::Point(), outerSize);
    gfx::Rect innerRect(gfx::Point(1, 1), innerSize);
    setViewportSize(outerSize);

    initializeRenderer();

    scoped_ptr<SharedQuadState> sharedQuadState = SharedQuadState::Create();
    sharedQuadState->SetAll(gfx::Transform(), outerRect, outerRect, outerRect, false, 1.0);
    RenderPass::Id rootRenderPassId = RenderPass::Id(1, 1);
    scoped_ptr<TestRenderPass> rootRenderPass = TestRenderPass::Create();
    rootRenderPass->SetNew(rootRenderPassId, outerRect, gfx::Rect(), gfx::Transform());
    scoped_ptr<SolidColorDrawQuad> outerQuad = SolidColorDrawQuad::Create();
    outerQuad->SetNew(sharedQuadState.get(), outerRect, SK_ColorYELLOW);
    scoped_ptr<SolidColorDrawQuad> innerQuad = SolidColorDrawQuad::Create();
    innerQuad->SetNew(sharedQuadState.get(), innerRect, SK_ColorCYAN);
    rootRenderPass->AppendQuad(innerQuad.PassAs<DrawQuad>());
    rootRenderPass->AppendQuad(outerQuad.PassAs<DrawQuad>());

    RenderPassList list;
    list.push_back(rootRenderPass.PassAs<RenderPass>());
    renderer()->drawFrame(list);

    scoped_array<SkColor> pixels(new SkColor[deviceViewportSize().width() * deviceViewportSize().height()]);
    renderer()->getFramebufferPixels(pixels.get(), outerRect);

// FIXME: This fails on Android. Endianness maybe?
// Yellow: expects 0xFFFFFF00, was 0xFF00FFFF on android.
// Cyan:   expects 0xFF00FFFF, was 0xFFFFFF00 on android.
// http://crbug.com/154528
#ifndef OS_ANDROID
    EXPECT_EQ(SK_ColorYELLOW, pixels[0]);
    EXPECT_EQ(SK_ColorYELLOW, pixels[outerPixels - 1]);
    EXPECT_EQ(SK_ColorCYAN, pixels[outerSize.width() + 1]);
    EXPECT_EQ(SK_ColorCYAN, pixels[outerPixels - outerSize.width() - 2]);
#endif
}

TEST_F(SoftwareRendererTest, tileQuad)
{
    gfx::Size outerSize(100, 100);
    int outerPixels = outerSize.width() * outerSize.height();
    gfx::Size innerSize(98, 98);
    int innerPixels = innerSize.width() * innerSize.height();
    gfx::Rect outerRect(gfx::Point(), outerSize);
    gfx::Rect innerRect(gfx::Point(1, 1), innerSize);
    setViewportSize(outerSize);
    initializeRenderer();

    ResourceProvider::ResourceId resourceYellow = resourceProvider()->createResource(outerSize, GL_RGBA, ResourceProvider::TextureUsageAny);
    ResourceProvider::ResourceId resourceCyan = resourceProvider()->createResource(innerSize, GL_RGBA, ResourceProvider::TextureUsageAny);

    SkColor yellow = SK_ColorYELLOW;
    SkColor cyan = SK_ColorCYAN;
    scoped_array<SkColor> yellowPixels(new SkColor[outerPixels]);
    scoped_array<SkColor> cyanPixels(new SkColor[innerPixels]);
    for (int i = 0; i < outerPixels; i++)
      yellowPixels[i] = yellow;
    for (int i = 0; i < innerPixels; i++)
      cyanPixels[i] = cyan;

    resourceProvider()->setPixels(resourceYellow, reinterpret_cast<uint8_t*>(yellowPixels.get()), gfx::Rect(gfx::Point(), outerSize), gfx::Rect(gfx::Point(), outerSize), gfx::Vector2d());
    resourceProvider()->setPixels(resourceCyan, reinterpret_cast<uint8_t*>(cyanPixels.get()), gfx::Rect(gfx::Point(), innerSize), gfx::Rect(gfx::Point(), innerSize), gfx::Vector2d());

    gfx::Rect rect = gfx::Rect(gfx::Point(), deviceViewportSize());

    scoped_ptr<SharedQuadState> sharedQuadState = SharedQuadState::Create();
    sharedQuadState->SetAll(gfx::Transform(), outerRect, outerRect, outerRect, false, 1.0);
    RenderPass::Id rootRenderPassId = RenderPass::Id(1, 1);
    scoped_ptr<TestRenderPass> rootRenderPass = TestRenderPass::Create();
    rootRenderPass->SetNew(rootRenderPassId, gfx::Rect(gfx::Point(), deviceViewportSize()), gfx::Rect(), gfx::Transform());
    scoped_ptr<TileDrawQuad> outerQuad = TileDrawQuad::Create();
    outerQuad->SetNew(sharedQuadState.get(), outerRect, outerRect, resourceYellow, gfx::RectF(gfx::PointF(), outerSize), outerSize, false, false, false, false, false);
    scoped_ptr<TileDrawQuad> innerQuad = TileDrawQuad::Create();
    innerQuad->SetNew(sharedQuadState.get(), innerRect, innerRect, resourceCyan, gfx::RectF(gfx::PointF(), innerSize), innerSize, false, false, false, false, false);
    rootRenderPass->AppendQuad(innerQuad.PassAs<DrawQuad>());
    rootRenderPass->AppendQuad(outerQuad.PassAs<DrawQuad>());

    RenderPassList list;
    list.push_back(rootRenderPass.PassAs<RenderPass>());
    renderer()->drawFrame(list);

    scoped_array<SkColor> pixels(new SkColor[deviceViewportSize().width() * deviceViewportSize().height()]);
    renderer()->getFramebufferPixels(pixels.get(), outerRect);

    EXPECT_EQ(SK_ColorYELLOW, pixels[0]);
    EXPECT_EQ(SK_ColorYELLOW, pixels[outerPixels - 1]);
    EXPECT_EQ(SK_ColorCYAN, pixels[outerSize.width() + 1]);
    EXPECT_EQ(SK_ColorCYAN, pixels[outerPixels - outerSize.width() - 2]);
}

TEST_F(SoftwareRendererTest, shouldClearRootRenderPass)
{
    gfx::Rect viewportRect(gfx::Size(100, 100));
    size_t viewportPixels = viewportRect.width() * viewportRect.height();
    setViewportSize(viewportRect.size());
    setShouldClearRootRenderPass(false);
    initializeRenderer();

    RenderPassList list;
    scoped_array<SkColor> pixels(new SkColor[viewportPixels]);

    // Draw a fullscreen green quad in a first frame.
    RenderPass::Id rootClearPassId(1, 0);
    TestRenderPass* rootClearPass = addRenderPass(list, rootClearPassId, viewportRect, gfx::Transform());
    addQuad(rootClearPass, viewportRect, SK_ColorGREEN);

    renderer()->decideRenderPassAllocationsForFrame(list);
    renderer()->drawFrame(list);
    renderer()->getFramebufferPixels(pixels.get(), viewportRect);

    EXPECT_EQ(SK_ColorGREEN, pixels[0]);
    EXPECT_EQ(SK_ColorGREEN, pixels[viewportPixels - 1]);

    list.clear();

    // Draw a smaller magenta rect without filling the viewport in a separate frame.
    gfx::Rect smallerRect(20, 20, 60, 60);

    RenderPass::Id rootSmallerPassId(2, 0);
    TestRenderPass* rootSmallerPass = addRenderPass(list, rootSmallerPassId, viewportRect, gfx::Transform());
    addQuad(rootSmallerPass, smallerRect, SK_ColorMAGENTA);

    renderer()->decideRenderPassAllocationsForFrame(list);
    renderer()->drawFrame(list);
    renderer()->getFramebufferPixels(pixels.get(), viewportRect);

    // If we didn't clear, the borders should still be green.
    EXPECT_EQ(SK_ColorGREEN, pixels[0]);
    EXPECT_EQ(SK_ColorGREEN, pixels[viewportPixels - 1]);

    EXPECT_EQ(SK_ColorMAGENTA, pixels[smallerRect.y() * viewportRect.width() + smallerRect.x()]);
    EXPECT_EQ(SK_ColorMAGENTA, pixels[(smallerRect.bottom() - 1) * viewportRect.width() + smallerRect.right() - 1]);
}

}  // namespace
}  // namespace cc
