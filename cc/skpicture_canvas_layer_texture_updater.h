// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef SkPictureCanvasLayerTextureUpdater_h
#define SkPictureCanvasLayerTextureUpdater_h

#include "cc/canvas_layer_texture_updater.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// This class records the contentRect into an SkPicture. Subclasses, provide
// different implementations of tile updating based on this recorded picture.
// The BitmapSkPictureCanvasLayerTextureUpdater and
// FrameBufferSkPictureCanvasLayerTextureUpdater are two examples of such
// implementations.
class SkPictureCanvasLayerTextureUpdater : public CanvasLayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(SkPictureCanvasLayerTextureUpdater*, scoped_ptr<PrioritizedTexture>);
        virtual ~Texture();

        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        SkPictureCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        SkPictureCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static scoped_refptr<SkPictureCanvasLayerTextureUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerTextureUpdater::Texture> createTexture(PrioritizedTextureManager*) OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;

protected:
    explicit SkPictureCanvasLayerTextureUpdater(scoped_ptr<LayerPainter>);
    virtual ~SkPictureCanvasLayerTextureUpdater();

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
#endif // SkPictureCanvasLayerTextureUpdater_h
