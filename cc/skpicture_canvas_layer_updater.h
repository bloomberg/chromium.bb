// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef SkPictureCanvasLayerUpdater_h
#define SkPictureCanvasLayerUpdater_h

#include "cc/canvas_layer_updater.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// This class records the contentRect into an SkPicture. Subclasses, provide
// different implementations of tile updating based on this recorded picture.
// The BitmapSkPictureCanvasLayerUpdater and
// FrameBufferSkPictureCanvasLayerUpdater are two examples of such
// implementations.
class SkPictureCanvasLayerUpdater : public CanvasLayerUpdater {
public:
    class Resource : public LayerUpdater::Resource {
    public:
        Resource(SkPictureCanvasLayerUpdater*, scoped_ptr<PrioritizedTexture>);
        virtual ~Resource();

        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        SkPictureCanvasLayerUpdater* updater() { return m_updater; }

        SkPictureCanvasLayerUpdater* m_updater;
    };

    static scoped_refptr<SkPictureCanvasLayerUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerUpdater::Resource> createResource(PrioritizedTextureManager*) OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;

protected:
    explicit SkPictureCanvasLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~SkPictureCanvasLayerUpdater();

    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats&) OVERRIDE;
    void drawPicture(SkCanvas*);
    void updateTexture(TextureUpdateQueue& queue, PrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate);

    bool layerIsOpaque() const { return m_layerIsOpaque; }

private:
    // Recording canvas.
    SkPicture m_picture;
    // True when it is known that all output pixels will be opaque.
    bool m_layerIsOpaque;
};

} // namespace cc
#endif // SkPictureCanvasLayerUpdater_h
