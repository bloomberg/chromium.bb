// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef SkPictureCanvasLayerTextureUpdater_h
#define SkPictureCanvasLayerTextureUpdater_h

#include "CanvasLayerTextureUpdater.h"
#include "SkPicture.h"

class SkCanvas;

namespace cc {

class LayerPainterChromium;

// This class records the contentRect into an SkPicture. Subclasses, provide
// different implementations of tile updating based on this recorded picture.
// The BitmapSkPictureCanvasLayerTextureUpdater and
// FrameBufferSkPictureCanvasLayerTextureUpdater are two examples of such
// implementations.
class SkPictureCanvasLayerTextureUpdater : public CanvasLayerTextureUpdater {
public:
    virtual ~SkPictureCanvasLayerTextureUpdater();

    virtual void setOpaque(bool) OVERRIDE;

protected:
    explicit SkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium>);

    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats&) OVERRIDE;
    void drawPicture(SkCanvas*);
    void updateTexture(CCTextureUpdateQueue& queue, CCPrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate);

    bool layerIsOpaque() const { return m_layerIsOpaque; }

private:
    // Recording canvas.
    SkPicture m_picture;
    // True when it is known that all output pixels will be opaque.
    bool m_layerIsOpaque;
};

} // namespace cc
#endif // SkPictureCanvasLayerTextureUpdater_h
