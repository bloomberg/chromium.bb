// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/main_thread_event_queue.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/common/input_messages.h"

namespace content {

namespace {

// Time interval at which touchmove events will be skipped during rAF signal.
const base::TimeDelta kAsyncTouchMoveInterval =
    base::TimeDelta::FromMilliseconds(200);

const size_t kTenSeconds = 10 * 1000 * 1000;

bool IsContinuousEvent(const std::unique_ptr<EventWithDispatchType>& event) {
  switch (event->event().type()) {
    case blink::WebInputEvent::MouseMove:
    case blink::WebInputEvent::MouseWheel:
    case blink::WebInputEvent::TouchMove:
      return true;
    default:
      return false;
  }
}

bool IsAsyncTouchMove(const std::unique_ptr<EventWithDispatchType>& event) {
  if (event->event().type() != blink::WebInputEvent::TouchMove)
    return false;
  const blink::WebTouchEvent& touch_event =
      static_cast<const blink::WebTouchEvent&>(event->event());
  return touch_event.movedBeyondSlopRegion && !event->originallyCancelable();
}

}  // namespace

EventWithDispatchType::EventWithDispatchType(
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency,
    InputEventDispatchType dispatch_type,
    bool originally_cancelable)
    : ScopedWebInputEventWithLatencyInfo(std::move(event), latency),
      dispatch_type_(dispatch_type),
      non_blocking_coalesced_count_(0),
      creation_timestamp_(base::TimeTicks::Now()),
      last_coalesced_timestamp_(creation_timestamp_),
      originally_cancelable_(originally_cancelable) {}

EventWithDispatchType::~EventWithDispatchType() {}

void EventWithDispatchType::CoalesceWith(const EventWithDispatchType& other) {
  // If this event was blocking push the event id to the blocking
  // list before updating the dispatch_type of this event.
  if (dispatch_type_ == DISPATCH_TYPE_BLOCKING) {
    blocking_coalesced_event_ids_.push_back(
        ui::WebInputEventTraits::GetUniqueTouchEventId(event()));
  } else {
    non_blocking_coalesced_count_++;
  }
  ScopedWebInputEventWithLatencyInfo::CoalesceWith(other);
  dispatch_type_ = other.dispatch_type_;
  last_coalesced_timestamp_ = base::TimeTicks::Now();
  originally_cancelable_ = other.originally_cancelable_;
}

MainThreadEventQueue::SharedState::SharedState()
    : sent_main_frame_request_(false) {}

MainThreadEventQueue::SharedState::~SharedState() {}

MainThreadEventQueue::MainThreadEventQueue(
    int routing_id,
    MainThreadEventQueueClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    blink::scheduler::RendererScheduler* renderer_scheduler)
    : routing_id_(routing_id),
      client_(client),
      last_touch_start_forced_nonblocking_due_to_fling_(false),
      enable_fling_passive_listener_flag_(base::FeatureList::IsEnabled(
          features::kPassiveEventListenersDueToFling)),
      enable_non_blocking_due_to_main_thread_responsiveness_flag_(
          base::FeatureList::IsEnabled(
              features::kMainThreadBusyScrollIntervention)),
      handle_raf_aligned_touch_input_(
          base::FeatureList::IsEnabled(features::kRafAlignedTouchInputEvents)),
      handle_raf_aligned_mouse_input_(
          base::FeatureList::IsEnabled(features::kRafAlignedMouseInputEvents)),
      main_task_runner_(main_task_runner),
      renderer_scheduler_(renderer_scheduler) {
  if (enable_non_blocking_due_to_main_thread_responsiveness_flag_) {
    std::string group = base::FieldTrialList::FindFullName(
        "MainThreadResponsivenessScrollIntervention");

    // The group name will be of the form Enabled$THRESHOLD_MS. Trim the prefix
    // "Enabled", and parse the threshold.
    int threshold_ms = 0;
    std::string prefix = "Enabled";
    group.erase(0, prefix.length());
    base::StringToInt(group, &threshold_ms);

    if (threshold_ms <= 0) {
      enable_non_blocking_due_to_main_thread_responsiveness_flag_ = false;
    } else {
      main_thread_responsiveness_threshold_ =
          base::TimeDelta::FromMilliseconds(threshold_ms);
    }
  }
}

MainThreadEventQueue::~MainThreadEventQueue() {}

bool MainThreadEventQueue::HandleEvent(
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency,
    InputEventDispatchType original_dispatch_type,
    InputEventAckState ack_result) {
  DCHECK(original_dispatch_type == DISPATCH_TYPE_BLOCKING ||
         original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING);
  DCHECK(ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
         ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
         ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  bool non_blocking = original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING ||
                      ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING;
  bool is_wheel = event->type() == blink::WebInputEvent::MouseWheel;
  bool is_touch = blink::WebInputEvent::isTouchEventType(event->type());
  bool originally_cancelable = false;

  if (is_touch) {
    blink::WebTouchEvent* touch_event =
        static_cast<blink::WebTouchEvent*>(event.get());

    originally_cancelable =
        touch_event->dispatchType == blink::WebInputEvent::Blocking;

    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    if (non_blocking) {
      touch_event->dispatchType =
          blink::WebInputEvent::ListenersNonBlockingPassive;
    }
    if (touch_event->type() == blink::WebInputEvent::TouchStart)
      last_touch_start_forced_nonblocking_due_to_fling_ = false;

    if (enable_fling_passive_listener_flag_ &&
        touch_event->touchStartOrFirstTouchMove &&
        touch_event->dispatchType == blink::WebInputEvent::Blocking) {
      // If the touch start is forced to be passive due to fling, its following
      // touch move should also be passive.
      if (ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
          last_touch_start_forced_nonblocking_due_to_fling_) {
        touch_event->dispatchType =
            blink::WebInputEvent::ListenersForcedNonBlockingDueToFling;
        non_blocking = true;
        last_touch_start_forced_nonblocking_due_to_fling_ = true;
      }
    }

    if (enable_non_blocking_due_to_main_thread_responsiveness_flag_ &&
        touch_event->dispatchType == blink::WebInputEvent::Blocking) {
      bool passive_due_to_unresponsive_main =
          renderer_scheduler_->MainThreadSeemsUnresponsive(
              main_thread_responsiveness_threshold_);
      if (passive_due_to_unresponsive_main) {
        touch_event->dispatchType = blink::WebInputEvent::
            ListenersForcedNonBlockingDueToMainThreadResponsiveness;
        non_blocking = true;
      }
    }
    // If the event is non-cancelable ACK it right away.
    if (!non_blocking &&
        touch_event->dispatchType != blink::WebInputEvent::Blocking)
      non_blocking = true;
  }

  if (is_wheel) {
    blink::WebMouseWheelEvent* wheel_event =
        static_cast<blink::WebMouseWheelEvent*>(event.get());
    originally_cancelable =
        wheel_event->dispatchType == blink::WebInputEvent::Blocking;
    if (non_blocking) {
      // Adjust the |dispatchType| on the event since the compositor
      // determined all event listeners are passive.
      wheel_event->dispatchType =
          blink::WebInputEvent::ListenersNonBlockingPassive;
    }
  }

  InputEventDispatchType dispatch_type =
      non_blocking ? DISPATCH_TYPE_NON_BLOCKING : DISPATCH_TYPE_BLOCKING;

  std::unique_ptr<EventWithDispatchType> event_with_dispatch_type(
      new EventWithDispatchType(std::move(event), latency, dispatch_type,
                                originally_cancelable));

  QueueEvent(std::move(event_with_dispatch_type));

  // send an ack when we are non-blocking.
  return non_blocking;
}

void MainThreadEventQueue::DispatchInFlightEvent() {
  if (in_flight_event_) {
    // Report the coalesced count only for continuous events; otherwise
    // the zero value would be dominated by non-continuous events.
    base::TimeTicks now = base::TimeTicks::Now();
    if (IsContinuousEvent(in_flight_event_)) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.MainThreadEventQueue.Continuous.QueueingTime",
          (now - in_flight_event_->creationTimestamp()).InMicroseconds(), 1,
          kTenSeconds, 50);

      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.MainThreadEventQueue.Continuous.FreshnessTime",
          (now - in_flight_event_->lastCoalescedTimestamp()).InMicroseconds(),
          1, kTenSeconds, 50);

      UMA_HISTOGRAM_COUNTS_1000("Event.MainThreadEventQueue.CoalescedCount",
                                in_flight_event_->coalescedCount());
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.MainThreadEventQueue.NonContinuous.QueueingTime",
          (now - in_flight_event_->creationTimestamp()).InMicroseconds(), 1,
          kTenSeconds, 50);
    }

    InputEventDispatchType dispatch_type = in_flight_event_->dispatchType();
    if (!in_flight_event_->blockingCoalescedEventIds().empty()) {
      switch (dispatch_type) {
        case DISPATCH_TYPE_BLOCKING:
          dispatch_type = DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN;
          break;
        case DISPATCH_TYPE_NON_BLOCKING:
          dispatch_type = DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN;
          break;
        default:
          NOTREACHED();
      }
    }
    client_->HandleEventOnMainThread(
        routing_id_, &in_flight_event_->coalesced_event(),
        in_flight_event_->latencyInfo(), dispatch_type);
  }

  in_flight_event_.reset();
}

