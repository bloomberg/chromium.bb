// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BitmapContentLayerUpdater_h
#define BitmapContentLayerUpdater_h

#include "cc/content_layer_updater.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// This class rasterizes the contentRect into a skia bitmap canvas. It then updates
// textures by copying from the canvas into the texture, using MapSubImage if
// possible.
class BitmapContentLayerUpdater : public ContentLayerUpdater {
public:
    class Resource : public LayerUpdater::Resource {
    public:
        Resource(BitmapContentLayerUpdater*, scoped_ptr<PrioritizedTexture>);
        virtual ~Resource();

        virtual void update(ResourceUpdateQueue&, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        BitmapContentLayerUpdater* updater() { return m_updater; }

        BitmapContentLayerUpdater* m_updater;
    };

    static scoped_refptr<BitmapContentLayerUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerUpdater::Resource> createResource(PrioritizedTextureManager*) OVERRIDE;
    virtual void prepareToUpdate(const gfx::Rect& contentRect, const gfx::Size& tileSize, float contentsWidthScale, float contentsHeightScale, gfx::Rect& resultingOpaqueRect, RenderingStats&) OVERRIDE;
    void updateTexture(ResourceUpdateQueue&, PrioritizedTexture*, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate);

    virtual void setOpaque(bool) OVERRIDE;

protected:
    explicit BitmapContentLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~BitmapContentLayerUpdater();

    scoped_ptr<SkCanvas> m_canvas;
    gfx::Size m_canvasSize;
    bool m_opaque;
};

}  // namespace cc

#endif  // BitmapContentLayerUpdater_h
