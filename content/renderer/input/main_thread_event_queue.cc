// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/main_thread_event_queue.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/common/input_messages.h"
#include "content/renderer/render_widget.h"

namespace content {

namespace {

const size_t kTenSeconds = 10 * 1000 * 1000;
const base::TimeDelta kMaxRafDelay =
    base::TimeDelta::FromMilliseconds(5 * 1000);

class QueuedClosure : public MainThreadEventQueueTask {
 public:
  QueuedClosure(const base::Closure& closure) : closure_(closure) {}

  ~QueuedClosure() override {}

  FilterResult FilterNewEvent(
      const MainThreadEventQueueTask& other_task) override {
    return other_task.IsWebInputEvent() ? FilterResult::KeepIterating
                                        : FilterResult::StopIterating;
  }

  bool IsWebInputEvent() const override { return false; }

  void Dispatch(MainThreadEventQueue*) override { closure_.Run(); }

 private:
  base::Closure closure_;
};

// Time interval at which touchmove events during scroll will be skipped
// during rAF signal.
const base::TimeDelta kAsyncTouchMoveInterval =
    base::TimeDelta::FromMilliseconds(200);

}  // namespace

class QueuedWebInputEvent : public ScopedWebInputEventWithLatencyInfo,
                            public MainThreadEventQueueTask {
 public:
  QueuedWebInputEvent(ui::WebScopedInputEvent event,
                      const ui::LatencyInfo& latency,
                      InputEventDispatchType dispatch_type,
                      bool originally_cancelable)
      : ScopedWebInputEventWithLatencyInfo(std::move(event), latency),
        dispatch_type_(dispatch_type),
        non_blocking_coalesced_count_(0),
        creation_timestamp_(base::TimeTicks::Now()),
        last_coalesced_timestamp_(creation_timestamp_),
        originally_cancelable_(originally_cancelable) {}

  ~QueuedWebInputEvent() override {}

  FilterResult FilterNewEvent(
      const MainThreadEventQueueTask& other_task) override {
    if (!other_task.IsWebInputEvent())
      return FilterResult::StopIterating;

    const QueuedWebInputEvent& other_event =
        static_cast<const QueuedWebInputEvent&>(other_task);
    if (other_event.event().GetType() ==
        blink::WebInputEvent::kTouchScrollStarted) {
      return HandleTouchScrollStartQueued();
    }

    if (!event().IsSameEventClass(other_event.event()))
      return FilterResult::KeepIterating;

    if (!ScopedWebInputEventWithLatencyInfo::CanCoalesceWith(other_event))
      return FilterResult::StopIterating;

    // If the other event was blocking store its unique touch event id to
    // ack later.
    if (other_event.dispatch_type_ == DISPATCH_TYPE_BLOCKING) {
      blocking_coalesced_event_ids_.push_back(
          ui::WebInputEventTraits::GetUniqueTouchEventId(other_event.event()));
    } else {
      non_blocking_coalesced_count_++;
    }
    ScopedWebInputEventWithLatencyInfo::CoalesceWith(other_event);
    last_coalesced_timestamp_ = base::TimeTicks::Now();

    // The newest event (|other_item|) always wins when updating fields.
    originally_cancelable_ = other_event.originally_cancelable_;

    return FilterResult::CoalescedEvent;
  }

  bool IsWebInputEvent() const override { return true; }

  void Dispatch(MainThreadEventQueue* queue) override {
    // Report the coalesced count only for continuous events; otherwise
    // the zero value would be dominated by non-continuous events.
    base::TimeTicks now = base::TimeTicks::Now();
    if (IsContinuousEvent()) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.MainThreadEventQueue.Continuous.QueueingTime",
          (now - creationTimestamp()).InMicroseconds(), 1, kTenSeconds, 50);

      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.MainThreadEventQueue.Continuous.FreshnessTime",
          (now - lastCoalescedTimestamp()).InMicroseconds(), 1, kTenSeconds,
          50);