void MainThreadEventQueue::PossiblyScheduleMainFrame() {
  if (IsRafAlignedInputDisabled())
    return;
  bool needs_main_frame = false;
  {
    base::AutoLock lock(shared_state_lock_);
    if (!shared_state_.sent_main_frame_request_ &&
        !shared_state_.events_.empty() &&
        IsRafAlignedEvent(shared_state_.events_.front()->event())) {
      needs_main_frame = !shared_state_.sent_main_frame_request_;
      shared_state_.sent_main_frame_request_ = false;
    }
  }
  if (needs_main_frame)
    client_->NeedsMainFrame(routing_id_);
}

void MainThreadEventQueue::DispatchSingleEvent() {
  {
    base::AutoLock lock(shared_state_lock_);
    if (shared_state_.events_.empty())
      return;

    in_flight_event_ = shared_state_.events_.Pop();
  }
  DispatchInFlightEvent();
  PossiblyScheduleMainFrame();
}

void MainThreadEventQueue::EventHandled(blink::WebInputEvent::Type type,
                                        blink::WebInputEventResult result,
                                        InputEventAckState ack_result) {
  if (in_flight_event_) {
    for (const auto id : in_flight_event_->blockingCoalescedEventIds()) {
      client_->SendInputEventAck(routing_id_, type, ack_result, id);
      if (renderer_scheduler_) {
        renderer_scheduler_->DidHandleInputEventOnMainThread(
            in_flight_event_->event(), result);
      }
    }
  }
}

