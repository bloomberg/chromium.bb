// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CanvasLayerTextureUpdater_h
#define CanvasLayerTextureUpdater_h

#include "LayerTextureUpdater.h"

class SkCanvas;

namespace cc {

class LayerPainterChromium;

// Base class for BitmapCanvasLayerTextureUpdater and
// SkPictureCanvasLayerTextureUpdater that reduces code duplication between
// their respective paintContents implementations.
class CanvasLayerTextureUpdater : public LayerTextureUpdater {
public:
    virtual ~CanvasLayerTextureUpdater();

protected:
    explicit CanvasLayerTextureUpdater(scoped_ptr<LayerPainterChromium>);

    void paintContents(SkCanvas*, const IntRect& contentRect, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats&);
    const IntRect& contentRect() const { return m_contentRect; }

private:
    IntRect m_contentRect;
    scoped_ptr<LayerPainterChromium> m_painter;
};

}  // namespace cc

#endif  // CanvasLayerTextureUpdater_h