      UMA_HISTOGRAM_COUNTS_1000("Event.MainThreadEventQueue.CoalescedCount",
                                coalescedCount());
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.MainThreadEventQueue.NonContinuous.QueueingTime",
          (now - creationTimestamp()).InMicroseconds(), 1, kTenSeconds, 50);
    }

    InputEventAckState ack_result = queue->HandleEventOnMainThread(
        coalesced_event(), latencyInfo(), dispatchType());
    for (const auto id : blockingCoalescedEventIds())
      queue->SendInputEventAck(event(), ack_result, id);
  }

  bool originallyCancelable() const { return originally_cancelable_; }

 private:
  FilterResult HandleTouchScrollStartQueued() {
    // A TouchScrollStart will queued after this touch move which will make all
    // previous touch moves that are queued uncancelable.
    switch (event().GetType()) {
      case blink::WebInputEvent::kTouchMove: {
        blink::WebTouchEvent& touch_event =
            static_cast<blink::WebTouchEvent&>(event());
        if (touch_event.dispatch_type ==
            blink::WebInputEvent::DispatchType::kBlocking) {
          touch_event.dispatch_type =
              blink::WebInputEvent::DispatchType::kEventNonBlocking;
        }
        return FilterResult::KeepIterating;
      }
      case blink::WebInputEvent::kTouchStart:
      case blink::WebInputEvent::kTouchEnd:
        return FilterResult::StopIterating;
      default:
        return FilterResult::KeepIterating;
    }
  }

  const std::deque<uint32_t>& blockingCoalescedEventIds() const {
    return blocking_coalesced_event_ids_;
  }
  InputEventDispatchType dispatchType() const { return dispatch_type_; }
  base::TimeTicks creationTimestamp() const { return creation_timestamp_; }
  base::TimeTicks lastCoalescedTimestamp() const {
    return last_coalesced_timestamp_;
  }

  size_t coalescedCount() const {
    return non_blocking_coalesced_count_ + blocking_coalesced_event_ids_.size();
  }

  bool IsContinuousEvent() const {
    switch (event().GetType()) {
      case blink::WebInputEvent::kMouseMove:
      case blink::WebInputEvent::kMouseWheel:
      case blink::WebInputEvent::kTouchMove:
        return true;
      default:
        return false;
    }
  }

  InputEventDispatchType dispatch_type_;

  // Contains the unique touch event ids to be acked. If
  // the events are not TouchEvents the values will be 0. More importantly for
  // those cases the deque ends up containing how many additional ACKs
  // need to be sent.
  std::deque<uint32_t> blocking_coalesced_event_ids_;
  // Contains the number of non-blocking events coalesced.
  size_t non_blocking_coalesced_count_;
  base::TimeTicks creation_timestamp_;
  base::TimeTicks last_coalesced_timestamp_;

  // Whether the received event was originally cancelable or not. The compositor
  // input handler can change the event based on presence of event handlers so
  // this is the state at which the renderer received the event from the
  // browser.
  bool originally_cancelable_;
};

MainThreadEventQueue::SharedState::SharedState()
    : sent_main_frame_request_(false), sent_post_task_(false) {}

MainThreadEventQueue::SharedState::~SharedState() {}

MainThreadEventQueue::MainThreadEventQueue(
    MainThreadEventQueueClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    blink::scheduler::RendererScheduler* renderer_scheduler,
    bool allow_raf_aligned_input)
    : client_(client),
      last_touch_start_forced_nonblocking_due_to_fling_(false),
      enable_fling_passive_listener_flag_(base::FeatureList::IsEnabled(
          features::kPassiveEventListenersDueToFling)),
      enable_non_blocking_due_to_main_thread_responsiveness_flag_(
          base::FeatureList::IsEnabled(
              features::kMainThreadBusyScrollIntervention)),
      handle_raf_aligned_touch_input_(
          allow_raf_aligned_input &&
          base::FeatureList::IsEnabled(features::kRafAlignedTouchInputEvents)),
      handle_raf_aligned_mouse_input_(
          allow_raf_aligned_input &&
          base::FeatureList::IsEnabled(features::kRafAlignedMouseInputEvents)),
      needs_low_latency_(false),
      main_task_runner_(main_task_runner),
      renderer_scheduler_(renderer_scheduler),
      use_raf_fallback_timer_(true) {
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
  raf_fallback_timer_.SetTaskRunner(main_task_runner);
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
  bool is_wheel = event->GetType() == blink::WebInputEvent::kMouseWheel;
  bool is_touch = blink::WebInputEvent::IsTouchEventType(event->GetType());
  bool originally_cancelable = false;

  if (is_touch) {
    blink::WebTouchEvent* touch_event =
        static_cast<blink::WebTouchEvent*>(event.get());

    originally_cancelable =
        touch_event->dispatch_type == blink::WebInputEvent::kBlocking;

    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    if (non_blocking) {
      touch_event->dispatch_type =
          blink::WebInputEvent::kListenersNonBlockingPassive;
    }
    if (touch_event->GetType() == blink::WebInputEvent::kTouchStart)
      last_touch_start_forced_nonblocking_due_to_fling_ = false;

    if (enable_fling_passive_listener_flag_ &&
        touch_event->touch_start_or_first_touch_move &&
        touch_event->dispatch_type == blink::WebInputEvent::kBlocking) {
      // If the touch start is forced to be passive due to fling, its following
      // touch move should also be passive.
      if (ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
          last_touch_start_forced_nonblocking_due_to_fling_) {
        touch_event->dispatch_type =
            blink::WebInputEvent::kListenersForcedNonBlockingDueToFling;
        non_blocking = true;
        last_touch_start_forced_nonblocking_due_to_fling_ = true;
      }
    }

    if (enable_non_blocking_due_to_main_thread_responsiveness_flag_ &&
        touch_event->dispatch_type == blink::WebInputEvent::kBlocking) {
      bool passive_due_to_unresponsive_main =
          renderer_scheduler_->MainThreadSeemsUnresponsive(
              main_thread_responsiveness_threshold_);
      if (passive_due_to_unresponsive_main) {
        touch_event->dispatch_type = blink::WebInputEvent::
            kListenersForcedNonBlockingDueToMainThreadResponsiveness;
        non_blocking = true;
      }
    }
    // If the event is non-cancelable ACK it right away.
    if (!non_blocking &&
        touch_event->dispatch_type != blink::WebInputEvent::kBlocking)
      non_blocking = true;
  }

  if (is_wheel) {
    blink::WebMouseWheelEvent* wheel_event =
        static_cast<blink::WebMouseWheelEvent*>(event.get());
    originally_cancelable =
        wheel_event->dispatch_type == blink::WebInputEvent::kBlocking;
    if (non_blocking) {
      // Adjust the |dispatchType| on the event since the compositor
      // determined all event listeners are passive.
      wheel_event->dispatch_type =
          blink::WebInputEvent::kListenersNonBlockingPassive;
    }
  }

  InputEventDispatchType dispatch_type =
      non_blocking ? DISPATCH_TYPE_NON_BLOCKING : DISPATCH_TYPE_BLOCKING;

  std::unique_ptr<QueuedWebInputEvent> event_with_dispatch_type(
      new QueuedWebInputEvent(std::move(event), latency, dispatch_type,
                              originally_cancelable));

  QueueEvent(std::move(event_with_dispatch_type));

  // send an ack when we are non-blocking.
  return non_blocking;
}