void MainThreadEventQueue::DispatchRafAlignedInput(base::TimeTicks frame_time) {
  if (IsRafAlignedInputDisabled())
    return;

  std::deque<std::unique_ptr<EventWithDispatchType>> events_to_process;
  {
    base::AutoLock lock(shared_state_lock_);
    shared_state_.sent_main_frame_request_ = false;

    while (!shared_state_.events_.empty()) {
      if (!IsRafAlignedEvent(shared_state_.events_.front()->event()))
        break;

      // Throttle touchmoves that are async.
      if (handle_raf_aligned_touch_input_ &&
          IsAsyncTouchMove(shared_state_.events_.front())) {
        if (shared_state_.events_.size() == 1 &&
            frame_time < shared_state_.last_async_touch_move_timestamp_ +
                             kAsyncTouchMoveInterval) {
          break;
        }
        shared_state_.last_async_touch_move_timestamp_ = frame_time;
      }
      events_to_process.emplace_back(shared_state_.events_.Pop());
    }
  }

  while(!events_to_process.empty()) {
    in_flight_event_ = std::move(events_to_process.front());
    events_to_process.pop_front();
    DispatchInFlightEvent();
  }
  PossiblyScheduleMainFrame();
}

void MainThreadEventQueue::SendEventNotificationToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MainThreadEventQueue::DispatchSingleEvent, this));
}

void MainThreadEventQueue::QueueEvent(
    std::unique_ptr<EventWithDispatchType> event) {
  bool is_raf_aligned = IsRafAlignedEvent(event->event());
  size_t send_notification_count = 0;
  bool needs_main_frame = false;
  {
    base::AutoLock lock(shared_state_lock_);
    size_t size_before = shared_state_.events_.size();

    // Stash if the tail of the queue was rAF aligned.
    bool was_raf_aligned = false;
    if (size_before > 0) {
      was_raf_aligned =
          IsRafAlignedEvent(shared_state_.events_.at(size_before - 1)->event());
    }
    shared_state_.events_.Queue(std::move(event));
    size_t size_after = shared_state_.events_.size();

    if (size_before != size_after) {
      if (IsRafAlignedInputDisabled()) {
        send_notification_count = 1;
      } else if (!is_raf_aligned) {
        send_notification_count = 1;
        // If we had just enqueued a non-rAF input event we will send a series
        // of normal post messages to ensure they are all handled right away.
        for (size_t pos = size_after - 1; pos >= 1; --pos) {
          if (IsRafAlignedEvent(shared_state_.events_.at(pos - 1)->event()))
            send_notification_count++;
          else
            break;
        }
      } else {
        needs_main_frame = !shared_state_.sent_main_frame_request_;
        shared_state_.sent_main_frame_request_ = true;
      }
    } else if (size_before > 0) {
      // The event was coalesced. The queue size didn't change but
      // the rAF alignment of the event may have and we need to schedule
      // a notification.
      bool is_coalesced_raf_aligned =
          IsRafAlignedEvent(shared_state_.events_.at(size_before - 1)->event());
      if (was_raf_aligned != is_coalesced_raf_aligned)
        send_notification_count = 1;
    }
  }

  for (size_t i = 0; i < send_notification_count; ++i)
    SendEventNotificationToMainThread();
  if (needs_main_frame)
    client_->NeedsMainFrame(routing_id_);
}

bool MainThreadEventQueue::IsRafAlignedInputDisabled() {
  return !handle_raf_aligned_mouse_input_ && !handle_raf_aligned_touch_input_;
}

bool MainThreadEventQueue::IsRafAlignedEvent(
    const blink::WebInputEvent& event) {
  switch (event.type()) {
    case blink::WebInputEvent::MouseMove:
    case blink::WebInputEvent::MouseWheel:
      return handle_raf_aligned_mouse_input_;
    case blink::WebInputEvent::TouchMove:
      return handle_raf_aligned_touch_input_;
    default:
      return false;
  }
}

}  // namespace content
