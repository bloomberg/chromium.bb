// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_
#define CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_

#include <deque>
#include "base/feature_list.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/common/input/web_input_event_queue.h"
#include "content/public/common/content_features.h"
#include "content/renderer/input/scoped_web_input_event_with_latency_info.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/latency_info.h"

namespace content {

class EventWithDispatchType : public ScopedWebInputEventWithLatencyInfo {
 public:
  EventWithDispatchType(ui::WebScopedInputEvent event,
                        const ui::LatencyInfo& latency,
                        InputEventDispatchType dispatch_type,
                        bool originally_cancelable);
  ~EventWithDispatchType();
  void CoalesceWith(const EventWithDispatchType& other);

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

  bool originallyCancelable() const { return originally_cancelable_; }

 private:
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

class CONTENT_EXPORT MainThreadEventQueueClient {
 public:
  // Handle an |event| that was previously queued (possibly
  // coalesced with another event) to the |routing_id|'s
  // channel. Implementors must implement this callback.
  virtual void HandleEventOnMainThread(
      int routing_id,
      const blink::WebCoalescedInputEvent* event,
      const ui::LatencyInfo& latency,
      InputEventDispatchType dispatch_type) = 0;

  virtual void SendInputEventAck(int routing_id,
                                 blink::WebInputEvent::Type type,
                                 InputEventAckState ack_result,
                                 uint32_t touch_event_id) = 0;
  virtual void NeedsMainFrame(int routing_id) = 0;
};

// MainThreadEventQueue implements a queue for events that need to be
// queued between the compositor and main threads. This queue is managed
// by a lock where events are enqueued by the compositor thread
// and dequeued by the main thread.
//
// Below some example flows are how the code behaves.
// Legend: B=Browser, C=Compositor, M=Main Thread, NB=Non-blocking
//         BL=Blocking, PT=Post Task, ACK=Acknowledgement
//
// Normal blocking event sent to main thread.
//   B        C        M
//   ---(BL)-->
//         (queue)
//            ---(PT)-->
//                  (deque)
//   <-------(ACK)------
//
// Non-blocking event sent to main thread.
//   B        C        M
//   ---(NB)-->
//         (queue)
//            ---(PT)-->
//                  (deque)
//
// Non-blocking followed by blocking event sent to main thread.
//   B        C        M
//   ---(NB)-->
//         (queue)
//            ---(PT)-->
//   ---(BL)-->
//         (queue)
//            ---(PT)-->
//                  (deque)
//                  (deque)
//   <-------(ACK)------
//
class CONTENT_EXPORT MainThreadEventQueue
    : public base::RefCountedThreadSafe<MainThreadEventQueue> {
 public:
  MainThreadEventQueue(
      int routing_id,
      MainThreadEventQueueClient* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      blink::scheduler::RendererScheduler* renderer_scheduler);

  // Called once the compositor has handled |event| and indicated that it is
  // a non-blocking event to be queued to the main thread.
  bool HandleEvent(ui::WebScopedInputEvent event,
                   const ui::LatencyInfo& latency,
                   InputEventDispatchType dispatch_type,
                   InputEventAckState ack_result);
  void DispatchRafAlignedInput(base::TimeTicks frame_time);

  // Call once the main thread has handled an outstanding |type| event
  // in flight.
  void EventHandled(blink::WebInputEvent::Type type,
                    blink::WebInputEventResult result,
                    InputEventAckState ack_result);

 private:
  friend class base::RefCountedThreadSafe<MainThreadEventQueue>;
  ~MainThreadEventQueue();
  void QueueEvent(std::unique_ptr<EventWithDispatchType> event);
  void SendEventNotificationToMainThread();
  void DispatchSingleEvent();
  void DispatchInFlightEvent();
  void PossiblyScheduleMainFrame();

  void SendEventToMainThread(const blink::WebInputEvent* event,
                             const ui::LatencyInfo& latency,
                             InputEventDispatchType original_dispatch_type);

  bool IsRafAlignedInputDisabled();
  bool IsRafAlignedEvent(const blink::WebInputEvent& event);

  friend class MainThreadEventQueueTest;
  friend class MainThreadEventQueueInitializationTest;
  int routing_id_;
  MainThreadEventQueueClient* client_;
  std::unique_ptr<EventWithDispatchType> in_flight_event_;
  bool last_touch_start_forced_nonblocking_due_to_fling_;
  bool enable_fling_passive_listener_flag_;
  bool enable_non_blocking_due_to_main_thread_responsiveness_flag_;
  base::TimeDelta main_thread_responsiveness_threshold_;
  bool handle_raf_aligned_touch_input_;
  bool handle_raf_aligned_mouse_input_;

  // Contains data to be shared between main thread and compositor thread.
  struct SharedState {
    SharedState();
    ~SharedState();

    WebInputEventQueue<EventWithDispatchType> events_;
    bool sent_main_frame_request_;
    base::TimeTicks last_async_touch_move_timestamp_;
  };

  // Lock used to serialize |shared_state_|.
  base::Lock shared_state_lock_;
  SharedState shared_state_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  blink::scheduler::RendererScheduler* renderer_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadEventQueue);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_
