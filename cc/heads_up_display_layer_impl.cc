// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/heads_up_display_layer_impl.h"

#include <limits>

#include "base/stringprintf.h"
#include "cc/debug_colors.h"
#include "cc/debug_rect_history.h"
#include "cc/font_atlas.h"
#include "cc/frame_rate_counter.h"
#include "cc/layer_tree_impl.h"
#include "cc/quad_sink.h"
#include "cc/renderer.h"
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
    SkPaint paint;
#if (SK_R32_SHIFT || SK_B32_SHIFT != 16)
    // The SkCanvas is in RGBA but the shader is expecting BGRA, so we need to
    // swizzle our colors when drawing to the SkCanvas.
    SkColorMatrix swizzleMatrix;
    for (int i = 0; i < 20; ++i)
        swizzleMatrix.fMat[i] = 0;
    swizzleMatrix.fMat[0 + 5 * 2] = 1;
    swizzleMatrix.fMat[1 + 5 * 1] = 1;
    swizzleMatrix.fMat[2 + 5 * 0] = 1;
    swizzleMatrix.fMat[3 + 5 * 3] = 1;

    skia::RefPtr<SkColorMatrixFilter> filter =
        skia::AdoptRef(new SkColorMatrixFilter(swizzleMatrix));
    paint.setColorFilter(filter.get());
#endif
    return paint;
}

HeadsUpDisplayLayerImpl::HeadsUpDisplayLayerImpl(LayerTreeImpl* treeImpl, int id)
    : LayerImpl(treeImpl, id)
    , m_averageFPS(0)
    , m_minFPS(0)
    , m_maxFPS(0)
{
}

HeadsUpDisplayLayerImpl::~HeadsUpDisplayLayerImpl()
{
}

void HeadsUpDisplayLayerImpl::setFontAtlas(scoped_ptr<FontAtlas> fontAtlas)
{
    m_fontAtlas = fontAtlas.Pass();
}

void HeadsUpDisplayLayerImpl::willDraw(ResourceProvider* resourceProvider)
{
    LayerImpl::willDraw(resourceProvider);

    if (!m_hudTexture)
        m_hudTexture = ScopedResource::create(resourceProvider);

    // TODO(danakj): Scale the HUD by deviceScale to make it more friendly under high DPI.

    // TODO(danakj): The HUD could swap between two textures instead of creating a texture every frame in ubercompositor.
    if (m_hudTexture->size() != bounds() || resourceProvider->inUseByConsumer(m_hudTexture->id()))
        m_hudTexture->Free();

    if (!m_hudTexture->id()) {
        m_hudTexture->Allocate(bounds(), GL_RGBA, ResourceProvider::TextureUsageAny);
        // TODO(epenner): This texture was being used before setPixels was called,
        // which is now not allowed (it's an uninitialized read). This should be fixed
        // and this allocateForTesting() removed.
        // http://crbug.com/166784
        resourceProvider->allocateForTesting(m_hudTexture->id());
    }
}

void HeadsUpDisplayLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    if (!m_hudTexture->id())
        return;

    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());

    gfx::Rect quadRect(gfx::Point(), bounds());
    gfx::Rect opaqueRect(contentsOpaque() ? quadRect : gfx::Rect());
    bool premultipliedAlpha = true;
    gfx::RectF uvRect(0, 0, 1, 1);
    const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    bool flipped = false;
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(sharedQuadState, quadRect, opaqueRect, m_hudTexture->id(), premultipliedAlpha, uvRect, vertex_opacity, flipped);
    quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData);
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

void HeadsUpDisplayLayerImpl::didLoseOutputSurface()
{
    m_hudTexture.reset();
}

bool HeadsUpDisplayLayerImpl::layerIsAlwaysDamaged() const
{
    return true;
}

void HeadsUpDisplayLayerImpl::drawHudContents(SkCanvas* canvas)
{
    const LayerTreeDebugState& debugState = layerTreeImpl()->debug_state();

    if (debugState.showPlatformLayerTree) {
        SkPaint paint = createPaint();
        paint.setColor(SkColorSetARGB(192, 0, 0, 0));
        canvas->drawRect(SkRect::MakeXYWH(0, 0, bounds().width(), bounds().height()), paint);
    }

    int platformLayerTreeTop = 0;

    if (debugState.showFPSCounter)
        platformLayerTreeTop = drawFPSCounter(canvas, layerTreeImpl()->frame_rate_counter());

    if (debugState.showPlatformLayerTree && m_fontAtlas) {
        std::string layerTree = layerTreeImpl()->layer_tree_as_text();
        m_fontAtlas->drawText(canvas, createPaint(), layerTree, gfx::Point(2, platformLayerTreeTop), bounds());
    }

    if (debugState.showHudRects())
        drawDebugRects(canvas, layerTreeImpl()->debug_rect_history());
}

