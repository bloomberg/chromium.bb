// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef LayerPainterChromium_h
#define LayerPainterChromium_h

class SkCanvas;

namespace cc {

class FloatRect;
class IntRect;

class LayerPainter {
public:
    virtual ~LayerPainter() { }
    virtual void paint(SkCanvas*, const IntRect& contentRect, FloatRect& opaque) = 0;
};

} // namespace cc
#endif // LayerPainterChromium_h
