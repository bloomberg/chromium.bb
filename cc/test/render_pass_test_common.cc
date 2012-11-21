// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/render_pass_test_common.h"

#include "cc/checkerboard_draw_quad.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/io_surface_draw_quad.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/shared_quad_state.h"
#include "cc/stream_video_draw_quad.h"
#include "cc/texture_draw_quad.h"
#include "cc/tile_draw_quad.h"
#include "cc/yuv_video_draw_quad.h"
#include "cc/resource_provider.h"
#include <public/WebTransformationMatrix.h>

namespace WebKitTests {

using cc::DrawQuad;
using WebKit::WebTransformationMatrix;

void TestRenderPass::appendOneOfEveryQuadType(cc::ResourceProvider* resourceProvider)
{
    gfx::Rect rect(0, 0, 100, 100);
    gfx::Rect opaqueRect(10, 10, 80, 80);
    cc::ResourceProvider::ResourceId textureResource = resourceProvider->createResourceFromExternalTexture(1);
    scoped_ptr<cc::SharedQuadState> sharedState = cc::SharedQuadState::Create();
    sharedState->SetAll(WebTransformationMatrix(), rect, rect, 1);

    scoped_ptr<cc::CheckerboardDrawQuad> checkerboardQuad = cc::CheckerboardDrawQuad::Create();
    checkerboardQuad->SetNew(sharedState.get(), rect, SK_ColorRED);
    appendQuad(checkerboardQuad.PassAs<DrawQuad>());

    scoped_ptr<cc::DebugBorderDrawQuad> debugBorderQuad = cc::DebugBorderDrawQuad::Create();
    debugBorderQuad->SetNew(sharedState.get(), rect, SK_ColorRED, 1);
    appendQuad(debugBorderQuad.PassAs<DrawQuad>());

    scoped_ptr<cc::IOSurfaceDrawQuad> ioSurfaceQuad = cc::IOSurfaceDrawQuad::Create();
    ioSurfaceQuad->SetNew(sharedState.get(), rect, opaqueRect, gfx::Size(50, 50), 1, cc::IOSurfaceDrawQuad::FLIPPED);
    appendQuad(ioSurfaceQuad.PassAs<DrawQuad>());

    scoped_ptr<cc::RenderPassDrawQuad> renderPassQuad = cc::RenderPassDrawQuad::Create();
    renderPassQuad->SetNew(sharedState.get(), rect, cc::RenderPass::Id(1, 1), false, 0, rect, 0, 0, 0, 0);
    appendQuad(renderPassQuad.PassAs<DrawQuad>());

    scoped_ptr<cc::SolidColorDrawQuad> solidColorQuad = cc::SolidColorDrawQuad::Create();
    solidColorQuad->SetNew(sharedState.get(), rect, SK_ColorRED);
    appendQuad(solidColorQuad.PassAs<DrawQuad>());

    scoped_ptr<cc::StreamVideoDrawQuad> streamVideoQuad = cc::StreamVideoDrawQuad::Create();
    streamVideoQuad->SetNew(sharedState.get(), rect, opaqueRect, 1, WebKit::WebTransformationMatrix());
    appendQuad(streamVideoQuad.PassAs<DrawQuad>());

    scoped_ptr<cc::TextureDrawQuad> textureQuad = cc::TextureDrawQuad::Create();
    textureQuad->SetNew(sharedState.get(), rect, opaqueRect, textureResource, false, rect, false);
    appendQuad(textureQuad.PassAs<DrawQuad>());

    scoped_ptr<cc::TileDrawQuad> tileQuad = cc::TileDrawQuad::Create();
    tileQuad->SetNew(sharedState.get(), rect, opaqueRect, textureResource, gfx::RectF(0, 0, 100, 100), gfx::Size(100, 100), false, false, false, false, false);
    appendQuad(tileQuad.PassAs<DrawQuad>());

    cc::VideoLayerImpl::FramePlane planes[3];
    for (int i = 0; i < 3; ++i) {
        planes[i].resourceId = resourceProvider->createResourceFromExternalTexture(1);
        planes[i].size = gfx::Size(100, 100);
        planes[i].format = GL_LUMINANCE;
    }
    scoped_ptr<cc::YUVVideoDrawQuad> yuvQuad = cc::YUVVideoDrawQuad::Create();
    yuvQuad->SetNew(sharedState.get(), rect, opaqueRect, gfx::Size(100, 100), planes[0], planes[1], planes[2]);
    appendQuad(yuvQuad.PassAs<DrawQuad>());

    appendSharedQuadState(sharedState.Pass());
}

}  // namespace WebKitTests
