// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/mouse_wheel_event_queue.h"
#include "content/browser/renderer_host/input/touch_action_filter.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/common/input/input_event_ack_source.h"
#include "content/common/input/input_event_stream_validator.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/public/browser/native_web_keyboard_event.h"

namespace ui {
class LatencyInfo;
struct DidOverscrollParams;
}  // namespace ui

namespace content {

class InputDispositionHandler;

class CONTENT_EXPORT InputRouterImplClient : public InputRouterClient {
 public:
  virtual mojom::WidgetInputHandler* GetWidgetInputHandler() = 0;
};

// A default implementation for browser input event routing.
class CONTENT_EXPORT InputRouterImpl
    : public NON_EXPORTED_BASE(InputRouter),
      public NON_EXPORTED_BASE(GestureEventQueueClient),
      public NON_EXPORTED_BASE(MouseWheelEventQueueClient),
      public NON_EXPORTED_BASE(TouchEventQueueClient),
      public NON_EXPORTED_BASE(TouchpadTapSuppressionControllerClient) {
 public:
  InputRouterImpl(InputRouterImplClient* client,
                  InputDispositionHandler* disposition_handler,
                  const Config& config);
  ~InputRouterImpl() override;

  // InputRouter
  void SendMouseEvent(const MouseEventWithLatencyInfo& mouse_event) override;
  void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override;
  void SendKeyboardEvent(
      const NativeWebKeyboardEventWithLatencyInfo& key_event) override;
  void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) override;
  void SendTouchEvent(const TouchEventWithLatencyInfo& touch_event) override;
  void NotifySiteIsMobileOptimized(bool is_mobile_optimized) override;
  bool HasPendingEvents() const override;
  void SetDeviceScaleFactor(float device_scale_factor) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override;

  void SetFrameTreeNodeId(int frame_tree_node_id) override;

  cc::TouchAction AllowedTouchAction() override;

 private:
  friend class InputRouterImplTest;

  // Keeps track of last position of touch points and sets MovementXY for them.
  void SetMovementXYForTouchPoints(blink::WebTouchEvent* event);

  // TouchpadTapSuppressionControllerClient
  void SendMouseEventImmediately(
      const MouseEventWithLatencyInfo& mouse_event) override;

  // TouchEventQueueClient
  void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) override;
  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckState ack_result) override;
  void OnFilteringTouchEvent(const blink::WebTouchEvent& touch_event) override;

  // GestureEventFilterClient
  void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) override;
  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckState ack_result) override;

  // MouseWheelEventQueueClient
  void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& touch_event) override;
  void OnMouseWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                            InputEventAckState ack_result) override;
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override;

  bool SendMoveCaret(std::unique_ptr<IPC::Message> message);
  bool SendSelectMessage(std::unique_ptr<IPC::Message> message);
  bool Send(IPC::Message* message);

  void FilterAndSendWebInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info,
      mojom::WidgetInputHandler::DispatchEventCallback callback);

  void KeyboardEventHandled(
      const NativeWebKeyboardEventWithLatencyInfo& event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll);

  void MouseEventHandled(
      const MouseEventWithLatencyInfo& event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll);

  void TouchEventHandled(
      const TouchEventWithLatencyInfo& touch_event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll);
  void GestureEventHandled(
      const GestureEventWithLatencyInfo& gesture_event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll);
  void MouseWheelEventHandled(
      const MouseWheelEventWithLatencyInfo& event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll);

  // IPC message handlers
  void OnDidOverscroll(const ui::DidOverscrollParams& params);
  void OnHasTouchEventHandlers(bool has_handlers);
  void OnSetTouchAction(cc::TouchAction touch_action);
  void OnDidStopFlinging();

  // Dispatches the ack'ed event to |ack_handler_|.
  void ProcessKeyboardAck(blink::WebInputEvent::Type type,
                          InputEventAckState ack_result,
                          const ui::LatencyInfo& latency);

  // Forwards a valid |next_mouse_move_| if |type| is MouseMove.
  void ProcessMouseAck(blink::WebInputEvent::Type type,
                       InputEventAckState ack_result,
                       const ui::LatencyInfo& latency);

  // Dispatches the ack'ed event to |ack_handler_|, forwarding queued events
  // from |coalesced_mouse_wheel_events_|.
  void ProcessWheelAck(InputEventAckState ack_result,
                       const ui::LatencyInfo& latency);

  // Forwards the event ack to |gesture_event_queue|, potentially triggering
  // dispatch of queued gesture events.
  void ProcessGestureAck(blink::WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         const ui::LatencyInfo& latency);

  // Forwards the event ack to |touch_event_queue_|, potentially triggering
  // dispatch of queued touch events, or the creation of gesture events.
  void ProcessTouchAck(InputEventAckState ack_result,
                       const ui::LatencyInfo& latency,
                       uint32_t unique_touch_event_id);

  // Called when a touch timeout-affecting bit has changed, in turn toggling the
  // touch ack timeout feature of the |touch_event_queue_| as appropriate. Input
  // to that determination includes current view properties and the allowed
  // touch action. Note that this will only affect platforms that have a
  // non-zero touch timeout configuration.
  void UpdateTouchAckTimeoutEnabled();

  InputRouterImplClient* client_;
  InputDispositionHandler* disposition_handler_;
  int frame_tree_node_id_;

  // Whether there are any active flings in the renderer. As the fling
  // end notification is asynchronous, we use a count rather than a boolean
  // to avoid races in bookkeeping when starting a new fling.
  int active_renderer_fling_count_;

  // Whether the TouchScrollStarted event has been sent for the current
  // gesture scroll yet.
  bool touch_scroll_started_sent_;

  bool wheel_scroll_latching_enabled_;
  bool raf_aligned_touch_enabled_;
  MouseWheelEventQueue wheel_event_queue_;
  std::unique_ptr<TouchEventQueue> touch_event_queue_;
  GestureEventQueue gesture_event_queue_;
  TouchActionFilter touch_action_filter_;
  InputEventStreamValidator input_stream_validator_;
  InputEventStreamValidator output_stream_validator_;

  float device_scale_factor_;

  // Last touch position relative to screen. Used to compute movementX/Y.
  base::flat_map<int, gfx::Point> global_touch_position_;

  base::WeakPtr<InputRouterImpl> weak_this_;
  base::WeakPtrFactory<InputRouterImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputRouterImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_IMPL_H_
