// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/heads_up_display_layer_impl.h"

#include "base/stringprintf.h"
#include "cc/debug_rect_history.h"
#include "cc/font_atlas.h"
#include "cc/frame_rate_counter.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/quad_sink.h"
#include "cc/texture_draw_quad.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace cc {

static inline SkPaint createPaint()
{
    // The SkCanvas is in RGBA but the shader is expecting BGRA, so we need to
    // swizzle our colors when drawing to the SkCanvas.
    SkColorMatrix swizzleMatrix;
    for (int i = 0; i < 20; ++i)
        swizzleMatrix.fMat[i] = 0;
    swizzleMatrix.fMat[0 + 5 * 2] = 1;
    swizzleMatrix.fMat[1 + 5 * 1] = 1;
    swizzleMatrix.fMat[2 + 5 * 0] = 1;
    swizzleMatrix.fMat[3 + 5 * 3] = 1;

    SkPaint paint;
    paint.setColorFilter(new SkColorMatrixFilter(swizzleMatrix))->unref();
    return paint;
}

HeadsUpDisplayLayerImpl::HeadsUpDisplayLayerImpl(int id)
    : LayerImpl(id)
    , m_averageFPS(0)
    , m_stdDeviation(0)
    , m_showFPSCounter(false)
{
}

HeadsUpDisplayLayerImpl::~HeadsUpDisplayLayerImpl()
{
}

void HeadsUpDisplayLayerImpl::setFontAtlas(scoped_ptr<FontAtlas> fontAtlas)
{
    m_fontAtlas = fontAtlas.Pass();
}

void HeadsUpDisplayLayerImpl::setShowFPSCounter(bool show)
{
    m_showFPSCounter = show;
}

void HeadsUpDisplayLayerImpl::willDraw(ResourceProvider* resourceProvider)
{
    LayerImpl::willDraw(resourceProvider);

    if (!m_hudTexture)
        m_hudTexture = ScopedResource::create(resourceProvider);

    // FIXME: Scale the HUD by deviceScale to make it more friendly under high DPI.

    if (m_hudTexture->size() != bounds())
        m_hudTexture->free();

    if (!m_hudTexture->id())
        m_hudTexture->allocate(Renderer::ImplPool, bounds(), GL_RGBA, ResourceProvider::TextureUsageAny);
}

void HeadsUpDisplayLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    if (!m_hudTexture->id())
        return;

    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());

    gfx::Rect quadRect(gfx::Point(), bounds());
    bool premultipliedAlpha = true;
    gfx::RectF uvRect(0, 0, 1, 1);
    bool flipped = false;
    quadSink.append(TextureDrawQuad::create(sharedQuadState, quadRect, m_hudTexture->id(), premultipliedAlpha, uvRect, flipped).PassAs<DrawQuad>(), appendQuadsData);
}

void HeadsUpDisplayLayerImpl::updateHudTexture(ResourceProvider* resourceProvider)
{
    if (!m_hudTexture->id())
        return;

    SkISize canvasSize;
    if (m_hudCanvas)
        canvasSize = m_hudCanvas->getDeviceSize();
    else
        canvasSize.set(0, 0);

    if (canvasSize.fWidth != bounds().width() || canvasSize.fHeight != bounds().height() || !m_hudCanvas)
        m_hudCanvas = make_scoped_ptr(skia::CreateBitmapCanvas(bounds().width(), bounds().height(), false /* opaque */));

    m_hudCanvas->clear(SkColorSetARGB(0, 0, 0, 0));
    drawHudContents(m_hudCanvas.get());

    const SkBitmap* bitmap = &m_hudCanvas->getDevice()->accessBitmap(false);
    SkAutoLockPixels locker(*bitmap);

    gfx::Rect layerRect(gfx::Point(), bounds());
    DCHECK(bitmap->config() == SkBitmap::kARGB_8888_Config);
    resourceProvider->setPixels(m_hudTexture->id(), static_cast<const uint8_t*>(bitmap->getPixels()), layerRect, layerRect, gfx::Vector2d());
}