void MainThreadEventQueue::QueueClosure(const base::Closure& closure) {
  bool needs_post_task = false;
  std::unique_ptr<QueuedClosure> item(new QueuedClosure(closure));
  {
    base::AutoLock lock(shared_state_lock_);
    shared_state_.events_.Queue(std::move(item));
    needs_post_task = !shared_state_.sent_post_task_;
    shared_state_.sent_post_task_ = true;
  }

  if (needs_post_task)
    PostTaskToMainThread();
}

void MainThreadEventQueue::PossiblyScheduleMainFrame() {
  if (IsRafAlignedInputDisabled())
    return;
  bool needs_main_frame = false;
  {
    base::AutoLock lock(shared_state_lock_);
    if (!shared_state_.sent_main_frame_request_ &&
        !shared_state_.events_.empty() &&
        IsRafAlignedEvent(shared_state_.events_.front())) {
      needs_main_frame = true;
      shared_state_.sent_main_frame_request_ = true;
    }
  }
  if (needs_main_frame)
    SetNeedsMainFrame();
}

void MainThreadEventQueue::DispatchEvents() {
  size_t events_to_process;

  // Record the queue size so that we only process
  // that maximum number of events.
  {
    base::AutoLock lock(shared_state_lock_);
    shared_state_.sent_post_task_ = false;
    events_to_process = shared_state_.events_.size();

    // Don't process rAF aligned events at tail of queue.
    while (events_to_process > 0 &&
           IsRafAlignedEvent(shared_state_.events_.at(events_to_process - 1))) {
      --events_to_process;
    }
  }

  while (events_to_process--) {
    std::unique_ptr<MainThreadEventQueueTask> task;
    {
      base::AutoLock lock(shared_state_lock_);
      if (shared_state_.events_.empty())
        return;
      task = shared_state_.events_.Pop();
    }

    // Dispatching the event is outside of critical section.
    task->Dispatch(this);
  }
  PossiblyScheduleMainFrame();
}

static bool IsAsyncTouchMove(
    const std::unique_ptr<MainThreadEventQueueTask>& queued_item) {
  if (!queued_item->IsWebInputEvent())
    return false;
  const QueuedWebInputEvent* event =
      static_cast<const QueuedWebInputEvent*>(queued_item.get());
  if (event->event().GetType() != blink::WebInputEvent::kTouchMove)
    return false;
  const blink::WebTouchEvent& touch_event =
      static_cast<const blink::WebTouchEvent&>(event->event());
  return touch_event.moved_beyond_slop_region && !event->originallyCancelable();
}

