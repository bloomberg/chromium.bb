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
    scoped_ptr<cc::SharedQuadState> sharedState = cc::SharedQuadState::create(WebTransformationMatrix(), rect, rect, 1);

    appendQuad(cc::CheckerboardDrawQuad::create(sharedState.get(), rect, SK_ColorRED).PassAs<DrawQuad>());
    appendQuad(cc::DebugBorderDrawQuad::create(sharedState.get(), rect, SK_ColorRED, 1).Pass().PassAs<DrawQuad>());
    appendQuad(cc::IOSurfaceDrawQuad::create(sharedState.get(), rect, opaqueRect, gfx::Size(50, 50), 1, cc::IOSurfaceDrawQuad::Flipped).PassAs<DrawQuad>());

    cc::RenderPass::Id passId(1, 1);
    appendQuad(cc::RenderPassDrawQuad::create(sharedState.get(), rect, passId, false, 0, rect, 0, 0, 0, 0).PassAs<DrawQuad>());

    appendQuad(cc::SolidColorDrawQuad::create(sharedState.get(), rect, SK_ColorRED).PassAs<DrawQuad>());
    appendQuad(cc::StreamVideoDrawQuad::create(sharedState.get(), rect, opaqueRect, 1, WebKit::WebTransformationMatrix()).PassAs<DrawQuad>());
    appendQuad(cc::TextureDrawQuad::create(sharedState.get(), rect, opaqueRect, textureResource, false, rect, false).PassAs<DrawQuad>());

    appendQuad(cc::TileDrawQuad::create(sharedState.get(), rect, opaqueRect, textureResource, gfx::Vector2d(), gfx::Size(100, 100), false, false, false, false, false).PassAs<DrawQuad>());

    cc::VideoLayerImpl::FramePlane planes[3];
    for (int i = 0; i < 3; ++i) {
        planes[i].resourceId = resourceProvider->createResourceFromExternalTexture(1);
        planes[i].size = gfx::Size(100, 100);
        planes[i].format = GL_LUMINANCE;
    }
    appendQuad(cc::YUVVideoDrawQuad::create(sharedState.get(), rect, opaqueRect, gfx::Size(100, 100), planes[0], planes[1], planes[2]).PassAs<DrawQuad>());
    appendSharedQuadState(sharedState.Pass());
}

}  // namespace WebKitTests
