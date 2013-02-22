// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/render_surface_filters.h"

#include "base/logging.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkMagnifierImageFilter.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"
#include "third_party/skia/include/gpu/SkGrPixelRef.h"
#include "ui/gfx/size_f.h"

namespace cc {

namespace {

void getBrightnessMatrix(float amount, SkScalar matrix[20])
{
    // Spec implementation
    // (http://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#brightnessEquivalent)
    // <feFunc[R|G|B] type="linear" slope="[amount]">
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = amount;
    matrix[18] = 1;
}

void getSaturatingBrightnessMatrix(float amount, SkScalar matrix[20])
{
    // Legacy implementation used by internal clients.
    // <feFunc[R|G|B] type="linear" intercept="[amount]"/>
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = matrix[18] = 1;
    matrix[4] = matrix[9] = matrix[14] = amount * 255;
}

void getContrastMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = amount;
    matrix[4] = matrix[9] = matrix[14] = (-0.5f * amount + 0.5f) * 255;
    matrix[18] = 1;
}

void getSaturateMatrix(float amount, SkScalar matrix[20])
{
    // Note, these values are computed to ensure matrixNeedsClamping is false
    // for amount in [0..1]
    matrix[0] = 0.213f + 0.787f * amount;
    matrix[1] = 0.715f - 0.715f * amount;
    matrix[2] = 1.f - (matrix[0] + matrix[1]);
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - 0.213f * amount;
    matrix[6] = 0.715f + 0.285f * amount;
    matrix[7] = 1.f - (matrix[5] + matrix[6]);
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - 0.213f * amount;
    matrix[11] = 0.715f - 0.715f * amount;
    matrix[12] = 1.f - (matrix[10] + matrix[11]);
    matrix[13] = matrix[14] = 0;
    matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
    matrix[18] = 1;
}

void getHueRotateMatrix(float hue, SkScalar matrix[20])
{
    const float kPi = 3.1415926535897932384626433832795f;

    float cosHue = cosf(hue * kPi / 180);
    float sinHue = sinf(hue * kPi / 180);
    matrix[0] = 0.213f + cosHue * 0.787f - sinHue * 0.213f;
    matrix[1] = 0.715f - cosHue * 0.715f - sinHue * 0.715f;
    matrix[2] = 0.072f - cosHue * 0.072f + sinHue * 0.928f;
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - cosHue * 0.213f + sinHue * 0.143f;
    matrix[6] = 0.715f + cosHue * 0.285f + sinHue * 0.140f;
    matrix[7] = 0.072f - cosHue * 0.072f - sinHue * 0.283f;
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - cosHue * 0.213f - sinHue * 0.787f;
    matrix[11] = 0.715f - cosHue * 0.715f + sinHue * 0.715f;
    matrix[12] = 0.072f + cosHue * 0.928f + sinHue * 0.072f;
    matrix[13] = matrix[14] = 0;
    matrix[15] = matrix[16] = matrix[17] = 0;
    matrix[18] = 1;
    matrix[19] = 0;
}

void getInvertMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = 1 - 2 * amount;
    matrix[4] = matrix[9] = matrix[14] = amount * 255;
    matrix[18] = 1;
}

void getOpacityMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = 1;
    matrix[18] = amount;
}

void getGrayscaleMatrix(float amount, SkScalar matrix[20])
{
    // Note, these values are computed to ensure matrixNeedsClamping is false
    // for amount in [0..1]
    matrix[0] = 0.2126f + 0.7874f * amount;
    matrix[1] = 0.7152f - 0.7152f * amount;
    matrix[2] = 1.f - (matrix[0] + matrix[1]);
    matrix[3] = matrix[4] = 0;

    matrix[5] = 0.2126f - 0.2126f * amount;
    matrix[6] = 0.7152f + 0.2848f * amount;
    matrix[7] = 1.f - (matrix[5] + matrix[6]);
    matrix[8] = matrix[9] = 0;

    matrix[10] = 0.2126f - 0.2126f * amount;
    matrix[11] = 0.7152f - 0.7152f * amount;
    matrix[12] = 1.f - (matrix[10] + matrix[11]);
    matrix[13] = matrix[14] = 0;

    matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
    matrix[18] = 1;
}

