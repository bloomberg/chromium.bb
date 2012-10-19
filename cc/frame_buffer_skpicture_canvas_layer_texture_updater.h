// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef FrameBufferSkPictureCanvasLayerTextureUpdater_h
#define FrameBufferSkPictureCanvasLayerTextureUpdater_h

#include "cc/skpicture_canvas_layer_texture_updater.h"

namespace cc {

// This class records the contentRect into an SkPicture, then uses accelerated
// drawing to update the texture. The accelerated drawing goes to an
// intermediate framebuffer and then is copied to the destination texture once done.
class FrameBufferSkPictureCanvasLayerTextureUpdater : public SkPictureCanvasLayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(FrameBufferSkPictureCanvasLayerTextureUpdater*, scoped_ptr<PrioritizedTexture>);
        virtual ~Texture();

        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        FrameBufferSkPictureCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        FrameBufferSkPictureCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static scoped_refptr<FrameBufferSkPictureCanvasLayerTextureUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerTextureUpdater::Texture> createTexture(PrioritizedTextureManager*) OVERRIDE;
    virtual SampledTexelFormat sampledTexelFormat(GLenum textureFormat) OVERRIDE;

private:
    explicit FrameBufferSkPictureCanvasLayerTextureUpdater(scoped_ptr<LayerPainter>);
    virtual ~FrameBufferSkPictureCanvasLayerTextureUpdater();
};
} // namespace cc
#endif // FrameBufferSkPictureCanvasLayerTextureUpdater_h
