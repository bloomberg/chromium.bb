// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_TASK_H_
#define CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_TASK_H_

#include "content/common/input/input_event_ack_state.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"

namespace content {

class MainThreadEventQueueClient;

// A work item to execute from the main thread event queue.
// The MainThreadEventQueue supports 2 types of tasks (subclasses):
// 1) Closures
// 2) WebInputEvent
class MainThreadEventQueueTask {
 public:
  virtual ~MainThreadEventQueueTask() {}

  enum class CoalesceResult {
    Coalesced,
    CannotCoalesce,
    // Keep iterating on the queue looking for a matching event with the
    // same modality.
    KeepSearching,
  };

  virtual CoalesceResult CoalesceWith(const MainThreadEventQueueTask&) = 0;
  virtual bool IsWebInputEvent() const = 0;
  virtual void Dispatch(int routing_id, MainThreadEventQueueClient*) = 0;
  virtual void EventHandled(
      int routing_id,
      blink::scheduler::RendererScheduler* renderer_scheduler,
      MainThreadEventQueueClient* client,
      blink::WebInputEvent::Type type,
      blink::WebInputEventResult result,
      InputEventAckState ack_result) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_TASK_H_
