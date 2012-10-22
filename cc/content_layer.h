// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentLayerChromium_h
#define ContentLayerChromium_h

#include "base/basictypes.h"
#include "cc/layer_painter.h"
#include "cc/tiled_layer.h"

class SkCanvas;

namespace cc {

class ContentLayerClient;
class FloatRect;
class IntRect;
class LayerTextureUpdater;

class ContentLayerPainter : public LayerPainter {
public:
    static scoped_ptr<ContentLayerPainter> create(ContentLayerClient*);

    virtual void paint(SkCanvas*, const IntRect& contentRect, FloatRect& opaque) OVERRIDE;

private:
    explicit ContentLayerPainter(ContentLayerClient*);

    ContentLayerClient* m_client;

    DISALLOW_COPY_AND_ASSIGN(ContentLayerPainter);
};

// A layer that renders its contents into an SkCanvas.
class ContentLayer : public TiledLayer {
public:
    static scoped_refptr<ContentLayer> create(ContentLayerClient*);

    void clearClient() { m_client = 0; }

    virtual bool drawsContent() const OVERRIDE;
    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;
    virtual void update(TextureUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;
    virtual bool needMoreUpdates() OVERRIDE;

    virtual void setContentsOpaque(bool) OVERRIDE;

protected:
    explicit ContentLayer(ContentLayerClient*);
    virtual ~ContentLayer();

private:
    virtual LayerTextureUpdater* textureUpdater() const OVERRIDE;
    virtual void createTextureUpdaterIfNeeded() OVERRIDE;

    ContentLayerClient* m_client;
    scoped_refptr<LayerTextureUpdater> m_textureUpdater;
};

}
#endif
