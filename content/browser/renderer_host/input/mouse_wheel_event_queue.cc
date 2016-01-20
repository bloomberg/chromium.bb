// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_event_queue.h"

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"

using blink::WebInputEvent;
using blink::WebMouseWheelEvent;
using ui::LatencyInfo;

namespace content {

// This class represents a single queued mouse wheel event. Its main use
// is that it is reported via trace events.
class QueuedWebMouseWheelEvent : public MouseWheelEventWithLatencyInfo {
 public:
  QueuedWebMouseWheelEvent(const MouseWheelEventWithLatencyInfo& original_event)
      : MouseWheelEventWithLatencyInfo(original_event) {
    TRACE_EVENT_ASYNC_BEGIN0("input", "MouseWheelEventQueue::QueueEvent", this);
  }

  ~QueuedWebMouseWheelEvent() {
    TRACE_EVENT_ASYNC_END0("input", "MouseWheelEventQueue::QueueEvent", this);
  }

 private:
  bool original_can_scroll_;
  DISALLOW_COPY_AND_ASSIGN(QueuedWebMouseWheelEvent);
};

MouseWheelEventQueue::MouseWheelEventQueue(MouseWheelEventQueueClient* client,
                                           bool send_gestures,
                                           int64_t scroll_transaction_ms)
    : client_(client),
      needs_scroll_begin_(true),
      send_gestures_(send_gestures),
      scroll_transaction_ms_(scroll_transaction_ms),
      scrolling_device_(blink::WebGestureDeviceUninitialized) {
  DCHECK(client);
}

MouseWheelEventQueue::~MouseWheelEventQueue() {
  if (!wheel_queue_.empty())
    STLDeleteElements(&wheel_queue_);
}

void MouseWheelEventQueue::QueueEvent(
    const MouseWheelEventWithLatencyInfo& event) {
  TRACE_EVENT0("input", "MouseWheelEventQueue::QueueEvent");

  if (event_sent_for_gesture_ack_ && !wheel_queue_.empty()) {
    QueuedWebMouseWheelEvent* last_event = wheel_queue_.back();
    if (last_event->CanCoalesceWith(event)) {
      last_event->CoalesceWith(event);
      TRACE_EVENT_INSTANT2("input", "MouseWheelEventQueue::CoalescedWheelEvent",
                           TRACE_EVENT_SCOPE_THREAD, "total_dx",
                           last_event->event.deltaX, "total_dy",
                           last_event->event.deltaY);
      return;
    }
  }

  wheel_queue_.push_back(new QueuedWebMouseWheelEvent(event));
  TryForwardNextEventToRenderer();
  LOCAL_HISTOGRAM_COUNTS_100("Renderer.WheelQueueSize", wheel_queue_.size());
}

void MouseWheelEventQueue::ProcessMouseWheelAck(
    InputEventAckState ack_result,
    const LatencyInfo& latency_info) {
  TRACE_EVENT0("input", "MouseWheelEventQueue::ProcessMouseWheelAck");
  if (!event_sent_for_gesture_ack_)
    return;

  event_sent_for_gesture_ack_->latency.AddNewLatencyFrom(latency_info);
  client_->OnMouseWheelEventAck(*event_sent_for_gesture_ack_, ack_result);

  // If event wasn't consumed then generate a gesture scroll for it.
  if (send_gestures_ && ack_result != INPUT_EVENT_ACK_STATE_CONSUMED &&
      event_sent_for_gesture_ack_->event.canScroll &&
      (scrolling_device_ == blink::WebGestureDeviceUninitialized ||
       scrolling_device_ == blink::WebGestureDeviceTouchpad)) {
    GestureEventWithLatencyInfo scroll_update;
    scroll_update.event.type = WebInputEvent::GestureScrollUpdate;
    scroll_update.event.sourceDevice = blink::WebGestureDeviceTouchpad;
    scroll_update.event.resendingPluginId = -1;
    scroll_update.event.data.scrollUpdate.deltaX =
        event_sent_for_gesture_ack_->event.deltaX;
    scroll_update.event.data.scrollUpdate.deltaY =
        event_sent_for_gesture_ack_->event.deltaY;
    if (event_sent_for_gesture_ack_->event.scrollByPage) {
      scroll_update.event.data.scrollUpdate.deltaUnits =
          blink::WebGestureEvent::Page;

      // Turn page scrolls into a *single* page scroll because
      // the magnitude the number of ticks is lost when coalescing.
      if (scroll_update.event.data.scrollUpdate.deltaX)
        scroll_update.event.data.scrollUpdate.deltaX =
            scroll_update.event.data.scrollUpdate.deltaX > 0 ? 1 : -1;
      if (scroll_update.event.data.scrollUpdate.deltaY)
        scroll_update.event.data.scrollUpdate.deltaY =
            scroll_update.event.data.scrollUpdate.deltaY > 0 ? 1 : -1;
    } else {
      scroll_update.event.data.scrollUpdate.deltaUnits =
          event_sent_for_gesture_ack_->event.hasPreciseScrollingDeltas
              ? blink::WebGestureEvent::PrecisePixels
              : blink::WebGestureEvent::Pixels;
    }
    SendGesture(scroll_update);
  }

  event_sent_for_gesture_ack_.reset();
  TryForwardNextEventToRenderer();
}

void MouseWheelEventQueue::OnGestureScrollEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (gesture_event.event.type == blink::WebInputEvent::GestureScrollBegin) {
    // If there is a current scroll going on and a new scroll that isn't
    // wheel based cancel current one by sending a ScrollEnd.
    if (scroll_end_timer_.IsRunning() &&
        gesture_event.event.sourceDevice != blink::WebGestureDeviceTouchpad) {
      scroll_end_timer_.Reset();
      SendScrollEnd();
    }
    scrolling_device_ = gesture_event.event.sourceDevice;
  } else if (scrolling_device_ == gesture_event.event.sourceDevice &&
             (gesture_event.event.type ==
                  blink::WebInputEvent::GestureScrollEnd ||
              gesture_event.event.type ==
                  blink::WebInputEvent::GestureFlingStart)) {
    scrolling_device_ = blink::WebGestureDeviceUninitialized;
  }
}

