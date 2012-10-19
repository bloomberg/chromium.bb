// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasLayerTextureUpdater_h
#define CanvasLayerTextureUpdater_h

#include "cc/layer_texture_updater.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// Base class for BitmapCanvasLayerTextureUpdater and
// SkPictureCanvasLayerTextureUpdater that reduces code duplication between
// their respective paintContents implementations.
class CanvasLayerTextureUpdater : public LayerTextureUpdater {
protected:
    explicit CanvasLayerTextureUpdater(scoped_ptr<LayerPainter>);
    virtual ~CanvasLayerTextureUpdater();

    void paintContents(SkCanvas*, const IntRect& contentRect, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats&);
    const IntRect& contentRect() const { return m_contentRect; }

private:
    IntRect m_contentRect;
    scoped_ptr<LayerPainter> m_painter;
};

}  // namespace cc

#endif  // CanvasLayerTextureUpdater_h
