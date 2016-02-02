// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/scheduler/child/child_scheduler.h"
#include "components/scheduler/child/single_thread_idle_task_runner.h"
#include "components/scheduler/renderer/render_widget_scheduling_state.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace cc {
struct BeginFrameArgs;
}

namespace blink {
class WebThread;
}

namespace scheduler {

class RenderWidgetSchedulingState;

class SCHEDULER_EXPORT RendererScheduler : public ChildScheduler {
 public:
  ~RendererScheduler() override;
  static scoped_ptr<RendererScheduler> Create();

  // Returns the compositor task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  CompositorTaskRunner() = 0;

  // Keep RendererScheduler::UseCaseToString in sync with this enum.
  enum class UseCase {
    NONE,
    COMPOSITOR_GESTURE,
    MAIN_THREAD_GESTURE,
    SYNCHRONIZED_GESTURE,  // Both threads in the critical path.
    TOUCHSTART,
    LOADING,
    // Must be the last entry.
    USE_CASE_COUNT,
    FIRST_USE_CASE = NONE,
  };
  static const char* UseCaseToString(UseCase use_case);

  // Creates a WebThread implementation for the renderer main thread.
  virtual scoped_ptr<blink::WebThread> CreateMainThread() = 0;

  // Returns the loading task runner.  This queue is intended for tasks related
  // to resource dispatch, foreground HTML parsing, etc...
  virtual scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() = 0;

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
  virtual scoped_ptr<RenderWidgetSchedulingState>
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

  // Tells the scheduler that a navigation task is pending. While any navigation
  // tasks are pending, the scheduler will ensure that loading tasks are not
  // blocked even if they are expensive. Must be called on the main thread.
  virtual void AddPendingNavigation() = 0;

  // Tells the scheduler that a navigation task is no longer pending.
  // Must be called on the main thread.
  virtual void RemovePendingNavigation() = 0;

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

  // Returns a double which is the number of seconds since epoch (Jan 1, 1970).
  // This may represent either the real time, or a virtual time depending on
  // whether or not the system is currently running a task associated with a
  // virtual time domain or real time domain.
  virtual double VirtualTimeSeconds() const = 0;

  // Returns a microsecond resolution platform dependant time source.
  // This may represent either the real time, or a virtual time depending on
  // whether or not the system is currently running a task associated with a
  // virtual time domain or real time domain.
  virtual double MonotonicallyIncreasingVirtualTimeSeconds() const = 0;

 protected:
  RendererScheduler();
  DISALLOW_COPY_AND_ASSIGN(RendererScheduler);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_