void MouseWheelEventQueue::TryForwardNextEventToRenderer() {
  TRACE_EVENT0("input", "MouseWheelEventQueue::TryForwardNextEventToRenderer");

  if (wheel_queue_.empty() || event_sent_for_gesture_ack_)
    return;

  event_sent_for_gesture_ack_.reset(wheel_queue_.front());
  wheel_queue_.pop_front();

  MouseWheelEventWithLatencyInfo send_event(*event_sent_for_gesture_ack_);
  if (send_gestures_)
    send_event.event.canScroll = false;

  client_->SendMouseWheelEventImmediately(send_event);
}

void MouseWheelEventQueue::SendScrollEnd() {
  GestureEventWithLatencyInfo scroll_end;
  scroll_end.event.type = WebInputEvent::GestureScrollEnd;
  scroll_end.event.sourceDevice = blink::WebGestureDeviceTouchpad;
  scroll_end.event.resendingPluginId = -1;
  SendGesture(scroll_end);
}

void MouseWheelEventQueue::SendGesture(
    const GestureEventWithLatencyInfo& gesture) {
  switch (gesture.event.type) {
    case WebInputEvent::GestureScrollUpdate:
      if (needs_scroll_begin_) {
        GestureEventWithLatencyInfo scroll_begin(gesture);
        scroll_begin.event.type = WebInputEvent::GestureScrollBegin;
        scroll_begin.event.data.scrollBegin.deltaXHint =
            gesture.event.data.scrollUpdate.deltaX;
        scroll_begin.event.data.scrollBegin.deltaYHint =
            gesture.event.data.scrollUpdate.deltaY;
        scroll_begin.event.data.scrollBegin.targetViewport = false;
        scroll_begin.event.data.scrollBegin.deltaHintUnits =
            gesture.event.data.scrollUpdate.deltaUnits;

        SendGesture(scroll_begin);
      }
      if (scroll_end_timer_.IsRunning()) {
        scroll_end_timer_.Reset();
      } else {
        scroll_end_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(
                                               scroll_transaction_ms_),
                                this, &MouseWheelEventQueue::SendScrollEnd);
      }
      break;
    case WebInputEvent::GestureScrollEnd:
      needs_scroll_begin_ = true;
      break;
    case WebInputEvent::GestureScrollBegin:
      needs_scroll_begin_ = false;
      break;
    default:
      return;
  }
  client_->SendGestureEvent(gesture);
}

}  // namespace content
