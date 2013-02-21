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
    if (ShouldForwardToRenderer(event))
      render_widget_host_->ForwardTouchEventImmediately(event);
    else
      PopTouchEventToView(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
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
  if (touch_queue_.empty())
    return;

  // Update the ACK status for each touch point in the ACKed event.
  const WebKit::WebTouchEvent& event = touch_queue_.front()->coalesced_event();
  if (event.type == WebKit::WebInputEvent::TouchEnd ||
      event.type == WebKit::WebInputEvent::TouchCancel) {
    // The points have been released. Erase the ACK states.
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const WebKit::WebTouchPoint& point = event.touches[i];
      if (point.state == WebKit::WebTouchPoint::StateReleased ||
          point.state == WebKit::WebTouchPoint::StateCancelled)
        touch_ack_states_.erase(point.id);
    }
  } else if (event.type == WebKit::WebInputEvent::TouchStart) {
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const WebKit::WebTouchPoint& point = event.touches[i];
      if (point.state == WebKit::WebTouchPoint::StatePressed)
        touch_ack_states_[point.id] = ack_result;
    }
  }

  PopTouchEventToView(ack_result);

  // If there are queued touch events, then try to forward them to the renderer
  // immediately, or ACK the events back to the view if appropriate.
  while (!touch_queue_.empty()) {
    const WebKit::WebTouchEvent& touch =
        touch_queue_.front()->coalesced_event();
    if (ShouldForwardToRenderer(touch)) {
      render_widget_host_->ForwardTouchEventImmediately(touch);
      break;
    }
    PopTouchEventToView(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
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

bool TouchEventQueue::ShouldForwardToRenderer(
    const WebKit::WebTouchEvent& event) const {
  // Touch press events should always be forwarded to the renderer.
  if (event.type == WebKit::WebInputEvent::TouchStart)
    return true;

  for (unsigned int i = 0; i < event.touchesLength; ++i) {
    const WebKit::WebTouchPoint& point = event.touches[i];
    // If a point has been stationary, then don't take it into account.
    if (point.state == WebKit::WebTouchPoint::StateStationary)
      continue;

    if (touch_ack_states_.count(point.id) > 0) {
      if (touch_ack_states_.find(point.id)->second !=
          INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS)
        return true;
    } else {
      // If the ACK status of a point is unknown, then the event should be
      // forwarded to the renderer.
      return true;
    }
  }

  return false;
}

}  // namespace content
