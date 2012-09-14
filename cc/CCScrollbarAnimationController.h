// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScrollbarAnimationController_h
#define CCScrollbarAnimationController_h

#include "FloatPoint.h"
#include "IntSize.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

class CCLayerImpl;
class CCScrollbarLayerImpl;

// This abstract class represents the compositor-side analogy of ScrollbarAnimator.
// Individual platforms should subclass it to provide specialized implementation.
class CCScrollbarAnimationController {
public:
    // Implemented by subclass.
    static PassOwnPtr<CCScrollbarAnimationController> create(CCLayerImpl* scrollLayer);

    virtual ~CCScrollbarAnimationController();

    virtual bool animate(double monotonicTime) { return false; }
    void didPinchGestureBegin();
    void didPinchGestureUpdate();
    void didPinchGestureEnd();
    void updateScrollOffset(CCLayerImpl* scrollLayer);

    void setHorizontalScrollbarLayer(CCScrollbarLayerImpl* layer) { m_horizontalScrollbarLayer = layer; }
    CCScrollbarLayerImpl* horizontalScrollbarLayer() const { return m_horizontalScrollbarLayer; }

    void setVerticalScrollbarLayer(CCScrollbarLayerImpl* layer) { m_verticalScrollbarLayer = layer; }
    CCScrollbarLayerImpl* verticalScrollbarLayer() const { return m_verticalScrollbarLayer; }

    FloatPoint currentPos() const { return m_currentPos; }
    IntSize totalSize() const { return m_totalSize; }
    IntSize maximum() const { return m_maximum; }

    virtual void didPinchGestureBeginAtTime(double monotonicTime) { }
    virtual void didPinchGestureUpdateAtTime(double monotonicTime) { }
    virtual void didPinchGestureEndAtTime(double monotonicTime) { }
    virtual void updateScrollOffsetAtTime(CCLayerImpl* scrollLayer, double monotonicTime);

protected:
    explicit CCScrollbarAnimationController(CCLayerImpl* scrollLayer);

private:
    static IntSize getScrollLayerBounds(const CCLayerImpl*);

    // Beware of dangling pointer. Always update these during tree synchronization.
    CCScrollbarLayerImpl* m_horizontalScrollbarLayer;
    CCScrollbarLayerImpl* m_verticalScrollbarLayer;

    FloatPoint m_currentPos;
    IntSize m_totalSize;
    IntSize m_maximum;
};

} // namespace cc

#endif // CCScrollbarAnimationController_h
