// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/touch_event_queue.h"

#include "base/stl_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/port/browser/render_widget_host_view_port.h"

namespace content {

typedef std::vector<WebKit::WebTouchEvent> WebTouchEventList;

// This class represents a single coalesced touch event. However, it also keeps
// track of all the original touch-events that were coalesced into a single
// event. The coalesced event is forwarded to the renderer, while the original
// touch-events are sent to the View (on ACK for the coalesced event) so that
// the View receives the event with their original timestamp.
class CoalescedWebTouchEvent {
 public:
  explicit CoalescedWebTouchEvent(const WebKit::WebTouchEvent& event)
      : coalesced_event_(event) {
    events_.push_back(event);
  }

  ~CoalescedWebTouchEvent() {}

  // Coalesces the event with the existing event if possible. Returns whether
  // the event was coalesced.
  bool CoalesceEventIfPossible(const WebKit::WebTouchEvent& event) {
    if (coalesced_event_.type == WebKit::WebInputEvent::TouchMove &&
        event.type == WebKit::WebInputEvent::TouchMove &&
        coalesced_event_.modifiers == event.modifiers &&
        coalesced_event_.touchesLength == event.touchesLength) {
      events_.push_back(event);
      // The WebTouchPoints include absolute position information. So it is
      // sufficient to simply replace the previous event with the new event.
      // However, it is necessary to make sure that all the points have the
      // correct state, i.e. the touch-points that moved in the last event, but
      // didn't change in the current event, will have Stationary state. It is
      // necessary to change them back to Moved state.
      const WebKit::WebTouchEvent last_event = coalesced_event_;
      coalesced_event_ = event;
      for (unsigned i = 0; i < last_event.touchesLength; ++i) {
        if (last_event.touches[i].state == WebKit::WebTouchPoint::StateMoved)
          coalesced_event_.touches[i].state = WebKit::WebTouchPoint::StateMoved;
      }
      return true;
    }

    return false;
  }

  const WebKit::WebTouchEvent& coalesced_event() const {
    return coalesced_event_;
  }

  WebTouchEventList::const_iterator begin() const {
    return events_.begin();
  }

  WebTouchEventList::const_iterator end() const {
    return events_.end();
  }

  size_t size() const { return events_.size(); }

 private:
  // This is the event that is forwarded to the renderer.
  WebKit::WebTouchEvent coalesced_event_;

  // This is the list of the original events that were coalesced.
  WebTouchEventList events_;

  DISALLOW_COPY_AND_ASSIGN(CoalescedWebTouchEvent);
};

TouchEventQueue::TouchEventQueue(RenderWidgetHostImpl* host)
    : render_widget_host_(host) {
}

TouchEventQueue::~TouchEventQueue() {
  if (!touch_queue_.empty())
    STLDeleteElements(&touch_queue_);
}

void TouchEventQueue::QueueEvent(const WebKit::WebTouchEvent& event) {
  if (touch_queue_.empty()) {
    // There is no touch event in the queue. Forward it to the renderer
    // immediately.
    touch_queue_.push_back(new CoalescedWebTouchEvent(event));
    render_widget_host_->ForwardTouchEventImmediately(event);
    return;
  }

  // If the last queued touch-event was a touch-move, and the current event is
  // also a touch-move, then the events can be coalesced into a single event.
  if (touch_queue_.size() > 1) {
    CoalescedWebTouchEvent* last_event = touch_queue_.back();
    if (last_event->CoalesceEventIfPossible(event))
      return;
  }
  touch_queue_.push_back(new CoalescedWebTouchEvent(event));
}

void TouchEventQueue::ProcessTouchAck(InputEventAckState ack_result) {
  PopTouchEventToView(ack_result);
  // If there's a queued touch-event, then forward it to the renderer now.
  if (!touch_queue_.empty()) {
    render_widget_host_->ForwardTouchEventImmediately(
        touch_queue_.front()->coalesced_event());
  }
}

void TouchEventQueue::FlushQueue() {
  while (!touch_queue_.empty())
    PopTouchEventToView(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
}

void TouchEventQueue::Reset() {
  touch_queue_.clear();
}

size_t TouchEventQueue::GetQueueSize() const {
  return touch_queue_.size();
}

const WebKit::WebTouchEvent& TouchEventQueue::GetLatestEvent() const {
  return touch_queue_.back()->coalesced_event();
}

void TouchEventQueue::PopTouchEventToView(InputEventAckState ack_result) {
  if (touch_queue_.empty())
    return;
  scoped_ptr<CoalescedWebTouchEvent> acked_event(touch_queue_.front());
  touch_queue_.pop_front();

  // Note that acking the touch-event may result in multiple gestures being sent
  // to the renderer.
  RenderWidgetHostViewPort* view = RenderWidgetHostViewPort::FromRWHV(
      render_widget_host_->GetView());
  for (WebTouchEventList::const_iterator iter = acked_event->begin(),
       end = acked_event->end();
       iter != end; ++iter) {
    view->ProcessAckedTouchEvent((*iter), ack_result);
  }
}

}  // namespace content
