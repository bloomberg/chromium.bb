// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef ContentLayerChromium_h
#define ContentLayerChromium_h

#include "base/basictypes.h"
#include "LayerPainterChromium.h"
#include "TiledLayerChromium.h"

class SkCanvas;

namespace cc {

class ContentLayerChromiumClient;
class FloatRect;
class IntRect;
class LayerTextureUpdater;

class ContentLayerPainter : public LayerPainterChromium {
public:
    static scoped_ptr<ContentLayerPainter> create(ContentLayerChromiumClient*);

    virtual void paint(SkCanvas*, const IntRect& contentRect, FloatRect& opaque) OVERRIDE;

private:
    explicit ContentLayerPainter(ContentLayerChromiumClient*);

    ContentLayerChromiumClient* m_client;

    DISALLOW_COPY_AND_ASSIGN(ContentLayerPainter);
};

// A layer that renders its contents into an SkCanvas.
class ContentLayerChromium : public TiledLayerChromium {
public:
    static scoped_refptr<ContentLayerChromium> create(ContentLayerChromiumClient*);

    void clearClient() { m_client = 0; }

    virtual bool drawsContent() const OVERRIDE;
    virtual void setTexturePriorities(const CCPriorityCalculator&) OVERRIDE;
    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual bool needMoreUpdates() OVERRIDE;

    virtual void setContentsOpaque(bool) OVERRIDE;

protected:
    explicit ContentLayerChromium(ContentLayerChromiumClient*);
    virtual ~ContentLayerChromium();

private:
    virtual LayerTextureUpdater* textureUpdater() const OVERRIDE;
    virtual void createTextureUpdaterIfNeeded() OVERRIDE;

    ContentLayerChromiumClient* m_client;
    RefPtr<LayerTextureUpdater> m_textureUpdater;
};

}
#endif
