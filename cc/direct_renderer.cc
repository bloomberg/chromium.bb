// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/direct_renderer.h"

#include <vector>

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "cc/math_util.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

static gfx::Transform orthoProjectionMatrix(float left, float right, float bottom, float top)
{
    // Use the standard formula to map the clipping frustum to the cube from
    // [-1, -1, -1] to [1, 1, 1].
    float deltaX = right - left;
    float deltaY = top - bottom;
    gfx::Transform proj;
    if (!deltaX || !deltaY)
        return proj;
    proj.matrix().setDouble(0, 0, 2.0f / deltaX);
    proj.matrix().setDouble(0, 3, -(right + left) / deltaX);
    proj.matrix().setDouble(1, 1, 2.0f / deltaY);
    proj.matrix().setDouble(1, 3, -(top + bottom) / deltaY);

    // Z component of vertices is always set to zero as we don't use the depth buffer
    // while drawing.
    proj.matrix().setDouble(2, 2, 0);

    return proj;
}

static gfx::Transform windowMatrix(int x, int y, int width, int height)
{
    gfx::Transform canvas;

    // Map to window position and scale up to pixel coordinates.
    canvas.Translate3d(x, y, 0);
    canvas.Scale3d(width, height, 0);

    // Map from ([-1, -1] to [1, 1]) -> ([0, 0] to [1, 1])
    canvas.Translate3d(0.5, 0.5, 0.5);
    canvas.Scale3d(0.5, 0.5, 0.5);

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
void DirectRenderer::quadRectTransform(gfx::Transform* quadRectTransform, const gfx::Transform& quadTransform, const gfx::RectF& quadRect)
{
    *quadRectTransform = quadTransform;
    quadRectTransform->Translate(0.5 * quadRect.width() + quadRect.x(), 0.5 * quadRect.height() + quadRect.y());
    quadRectTransform->Scale(quadRect.width(), quadRect.height());
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
    gfx::Rect framebufferOutputRect = frame.currentRenderPass->output_rect;
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

void DirectRenderer::setEnlargePassTextureAmountForTesting(gfx::Vector2d amount)
{
    m_enlargePassTextureAmount = amount;
}

void DirectRenderer::decideRenderPassAllocationsForFrame(const RenderPassList& renderPassesInDrawOrder)
{
    base::hash_map<RenderPass::Id, const RenderPass*> renderPassesInFrame;
    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i)
        renderPassesInFrame.insert(std::pair<RenderPass::Id, const RenderPass*>(renderPassesInDrawOrder[i]->id, renderPassesInDrawOrder[i]));

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

        bool sizeAppropriate = texture->size().width() >= requiredSize.width() &&
                               texture->size().height() >= requiredSize.height();
        if (texture->id() && (!sizeAppropriate || texture->format() != requiredFormat))
            texture->Free();
    }

    // Delete RenderPass textures from the previous frame that will not be used again.
    for (size_t i = 0; i < passesToDelete.size(); ++i)
        m_renderPassTextures.erase(passesToDelete[i]);

    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i) {
        if (!m_renderPassTextures.contains(renderPassesInDrawOrder[i]->id)) {
          scoped_ptr<CachedResource> texture = CachedResource::create(m_resourceProvider);
            m_renderPassTextures.set(renderPassesInDrawOrder[i]->id, texture.Pass());
        }
    }
}

void DirectRenderer::drawFrame(RenderPassList& renderPassesInDrawOrder)
{
    TRACE_EVENT0("cc", "DirectRenderer::drawFrame");
    HISTOGRAM_COUNTS("Renderer4.renderPassCount", renderPassesInDrawOrder.size());

    const RenderPass* rootRenderPass = renderPassesInDrawOrder.last();
    DCHECK(rootRenderPass);

    DrawingFrame frame;
    frame.rootRenderPass = rootRenderPass;
    frame.rootDamageRect = capabilities().usingPartialSwap ? rootRenderPass->damage_rect : rootRenderPass->output_rect;
    frame.rootDamageRect.Intersect(gfx::Rect(gfx::Point(), viewportSize()));

    beginDrawingFrame(frame);
    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i)
        drawRenderPass(frame, renderPassesInDrawOrder[i]);
    finishDrawingFrame(frame);

    renderPassesInDrawOrder.clear();
}