void MainThreadEventQueue::RafFallbackTimerFired() {
  UMA_HISTOGRAM_BOOLEAN("Event.MainThreadEventQueue.FlushQueueNoBeginMainFrame",
                        true);
  DispatchRafAlignedInput(base::TimeTicks::Now());
}

void MainThreadEventQueue::DispatchRafAlignedInput(base::TimeTicks frame_time) {
  if (IsRafAlignedInputDisabled())
    return;

  raf_fallback_timer_.Stop();
  size_t queue_size_at_start;

  // Record the queue size so that we only process
  // that maximum number of events.
  {
    base::AutoLock lock(shared_state_lock_);
    shared_state_.sent_main_frame_request_ = false;
    queue_size_at_start = shared_state_.events_.size();
  }

  while (queue_size_at_start--) {
    std::unique_ptr<MainThreadEventQueueTask> task;
    {
      base::AutoLock lock(shared_state_lock_);

      if (shared_state_.events_.empty())
        return;

      if (IsRafAlignedEvent(shared_state_.events_.front())) {
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
      }
      task = shared_state_.events_.Pop();
    }
    // Dispatching the event is outside of critical section.
    task->Dispatch(this);
  }

  PossiblyScheduleMainFrame();
}

void MainThreadEventQueue::PostTaskToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MainThreadEventQueue::DispatchEvents, this));
}

void MainThreadEventQueue::QueueEvent(
    std::unique_ptr<MainThreadEventQueueTask> event) {
  bool is_raf_aligned = IsRafAlignedEvent(event);
  bool needs_main_frame = false;
  bool needs_post_task = false;
  {
    base::AutoLock lock(shared_state_lock_);
    size_t size_before = shared_state_.events_.size();
    shared_state_.events_.Queue(std::move(event));
    size_t size_after = shared_state_.events_.size();

    if (size_before != size_after) {
      if (!is_raf_aligned) {
        needs_post_task = !shared_state_.sent_post_task_;
        shared_state_.sent_post_task_ = true;
      } else {
        needs_main_frame = !shared_state_.sent_main_frame_request_;
        shared_state_.sent_main_frame_request_ = true;
      }
    }
  }

  if (needs_post_task)
    PostTaskToMainThread();
  if (needs_main_frame)
    SetNeedsMainFrame();
}

bool MainThreadEventQueue::IsRafAlignedInputDisabled() const {
  return !handle_raf_aligned_mouse_input_ && !handle_raf_aligned_touch_input_;
}

bool MainThreadEventQueue::IsRafAlignedEvent(
    const std::unique_ptr<MainThreadEventQueueTask>& item) const {
  if (!item->IsWebInputEvent())
    return false;
  const QueuedWebInputEvent* event =
      static_cast<const QueuedWebInputEvent*>(item.get());
  switch (event->event().GetType()) {
    case blink::WebInputEvent::kMouseMove:
    case blink::WebInputEvent::kMouseWheel:
      return handle_raf_aligned_mouse_input_ && !needs_low_latency_;
    case blink::WebInputEvent::kTouchMove:
      return handle_raf_aligned_touch_input_ && !needs_low_latency_;
    default:
      return false;
  }
}

InputEventAckState MainThreadEventQueue::HandleEventOnMainThread(
    const blink::WebCoalescedInputEvent& event,
    const ui::LatencyInfo& latency,
    InputEventDispatchType dispatch_type) {
  if (client_)
    return client_->HandleInputEvent(event, latency, dispatch_type);
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void MainThreadEventQueue::SendInputEventAck(const blink::WebInputEvent& event,
                                             InputEventAckState ack_result,
                                             uint32_t touch_event_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!client_)
    return;
  client_->SendInputEventAck(event.GetType(), ack_result, touch_event_id);
  if (renderer_scheduler_) {
    renderer_scheduler_->DidHandleInputEventOnMainThread(
        event, ack_result == INPUT_EVENT_ACK_STATE_CONSUMED
                   ? blink::WebInputEventResult::kHandledApplication
                   : blink::WebInputEventResult::kNotHandled);
  }
}

void MainThreadEventQueue::SetNeedsMainFrame() {
  if (main_task_runner_->BelongsToCurrentThread()) {
    if (use_raf_fallback_timer_) {
      raf_fallback_timer_.Start(
          FROM_HERE, kMaxRafDelay,
          base::Bind(&MainThreadEventQueue::RafFallbackTimerFired, this));
    }
    if (client_)
      client_->SetNeedsMainFrame();
    return;
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MainThreadEventQueue::SetNeedsMainFrame, this));
}

void MainThreadEventQueue::ClearClient() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_ = nullptr;
}

void MainThreadEventQueue::SetNeedsLowLatency(bool low_latency) {
  needs_low_latency_ = low_latency;
}

}  // namespace content