void HeadsUpDisplayLayerImpl::didDraw(ResourceProvider* resourceProvider)
{
    LayerImpl::didDraw(resourceProvider);

    if (!m_hudTexture->id())
        return;

    // FIXME: the following assert will not be true when sending resources to a
    // parent compositor. We will probably need to hold on to m_hudTexture for
    // longer, and have several HUD textures in the pipeline.
    DCHECK(!resourceProvider->inUseByConsumer(m_hudTexture->id()));
}

void HeadsUpDisplayLayerImpl::didLoseContext()
{
    m_hudTexture.reset();
}

bool HeadsUpDisplayLayerImpl::layerIsAlwaysDamaged() const
{
    return true;
}

void HeadsUpDisplayLayerImpl::drawHudContents(SkCanvas* canvas)
{
    const LayerTreeSettings& settings = layerTreeHostImpl()->settings();

    if (settings.showPlatformLayerTree) {
        SkPaint paint = createPaint();
        paint.setColor(SkColorSetARGB(192, 0, 0, 0));
        canvas->drawRect(SkRect::MakeXYWH(0, 0, bounds().width(), bounds().height()), paint);
    }

    int platformLayerTreeTop = 0;

    if (m_showFPSCounter)
        platformLayerTreeTop = drawFPSCounter(canvas, layerTreeHostImpl()->fpsCounter());

    if (settings.showPlatformLayerTree && m_fontAtlas.get()) {
        std::string layerTree = layerTreeHostImpl()->layerTreeAsText();
        m_fontAtlas->drawText(canvas, createPaint(), layerTree, gfx::Point(2, platformLayerTreeTop), bounds());
    }

    if (settings.showDebugRects())
        drawDebugRects(canvas, layerTreeHostImpl()->debugRectHistory());
}

int HeadsUpDisplayLayerImpl::drawFPSCounter(SkCanvas* canvas, FrameRateCounter* fpsCounter)
{
    const int left = 2;
    const int top = 2;

    const int padding = 4;

    const int fontHeight = m_fontAtlas.get() ? m_fontAtlas->fontHeight() : 0;
    const int graphWidth = fpsCounter->timeStampHistorySize() - 3;
    const int graphHeight = 40;

    const int width = graphWidth + 2 * padding;
    const int height = fontHeight + graphHeight + 4 * padding + 2;

    SkPaint paint = createPaint();

    // Draw background.
    paint.setColor(SkColorSetARGB(215, 17, 17, 17));
    canvas->drawRect(SkRect::MakeXYWH(left, top, width, height), paint);

    SkRect textBounds = SkRect::MakeXYWH(left + padding, top + padding, graphWidth, fontHeight);
    SkRect graphBounds = SkRect::MakeXYWH(left + padding, textBounds.bottom() + 2 * padding, graphWidth, graphHeight);

    drawFPSCounterText(canvas, paint, fpsCounter, textBounds);
    drawFPSCounterGraph(canvas, paint, fpsCounter, graphBounds);

    return top + height;
}

void HeadsUpDisplayLayerImpl::drawFPSCounterText(SkCanvas* canvas, SkPaint& paint, FrameRateCounter* fpsCounter, SkRect bounds)
{
    // Update FPS text - not every frame so text is readable
    if (base::TimeDelta(fpsCounter->timeStampOfRecentFrame(0) - textUpdateTime).InSecondsF() > 0.25) {
        fpsCounter->getAverageFPSAndStandardDeviation(m_averageFPS, m_stdDeviation);
        textUpdateTime = fpsCounter->timeStampOfRecentFrame(0);
    }

    // Draw FPS text.
    if (m_fontAtlas.get()) {
        std::string fpsText = base::StringPrintf("FPS:%5.1f", m_averageFPS);
        std::string deviationText = base::StringPrintf("+/-%4.1f", m_stdDeviation);

        int deviationWidth = m_fontAtlas->textSize(deviationText).width();
        gfx::Size textArea(bounds.width(), bounds.height());

        paint.setColor(SK_ColorRED);
        m_fontAtlas->drawText(canvas, paint, fpsText, gfx::Point(bounds.left(), bounds.top()), textArea);
        m_fontAtlas->drawText(canvas, paint, deviationText, gfx::Point(bounds.right() - deviationWidth, bounds.top()), textArea);
    }
}

