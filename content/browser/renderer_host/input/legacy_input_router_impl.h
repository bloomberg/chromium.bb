// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_LEGACY_INPUT_ROUTER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_LEGACY_INPUT_ROUTER_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/mouse_wheel_event_queue.h"
#include "content/browser/renderer_host/input/touch_action_filter.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/common/input/input_event_stream_validator.h"
#include "content/public/browser/native_web_keyboard_event.h"

namespace IPC {
class Sender;
}

namespace ui {
class LatencyInfo;
struct DidOverscrollParams;
}

namespace content {

class InputDispositionHandler;
class InputRouterClient;
struct InputEventAck;

// An implementation for browser input event routing based on
// Chrome IPC. This class is named "legacy" because it is largely tied to
// Chrome IPC which is deprecated. This class will be replaced with a Mojo
// backed transport. See crbug.com/722928.
class CONTENT_EXPORT LegacyInputRouterImpl
    : public InputRouter,
      public GestureEventQueueClient,
      public MouseWheelEventQueueClient,
      public TouchEventQueueClient,
      public TouchpadTapSuppressionControllerClient {
 public:
  LegacyInputRouterImpl(IPC::Sender* sender,
                        InputRouterClient* client,
                        InputDispositionHandler* disposition_handler,
                        int routing_id,
                        const Config& config);
  ~LegacyInputRouterImpl() override;

  bool SendInput(std::unique_ptr<IPC::Message> message);

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
  void BindHost(mojom::WidgetInputHandlerHostRequest request) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override;

  void SetFrameTreeNodeId(int frameTreeNodeId) override;

  cc::TouchAction AllowedTouchAction() override;

  void SetForceEnableZoom(bool enabled) override;

  int routing_id() const { return routing_id_; }

 private:
  friend class LegacyInputRouterImplTest;
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           SubframeTouchEventRouting);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           MainframeTouchEventRouting);

  // Keeps track of last position of touch points and sets MovementXY for them.
  void SetMovementXYForTouchPoints(blink::WebTouchEvent* event);

  // TouchpadTapSuppressionControllerClient
  void SendMouseEventImmediately(
      const MouseEventWithLatencyInfo& mouse_event) override;

  // TouchEventQueueClient
  void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) override;
  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckSource ack_source,
                       InputEventAckState ack_result) override;
  void OnFilteringTouchEvent(const blink::WebTouchEvent& touch_event) override;

  // GestureEventFilterClient
  void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) override;
  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckSource ack_source,
                         InputEventAckState ack_result) override;

  // MouseWheelEventQueueClient
  void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& touch_event) override;
  void OnMouseWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                            InputEventAckSource ack_source,
                            InputEventAckState ack_result) override;
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override;

  bool SendMoveCaret(std::unique_ptr<IPC::Message> message);
  bool SendSelectMessage(std::unique_ptr<IPC::Message> message);
  bool Send(IPC::Message* message);

  // Filters and forwards |input_event| to the appropriate handler.
  void FilterAndSendWebInputEvent(const blink::WebInputEvent& input_event,
                                  const ui::LatencyInfo& latency_info);

  // Utility routine for filtering and forwarding |input_event| to the
  // appropriate handler. |input_event| will be offered to the overscroll
  // controller, client and renderer, in that order.
  void OfferToHandlers(const blink::WebInputEvent& input_event,
                       const ui::LatencyInfo& latency_info);

  // Returns true if |input_event| was consumed by the client.
  bool OfferToClient(const blink::WebInputEvent& input_event,
                     const ui::LatencyInfo& latency_info);

  // Returns true if |input_event| was successfully sent to the renderer
  // as an async IPC Message.
  bool OfferToRenderer(const blink::WebInputEvent& input_event,
                       const ui::LatencyInfo& latency_info,
                       InputEventDispatchType dispatch_type);

  // IPC message handlers
  void OnInputEventAck(const InputEventAck& ack);
  void OnDidOverscroll(const ui::DidOverscrollParams& params);
  void OnMsgMoveCaretAck();
  void OnSelectMessageAck();
  void OnHasTouchEventHandlers(bool has_handlers);
  void OnSetTouchAction(cc::TouchAction touch_action);
  void OnSetWhiteListedTouchAction(cc::TouchAction white_listed_touch_action,
                                   uint32_t unique_touch_event_id,
                                   InputEventAckState ack_result);
  void OnDidStopFlinging();

  // Note: This function may result in |this| being deleted, and as such
  // should be the last method called in any internal chain of event handling.
  void ProcessInputEventAck(blink::WebInputEvent::Type event_type,
                            InputEventAckSource ack_source,
                            InputEventAckState ack_result,
                            const ui::LatencyInfo& latency_info,
                            uint32_t unique_touch_event_id);

  // Dispatches the ack'ed event to |ack_handler_|.
  void ProcessKeyboardAck(blink::WebInputEvent::Type type,
                          InputEventAckSource ack_source,
                          InputEventAckState ack_result,
                          const ui::LatencyInfo& latency);

  // Forwards a valid |next_mouse_move_| if |type| is MouseMove.
  void ProcessMouseAck(blink::WebInputEvent::Type type,
                       InputEventAckSource ack_source,
                       InputEventAckState ack_result,
                       const ui::LatencyInfo& latency);

  // Dispatches the ack'ed event to |ack_handler_|, forwarding queued events
  // from |coalesced_mouse_wheel_events_|.
  void ProcessWheelAck(InputEventAckSource ack_source,
                       InputEventAckState ack_result,
                       const ui::LatencyInfo& latency);

  // Forwards the event ack to |gesture_event_queue|, potentially triggering
  // dispatch of queued gesture events.
  void ProcessGestureAck(blink::WebInputEvent::Type type,
                         InputEventAckSource ack_source,
                         InputEventAckState ack_result,
                         const ui::LatencyInfo& latency);

  // Forwards the event ack to |touch_event_queue_|, potentially triggering
  // dispatch of queued touch events, or the creation of gesture events.
  void ProcessTouchAck(InputEventAckSource ack_source,
                       InputEventAckState ack_result,
                       const ui::LatencyInfo& latency,
                       uint32_t unique_touch_event_id);

  // Called when a touch timeout-affecting bit has changed, in turn toggling the
  // touch ack timeout feature of the |touch_event_queue_| as appropriate. Input
  // to that determination includes current view properties and the allowed
  // touch action. Note that this will only affect platforms that have a
  // non-zero touch timeout configuration.
  void UpdateTouchAckTimeoutEnabled();

  IPC::Sender* sender_;
  InputRouterClient* client_;
  InputDispositionHandler* disposition_handler_;
  int routing_id_;
  int frame_tree_node_id_;

  // (Similar to |mouse_move_pending_|.) True while waiting for SelectRange_ACK
  // or MoveRangeSelectionExtent_ACK.
  bool select_message_pending_;

  // Queue of pending select messages to send after receiving the next select
  // message ack.
  base::circular_deque<std::unique_ptr<IPC::Message>> pending_select_messages_;

  // True while waiting for MoveCaret_ACK.
  bool move_caret_pending_;

  // The next MoveCaret to send, if any.
  std::unique_ptr<IPC::Message> next_move_caret_;

  // A queue of the mouse move events sent to the renderer. Similar
  // to |key_queue_|.
  using MouseEventQueue = base::circular_deque<MouseEventWithLatencyInfo>;
  MouseEventQueue mouse_event_queue_;

  // A queue of keyboard events. We can't trust data from the renderer so we
  // stuff key events into a queue and pop them out on ACK, feeding our copy
  // back to whatever unhandled handler instead of the returned version.
  using KeyQueue = base::circular_deque<NativeWebKeyboardEventWithLatencyInfo>;
  KeyQueue key_queue_;

  // Whether there are any active flings in the renderer. As the fling
  // end notification is asynchronous, we use a count rather than a boolean
  // to avoid races in bookkeeping when starting a new fling.
  int active_renderer_fling_count_;

  // Whether the TouchScrollStarted event has been sent for the current
  // gesture scroll yet.
  bool touch_scroll_started_sent_;

  bool wheel_scroll_latching_enabled_;

  MouseWheelEventQueue wheel_event_queue_;
  std::unique_ptr<TouchEventQueue> touch_event_queue_;
  GestureEventQueue gesture_event_queue_;
  TouchActionFilter touch_action_filter_;
  InputEventStreamValidator input_stream_validator_;
  InputEventStreamValidator output_stream_validator_;

  float device_scale_factor_;

  // Last touch position relative to screen. Used to compute movementX/Y.
  std::map<int, gfx::Point> global_touch_position_;

  DISALLOW_COPY_AND_ASSIGN(LegacyInputRouterImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_LEGACY_INPUT_ROUTER_IMPL_H_
