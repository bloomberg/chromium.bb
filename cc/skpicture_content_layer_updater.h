// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_SKPICTURE_CONTENT_LAYER_UPDATER_H_
#define CC_SKPICTURE_CONTENT_LAYER_UPDATER_H_

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
        Resource(SkPictureContentLayerUpdater*, scoped_ptr<PrioritizedResource>);
        virtual ~Resource();

        virtual void update(ResourceUpdateQueue&, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate, RenderingStats*) OVERRIDE;

    private:
        SkPictureContentLayerUpdater* updater() { return m_updater; }

        SkPictureContentLayerUpdater* m_updater;
    };

    static scoped_refptr<SkPictureContentLayerUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerUpdater::Resource> createResource(PrioritizedResourceManager*) OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;

protected:
    explicit SkPictureContentLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~SkPictureContentLayerUpdater();

    virtual void prepareToUpdate(const gfx::Rect& contentRect, const gfx::Size& tileSize, float contentsWidthScale, float contentsHeightScale, gfx::Rect& resultingOpaqueRect, RenderingStats*) OVERRIDE;
    void drawPicture(SkCanvas*);
    void updateTexture(ResourceUpdateQueue& queue, PrioritizedResource* texture, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate);

    bool layerIsOpaque() const { return m_layerIsOpaque; }

private:
    // Recording canvas.
    SkPicture m_picture;
    // True when it is known that all output pixels will be opaque.
    bool m_layerIsOpaque;
};

} // namespace cc
#endif  // CC_SKPICTURE_CONTENT_LAYER_UPDATER_H_