void HeadsUpDisplayLayerImpl::drawFPSCounterGraph(SkCanvas* canvas, SkPaint& paint, FrameRateCounter* fpsCounter, SkRect bounds)
{
    const double loFPS = 0;
    const double hiFPS = 80;

    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(1);

    // Draw top and bottom line.
    paint.setColor(SkColorSetRGB(150, 150, 150));
    canvas->drawLine(bounds.left(), bounds.top() - 1, bounds.right(), bounds.top() - 1, paint);
    canvas->drawLine(bounds.left(), bounds.bottom(), bounds.right(), bounds.bottom(), paint);

    // Draw 60fps line.
    paint.setColor(SkColorSetRGB(100, 100, 100));
    canvas->drawLine(bounds.left(), bounds.top() + bounds.height() / 4, bounds.right(), bounds.top() + bounds.height() / 4, paint);

    // Draw FPS graph.
    int x = 0;
    SkPath path;

    for (int i = 1; i < fpsCounter->timeStampHistorySize() - 1; ++i) {
        base::TimeDelta delta = fpsCounter->timeStampOfRecentFrame(i + 1) - fpsCounter->timeStampOfRecentFrame(i);

        // Skip plotting this particular instantaneous frame rate if it is not likely to have been valid.
        if (!fpsCounter->isBadFrameInterval(delta)) {
            double fps = 1.0 / delta.InSecondsF();

            // Clamp the FPS to the range we want to plot visually.
            double p = 1 - ((fps - loFPS) / (hiFPS - loFPS));
            if (p < 0)
                p = 0;
            if (p > 1)
                p = 1;

            // Plot this data point.
            SkPoint cur = SkPoint::Make(bounds.left() + x, bounds.top() + p * bounds.height());
            if (path.isEmpty())
                path.moveTo(cur);
            else
                path.lineTo(cur);
        }

        x += 1;
    }

    paint.setAntiAlias(true);
    paint.setColor(SK_ColorRED);
    canvas->drawPath(path, paint);
}

void HeadsUpDisplayLayerImpl::drawDebugRects(SkCanvas* canvas, DebugRectHistory* debugRectHistory)
{
    const std::vector<DebugRect>& debugRects = debugRectHistory->debugRects();

    for (size_t i = 0; i < debugRects.size(); ++i) {
        SkColor strokeColor = 0;
        SkColor fillColor = 0;

        switch (debugRects[i].type) {
        case PaintRectType:
            // Paint rects in red
            strokeColor = SkColorSetARGB(255, 255, 0, 0);
            fillColor = SkColorSetARGB(30, 255, 0, 0);
            break;
        case PropertyChangedRectType:
            // Property-changed rects in blue
            strokeColor = SkColorSetARGB(255, 255, 0, 0);
            fillColor = SkColorSetARGB(30, 0, 0, 255);
            break;
        case SurfaceDamageRectType:
            // Surface damage rects in yellow-orange
            strokeColor = SkColorSetARGB(255, 200, 100, 0);
            fillColor = SkColorSetARGB(30, 200, 100, 0);
            break;
        case ReplicaScreenSpaceRectType:
            // Screen space rects in green.
            strokeColor = SkColorSetARGB(255, 100, 200, 0);
            fillColor = SkColorSetARGB(30, 100, 200, 0);
            break;
        case ScreenSpaceRectType:
            // Screen space rects in purple.
            strokeColor = SkColorSetARGB(255, 100, 0, 200);
            fillColor = SkColorSetARGB(10, 100, 0, 200);
            break;
        case OccludingRectType:
            // Occluding rects in a reddish color.
            strokeColor = SkColorSetARGB(255, 200, 0, 100);
            fillColor = SkColorSetARGB(10, 200, 0, 100);
            break;
        }

        const gfx::RectF& rect = debugRects[i].rect;
        SkRect skRect = SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
        SkPaint paint = createPaint();
        paint.setColor(fillColor);
        canvas->drawRect(skRect, paint);

        paint.setColor(strokeColor);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(2);
        canvas->drawRect(skRect, paint);
    }
}

const char* HeadsUpDisplayLayerImpl::layerTypeAsString() const
{
    return "HeadsUpDisplayLayer";
}

}  // namespace cc
