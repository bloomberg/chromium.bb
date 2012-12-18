// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_PAINTER_H_
#define CC_LAYER_PAINTER_H_

#include "cc/cc_export.h"

class SkCanvas;

namespace gfx {
class Rect;
class RectF;
}

namespace cc {

class CC_EXPORT LayerPainter {
public:
    virtual ~LayerPainter() { }
    virtual void paint(SkCanvas*, gfx::Rect contentRect, gfx::RectF& opaque) = 0;
};

}  // namespace cc

#endif  // CC_LAYER_PAINTER_H_
