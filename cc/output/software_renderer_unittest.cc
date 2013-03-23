// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/software_renderer.h"

#include "cc/layers/quad_sink.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/software_output_device.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
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
        m_outputSurface = FakeOutputSurface::CreateSoftware(make_scoped_ptr(new SoftwareOutputDevice));
        resource_provider_ = ResourceProvider::Create(m_outputSurface.get());
        m_renderer = SoftwareRenderer::Create(this, m_outputSurface.get(), resourceProvider());
    }

    ResourceProvider* resourceProvider() const { return resource_provider_.get(); }
    SoftwareRenderer* renderer() const { return m_renderer.get(); }
    void setViewportSize(const gfx::Size& viewportSize) { m_viewportSize = viewportSize; }
    void setShouldClearRootRenderPass(bool clearRootRenderPass) { m_shouldClearRootRenderPass = clearRootRenderPass; }

    // RendererClient implementation.
    virtual gfx::Size DeviceViewportSize() const OVERRIDE { return m_viewportSize; }
    virtual const LayerTreeSettings& Settings() const OVERRIDE { return m_settings; }
    virtual void DidLoseOutputSurface() OVERRIDE { }
    virtual void OnSwapBuffersComplete() OVERRIDE { }
    virtual void SetFullRootLayerDamage() OVERRIDE { }
    virtual void SetManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { };
    virtual void EnforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE { };
    virtual bool HasImplThread() const OVERRIDE { return false; }
    virtual bool ShouldClearRootRenderPass() const OVERRIDE { return m_shouldClearRootRenderPass; }
    virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const
        OVERRIDE { return CompositorFrameMetadata(); }

protected:
    scoped_ptr<FakeOutputSurface> m_outputSurface;
    scoped_ptr<ResourceProvider> resource_provider_;
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
    gfx::Rect outerRect(outerSize);
    gfx::Rect innerRect(gfx::Point(1, 1), innerSize);
    setViewportSize(outerSize);

    initializeRenderer();

    scoped_ptr<SharedQuadState> sharedQuadState = SharedQuadState::Create();
    sharedQuadState->SetAll(gfx::Transform(), outerSize, outerRect, outerRect, false, 1.0);
    RenderPass::Id root_render_passId = RenderPass::Id(1, 1);
    scoped_ptr<TestRenderPass> root_render_pass = TestRenderPass::Create();
    root_render_pass->SetNew(root_render_passId, outerRect, outerRect, gfx::Transform());
    scoped_ptr<SolidColorDrawQuad> outerQuad = SolidColorDrawQuad::Create();
    outerQuad->SetNew(sharedQuadState.get(), outerRect, SK_ColorYELLOW);
    scoped_ptr<SolidColorDrawQuad> innerQuad = SolidColorDrawQuad::Create();
    innerQuad->SetNew(sharedQuadState.get(), innerRect, SK_ColorCYAN);
    root_render_pass->AppendQuad(innerQuad.PassAs<DrawQuad>());
    root_render_pass->AppendQuad(outerQuad.PassAs<DrawQuad>());

    RenderPassList list;
    list.push_back(root_render_pass.PassAs<RenderPass>());
    renderer()->DrawFrame(list);

    scoped_array<SkColor> pixels(new SkColor[DeviceViewportSize().width() * DeviceViewportSize().height()]);
    renderer()->GetFramebufferPixels(pixels.get(), outerRect);

