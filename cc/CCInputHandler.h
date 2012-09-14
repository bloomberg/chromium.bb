// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCInputHandler_h
#define CCInputHandler_h

#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace cc {

class IntPoint;
class IntSize;

// The CCInputHandler is a way for the embedders to interact with
// the impl thread side of the compositor implementation.
//
// There is one CCInputHandler for every CCLayerTreeHost. It is
// created on the main thread and used only on the impl thread.
//
// The CCInputHandler is constructed with a CCInputHandlerClient, which is the
// interface by which the handler can manipulate the LayerTree.
class CCInputHandlerClient {
    WTF_MAKE_NONCOPYABLE(CCInputHandlerClient);
public:
    enum ScrollStatus { ScrollOnMainThread, ScrollStarted, ScrollIgnored };
    enum ScrollInputType { Gesture, Wheel };

    // Selects a layer to be scrolled at a given point in window coordinates.
    // Returns ScrollStarted if the layer at the coordinates can be scrolled,
    // ScrollOnMainThread if the scroll event should instead be delegated to the
    // main thread, or ScrollIgnored if there is nothing to be scrolled at the
    // given coordinates.
    virtual ScrollStatus scrollBegin(const IntPoint&, ScrollInputType) = 0;

    // Scroll the selected layer starting at the given window coordinate. If
    // there is no room to move the layer in the requested direction, its first
    // ancestor layer that can be scrolled will be moved instead. Should only be
    // called if scrollBegin() returned ScrollStarted.
    virtual void scrollBy(const IntPoint&, const IntSize&) = 0;

    // Stop scrolling the selected layer. Should only be called if scrollBegin()
    // returned ScrollStarted.
    virtual void scrollEnd() = 0;

    virtual void pinchGestureBegin() = 0;
    virtual void pinchGestureUpdate(float magnifyDelta, const IntPoint& anchor) = 0;
    virtual void pinchGestureEnd() = 0;

    virtual void startPageScaleAnimation(const IntSize& targetPosition,
                                         bool anchorPoint,
                                         float pageScale,
                                         double startTime,
                                         double duration) = 0;

    // Request another callback to CCInputHandler::animate().
    virtual void scheduleAnimation() = 0;

protected:
    CCInputHandlerClient() { }
    virtual ~CCInputHandlerClient() { }
};

class CCInputHandler {
    WTF_MAKE_NONCOPYABLE(CCInputHandler);
public:
    virtual ~CCInputHandler() { }

    virtual void bindToClient(CCInputHandlerClient*) = 0;
    virtual void animate(double monotonicTime) = 0;

protected:
    CCInputHandler() { }
};

}

#endif
