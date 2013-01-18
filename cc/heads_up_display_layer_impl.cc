// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/heads_up_display_layer_impl.h"

#include "base/stringprintf.h"
#include "cc/debug_colors.h"
#include "cc/debug_rect_history.h"
#include "cc/font_atlas.h"
#include "cc/frame_rate_counter.h"
#include "cc/layer_tree_impl.h"
#include "cc/paint_time_counter.h"
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

HeadsUpDisplayLayerImpl::Graph::Graph(double indicatorValue, double startUpperBound)
    : value(0)
    , min(0)
    , max(0)
    , currentUpperBound(startUpperBound)
    , defaultUpperBound(startUpperBound)
    , indicator(indicatorValue)
{
}

double HeadsUpDisplayLayerImpl::Graph::updateUpperBound(Graph* graph)
{
    double targetUpperBound = std::max(graph->max, graph->defaultUpperBound);
    graph->currentUpperBound += (targetUpperBound - graph->currentUpperBound) * 0.5;
    return graph->currentUpperBound;
}

HeadsUpDisplayLayerImpl::HeadsUpDisplayLayerImpl(LayerTreeImpl* treeImpl, int id)
    : LayerImpl(treeImpl, id)
    , m_fpsGraph(60.0, 80.0)
    , m_paintTimeGraph(16.0, 48.0)
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
    gfx::PointF uv_top_left(0.f, 0.f);
    gfx::PointF uv_bottom_right(1.f, 1.f);
    const float vertex_opacity[] = {1.f, 1.f, 1.f, 1.f};
    bool flipped = false;
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(sharedQuadState, quadRect, opaqueRect, m_hudTexture->id(), premultipliedAlpha, uv_top_left, uv_bottom_right, vertex_opacity, flipped);
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

    FrameRateCounter* fpsCounter = layerTreeImpl()->frame_rate_counter();
    PaintTimeCounter* paintTimeCounter = layerTreeImpl()->paint_time_counter();

    if (debugState.showPlatformLayerTree) {
        SkPaint paint = createPaint();
        drawGraphBackground(canvas, &paint, SkRect::MakeXYWH(0, 0, bounds().width(), bounds().height()));
    }

    int top = 2;

    if (debugState.continuousPainting || debugState.showFPSCounter) {
        // Update numbers not every frame so text is readable
        base::TimeTicks now = base::TimeTicks::Now();
        if (base::TimeDelta(now - m_timeOfLastGraphUpdate).InSecondsF() > 0.25) {
            m_fpsGraph.value = fpsCounter->getAverageFPS();
            fpsCounter->getMinAndMaxFPS(m_fpsGraph.min, m_fpsGraph.max);

            base::TimeDelta latest, min, max;
            latest = paintTimeCounter->GetPaintTimeOfRecentFrame(paintTimeCounter->HistorySize() - 1);
            paintTimeCounter->GetMinAndMaxPaintTime(&min, &max);

            m_paintTimeGraph.value = latest.InMillisecondsF();
            m_paintTimeGraph.min = min.InMillisecondsF();
            m_paintTimeGraph.max = max.InMillisecondsF();

            m_timeOfLastGraphUpdate = now;
        }

        if (debugState.continuousPainting)
            top = drawPaintTimeDisplay(canvas, paintTimeCounter, top);
        // Don't show the FPS display when continuous painting is enabled, because it would show misleading numbers.
        else if (debugState.showFPSCounter)
            top = drawFPSDisplay(canvas, fpsCounter, top);
    }

    if (debugState.showPlatformLayerTree && m_fontAtlas) {
        std::string layerTree = layerTreeImpl()->layer_tree_as_text();
        m_fontAtlas->drawText(canvas, createPaint(), layerTree, gfx::Point(2, top), bounds());
    }

    if (debugState.showHudRects())
        drawDebugRects(canvas, layerTreeImpl()->debug_rect_history());
}

void HeadsUpDisplayLayerImpl::drawTextLeftAligned(SkCanvas* canvas, SkPaint* paint, const SkRect& bounds, const std::string& text)
{
    if (!m_fontAtlas)
        return;

    m_fontAtlas->drawText(canvas, *paint, text, gfx::Point(bounds.left(), bounds.top()), gfx::Size(bounds.width(), bounds.height()));
}

void HeadsUpDisplayLayerImpl::drawTextRightAligned(SkCanvas* canvas, SkPaint* paint, const SkRect& bounds, const std::string& text)
{
    if (!m_fontAtlas)
        return;

    int textWidth = m_fontAtlas->textSize(text).width();

    gfx::Point textPosition(bounds.right() - textWidth, bounds.top());
    gfx::Size textArea(bounds.width(), bounds.height());

    m_fontAtlas->drawText(canvas, *paint, text, textPosition, textArea);
}

void HeadsUpDisplayLayerImpl::drawGraphBackground(SkCanvas* canvas, SkPaint* paint, const SkRect& bounds)
{
    paint->setColor(SkColorSetARGB(215, 17, 17, 17));
    canvas->drawRect(bounds, *paint);
}