// FIXME: This fails on Android. Endianness maybe?
// Yellow: expects 0xFFFFFF00, was 0xFF00FFFF on android.
// Cyan:   expects 0xFF00FFFF, was 0xFFFFFF00 on android.
// http://crbug.com/154528
#if !defined(OS_ANDROID)
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
    gfx::Rect outerRect(outerSize);
    gfx::Rect innerRect(gfx::Point(1, 1), innerSize);
    setViewportSize(outerSize);
    initializeRenderer();

    ResourceProvider::ResourceId resourceYellow = resourceProvider()->CreateResource(outerSize, GL_RGBA, ResourceProvider::TextureUsageAny);
    ResourceProvider::ResourceId resourceCyan = resourceProvider()->CreateResource(innerSize, GL_RGBA, ResourceProvider::TextureUsageAny);

    SkColor yellow = SK_ColorYELLOW;
    SkColor cyan = SK_ColorCYAN;
    scoped_array<SkColor> yellowPixels(new SkColor[outerPixels]);
    scoped_array<SkColor> cyanPixels(new SkColor[innerPixels]);
    for (int i = 0; i < outerPixels; i++)
      yellowPixels[i] = yellow;
    for (int i = 0; i < innerPixels; i++)
      cyanPixels[i] = cyan;

    resourceProvider()->SetPixels(resourceYellow, reinterpret_cast<uint8_t*>(yellowPixels.get()), gfx::Rect(outerSize), gfx::Rect(outerSize), gfx::Vector2d());
    resourceProvider()->SetPixels(resourceCyan, reinterpret_cast<uint8_t*>(cyanPixels.get()), gfx::Rect(innerSize), gfx::Rect(innerSize), gfx::Vector2d());

    gfx::Rect rootRect = gfx::Rect(DeviceViewportSize());

    scoped_ptr<SharedQuadState> sharedQuadState = SharedQuadState::Create();
    sharedQuadState->SetAll(gfx::Transform(), outerSize, outerRect, outerRect, false, 1.0);
    RenderPass::Id root_render_passId = RenderPass::Id(1, 1);
    scoped_ptr<TestRenderPass> root_render_pass = TestRenderPass::Create();
    root_render_pass->SetNew(root_render_passId, rootRect, rootRect, gfx::Transform());
    scoped_ptr<TileDrawQuad> outerQuad = TileDrawQuad::Create();
    outerQuad->SetNew(sharedQuadState.get(), outerRect, outerRect, resourceYellow, gfx::RectF(outerSize), outerSize, false);
    scoped_ptr<TileDrawQuad> innerQuad = TileDrawQuad::Create();
    innerQuad->SetNew(sharedQuadState.get(), innerRect, innerRect, resourceCyan, gfx::RectF(innerSize), innerSize, false);
    root_render_pass->AppendQuad(innerQuad.PassAs<DrawQuad>());
    root_render_pass->AppendQuad(outerQuad.PassAs<DrawQuad>());

    RenderPassList list;
    list.push_back(root_render_pass.PassAs<RenderPass>());
    renderer()->DrawFrame(list);

    scoped_array<SkColor> pixels(new SkColor[DeviceViewportSize().width() * DeviceViewportSize().height()]);
    renderer()->GetFramebufferPixels(pixels.get(), outerRect);

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

    renderer()->DecideRenderPassAllocationsForFrame(list);
    renderer()->DrawFrame(list);
    renderer()->GetFramebufferPixels(pixels.get(), viewportRect);

    EXPECT_EQ(SK_ColorGREEN, pixels[0]);
    EXPECT_EQ(SK_ColorGREEN, pixels[viewportPixels - 1]);

    list.clear();

    // Draw a smaller magenta rect without filling the viewport in a separate frame.
    gfx::Rect smallerRect(20, 20, 60, 60);

    RenderPass::Id rootSmallerPassId(2, 0);
    TestRenderPass* rootSmallerPass = addRenderPass(list, rootSmallerPassId, viewportRect, gfx::Transform());
    addQuad(rootSmallerPass, smallerRect, SK_ColorMAGENTA);

    renderer()->DecideRenderPassAllocationsForFrame(list);
    renderer()->DrawFrame(list);
    renderer()->GetFramebufferPixels(pixels.get(), viewportRect);

    // If we didn't clear, the borders should still be green.
    EXPECT_EQ(SK_ColorGREEN, pixels[0]);
    EXPECT_EQ(SK_ColorGREEN, pixels[viewportPixels - 1]);

    EXPECT_EQ(SK_ColorMAGENTA, pixels[smallerRect.y() * viewportRect.width() + smallerRect.x()]);
    EXPECT_EQ(SK_ColorMAGENTA, pixels[(smallerRect.bottom() - 1) * viewportRect.width() + smallerRect.right() - 1]);
}

}  // namespace
}  // namespace cc
