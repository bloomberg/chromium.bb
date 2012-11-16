// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/direct_renderer.h"

#include <vector>

#include "base/debug/trace_event.h"
#include "cc/math_util.h"
#include "ui/gfx/rect_conversions.h"
#include <public/WebTransformationMatrix.h>

using WebKit::WebTransformationMatrix;

static WebTransformationMatrix orthoProjectionMatrix(float left, float right, float bottom, float top)
{
    // Use the standard formula to map the clipping frustum to the cube from
    // [-1, -1, -1] to [1, 1, 1].
    float deltaX = right - left;
    float deltaY = top - bottom;
    WebTransformationMatrix proj;
    if (!deltaX || !deltaY)
        return proj;
    proj.setM11(2.0f / deltaX);
    proj.setM41(-(right + left) / deltaX);
    proj.setM22(2.0f / deltaY);
    proj.setM42(-(top + bottom) / deltaY);

    // Z component of vertices is always set to zero as we don't use the depth buffer
    // while drawing.
    proj.setM33(0);

    return proj;
}

static WebTransformationMatrix windowMatrix(int x, int y, int width, int height)
{
    WebTransformationMatrix canvas;

    // Map to window position and scale up to pixel coordinates.
    canvas.translate3d(x, y, 0);
    canvas.scale3d(width, height, 0);

    // Map from ([-1, -1] to [1, 1]) -> ([0, 0] to [1, 1])
    canvas.translate3d(0.5, 0.5, 0.5);
    canvas.scale3d(0.5, 0.5, 0.5);

    return canvas;
}

namespace cc {

DirectRenderer::DrawingFrame::DrawingFrame()
    : rootRenderPass(0)
    , currentRenderPass(0)
    , currentTexture(0)
    , flippedY(false)
{
}

DirectRenderer::DrawingFrame::~DrawingFrame()
{
}

//
// static
gfx::RectF DirectRenderer::quadVertexRect()
{
    return gfx::RectF(-0.5, -0.5, 1, 1);
}

// static
void DirectRenderer::quadRectTransform(WebKit::WebTransformationMatrix* quadRectTransform, const WebKit::WebTransformationMatrix& quadTransform, const gfx::RectF& quadRect)
{
    *quadRectTransform = quadTransform;
    quadRectTransform->translate(0.5 * quadRect.width() + quadRect.x(), 0.5 * quadRect.height() + quadRect.y());
    quadRectTransform->scaleNonUniform(quadRect.width(), quadRect.height());
}

// static
void DirectRenderer::initializeMatrices(DrawingFrame& frame, const gfx::Rect& drawRect, bool flipY)
{
    if (flipY)
        frame.projectionMatrix = orthoProjectionMatrix(drawRect.x(), drawRect.right(), drawRect.bottom(), drawRect.y());
    else
        frame.projectionMatrix = orthoProjectionMatrix(drawRect.x(), drawRect.right(), drawRect.y(), drawRect.bottom());
    frame.windowMatrix = windowMatrix(0, 0, drawRect.width(), drawRect.height());
    frame.flippedY = flipY;
}

// static
gfx::Rect DirectRenderer::moveScissorToWindowSpace(const DrawingFrame& frame, gfx::RectF scissorRect)
{
    gfx::Rect scissorRectInCanvasSpace = gfx::ToEnclosingRect(scissorRect);
    // The scissor coordinates must be supplied in viewport space so we need to offset
    // by the relative position of the top left corner of the current render pass.
    gfx::Rect framebufferOutputRect = frame.currentRenderPass->outputRect();
    scissorRectInCanvasSpace.set_x(scissorRectInCanvasSpace.x() - framebufferOutputRect.x());
    if (frame.flippedY && !frame.currentTexture)
        scissorRectInCanvasSpace.set_y(framebufferOutputRect.height() - (scissorRectInCanvasSpace.bottom() - framebufferOutputRect.y()));
    else
        scissorRectInCanvasSpace.set_y(scissorRectInCanvasSpace.y() - framebufferOutputRect.y());
    return scissorRectInCanvasSpace;
}

DirectRenderer::DirectRenderer(RendererClient* client, ResourceProvider* resourceProvider)
    : Renderer(client)
    , m_resourceProvider(resourceProvider)
{
}

DirectRenderer::~DirectRenderer()
{
}

void DirectRenderer::decideRenderPassAllocationsForFrame(const RenderPassList& renderPassesInDrawOrder)
{
    base::hash_map<RenderPass::Id, const RenderPass*> renderPassesInFrame;
    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i)
        renderPassesInFrame.insert(std::pair<RenderPass::Id, const RenderPass*>(renderPassesInDrawOrder[i]->id(), renderPassesInDrawOrder[i]));