void getSepiaMatrix(float amount, SkScalar matrix[20])
{
    matrix[0] = 0.393f + 0.607f * amount;
    matrix[1] = 0.769f - 0.769f * amount;
    matrix[2] = 0.189f - 0.189f * amount;
    matrix[3] = matrix[4] = 0;

    matrix[5] = 0.349f - 0.349f * amount;
    matrix[6] = 0.686f + 0.314f * amount;
    matrix[7] = 0.168f - 0.168f * amount;
    matrix[8] = matrix[9] = 0;

    matrix[10] = 0.272f - 0.272f * amount;
    matrix[11] = 0.534f - 0.534f * amount;
    matrix[12] = 0.131f + 0.869f * amount;
    matrix[13] = matrix[14] = 0;

    matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
    matrix[18] = 1;
}

// The 5x4 matrix is really a "compressed" version of a 5x5 matrix that'd have
// (0 0 0 0 1) as a last row, and that would be applied to a 5-vector extended
// from the 4-vector color with a 1.
void multColorMatrix(SkScalar a[20], SkScalar b[20], SkScalar out[20])
{
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 5; ++i) {
            out[i+j*5] = i == 4 ? a[4+j*5] : 0;
            for (int k = 0; k < 4; ++k)
                out[i+j*5] += a[k+j*5] * b[i+k*5];
        }
    }
}

// To detect if we need to apply clamping after applying a matrix, we check if
// any output component might go outside of [0, 255] for any combination of
// input components in [0..255].
// Each output component is an affine transformation of the input component, so
// the minimum and maximum values are for any combination of minimum or maximum
// values of input components (i.e. 0 or 255).
// E.g. if R' = x*R + y*G + z*B + w*A + t
// Then the maximum value will be for R=255 if x>0 or R=0 if x<0, and the
// minimum value will be for R=0 if x>0 or R=255 if x<0.
// Same goes for all components.
bool componentNeedsClamping(SkScalar row[5])
{
    SkScalar maxValue = row[4] / 255;
    SkScalar minValue = row[4] / 255;
    for (int i = 0; i < 4; ++i) {
        if (row[i] > 0)
            maxValue += row[i];
        else
            minValue += row[i];
    }
    return (maxValue > 1) || (minValue < 0);
}

bool matrixNeedsClamping(SkScalar matrix[20])
{
    return componentNeedsClamping(matrix)
        || componentNeedsClamping(matrix+5)
        || componentNeedsClamping(matrix+10)
        || componentNeedsClamping(matrix+15);
}

