// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_H_

#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/passthrough_touch_event_queue.h"
#include "content/common/widget.mojom.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/input_event_ack_state.h"
#include "ipc/ipc_listener.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {

// The InputRouter allows the embedder to customize how input events are
// sent to the renderer, and how responses are dispatched to the browser.
// While the router should respect the relative order in which events are
// received, it is free to customize when those events are dispatched.
class InputRouter : public IPC::Listener {
 public:
  struct CONTENT_EXPORT Config {
    Config();
    GestureEventQueue::Config gesture_config;
    PassthroughTouchEventQueue::Config touch_config;
  };

  ~InputRouter() override {}

  // WebInputEvents
  virtual void SendMouseEvent(
      const MouseEventWithLatencyInfo& mouse_event) = 0;
  virtual void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) = 0;
  virtual void SendKeyboardEvent(
      const NativeWebKeyboardEventWithLatencyInfo& key_event) = 0;
  virtual void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) = 0;
  virtual void SendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) = 0;

  // Notify the router about whether the current page is mobile-optimized (i.e.,
  // the site has a mobile-friendly viewport).
  virtual void NotifySiteIsMobileOptimized(bool is_mobile_optimized) = 0;

  // Whether there are any events pending dispatch to or ack from the renderer.
  virtual bool HasPendingEvents() const = 0;

  // A scale factor to scale the coordinate in WebInputEvent from DIP
  // to viewport.
  virtual void SetDeviceScaleFactor(float device_scale_factor) = 0;

  // Sets the frame tree node id of associated frame, used when tracing
  // input event latencies to relate events to their target frames. Since
  // input always flows to Local Frame Roots, the |frameTreeNodeId| is
  // relative to the Frame associated with the Local Frame Root for the
  // widget owning this InputRouter.
  virtual void SetFrameTreeNodeId(int frameTreeNodeId) = 0;

  // Return the currently allowed touch-action.
  virtual cc::TouchAction AllowedTouchAction() = 0;

  virtual void SetForceEnableZoom(bool enabled) = 0;

  // Associate this InputRouter with a remote host channel.
  virtual void BindHost(mojom::WidgetInputHandlerHostRequest request,
                        bool frame_handler) = 0;

  // Used to progress an active fling on every begin frame.
  virtual void ProgressFling(base::TimeTicks current_time) = 0;

  // Used to stop an active fling if such exists.
  virtual void StopFling() = 0;

  // Used to check if a fling cancellation is deferred due to boosting or not.
  virtual bool FlingCancellationIsDeferred() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_H_