int HeadsUpDisplayLayerImpl::drawFPSCounter(SkCanvas* canvas, FrameRateCounter* fpsCounter)
{
    const int padding = 4;
    const int gap = 6;

    const int fontHeight = m_fontAtlas.get() ? m_fontAtlas->fontHeight() : 0;

    const int graphWidth = fpsCounter->timeStampHistorySize() - 3;
    const int graphHeight = 40;

    const int histogramWidth = 37;

    const int width = graphWidth + histogramWidth + 4 * padding;
    const int height = fontHeight + graphHeight + 4 * padding + 2;

    const int left = bounds().width() - width - 2;
    const int top = 2;

    SkPaint paint = createPaint();

    // Draw background.
    paint.setColor(SkColorSetARGB(215, 17, 17, 17));
    canvas->drawRect(SkRect::MakeXYWH(left, top, width, height), paint);

    SkRect textBounds = SkRect::MakeXYWH(left + padding, top + padding, graphWidth + histogramWidth + gap + 2, fontHeight);
    SkRect graphBounds = SkRect::MakeXYWH(left + padding, textBounds.bottom() + 2 * padding, graphWidth, graphHeight);
    SkRect histogramBounds = SkRect::MakeXYWH(graphBounds.right() + gap, graphBounds.top(), histogramWidth, graphHeight);

    drawFPSCounterText(canvas, paint, fpsCounter, textBounds);
    drawFPSCounterGraphAndHistogram(canvas, paint, fpsCounter, graphBounds, histogramBounds);

    return top + height;
}

void HeadsUpDisplayLayerImpl::drawFPSCounterText(SkCanvas* canvas, SkPaint& paint, FrameRateCounter* fpsCounter, SkRect bounds)
{
    // Update FPS text - not every frame so text is readable
    base::TimeTicks now = base::TimeTicks::Now();
    if (base::TimeDelta(now - textUpdateTime).InSecondsF() > 0.25) {
        m_averageFPS = fpsCounter->getAverageFPS();
        textUpdateTime = now;
    }

    // Draw FPS text.
    if (m_fontAtlas.get()) {
        std::string fpsText = base::StringPrintf("FPS:%5.1f", m_averageFPS);
        std::string minMaxText = base::StringPrintf("%.0f-%.0f", std::min( m_minFPS, m_maxFPS), m_maxFPS);

        int minMaxWidth = m_fontAtlas->textSize(minMaxText).width();
        gfx::Size textArea(bounds.width(), bounds.height());

        paint.setColor(SK_ColorRED);
        m_fontAtlas->drawText(canvas, paint, fpsText, gfx::Point(bounds.left(), bounds.top()), textArea);
        m_fontAtlas->drawText(canvas, paint, minMaxText, gfx::Point(bounds.right() - minMaxWidth, bounds.top()), textArea);
    }
}

void HeadsUpDisplayLayerImpl::drawFPSCounterGraphAndHistogram(SkCanvas* canvas, SkPaint& paint, FrameRateCounter* fpsCounter, SkRect graphBounds, SkRect histogramBounds)
{
    const double loFPS = 0;
    const double hiFPS = std::max(m_maxFPS + 10.0, 80.0);

    // Draw top and bottom line.
    paint.setColor(SkColorSetRGB(130, 130, 130));
    canvas->drawLine(graphBounds.left(), graphBounds.top() - 1, graphBounds.right(), graphBounds.top() - 1, paint);
    canvas->drawLine(graphBounds.left(), graphBounds.bottom(), graphBounds.right(), graphBounds.bottom(), paint);

    // Draw 60fps line.
    const double top60 = graphBounds.top() + graphBounds.height() * (1 - ((60 - loFPS) / (hiFPS - loFPS))) - 1;
    paint.setColor(SkColorSetRGB(100, 100, 100));
    canvas->drawLine(graphBounds.left(), top60, graphBounds.right(), top60, paint);

    // Collect graph and histogram data.
    int x = 0;
    SkPath path;

    m_minFPS = std::numeric_limits<double>::max();
    m_maxFPS = 0;

    const int histogramSize = 20;
    double histogram[histogramSize] = {0};
    double maxBucketValue = 0;

    for (size_t i = 1; i < fpsCounter->timeStampHistorySize() - 1; ++i) {
        base::TimeDelta delta = fpsCounter->timeStampOfRecentFrame(i + 1) - fpsCounter->timeStampOfRecentFrame(i);

        // Skip this particular instantaneous frame rate if it is not likely to have been valid.
        if (!fpsCounter->isBadFrameInterval(delta)) {

            double fps = 1.0 / delta.InSecondsF();

            m_minFPS = std::min(fps, m_minFPS);
            m_maxFPS = std::max(fps, m_maxFPS);

            // Clamp the FPS to the range we want to plot visually.
            double p = (fps - loFPS) / (hiFPS - loFPS);
            if (p < 0)
                p = 0;
            if (p > 1)
                p = 1;

            // Plot this data point.
            SkPoint cur = SkPoint::Make(graphBounds.left() + x, graphBounds.bottom() - p * graphBounds.height());
            if (path.isEmpty())
                path.moveTo(cur);
            else
                path.lineTo(cur);

            // Use the fps value to find the right bucket in the histogram.
            int bucketIndex = floor(p * (histogramSize - 1));

            // Add the delta time to take the time spent at that fps rate into account.
            histogram[bucketIndex] += delta.InSecondsF();
            maxBucketValue = std::max(histogram[bucketIndex], maxBucketValue);
        }

        x++;
    }

    // Draw FPS histogram.
    paint.setColor(SkColorSetRGB(130, 130, 130));
    canvas->drawLine(histogramBounds.left() - 1, histogramBounds.top() - 1, histogramBounds.left() - 1, histogramBounds.bottom() + 1, paint);
    canvas->drawLine(histogramBounds.right() + 1, histogramBounds.top() - 1, histogramBounds.right() + 1, histogramBounds.bottom() + 1, paint);

    paint.setColor(SK_ColorRED);
    const double barHeight = histogramBounds.height() / histogramSize;

    for (int i = histogramSize - 1; i >= 0; --i) {
        if (histogram[i] > 0) {
            double barWidth = histogram[i] / maxBucketValue * histogramBounds.width();
            canvas->drawRect(SkRect::MakeXYWH(histogramBounds.left(), histogramBounds.bottom() - (i + 1) * barHeight, barWidth, 1), paint);
        }
    }

    // Draw FPS graph.
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(1);

    canvas->drawPath(path, paint);
}

