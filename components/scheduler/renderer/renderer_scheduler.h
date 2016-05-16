// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/scheduler/child/child_scheduler.h"
#include "components/scheduler/child/single_thread_idle_task_runner.h"
#include "components/scheduler/renderer/render_widget_scheduling_state.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebScheduler.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace base {
namespace trace_event {
class BlameContext;
}
}

namespace cc {
struct BeginFrameArgs;
}

namespace blink {
class WebLocalFrame;
class WebThread;
}

namespace scheduler {

class RenderWidgetSchedulingState;

class SCHEDULER_EXPORT RendererScheduler : public ChildScheduler {
 public:
  ~RendererScheduler() override;
  static std::unique_ptr<RendererScheduler> Create();

  // Returns the compositor task runner.
  virtual scoped_refptr<TaskQueue> CompositorTaskRunner() = 0;

  // Keep RendererScheduler::UseCaseToString in sync with this enum.
  enum class UseCase {
    // No active use case detected.
    NONE,
    // A continuous gesture (e.g., scroll, pinch) which is being driven by the
    // compositor thread.
    COMPOSITOR_GESTURE,
    // An unspecified touch gesture which is being handled by the main thread.
    // Note that since we don't have a full view of the use case, we should be
    // careful to prioritize all work equally.
    MAIN_THREAD_CUSTOM_INPUT_HANDLING,
    // A continuous gesture (e.g., scroll, pinch) which is being driven by the
    // compositor thread but also observed by the main thread. An example is
    // synchronized scrolling where a scroll listener on the main thread changes
    // page layout based on the current scroll position.
    SYNCHRONIZED_GESTURE,
    // A gesture has recently started and we are about to run main thread touch
    // listeners to find out the actual gesture type. To minimize touch latency,
    // only input handling work should run in this state.
    TOUCHSTART,
    // The page is loading.
    LOADING,
    // A continuous gesture (e.g., scroll) which is being handled by the main
    // thread.
    MAIN_THREAD_GESTURE,
    // Must be the last entry.
    USE_CASE_COUNT,
    FIRST_USE_CASE = NONE,
  };
  static const char* UseCaseToString(UseCase use_case);

  // Creates a WebThread implementation for the renderer main thread.
  virtual std::unique_ptr<blink::WebThread> CreateMainThread() = 0;

  // Returns the loading task runner.  This queue is intended for tasks related
  // to resource dispatch, foreground HTML parsing, etc...
  virtual scoped_refptr<TaskQueue> LoadingTaskRunner() = 0;

  // Returns the timer task runner.  This queue is intended for DOM Timers.
  // TODO(alexclarke): Get rid of this default timer queue.
  virtual scoped_refptr<TaskQueue> TimerTaskRunner() = 0;

  // Returns a new loading task runner. This queue is intended for tasks related
  // to resource dispatch, foreground HTML parsing, etc...
  virtual scoped_refptr<TaskQueue> NewLoadingTaskRunner(const char* name) = 0;

  // Returns a new timer task runner. This queue is intended for DOM Timers.
  virtual scoped_refptr<TaskQueue> NewTimerTaskRunner(const char* name) = 0;

  // Returns a new RenderWidgetSchedulingState.  The signals from this will be
  // used to make scheduling decisions.
  virtual std::unique_ptr<RenderWidgetSchedulingState>
  NewRenderWidgetSchedulingState() = 0;

  // Called to notify about the start of an extended period where no frames
  // need to be drawn. Must be called from the main thread.
  virtual void BeginFrameNotExpectedSoon() = 0;

  // Called to notify about the start of a new frame.  Must be called from the
  // main thread.
  virtual void WillBeginFrame(const cc::BeginFrameArgs& args) = 0;

  // Called to notify that a previously begun frame was committed. Must be
  // called from the main thread.
  virtual void DidCommitFrameToCompositor() = 0;

  // Keep RendererScheduler::InputEventStateToString in sync with this enum.
  enum class InputEventState {
    EVENT_CONSUMED_BY_COMPOSITOR,
    EVENT_FORWARDED_TO_MAIN_THREAD,
  };
  static const char* InputEventStateToString(InputEventState input_event_state);

  // Tells the scheduler that the system processed an input event. Called by the
  // compositor (impl) thread.  Note it's expected that every call to
  // DidHandleInputEventOnCompositorThread where |event_state| is
  // EVENT_FORWARDED_TO_MAIN_THREAD will be followed by a corresponding call
  // to DidHandleInputEventOnMainThread.
  virtual void DidHandleInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event,
      InputEventState event_state) = 0;

  // Tells the scheduler that the system processed an input event. Must be
  // called from the main thread.
  virtual void DidHandleInputEventOnMainThread(
      const blink::WebInputEvent& web_input_event) = 0;

  // Tells the scheduler that the system is displaying an input animation (e.g.
  // a fling). Called by the compositor (impl) thread.
  virtual void DidAnimateForInputOnCompositorThread() = 0;

  // Tells the scheduler that the renderer process has been backgrounded, i.e.,
  // there are no critical, user facing activities (visual, audio, etc...)
  // driven by this process. A stricter condition than |OnRendererHidden()|, the
  // process is assumed to be foregrounded when the scheduler is constructed.
  // Must be called on the main thread.
  virtual void OnRendererBackgrounded() = 0;

  // Tells the scheduler that the renderer process has been foregrounded.
  // This is the assumed state when the scheduler is constructed.
  // Must be called on the main thread.
  virtual void OnRendererForegrounded() = 0;

  // Tells the scheduler that the render process should be suspended. This can
  // only be done when the renderer is backgrounded. The renderer will be
  // automatically resumed when foregrounded.
  virtual void SuspendRenderer() = 0;

  // Tells the scheduler that a navigation task is pending. While any main-frame
  // navigation tasks are pending, the scheduler will ensure that loading tasks
  // are not blocked even if they are expensive. Must be called on the main
  // thread.
  virtual void AddPendingNavigation(
      blink::WebScheduler::NavigatingFrameType type) = 0;

  // Tells the scheduler that a navigation task is no longer pending.
  // Must be called on the main thread.
  virtual void RemovePendingNavigation(
      blink::WebScheduler::NavigatingFrameType type) = 0;

  // Tells the scheduler that a navigation has started.  The scheduler will
  // prioritize loading tasks for a short duration afterwards.
  // Must be called from the main thread.
  virtual void OnNavigationStarted() = 0;

  // Returns true if the scheduler has reason to believe that high priority work
  // may soon arrive on the main thread, e.g., if gesture events were observed
  // recently.
  // Must be called from the main thread.
  virtual bool IsHighPriorityWorkAnticipated() = 0;

  // Suspends the timer queue and increments the timer queue suspension count.
  // May only be called from the main thread.
  virtual void SuspendTimerQueue() = 0;

  // Decrements the timer queue suspension count and re-enables the timer queue
  // if the suspension count is zero and the current schduler policy allows it.
  virtual void ResumeTimerQueue() = 0;

  // Sets whether to allow suspension of timers after the backgrounded signal is
  // received via OnRendererBackgrounded. Defaults to disabled.
  virtual void SetTimerQueueSuspensionWhenBackgroundedEnabled(bool enabled) = 0;

  // Sets the default blame context to which top level work should be
  // attributed in this renderer. |blame_context| must outlive this scheduler.
  virtual void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context) = 0;

 protected:
  RendererScheduler();
  DISALLOW_COPY_AND_ASSIGN(RendererScheduler);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_
