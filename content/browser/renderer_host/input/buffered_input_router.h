// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INPUT_RENDERER_HOST_BUFFERED_INPUT_ROUTER_H_
#define CONTENT_BROWSER_INPUT_RENDERER_HOST_BUFFERED_INPUT_ROUTER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/renderer_host/input/browser_input_event.h"
#include "content/browser/renderer_host/input/input_queue.h"
#include "content/browser/renderer_host/input/input_queue_client.h"
#include "content/browser/renderer_host/input/input_router.h"

namespace IPC {
class Sender;
}

namespace content {

class InputAckHandler;
class RenderProcessHost;
class RenderWidgetHostImpl;

// Batches input events into EventPackets using a general input queue. Packets
// are sent the renderer on |Flush()|, called in response to flush requests.
class CONTENT_EXPORT BufferedInputRouter
    : public NON_EXPORTED_BASE(BrowserInputEventClient),
      public NON_EXPORTED_BASE(InputQueueClient),
      public NON_EXPORTED_BASE(InputRouter) {
 public:
  // |sender|, |client| and |ack_handler| must outlive the BufferedInputRouter.
  BufferedInputRouter(IPC::Sender* sender,
                      InputRouterClient* client,
                      InputAckHandler* ack_handler,
                      int routing_id);
  virtual ~BufferedInputRouter();

  // InputRouter
  virtual void Flush() OVERRIDE;
  virtual bool SendInput(scoped_ptr<IPC::Message> message) OVERRIDE;

  // Certain unhandled input event acks may create follow-up events, e.g.,
  // TouchEvent -> GestureEvent.  If these follow-up events are sent to the
  // router synchronously from the original event's |OnDispatched()| ack, they
  // will be inserted into the current input flush stream.
  virtual void SendMouseEvent(
      const MouseEventWithLatencyInfo& mouse_event) OVERRIDE;
  virtual void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) OVERRIDE;
  virtual void SendKeyboardEvent(const NativeWebKeyboardEvent& key_event,
                                 const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) OVERRIDE;
  virtual void SendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) OVERRIDE;
  virtual void SendMouseEventImmediately(
      const MouseEventWithLatencyInfo& mouse_event) OVERRIDE;
  virtual void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) OVERRIDE;
  virtual void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) OVERRIDE;
  virtual const NativeWebKeyboardEvent* GetLastKeyboardEvent() const OVERRIDE;
  virtual bool ShouldForwardTouchEvent() const OVERRIDE;
  virtual bool ShouldForwardGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) const OVERRIDE;

  // InputQueueClient
  virtual void Deliver(const EventPacket& packet) OVERRIDE;
  virtual void DidFinishFlush() OVERRIDE;
  virtual void SetNeedsFlush() OVERRIDE;

  // BrowserInputEventClient
  virtual void OnDispatched(const BrowserInputEvent& event,
                            InputEventDisposition disposition) OVERRIDE;
  // Events delivered to the router within the scope of
  // |OnDispatched()| will be added to |followup|.
  virtual void OnDispatched(const BrowserInputEvent& event,
                            InputEventDisposition disposition,
                            ScopedVector<BrowserInputEvent>* followup) OVERRIDE;

  // IPC::Receiver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  void OnWebInputEventAck(int64 event_id,
                          const WebKit::WebInputEvent& web_event,
                          const ui::LatencyInfo& latency_info,
                          InputEventAckState acked_result,
                          bool ack_from_input_queue);
  void OnEventPacketAck(int64 packet_id,
                        const InputEventDispositions& dispositions);
  void OnHasTouchEventHandlers(bool has_handlers);

  // Returns the non-zero ID associated with the |InputEvent| added to the
  // |input_queue_|. If the event was dropped or filtered, returns 0.
  int64 QueueWebEvent(const WebKit::WebInputEvent& web_event,
                      const ui::LatencyInfo& latency_info,
                      bool is_key_shortcut);
  // Used by |QueueWebEvent()|; returns true if an event was filtered and should
  // not be added to the |input_queue_|.
  bool FilterWebEvent(const WebKit::WebInputEvent& web_event,
                      const ui::LatencyInfo& latency_info);

  // Generates a monotonically increasing sequences of id's, starting with 1.
  int64 NextInputID();

  const InputQueue* input_queue() const { return input_queue_.get(); }

 private:
  InputRouterClient* client_;
  InputAckHandler* ack_handler_;
  IPC::Sender* sender_;
  int routing_id_;

  scoped_ptr<InputQueue> input_queue_;

  // TODO(jdduke): Remove when we can properly serialize NativeWebKeyboardEvent.
  // Alternatively, attach WebInputEvents to InputEvents but don't serialize.
  typedef std::map<int64, NativeWebKeyboardEvent> KeyMap;
  KeyMap queued_key_map_;

  // Necessary for |ShouldForwardTouchEvent()|.
  bool has_touch_handler_;
  int queued_touch_count_;

  // This is non-NULL ONLY in the scope of OnInputEventAck(event, injector).
  ScopedVector<BrowserInputEvent>* input_queue_override_;

  // Used to assign unique ID's to each InputEvent that is generated.
  int64 next_input_id_;

  // 0 if there no in-flight EventPacket.
  int64 in_flight_packet_id_;

  DISALLOW_COPY_AND_ASSIGN(BufferedInputRouter);
};

} // namespace content

#endif // CONTENT_BROWSER_INPUT_RENDERER_HOST_BUFFERED_INPUT_ROUTER_H_
