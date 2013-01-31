// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_HANDLER_H_
#define CC_INPUT_HANDLER_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "cc/cc_export.h"

namespace gfx {
class Point;
class PointF;
class Vector2d;
}

namespace cc {

// The InputHandler is a way for the embedders to interact with
// the impl thread side of the compositor implementation.
//
// There is one InputHandler for every LayerTreeHost. It is
// created on the main thread and used only on the impl thread.
//
// The InputHandler is constructed with a InputHandlerClient, which is the
// interface by which the handler can manipulate the LayerTree.
class CC_EXPORT InputHandlerClient {
public:
    enum ScrollStatus { ScrollOnMainThread, ScrollStarted, ScrollIgnored };
    enum ScrollInputType { Gesture, Wheel, NonBubblingGesture };

    // Selects a layer to be scrolled at a given point in viewport (logical
    // pixel) coordinates. Returns ScrollStarted if the layer at the coordinates
    // can be scrolled, ScrollOnMainThread if the scroll event should instead be
    // delegated to the main thread, or ScrollIgnored if there is nothing to be
    // scrolled at the given coordinates.
    virtual ScrollStatus scrollBegin(gfx::Point, ScrollInputType) = 0;

    // Scroll the selected layer starting at the given position. If the scroll
    // type given to scrollBegin was a gesture, then the scroll point and delta
    // should be in viewport (logical pixel) coordinates. Otherwise they are in
    // scrolling layer's (logical pixel) space. If there is no room to move the
    // layer in the requested direction, its first ancestor layer that can be
    // scrolled will be moved instead. If no layer can be moved in the requested
    // direction at all, then false is returned. If any layer is moved, then
    // true is returned.
    // Should only be called if scrollBegin() returned ScrollStarted.
    virtual bool scrollBy(const gfx::Point&, const gfx::Vector2d&) = 0;

    // Stop scrolling the selected layer. Should only be called if scrollBegin()
    // returned ScrollStarted.
    virtual void scrollEnd() = 0;

    virtual void pinchGestureBegin() = 0;
    virtual void pinchGestureUpdate(float magnifyDelta, gfx::Point anchor) = 0;
    virtual void pinchGestureEnd() = 0;

    virtual void startPageScaleAnimation(gfx::Vector2d targetOffset,
                                         bool anchorPoint,
                                         float pageScale,
                                         base::TimeTicks startTime,
                                         base::TimeDelta duration) = 0;

    // Request another callback to InputHandler::animate().
    virtual void scheduleAnimation() = 0;

    virtual bool haveTouchEventHandlersAt(const gfx::Point&) = 0;

protected:
    InputHandlerClient() { }
    virtual ~InputHandlerClient() { }

private:
    DISALLOW_COPY_AND_ASSIGN(InputHandlerClient);
};

class CC_EXPORT InputHandler {
public:
    virtual ~InputHandler() { }

    virtual void bindToClient(InputHandlerClient*) = 0;
    virtual void animate(base::TimeTicks time) = 0;
    virtual void mainThreadHasStoppedFlinging() = 0;

protected:
    InputHandler() { }

private:
    DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}

#endif  // CC_INPUT_HANDLER_H_
