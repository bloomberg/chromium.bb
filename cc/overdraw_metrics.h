// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OVERDRAW_METRICS_H_
#define CC_OVERDRAW_METRICS_H_

#include "base/memory/scoped_ptr.h"

namespace gfx {
class Rect;
class Transform;
}

namespace cc {
class LayerTreeHost;
class LayerTreeHostImpl;

// FIXME: compute overdraw metrics only occasionally, not every frame.
class OverdrawMetrics {
public:
    static scoped_ptr<OverdrawMetrics> create(bool recordMetricsForFrame) { return make_scoped_ptr(new OverdrawMetrics(recordMetricsForFrame)); }

    // These methods are used for saving metrics during update/commit.

    // Record pixels painted by WebKit into the texture updater, but does not mean the pixels were rasterized in main memory.
    void didPaint(const gfx::Rect& paintedRect);
    // Records that an invalid tile was culled and did not need to be painted/uploaded, and did not contribute to other tiles needing to be painted.
    void didCullTilesForUpload(int count);
    // Records pixels that were uploaded to texture memory.
    void didUpload(const gfx::Transform& transformToTarget, const gfx::Rect& uploadRect, const gfx::Rect& opaqueRect);
    // Record contents texture(s) behind present using the given number of bytes.
    void didUseContentsTextureMemoryBytes(size_t contentsTextureUseBytes);
    // Record RenderSurfaceImpl texture(s) being present using the given number of bytes.
    void didUseRenderSurfaceTextureMemoryBytes(size_t renderSurfaceUseBytes);

    // These methods are used for saving metrics during draw.

    // Record pixels that were not drawn to screen.
    void didCullForDrawing(const gfx::Transform& transformToTarget, const gfx::Rect& beforeCullRect, const gfx::Rect& afterCullRect);
    // Record pixels that were drawn to screen.
    void didDraw(const gfx::Transform& transformToTarget, const gfx::Rect& afterCullRect, const gfx::Rect& opaqueRect);

    void recordMetrics(const LayerTreeHost*) const;
    void recordMetrics(const LayerTreeHostImpl*) const;

    // Accessors for tests.
    float pixelsDrawnOpaque() const { return m_pixelsDrawnOpaque; }
    float pixelsDrawnTranslucent() const { return m_pixelsDrawnTranslucent; }
    float pixelsCulledForDrawing() const { return m_pixelsCulledForDrawing; }
    float pixelsPainted() const { return m_pixelsPainted; }
    float pixelsUploadedOpaque() const { return m_pixelsUploadedOpaque; }
    float pixelsUploadedTranslucent() const { return m_pixelsUploadedTranslucent; }
    int tilesCulledForUpload() const { return m_tilesCulledForUpload; }

private:
    enum MetricsType {
        UpdateAndCommit,
        DrawingToScreen
    };

    explicit OverdrawMetrics(bool recordMetricsForFrame);

    template<typename LayerTreeHostType>
    void recordMetricsInternal(MetricsType, const LayerTreeHostType*) const;

    // When false this class is a giant no-op.
    bool m_recordMetricsForFrame;

    // These values are used for saving metrics during update/commit.

    // Count of pixels that were painted due to invalidation.
    float m_pixelsPainted;
    // Count of pixels uploaded to textures and known to be opaque.
    float m_pixelsUploadedOpaque;
    // Count of pixels uploaded to textures and not known to be opaque.
    float m_pixelsUploadedTranslucent;
    // Count of tiles that were invalidated but not uploaded.
    int m_tilesCulledForUpload;
    // Count the number of bytes in contents textures.
    unsigned long long m_contentsTextureUseBytes;
    // Count the number of bytes in RenderSurfaceImpl textures.
    unsigned long long m_renderSurfaceTextureUseBytes;

    // These values are used for saving metrics during draw.

    // Count of pixels that are opaque (and thus occlude). Ideally this is no more than wiewport width x height.
    float m_pixelsDrawnOpaque;
    // Count of pixels that are possibly translucent, and cannot occlude.
    float m_pixelsDrawnTranslucent;
    // Count of pixels not drawn as they are occluded by somthing opaque.
    float m_pixelsCulledForDrawing;
};

} // namespace cc

#endif  // CC_OVERDRAW_METRICS_H_
