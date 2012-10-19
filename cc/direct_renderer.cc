// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCDirectRenderer.h"

#include "CCMathUtil.h"
#include <public/WebTransformationMatrix.h>
#include <vector>

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
FloatRect DirectRenderer::quadVertexRect()
{
    return FloatRect(-0.5, -0.5, 1, 1);
}

// static
void DirectRenderer::quadRectTransform(WebKit::WebTransformationMatrix* quadRectTransform, const WebKit::WebTransformationMatrix& quadTransform, const FloatRect& quadRect)
{
    *quadRectTransform = quadTransform;
    quadRectTransform->translate(0.5 * quadRect.width() + quadRect.x(), 0.5 * quadRect.height() + quadRect.y());
    quadRectTransform->scaleNonUniform(quadRect.width(), quadRect.height());
}

// static
void DirectRenderer::initializeMatrices(DrawingFrame& frame, const IntRect& drawRect, bool flipY)
{
    if (flipY)
        frame.projectionMatrix = orthoProjectionMatrix(drawRect.x(), drawRect.maxX(), drawRect.maxY(), drawRect.y());
    else
        frame.projectionMatrix = orthoProjectionMatrix(drawRect.x(), drawRect.maxX(), drawRect.y(), drawRect.maxY());
    frame.windowMatrix = windowMatrix(0, 0, drawRect.width(), drawRect.height());
    frame.flippedY = flipY;
}

// static
IntRect DirectRenderer::moveScissorToWindowSpace(const DrawingFrame& frame, FloatRect scissorRect)
{
    IntRect scissorRectInCanvasSpace = enclosingIntRect(scissorRect);
    // The scissor coordinates must be supplied in viewport space so we need to offset
    // by the relative position of the top left corner of the current render pass.
    IntRect framebufferOutputRect = frame.currentRenderPass->outputRect();
    scissorRectInCanvasSpace.setX(scissorRectInCanvasSpace.x() - framebufferOutputRect.x());
    if (frame.flippedY && !frame.currentTexture)
        scissorRectInCanvasSpace.setY(framebufferOutputRect.height() - (scissorRectInCanvasSpace.maxY() - framebufferOutputRect.y()));
    else
        scissorRectInCanvasSpace.setY(scissorRectInCanvasSpace.y() - framebufferOutputRect.y());
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
    ScopedPtrHashMap<RenderPass::Id, CachedTexture>::const_iterator passIterator;
    for (passIterator = m_renderPassTextures.begin(); passIterator != m_renderPassTextures.end(); ++passIterator) {
        base::hash_map<RenderPass::Id, const RenderPass*>::const_iterator it = renderPassesInFrame.find(passIterator->first);
        if (it == renderPassesInFrame.end()) {
            passesToDelete.push_back(passIterator->first);
            continue;
        }

        const RenderPass* renderPassInFrame = it->second;
        const IntSize& requiredSize = renderPassTextureSize(renderPassInFrame);
        GLenum requiredFormat = renderPassTextureFormat(renderPassInFrame);
        CachedTexture* texture = passIterator->second;
        DCHECK(texture);

        if (texture->id() && (texture->size() != requiredSize || texture->format() != requiredFormat))
            texture->free();
    }

    // Delete RenderPass textures from the previous frame that will not be used again.
    for (size_t i = 0; i < passesToDelete.size(); ++i)
        m_renderPassTextures.erase(passesToDelete[i]);

    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i) {
        if (!m_renderPassTextures.contains(renderPassesInDrawOrder[i]->id())) {
          scoped_ptr<CachedTexture> texture = CachedTexture::create(m_resourceProvider);
            m_renderPassTextures.set(renderPassesInDrawOrder[i]->id(), texture.Pass());
        }
    }
}

void DirectRenderer::drawFrame(const RenderPassList& renderPassesInDrawOrder, const RenderPassIdHashMap& renderPassesById)
{
    const RenderPass* rootRenderPass = renderPassesInDrawOrder.back();
    DCHECK(rootRenderPass);

    DrawingFrame frame;
    frame.renderPassesById = &renderPassesById;
    frame.rootRenderPass = rootRenderPass;
    frame.rootDamageRect = capabilities().usingPartialSwap ? rootRenderPass->damageRect() : rootRenderPass->outputRect();
    frame.rootDamageRect.intersect(IntRect(IntPoint::zero(), viewportSize()));

    beginDrawingFrame(frame);
    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i)
        drawRenderPass(frame, renderPassesInDrawOrder[i]);
    finishDrawingFrame(frame);
}

void DirectRenderer::drawRenderPass(DrawingFrame& frame, const RenderPass* renderPass)
{
    if (!useRenderPass(frame, renderPass))
        return;

    frame.scissorRectInRenderPassSpace = frame.currentRenderPass->outputRect();
    if (frame.rootDamageRect != frame.rootRenderPass->outputRect()) {
        WebTransformationMatrix inverseTransformToRoot = frame.currentRenderPass->transformToRootTarget().inverse();
        frame.scissorRectInRenderPassSpace.intersect(MathUtil::projectClippedRect(inverseTransformToRoot, frame.rootDamageRect));
    }

    enableScissorTestRect(moveScissorToWindowSpace(frame, frame.scissorRectInRenderPassSpace));
    clearFramebuffer(frame);

    const QuadList& quadList = renderPass->quadList();
    for (QuadList::constBackToFrontIterator it = quadList.backToFrontBegin(); it != quadList.backToFrontEnd(); ++it) {
        FloatRect quadScissorRect = frame.scissorRectInRenderPassSpace;
        quadScissorRect.intersect((*it)->clippedRectInTarget());
        if (!quadScissorRect.isEmpty()) {
            enableScissorTestRect(moveScissorToWindowSpace(frame, quadScissorRect));
            drawQuad(frame, *it);
        }
    }

    CachedTexture* texture = m_renderPassTextures.get(renderPass->id());
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

    CachedTexture* texture = m_renderPassTextures.get(renderPass->id());
    DCHECK(texture);
    if (!texture->id() && !texture->allocate(Renderer::ImplPool, renderPassTextureSize(renderPass), renderPassTextureFormat(renderPass), ResourceProvider::TextureUsageFramebuffer))
        return false;

    return bindFramebufferToTexture(frame, texture, renderPass->outputRect());
}

bool DirectRenderer::haveCachedResourcesForRenderPassId(RenderPass::Id id) const
{
    CachedTexture* texture = m_renderPassTextures.get(id);
    return texture && texture->id() && texture->isComplete();
}

// static
IntSize DirectRenderer::renderPassTextureSize(const RenderPass* pass)
{
    return pass->outputRect().size();
}

// static
GLenum DirectRenderer::renderPassTextureFormat(const RenderPass*)
{
    return GL_RGBA;
}

}