bool getColorMatrix(const WebKit::WebFilterOperation& op, SkScalar matrix[20])
{
    switch (op.type()) {
    case WebKit::WebFilterOperation::FilterTypeBrightness: {
        getBrightnessMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeSaturatingBrightness: {
        getSaturatingBrightnessMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeContrast: {
        getContrastMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeGrayscale: {
        getGrayscaleMatrix(1 - op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeSepia: {
        getSepiaMatrix(1 - op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeSaturate: {
        getSaturateMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeHueRotate: {
        getHueRotateMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeInvert: {
        getInvertMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeOpacity: {
        getOpacityMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeColorMatrix: {
        memcpy(matrix, op.matrix(), sizeof(SkScalar[20]));
        return true;
    }
    default:
        return false;
    }
}

class FilterBufferState {
public:
    FilterBufferState(GrContext* grContext, const gfx::SizeF& size, unsigned textureId)
        : m_grContext(grContext)
        , m_currentTexture(0)
    {
        // Wrap the source texture in a Ganesh platform texture.
        GrBackendTextureDesc backendTextureDescription;
        backendTextureDescription.fWidth = size.width();
        backendTextureDescription.fHeight = size.height();
        backendTextureDescription.fConfig = kSkia8888_GrPixelConfig;
        backendTextureDescription.fTextureHandle = textureId;
        skia::RefPtr<GrTexture> texture = skia::AdoptRef(grContext->wrapBackendTexture(backendTextureDescription));
        // Place the platform texture inside an SkBitmap.
        m_source.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
        skia::RefPtr<SkGrPixelRef> pixelRef = skia::AdoptRef(new SkGrPixelRef(texture.get()));
        m_source.setPixelRef(pixelRef.get());
    }

    ~FilterBufferState() { }

    bool init(int filterCount)
    {
        int scratchCount = std::min(2, filterCount);
        GrTextureDesc desc;
        desc.fFlags = kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
        desc.fSampleCnt = 0;
        desc.fWidth = m_source.width();
        desc.fHeight = m_source.height();
        desc.fConfig = kSkia8888_GrPixelConfig;
        for (int i = 0; i < scratchCount; ++i) {
            GrAutoScratchTexture scratchTexture(m_grContext, desc, GrContext::kExact_ScratchTexMatch);
            m_scratchTextures[i] = skia::AdoptRef(scratchTexture.detach());
            if (!m_scratchTextures[i])
                return false;
        }
        return true;
    }

    SkCanvas* canvas()
    {
        if (!m_canvas.get())
            createCanvas();
        return m_canvas.get();
    }

    const SkBitmap& source() { return m_source; }

    void swap()
    {
        m_canvas->flush();
        m_canvas.clear();
        m_device.clear();

        skia::RefPtr<SkGrPixelRef> pixelRef = skia::AdoptRef(new SkGrPixelRef(m_scratchTextures[m_currentTexture].get()));
        m_source.setPixelRef(pixelRef.get());
        m_currentTexture = 1 - m_currentTexture;
    }

private:
    void createCanvas()
    {
        DCHECK(m_scratchTextures[m_currentTexture].get());
        m_device = skia::AdoptRef(new SkGpuDevice(m_grContext, m_scratchTextures[m_currentTexture].get()));
        m_canvas = skia::AdoptRef(new SkCanvas(m_device.get()));
        m_canvas->clear(0x0);
    }

    GrContext* m_grContext;
    SkBitmap m_source;
    skia::RefPtr<GrTexture> m_scratchTextures[2];
    int m_currentTexture;
    skia::RefPtr<SkGpuDevice> m_device;
    skia::RefPtr<SkCanvas> m_canvas;
};

}  // namespace

WebKit::WebFilterOperations RenderSurfaceFilters::optimize(const WebKit::WebFilterOperations& filters)
{
    WebKit::WebFilterOperations newList;

    SkScalar accumulatedColorMatrix[20];
    bool haveAccumulatedColorMatrix = false;
    for (unsigned i = 0; i < filters.size(); ++i) {
        const WebKit::WebFilterOperation& op = filters.at(i);

        // If the filter is a color matrix, we may be able to combine it with
        // following filter(s) that also are color matrices.
        SkScalar matrix[20];
        if (getColorMatrix(op, matrix)) {
            if (haveAccumulatedColorMatrix) {
                SkScalar newMatrix[20];
                multColorMatrix(matrix, accumulatedColorMatrix, newMatrix);
                memcpy(accumulatedColorMatrix, newMatrix, sizeof(accumulatedColorMatrix));
            } else {
                memcpy(accumulatedColorMatrix, matrix, sizeof(accumulatedColorMatrix));
                haveAccumulatedColorMatrix = true;
            }

            // We can only combine matrices if clamping of color components
            // would have no effect.
            if (!matrixNeedsClamping(accumulatedColorMatrix))
                continue;
        }

        if (haveAccumulatedColorMatrix)
            newList.append(WebKit::WebFilterOperation::createColorMatrixFilter(accumulatedColorMatrix));
        haveAccumulatedColorMatrix = false;

        switch (op.type()) {
        case WebKit::WebFilterOperation::FilterTypeBlur:
        case WebKit::WebFilterOperation::FilterTypeDropShadow:
        case WebKit::WebFilterOperation::FilterTypeZoom:
            newList.append(op);
            break;
        case WebKit::WebFilterOperation::FilterTypeBrightness:
        case WebKit::WebFilterOperation::FilterTypeSaturatingBrightness:
        case WebKit::WebFilterOperation::FilterTypeContrast:
        case WebKit::WebFilterOperation::FilterTypeGrayscale:
        case WebKit::WebFilterOperation::FilterTypeSepia:
        case WebKit::WebFilterOperation::FilterTypeSaturate:
        case WebKit::WebFilterOperation::FilterTypeHueRotate:
        case WebKit::WebFilterOperation::FilterTypeInvert:
        case WebKit::WebFilterOperation::FilterTypeOpacity:
        case WebKit::WebFilterOperation::FilterTypeColorMatrix:
            break;
        }
    }
    if (haveAccumulatedColorMatrix)
        newList.append(WebKit::WebFilterOperation::createColorMatrixFilter(accumulatedColorMatrix));
    return newList;
}

SkBitmap RenderSurfaceFilters::apply(const WebKit::WebFilterOperations& filters, unsigned textureId, gfx::SizeF size, GrContext* grContext)
{
    DCHECK(grContext);

    WebKit::WebFilterOperations optimizedFilters = optimize(filters);
    FilterBufferState state(grContext, size, textureId);
    if (!state.init(optimizedFilters.size()))
        return SkBitmap();

    for (unsigned i = 0; i < optimizedFilters.size(); ++i) {
        const WebKit::WebFilterOperation& op = optimizedFilters.at(i);
        SkCanvas* canvas = state.canvas();
        switch (op.type()) {
        case WebKit::WebFilterOperation::FilterTypeColorMatrix: {
            SkPaint paint;
            skia::RefPtr<SkColorMatrixFilter> filter = skia::AdoptRef(new SkColorMatrixFilter(op.matrix()));
            paint.setColorFilter(filter.get());
            canvas->drawBitmap(state.source(), 0, 0, &paint);
            break;
        }
        case WebKit::WebFilterOperation::FilterTypeBlur: {
            float stdDeviation = op.amount();
            skia::RefPtr<SkImageFilter> filter = skia::AdoptRef(new SkBlurImageFilter(stdDeviation, stdDeviation));
            SkPaint paint;
            paint.setImageFilter(filter.get());
            canvas->drawSprite(state.source(), 0, 0, &paint);
            break;
        }
        case WebKit::WebFilterOperation::FilterTypeDropShadow: {
            skia::RefPtr<SkImageFilter> blurFilter = skia::AdoptRef(new SkBlurImageFilter(op.amount(), op.amount()));
            skia::RefPtr<SkColorFilter> colorFilter = skia::AdoptRef(SkColorFilter::CreateModeFilter(op.dropShadowColor(), SkXfermode::kSrcIn_Mode));
            SkPaint paint;
            paint.setImageFilter(blurFilter.get());
            paint.setColorFilter(colorFilter.get());
            paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
            canvas->saveLayer(0, &paint);
            canvas->drawBitmap(state.source(), op.dropShadowOffset().x, -op.dropShadowOffset().y);
            canvas->restore();
            canvas->drawBitmap(state.source(), 0, 0);
            break;
        }
        case WebKit::WebFilterOperation::FilterTypeZoom: {
            SkPaint paint;
            skia::RefPtr<SkImageFilter> zoomFilter = skia::AdoptRef(
                new SkMagnifierImageFilter(
                    SkRect::MakeXYWH(op.zoomRect().x,
                                     op.zoomRect().y,
                                     op.zoomRect().width,
                                     op.zoomRect().height),
                    op.amount()));
            paint.setImageFilter(zoomFilter.get());
            canvas->saveLayer(0, &paint);
            canvas->drawBitmap(state.source(), 0, 0);
            canvas->restore();
            break;
        }
        case WebKit::WebFilterOperation::FilterTypeBrightness:
        case WebKit::WebFilterOperation::FilterTypeSaturatingBrightness:
        case WebKit::WebFilterOperation::FilterTypeContrast:
        case WebKit::WebFilterOperation::FilterTypeGrayscale:
        case WebKit::WebFilterOperation::FilterTypeSepia:
        case WebKit::WebFilterOperation::FilterTypeSaturate:
        case WebKit::WebFilterOperation::FilterTypeHueRotate:
        case WebKit::WebFilterOperation::FilterTypeInvert:
        case WebKit::WebFilterOperation::FilterTypeOpacity:
            NOTREACHED();
            break;
        }
        state.swap();
    }
    return state.source();
}

}  // namespace cc
