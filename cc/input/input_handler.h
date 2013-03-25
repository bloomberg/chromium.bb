// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_INPUT_HANDLER_H_
#define CC_INPUT_INPUT_HANDLER_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "cc/base/cc_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"

namespace gfx {
class Point;
class PointF;
class Vector2d;
class Vector2dF;
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
  virtual ScrollStatus ScrollBegin(gfx::Point viewport_point,
                                   ScrollInputType type) = 0;

  // Scroll the selected layer starting at the given position. If the scroll
  // type given to ScrollBegin was a gesture, then the scroll point and delta
  // should be in viewport (logical pixel) coordinates. Otherwise they are in
  // scrolling layer's (logical pixel) space. If there is no room to move the
  // layer in the requested direction, its first ancestor layer that can be
  // scrolled will be moved instead. If no layer can be moved in the requested
  // direction at all, then false is returned. If any layer is moved, then
  // true is returned.
  // Should only be called if ScrollBegin() returned ScrollStarted.
  virtual bool ScrollBy(gfx::Point viewport_point,
                        gfx::Vector2dF scroll_delta) = 0;

  virtual bool ScrollVerticallyByPage(
      gfx::Point viewport_point,
      WebKit::WebScrollbar::ScrollDirection direction) = 0;

  // Stop scrolling the selected layer. Should only be called if ScrollBegin()
  // returned ScrollStarted.
  virtual void ScrollEnd() = 0;

  virtual void PinchGestureBegin() = 0;
  virtual void PinchGestureUpdate(float magnify_delta, gfx::Point anchor) = 0;
  virtual void PinchGestureEnd() = 0;

  virtual void StartPageScaleAnimation(gfx::Vector2d target_offset,
                                       bool anchor_point,
                                       float page_scale,
                                       base::TimeTicks start_time,
                                       base::TimeDelta duration) = 0;

  // Request another callback to InputHandler::Animate().
  virtual void ScheduleAnimation() = 0;

  virtual bool HaveTouchEventHandlersAt(gfx::Point viewport_point) = 0;

 protected:
  InputHandlerClient() {}
  virtual ~InputHandlerClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InputHandlerClient);
};

class CC_EXPORT InputHandler {
 public:
  virtual ~InputHandler() {}

  virtual void BindToClient(InputHandlerClient* client) = 0;
  virtual void Animate(base::TimeTicks time) = 0;
  virtual void MainThreadHasStoppedFlinging() = 0;

 protected:
  InputHandler() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace cc

#endif  // CC_INPUT_INPUT_HANDLER_H_
