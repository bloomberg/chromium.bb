// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef ContentLayerChromium_h
#define ContentLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerPainterChromium.h"
#include "TiledLayerChromium.h"

class SkCanvas;

namespace WebCore {

class FloatRect;
class IntRect;
class LayerTextureUpdater;

class ContentLayerDelegate {
public:
    virtual void paintContents(SkCanvas*, const IntRect& clip, FloatRect& opaque) = 0;

protected:
    virtual ~ContentLayerDelegate() { }
};

class ContentLayerPainter : public LayerPainterChromium {
    WTF_MAKE_NONCOPYABLE(ContentLayerPainter);
public:
    static PassOwnPtr<ContentLayerPainter> create(ContentLayerDelegate*);

    virtual void paint(SkCanvas*, const IntRect& contentRect, FloatRect& opaque) OVERRIDE;

private:
    explicit ContentLayerPainter(ContentLayerDelegate*);

    ContentLayerDelegate* m_delegate;
};

// A layer that renders its contents into an SkCanvas.
class ContentLayerChromium : public TiledLayerChromium {
public:
    static PassRefPtr<ContentLayerChromium> create(ContentLayerDelegate*);

    virtual ~ContentLayerChromium();

    void clearDelegate() { m_delegate = 0; }

    virtual bool drawsContent() const OVERRIDE;
    virtual void setTexturePriorities(const CCPriorityCalculator&) OVERRIDE;
    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual bool needMoreUpdates() OVERRIDE;

    virtual void setOpaque(bool) OVERRIDE;

protected:
    explicit ContentLayerChromium(ContentLayerDelegate*);


private:
    virtual LayerTextureUpdater* textureUpdater() const OVERRIDE { return m_textureUpdater.get(); }
    virtual void createTextureUpdaterIfNeeded() OVERRIDE;

    ContentLayerDelegate* m_delegate;
    RefPtr<LayerTextureUpdater> m_textureUpdater;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
