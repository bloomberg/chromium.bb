// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCROLLBAR_ANIMATION_CONTROLLER_H_
#define CC_SCROLLBAR_ANIMATION_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {

class LayerImpl;
class ScrollbarLayerImpl;

// This abstract class represents the compositor-side analogy of ScrollbarAnimator.
// Individual platforms should subclass it to provide specialized implementation.
class CC_EXPORT ScrollbarAnimationController {
public:
    static scoped_ptr<ScrollbarAnimationController> create(LayerImpl* scrollLayer);

    virtual ~ScrollbarAnimationController();

    virtual bool animate(double monotonicTime);
    void didPinchGestureBegin();
    void didPinchGestureUpdate();
    void didPinchGestureEnd();
    void updateScrollOffset(LayerImpl* scrollLayer);

    void setHorizontalScrollbarLayer(ScrollbarLayerImpl* layer);
    ScrollbarLayerImpl* horizontalScrollbarLayer() const { return m_horizontalScrollbarLayer; }

    void setVerticalScrollbarLayer(ScrollbarLayerImpl* layer);
    ScrollbarLayerImpl* verticalScrollbarLayer() const { return m_verticalScrollbarLayer; }

    gfx::Vector2dF currentOffset() const { return m_currentOffset; }
    gfx::Size totalSize() const { return m_totalSize; }
    gfx::Vector2d maximum() const { return m_maximum; }

    virtual void didPinchGestureBeginAtTime(double monotonicTime) { }
    virtual void didPinchGestureUpdateAtTime(double monotonicTime) { }
    virtual void didPinchGestureEndAtTime(double monotonicTime) { }
    virtual void updateScrollOffsetAtTime(LayerImpl* scrollLayer, double monotonicTime);

protected:
    explicit ScrollbarAnimationController(LayerImpl* scrollLayer);

private:
    // Beware of dangling pointer. Always update these during tree synchronization.
    ScrollbarLayerImpl* m_horizontalScrollbarLayer;
    ScrollbarLayerImpl* m_verticalScrollbarLayer;

    gfx::Vector2dF m_currentOffset;
    gfx::Size m_totalSize;
    gfx::Vector2d m_maximum;
};

} // namespace cc

#endif  // CC_SCROLLBAR_ANIMATION_CONTROLLER_H_
