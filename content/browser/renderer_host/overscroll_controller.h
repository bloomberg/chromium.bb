// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

namespace content {

class MockRenderWidgetHost;
class OverscrollControllerDelegate;
class RenderWidgetHostImpl;

enum OverscrollMode {
  OVERSCROLL_NONE,
  OVERSCROLL_NORTH,
  OVERSCROLL_SOUTH,
  OVERSCROLL_WEST,
  OVERSCROLL_EAST,
  OVERSCROLL_COUNT
};

// When a page is scrolled beyond the scrollable region, it will trigger an
// overscroll gesture. This controller receives the events that are dispatched
// to the renderer, and the ACKs of events, and updates the overscroll gesture
// status accordingly.
class OverscrollController {
 public:
  // Creates an overscroll controller for the specified RenderWidgetHost.
  // The RenderWidgetHost owns this overscroll controller.
  explicit OverscrollController(RenderWidgetHostImpl* widget_host);
  virtual ~OverscrollController();

  // This must be called when dispatching any event from the
  // RenderWidgetHostView so that the state of the overscroll gesture can be
  // updated properly.
  // Returns true if the event should be dispatched, false otherwise.
  bool WillDispatchEvent(const WebKit::WebInputEvent& event);

  // This must be called when the ACK for any event comes in. This updates the
  // overscroll gesture status as appropriate.
  void ReceivedEventACK(const WebKit::WebInputEvent& event, bool processed);

  OverscrollMode overscroll_mode() const { return overscroll_mode_; }

  void set_delegate(OverscrollControllerDelegate* delegate) {
    delegate_ = delegate;
  }

  // Resets internal states.
  void Reset();

 private:
  friend class MockRenderWidgetHost;

  // Returns true if the event indicates that the in-progress overscroll gesture
  // can now be completed.
  bool DispatchEventCompletesAction(
      const WebKit::WebInputEvent& event) const;

  // Returns true to indicate that dispatching the event should reset the
  // overscroll gesture status.
  bool DispatchEventResetsState(const WebKit::WebInputEvent& event) const;

  // Processes an event and updates internal state for overscroll.
  void ProcessEventForOverscroll(const WebKit::WebInputEvent& event);

  // Processes horizontal overscroll. This can update both the overscroll mode
  // and the over scroll amount (i.e. |overscroll_mode_|, |overscroll_delta_x_|
  // and |overscroll_delta_y_|).
  void ProcessOverscroll(float delta_x, float delta_y);

  // Completes the desired action from the current gesture.
  void CompleteAction();

  // Sets the overscroll mode (and triggers callback in the delegate when
  // appropriate).
  void SetOverscrollMode(OverscrollMode new_mode);

  // Returns whether the input event should be forwarded to the
  // GestureEventFilter.
  bool ShouldForwardToGestureFilter(const WebKit::WebInputEvent& event) const;

  // The RenderWidgetHost that owns this overscroll controller.
  RenderWidgetHostImpl* render_widget_host_;

  // The current state of overscroll gesture.
  OverscrollMode overscroll_mode_;

  // The amount of overscroll in progress. These values are invalid when
  // |overscroll_mode_| is set to OVERSCROLL_NONE.
  float overscroll_delta_x_;
  float overscroll_delta_y_;

  // The delegate that receives the overscroll updates. The delegate is not
  // owned by this controller.
  OverscrollControllerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_H_