gfx::RectF DirectRenderer::computeScissorRectForRenderPass(const DrawingFrame& frame)
{
    gfx::RectF renderPassScissor = frame.currentRenderPass->output_rect;

    if (frame.rootDamageRect == frame.rootRenderPass->output_rect)
        return renderPassScissor;

    gfx::Transform inverseTransform(gfx::Transform::kSkipInitialization);
    if (frame.currentRenderPass->transform_to_root_target.GetInverse(&inverseTransform)) {
        // Only intersect inverse-projected damage if the transform is invertible.
        gfx::RectF damageRectInRenderPassSpace = MathUtil::projectClippedRect(inverseTransform, frame.rootDamageRect);
        renderPassScissor.Intersect(damageRectInRenderPassSpace);
    }

    return renderPassScissor;
}

void DirectRenderer::setScissorStateForQuad(const DrawingFrame& frame, const DrawQuad& quad)
{
    if (quad.isClipped()) {
        gfx::RectF quadScissorRect = quad.clipRect();
        setScissorTestRect(moveScissorToWindowSpace(frame, quadScissorRect));
    }
    else
        ensureScissorTestDisabled();
}

void DirectRenderer::setScissorStateForQuadWithRenderPassScissor(const DrawingFrame& frame, const DrawQuad& quad, const gfx::RectF& renderPassScissor, bool* shouldSkipQuad)
{
    gfx::RectF quadScissorRect = renderPassScissor;

    if (quad.isClipped())
        quadScissorRect.Intersect(quad.clipRect());

    if (quadScissorRect.IsEmpty()) {
        *shouldSkipQuad = true;
        return;
    }

    *shouldSkipQuad = false;
    setScissorTestRect(moveScissorToWindowSpace(frame, quadScissorRect));
}

void DirectRenderer::finishDrawingQuadList()
{
}

void DirectRenderer::drawRenderPass(DrawingFrame& frame, const RenderPass* renderPass)
{
    TRACE_EVENT0("cc", "DirectRenderer::drawRenderPass");
    if (!useRenderPass(frame, renderPass))
        return;

    bool usingScissorAsOptimization = capabilities().usingPartialSwap;
    gfx::RectF renderPassScissor;

    if (usingScissorAsOptimization) {
        renderPassScissor = computeScissorRectForRenderPass(frame);
        setScissorTestRect(moveScissorToWindowSpace(frame, renderPassScissor));
    }

    if (frame.currentRenderPass != frame.rootRenderPass || m_client->shouldClearRootRenderPass())
        clearFramebuffer(frame);

    const QuadList& quadList = renderPass->quad_list;
    for (QuadList::constBackToFrontIterator it = quadList.backToFrontBegin(); it != quadList.backToFrontEnd(); ++it) {
        const DrawQuad& quad = *(*it);
        bool shouldSkipQuad = false;

        if (usingScissorAsOptimization)
            setScissorStateForQuadWithRenderPassScissor(frame, quad, renderPassScissor, &shouldSkipQuad);
        else
            setScissorStateForQuad(frame, quad);

        if (!shouldSkipQuad)
            drawQuad(frame, *it);
    }
    finishDrawingQuadList();

    CachedResource* texture = m_renderPassTextures.get(renderPass->id);
    if (texture)
        texture->setIsComplete(!renderPass->has_occlusion_from_outside_target_surface);
}

bool DirectRenderer::useRenderPass(DrawingFrame& frame, const RenderPass* renderPass)
{
    frame.currentRenderPass = renderPass;
    frame.currentTexture = 0;

    if (renderPass == frame.rootRenderPass) {
        bindFramebufferToOutputSurface(frame);
        initializeMatrices(frame, renderPass->output_rect, flippedFramebuffer());
        setDrawViewportSize(renderPass->output_rect.size());
        return true;
    }

    CachedResource* texture = m_renderPassTextures.get(renderPass->id);
    DCHECK(texture);

    gfx::Size size = renderPassTextureSize(renderPass);
    size.Enlarge(m_enlargePassTextureAmount.x(), m_enlargePassTextureAmount.y());
    if (!texture->id() && !texture->Allocate(size, renderPassTextureFormat(renderPass), ResourceProvider::TextureUsageFramebuffer))
        return false;

    return bindFramebufferToTexture(frame, texture, renderPass->output_rect);
}

bool DirectRenderer::haveCachedResourcesForRenderPassId(RenderPass::Id id) const
{
    CachedResource* texture = m_renderPassTextures.get(id);
    return texture && texture->id() && texture->isComplete();
}

// static
gfx::Size DirectRenderer::renderPassTextureSize(const RenderPass* pass)
{
    return pass->output_rect.size();
}

// static
GLenum DirectRenderer::renderPassTextureFormat(const RenderPass*)
{
    return GL_RGBA;
}

}  // namespace cc
