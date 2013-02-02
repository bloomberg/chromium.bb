// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_CONTENT_LAYER_H_
#define CC_CONTENT_LAYER_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/layer_painter.h"
#include "cc/tiled_layer.h"

class SkCanvas;

namespace cc {

class ContentLayerClient;
class LayerUpdater;

class CC_EXPORT ContentLayerPainter : public LayerPainter {
public:
    static scoped_ptr<ContentLayerPainter> create(ContentLayerClient*);

    virtual void paint(SkCanvas*, gfx::Rect contentRect, gfx::RectF& opaque) OVERRIDE;

private:
    explicit ContentLayerPainter(ContentLayerClient*);

    ContentLayerClient* m_client;

    DISALLOW_COPY_AND_ASSIGN(ContentLayerPainter);
};

// A layer that renders its contents into an SkCanvas.
class CC_EXPORT ContentLayer : public TiledLayer {
public:
    static scoped_refptr<ContentLayer> create(ContentLayerClient*);

    void clearClient() { m_client = 0; }

    virtual bool drawsContent() const OVERRIDE;
    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;
    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats*) OVERRIDE;
    virtual bool needMoreUpdates() OVERRIDE;

    virtual void setContentsOpaque(bool) OVERRIDE;

protected:
    explicit ContentLayer(ContentLayerClient*);
    virtual ~ContentLayer();

private:
    virtual LayerUpdater* updater() const OVERRIDE;
    virtual void createUpdaterIfNeeded() OVERRIDE;

    ContentLayerClient* m_client;
    scoped_refptr<LayerUpdater> m_updater;
};

}
#endif  // CC_CONTENT_LAYER_H_
