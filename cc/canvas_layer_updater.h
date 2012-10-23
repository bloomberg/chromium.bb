// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasLayerUpdater_h
#define CanvasLayerUpdater_h

#include "cc/layer_updater.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// Base class for BitmapCanvasLayerUpdater and
// SkPictureCanvasLayerUpdater that reduces code duplication between
// their respective paintContents implementations.
class CanvasLayerUpdater : public LayerUpdater {
protected:
    explicit CanvasLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~CanvasLayerUpdater();

    void paintContents(SkCanvas*, const IntRect& contentRect, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats&);
    const IntRect& contentRect() const { return m_contentRect; }

private:
    IntRect m_contentRect;
    scoped_ptr<LayerPainter> m_painter;
};

}  // namespace cc

#endif  // CanvasLayerUpdater_h
