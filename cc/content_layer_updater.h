// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentLayerUpdater_h
#define ContentLayerUpdater_h

#include "cc/layer_updater.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// Base class for BitmapContentLayerUpdater and
// SkPictureContentLayerUpdater that reduces code duplication between
// their respective paintContents implementations.
class ContentLayerUpdater : public LayerUpdater {
protected:
    explicit ContentLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~ContentLayerUpdater();

    void paintContents(SkCanvas*, const gfx::Rect& contentRect, float contentsWidthScale, float contentsHeightScale, gfx::Rect& resultingOpaqueRect, RenderingStats&);
    const gfx::Rect& contentRect() const { return m_contentRect; }

private:
    gfx::Rect m_contentRect;
    scoped_ptr<LayerPainter> m_painter;
};

}  // namespace cc

#endif  // ContentLayerUpdater_h
