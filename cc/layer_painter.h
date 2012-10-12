// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef LayerPainterChromium_h
#define LayerPainterChromium_h

#if USE(ACCELERATED_COMPOSITING)

class SkCanvas;

namespace cc {

class FloatRect;
class IntRect;

class LayerPainterChromium {
public:
    virtual ~LayerPainterChromium() { }
    virtual void paint(SkCanvas*, const IntRect& contentRect, FloatRect& opaque) = 0;
};

} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)
#endif // LayerPainterChromium_h
