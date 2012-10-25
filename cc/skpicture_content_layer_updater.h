// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef SkPictureContentLayerUpdater_h
#define SkPictureContentLayerUpdater_h

#include "cc/content_layer_updater.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// This class records the contentRect into an SkPicture. Subclasses, provide
// different implementations of tile updating based on this recorded picture.
// The BitmapSkPictureContentLayerUpdater and
// FrameBufferSkPictureContentLayerUpdater are two examples of such
// implementations.
class SkPictureContentLayerUpdater : public ContentLayerUpdater {
public:
    class Resource : public LayerUpdater::Resource {
    public:
        Resource(SkPictureContentLayerUpdater*, scoped_ptr<PrioritizedTexture>);
        virtual ~Resource();

        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        SkPictureContentLayerUpdater* updater() { return m_updater; }

        SkPictureContentLayerUpdater* m_updater;
    };

    static scoped_refptr<SkPictureContentLayerUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerUpdater::Resource> createResource(PrioritizedTextureManager*) OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;

protected:
    explicit SkPictureContentLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~SkPictureContentLayerUpdater();

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
#endif // SkPictureContentLayerUpdater_h