void HeadsUpDisplayLayerImpl::drawDebugRects(SkCanvas* canvas, DebugRectHistory* debugRectHistory)
{
    const std::vector<DebugRect>& debugRects = debugRectHistory->debugRects();
    float rectScale = 1 / layerTreeImpl()->device_scale_factor();

    canvas->save();
    canvas->scale(rectScale, rectScale);

    for (size_t i = 0; i < debugRects.size(); ++i) {
        SkColor strokeColor = 0;
        SkColor fillColor = 0;
        float strokeWidth = 0;

        switch (debugRects[i].type) {
        case PaintRectType:
            strokeColor = DebugColors::PaintRectBorderColor();
            fillColor = DebugColors::PaintRectFillColor();
            strokeWidth = DebugColors::PaintRectBorderWidth(layerTreeImpl());
            break;
        case PropertyChangedRectType:
            strokeColor = DebugColors::PropertyChangedRectBorderColor();
            fillColor = DebugColors::PropertyChangedRectFillColor();
            strokeWidth = DebugColors::PropertyChangedRectBorderWidth(layerTreeImpl());
            break;
        case SurfaceDamageRectType:
            strokeColor = DebugColors::SurfaceDamageRectBorderColor();
            fillColor = DebugColors::SurfaceDamageRectFillColor();
            strokeWidth = DebugColors::SurfaceDamageRectBorderWidth(layerTreeImpl());
            break;
        case ReplicaScreenSpaceRectType:
            strokeColor = DebugColors::ScreenSpaceSurfaceReplicaRectBorderColor();
            fillColor = DebugColors::ScreenSpaceSurfaceReplicaRectFillColor();
            strokeWidth = DebugColors::ScreenSpaceSurfaceReplicaRectBorderWidth(layerTreeImpl());
            break;
        case ScreenSpaceRectType:
            strokeColor = DebugColors::ScreenSpaceLayerRectBorderColor();
            fillColor = DebugColors::ScreenSpaceLayerRectFillColor();
            strokeWidth = DebugColors::ScreenSpaceLayerRectBorderWidth(layerTreeImpl());
            break;
        case OccludingRectType:
            strokeColor = DebugColors::OccludingRectBorderColor();
            fillColor = DebugColors::OccludingRectFillColor();
            strokeWidth = DebugColors::OccludingRectBorderWidth(layerTreeImpl());
            break;
        case NonOccludingRectType:
            strokeColor = DebugColors::NonOccludingRectBorderColor();
            fillColor = DebugColors::NonOccludingRectFillColor();
            strokeWidth = DebugColors::NonOccludingRectBorderWidth(layerTreeImpl());
            break;
        }

        const gfx::RectF& rect = debugRects[i].rect;
        SkRect skRect = SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
        SkPaint paint = createPaint();
        paint.setColor(fillColor);
        canvas->drawRect(skRect, paint);

        paint.setColor(strokeColor);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(SkFloatToScalar(strokeWidth));
        canvas->drawRect(skRect, paint);
    }

    canvas->restore();
}

const char* HeadsUpDisplayLayerImpl::layerTypeAsString() const
{
    return "HeadsUpDisplayLayer";
}

}  // namespace cc
