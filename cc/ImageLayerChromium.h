// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef ImageLayerChromium_h
#define ImageLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "ContentLayerChromium.h"
#include "SkBitmap.h"

namespace cc {

class ImageLayerTextureUpdater;

// A Layer that contains only an Image element.
class ImageLayerChromium : public TiledLayerChromium {
public:
    static PassRefPtr<ImageLayerChromium> create();
    virtual ~ImageLayerChromium();

    virtual bool drawsContent() const OVERRIDE;
    virtual void setTexturePriorities(const CCPriorityCalculator&) OVERRIDE;
    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual bool needsContentsScale() const OVERRIDE;

    void setBitmap(const SkBitmap& image);

private:
    ImageLayerChromium();

    void setTilingOption(TilingOption);

    virtual LayerTextureUpdater* textureUpdater() const OVERRIDE;
    virtual void createTextureUpdaterIfNeeded() OVERRIDE;
    virtual IntSize contentBounds() const OVERRIDE;

    SkBitmap m_bitmap;

    RefPtr<ImageLayerTextureUpdater> m_textureUpdater;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
