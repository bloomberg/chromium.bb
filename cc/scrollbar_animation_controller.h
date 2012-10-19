// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScrollbarAnimationController_h
#define CCScrollbarAnimationController_h

#include "base/memory/scoped_ptr.h"
#include "FloatPoint.h"
#include "IntSize.h"

namespace cc {

class LayerImpl;
class ScrollbarLayerImpl;

// This abstract class represents the compositor-side analogy of ScrollbarAnimator.
// Individual platforms should subclass it to provide specialized implementation.
class ScrollbarAnimationController {
public:
    static scoped_ptr<ScrollbarAnimationController> create(LayerImpl* scrollLayer);

    virtual ~ScrollbarAnimationController();

    virtual bool animate(double monotonicTime);
    void didPinchGestureBegin();
    void didPinchGestureUpdate();
    void didPinchGestureEnd();
    void updateScrollOffset(LayerImpl* scrollLayer);

    void setHorizontalScrollbarLayer(ScrollbarLayerImpl* layer) { m_horizontalScrollbarLayer = layer; }
    ScrollbarLayerImpl* horizontalScrollbarLayer() const { return m_horizontalScrollbarLayer; }

    void setVerticalScrollbarLayer(ScrollbarLayerImpl* layer) { m_verticalScrollbarLayer = layer; }
    ScrollbarLayerImpl* verticalScrollbarLayer() const { return m_verticalScrollbarLayer; }

    FloatPoint currentPos() const { return m_currentPos; }
    IntSize totalSize() const { return m_totalSize; }
    IntSize maximum() const { return m_maximum; }

    virtual void didPinchGestureBeginAtTime(double monotonicTime) { }
    virtual void didPinchGestureUpdateAtTime(double monotonicTime) { }
    virtual void didPinchGestureEndAtTime(double monotonicTime) { }
    virtual void updateScrollOffsetAtTime(LayerImpl* scrollLayer, double monotonicTime);

protected:
    explicit ScrollbarAnimationController(LayerImpl* scrollLayer);

private:
    static IntSize getScrollLayerBounds(const LayerImpl*);

    // Beware of dangling pointer. Always update these during tree synchronization.
    ScrollbarLayerImpl* m_horizontalScrollbarLayer;
    ScrollbarLayerImpl* m_verticalScrollbarLayer;

    FloatPoint m_currentPos;
    IntSize m_totalSize;
    IntSize m_maximum;
};

} // namespace cc

#endif // CCScrollbarAnimationController_h