void HeadsUpDisplayLayerImpl::drawGraphLines(SkCanvas* canvas, SkPaint* paint, const SkRect& bounds, const Graph& graph)
{
    // Draw top and bottom line.
    paint->setColor(SkColorSetRGB(130, 130, 130));
    canvas->drawLine(bounds.left(), bounds.top() - 1, bounds.right(), bounds.top() - 1, *paint);
    canvas->drawLine(bounds.left(), bounds.bottom(), bounds.right(), bounds.bottom(), *paint);

    // Draw indicator line.
    paint->setColor(SkColorSetRGB(100, 100, 100));
    const double indicatorTop = bounds.height() * (1 - graph.indicator / graph.currentUpperBound) - 1;
    canvas->drawLine(bounds.left(), bounds.top() + indicatorTop, bounds.right(), bounds.top() + indicatorTop, *paint);
}

int HeadsUpDisplayLayerImpl::drawFPSDisplay(SkCanvas* canvas, FrameRateCounter* fpsCounter, const int& top)
{
    const int padding = 4;
    const int gap = 6;

    const int fontHeight = m_fontAtlas.get() ? m_fontAtlas->fontHeight() : 0;

    const int graphWidth = fpsCounter->timeStampHistorySize() - 2;
    const int graphHeight = 40;

    const int histogramWidth = 37;

    const int width = graphWidth + histogramWidth + 4 * padding;
    const int height = fontHeight + graphHeight + 4 * padding + 2;

    const int left = bounds().width() - width - 2;

    SkPaint paint = createPaint();
    drawGraphBackground(canvas, &paint, SkRect::MakeXYWH(left, top, width, height));

    SkRect textBounds = SkRect::MakeXYWH(left + padding, top + padding, graphWidth + histogramWidth + gap + 2, fontHeight);
    SkRect graphBounds = SkRect::MakeXYWH(left + padding, textBounds.bottom() + 2 * padding, graphWidth, graphHeight);
    SkRect histogramBounds = SkRect::MakeXYWH(graphBounds.right() + gap, graphBounds.top(), histogramWidth, graphHeight);

    drawTextLeftAligned(canvas, &paint, textBounds, base::StringPrintf("FPS:%5.1f", m_fpsGraph.value));
    drawTextRightAligned(canvas, &paint, textBounds, base::StringPrintf("%.0f-%.0f", m_fpsGraph.min, m_fpsGraph.max));

    const double upperBound = Graph::updateUpperBound(&m_fpsGraph);
    drawGraphLines(canvas, &paint, graphBounds, m_fpsGraph);

    // Collect graph and histogram data.
    int x = 0;
    SkPath path;

    const int histogramSize = 20;
    double histogram[histogramSize] = {0};
    double maxBucketValue = 0;

    for (size_t i = 0; i < fpsCounter->timeStampHistorySize() - 1; ++i) {
        base::TimeDelta delta = fpsCounter->timeStampOfRecentFrame(i + 1) - fpsCounter->timeStampOfRecentFrame(i);

        // Skip this particular instantaneous frame rate if it is not likely to have been valid.
        if (!fpsCounter->isBadFrameInterval(delta)) {

            double fps = 1.0 / delta.InSecondsF();

            // Clamp the FPS to the range we want to plot visually.
            double p = fps / upperBound;
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

    return top + height + 2;
}

int HeadsUpDisplayLayerImpl::drawPaintTimeDisplay(SkCanvas* canvas, PaintTimeCounter* paintTimeCounter, const int& top)
{
    const int padding = 4;
    const int fontHeight = m_fontAtlas.get() ? m_fontAtlas->fontHeight() : 0;

    const int graphWidth = paintTimeCounter->HistorySize() * 2 - 1;
    const int graphHeight = 40;

    const int width = graphWidth + 2 * padding;
    const int height = fontHeight + graphHeight + 4 * padding + 2 + fontHeight + padding;

    const int left = bounds().width() - width - 2;

    SkPaint paint = createPaint();
    drawGraphBackground(canvas, &paint, SkRect::MakeXYWH(left, top, width, height));

    SkRect textBounds = SkRect::MakeXYWH(left + padding, top + padding, graphWidth, fontHeight);
    SkRect textBounds2 = SkRect::MakeXYWH(left + padding, textBounds.bottom() + padding, graphWidth, fontHeight);
    SkRect graphBounds = SkRect::MakeXYWH(left + padding, textBounds2.bottom() + 2 * padding, graphWidth, graphHeight);

    drawTextLeftAligned(canvas, &paint, textBounds, "Page paint time (ms)");
    drawTextLeftAligned(canvas, &paint, textBounds2, base::StringPrintf("%5.1f", m_paintTimeGraph.value));
    drawTextRightAligned(canvas, &paint, textBounds2, base::StringPrintf("%.1f-%.1f", m_paintTimeGraph.min, m_paintTimeGraph.max));

    const double upperBound = Graph::updateUpperBound(&m_paintTimeGraph);
    drawGraphLines(canvas, &paint, graphBounds, m_paintTimeGraph);

    // Same green as used for paint times in the WebInspector Timeline
    paint.setColor(SkColorSetRGB(95, 160, 80));
    for (size_t i = 0; i < paintTimeCounter->HistorySize(); ++i) {
        double pt = paintTimeCounter->GetPaintTimeOfRecentFrame(i).InMillisecondsF();

        if (pt == 0.0)
            continue;

        double p = pt / upperBound;
        if (p > 1)
            p = 1;

        canvas->drawRect(SkRect::MakeXYWH(graphBounds.left() + i * 2, graphBounds.bottom() - p * graphBounds.height(), 1, p * graphBounds.height()), paint);
    }

    return top + height + 2;
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