    std::vector<RenderPass::Id> passesToDelete;
    ScopedPtrHashMap<RenderPass::Id, CachedResource>::const_iterator passIterator;
    for (passIterator = m_renderPassTextures.begin(); passIterator != m_renderPassTextures.end(); ++passIterator) {
        base::hash_map<RenderPass::Id, const RenderPass*>::const_iterator it = renderPassesInFrame.find(passIterator->first);
        if (it == renderPassesInFrame.end()) {
            passesToDelete.push_back(passIterator->first);
            continue;
        }

        const RenderPass* renderPassInFrame = it->second;
        const gfx::Size& requiredSize = renderPassTextureSize(renderPassInFrame);
        GLenum requiredFormat = renderPassTextureFormat(renderPassInFrame);
        CachedResource* texture = passIterator->second;
        DCHECK(texture);

        if (texture->id() && (texture->size() != requiredSize || texture->format() != requiredFormat))
            texture->Free();
    }

    // Delete RenderPass textures from the previous frame that will not be used again.
    for (size_t i = 0; i < passesToDelete.size(); ++i)
        m_renderPassTextures.erase(passesToDelete[i]);

    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i) {
        if (!m_renderPassTextures.contains(renderPassesInDrawOrder[i]->id())) {
          scoped_ptr<CachedResource> texture = CachedResource::create(m_resourceProvider);
            m_renderPassTextures.set(renderPassesInDrawOrder[i]->id(), texture.Pass());
        }
    }
}

void DirectRenderer::drawFrame(const RenderPassList& renderPassesInDrawOrder, const RenderPassIdHashMap& renderPassesById)
{
    TRACE_EVENT0("cc", "DirectRenderer::drawFrame");
    const RenderPass* rootRenderPass = renderPassesInDrawOrder.back();
    DCHECK(rootRenderPass);

    DrawingFrame frame;
    frame.renderPassesById = &renderPassesById;
    frame.rootRenderPass = rootRenderPass;
    frame.rootDamageRect = capabilities().usingPartialSwap ? rootRenderPass->damageRect() : rootRenderPass->outputRect();
    frame.rootDamageRect.Intersect(gfx::Rect(gfx::Point(), viewportSize()));

    beginDrawingFrame(frame);
    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i)
        drawRenderPass(frame, renderPassesInDrawOrder[i]);
    finishDrawingFrame(frame);
}

void DirectRenderer::drawRenderPass(DrawingFrame& frame, const RenderPass* renderPass)
{
    TRACE_EVENT0("cc", "DirectRenderer::drawRenderPass");
    if (!useRenderPass(frame, renderPass))
        return;

    frame.scissorRectInRenderPassSpace = frame.currentRenderPass->outputRect();
    if (frame.rootDamageRect != frame.rootRenderPass->outputRect()) {
        WebTransformationMatrix inverseTransformToRoot = frame.currentRenderPass->transformToRootTarget().inverse();
        gfx::RectF damageRectInRenderPassSpace = MathUtil::projectClippedRect(inverseTransformToRoot, frame.rootDamageRect);
        frame.scissorRectInRenderPassSpace.Intersect(damageRectInRenderPassSpace);
    }

    setScissorTestRect(moveScissorToWindowSpace(frame, frame.scissorRectInRenderPassSpace));
    clearFramebuffer(frame);

    const QuadList& quadList = renderPass->quadList();
    for (QuadList::constBackToFrontIterator it = quadList.backToFrontBegin(); it != quadList.backToFrontEnd(); ++it) {
        gfx::RectF quadScissorRect = gfx::IntersectRects(frame.scissorRectInRenderPassSpace, (*it)->clippedRectInTarget());
        if (!quadScissorRect.IsEmpty()) {
            setScissorTestRect(moveScissorToWindowSpace(frame, quadScissorRect));
            drawQuad(frame, *it);
        }
    }

    CachedResource* texture = m_renderPassTextures.get(renderPass->id());
    if (texture)
        texture->setIsComplete(!renderPass->hasOcclusionFromOutsideTargetSurface());
}

bool DirectRenderer::useRenderPass(DrawingFrame& frame, const RenderPass* renderPass)
{
    frame.currentRenderPass = renderPass;
    frame.currentTexture = 0;

    if (renderPass == frame.rootRenderPass) {
        bindFramebufferToOutputSurface(frame);
        initializeMatrices(frame, renderPass->outputRect(), flippedFramebuffer());
        setDrawViewportSize(renderPass->outputRect().size());
        return true;
    }

    CachedResource* texture = m_renderPassTextures.get(renderPass->id());
    DCHECK(texture);
    if (!texture->id() && !texture->Allocate(Renderer::ImplPool, renderPassTextureSize(renderPass), renderPassTextureFormat(renderPass), ResourceProvider::TextureUsageFramebuffer))
        return false;

    return bindFramebufferToTexture(frame, texture, renderPass->outputRect());
}

bool DirectRenderer::haveCachedResourcesForRenderPassId(RenderPass::Id id) const
{
    CachedResource* texture = m_renderPassTextures.get(id);
    return texture && texture->id() && texture->isComplete();
}

// static
gfx::Size DirectRenderer::renderPassTextureSize(const RenderPass* pass)
{
    return pass->outputRect().size();
}

// static
GLenum DirectRenderer::renderPassTextureFormat(const RenderPass*)
{
    return GL_RGBA;
}

}  // namespace cc
