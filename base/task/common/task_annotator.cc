// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/task_annotator.h"

#include <array>

#include "base/debug/activity_tracker.h"
#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/pending_task.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"

namespace base {

namespace {

TaskAnnotator::ObserverForTesting* g_task_annotator_observer = nullptr;

// Information about the context in which a PendingTask is currently being
// executed.
struct TaskAnnotatorThreadInfo {
  // The pending task currently being executed.
  const PendingTask* pending_task = nullptr;
  // The pending task that was being executed when the |ipc_program_counter|
  // was set. This is used to detect a nested message loop, in which case the
  // global IPC decoration should not be applied.
  const PendingTask* ipc_message_handler_task = nullptr;
  // The program counter of the currently executing IPC message handler, if
  // there is one.
  const void* ipc_program_counter = nullptr;
};

// Returns the TLS slot that stores the PendingTask currently in progress on
// each thread. Used to allow creating a breadcrumb of program counters on the
// stack to help identify a task's origin in crashes.
TaskAnnotatorThreadInfo* GetTLSForCurrentPendingTask() {
  static NoDestructor<ThreadLocalOwnedPointer<TaskAnnotatorThreadInfo>>
      instance;
  if (!instance->Get())
    instance->Set(WrapUnique(new TaskAnnotatorThreadInfo));
  auto* tls_info = instance->Get();
  return tls_info;
}

}  // namespace

const PendingTask* TaskAnnotator::CurrentTaskForThread() {
  return GetTLSForCurrentPendingTask()->pending_task;
}

TaskAnnotator::TaskAnnotator() = default;

TaskAnnotator::~TaskAnnotator() = default;

void TaskAnnotator::WillQueueTask(const char* trace_event_name,
                                  PendingTask* pending_task,
                                  const char* task_queue_name) {
  DCHECK(trace_event_name);
  DCHECK(pending_task);
  DCHECK(task_queue_name);
  TRACE_EVENT_WITH_FLOW1(
      TRACE_DISABLED_BY_DEFAULT("toplevel.flow"), trace_event_name,
      TRACE_ID_MANGLE(GetTaskTraceID(*pending_task)),
      TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_DISALLOW_POSTTASK,
      "task_queue_name", task_queue_name);

  DCHECK(!pending_task->task_backtrace[0])
      << "Task backtrace was already set, task posted twice??";
  if (pending_task->task_backtrace[0])
    return;

  // Inherit the currently executing IPC handler context only if not in a nested
  // message loop.
  const auto* tls_info = GetTLSForCurrentPendingTask();
  if (tls_info->ipc_message_handler_task == tls_info->pending_task)
    pending_task->ipc_program_counter = tls_info->ipc_program_counter;

  const auto* parent_task = tls_info->pending_task;
  if (!parent_task)
    return;

  // If an IPC message handler context wasn't explicitly set, then inherit the
  // context from the parent task.
  if (!pending_task->ipc_program_counter)
    pending_task->ipc_program_counter = parent_task->ipc_program_counter;

  pending_task->task_backtrace[0] = parent_task->posted_from.program_counter();
  std::copy(parent_task->task_backtrace.begin(),
            parent_task->task_backtrace.end() - 1,
            pending_task->task_backtrace.begin() + 1);
  pending_task->task_backtrace_overflow =
      parent_task->task_backtrace_overflow ||
      parent_task->task_backtrace.back() != nullptr;
}

void TaskAnnotator::RunTask(const char* trace_event_name,
                            PendingTask* pending_task) {
  DCHECK(trace_event_name);
  DCHECK(pending_task);

  debug::ScopedTaskRunActivity task_activity(*pending_task);

  TRACE_EVENT_WITH_FLOW0(
      TRACE_DISABLED_BY_DEFAULT("toplevel.flow"), trace_event_name,
      TRACE_ID_MANGLE(GetTaskTraceID(*pending_task)), TRACE_EVENT_FLAG_FLOW_IN);

  // Before running the task, store the IPC context and the task backtrace with
  // the chain of PostTasks that resulted in this call and deliberately alias it
  // to ensure it is on the stack if the task crashes. Be careful not to assume
  // that the variable itself will have the expected value when displayed by the
  // optimizer in an optimized build. Look at a memory dump of the stack.
  static constexpr int kStackTaskTraceSnapshotSize =
      PendingTask::kTaskBacktraceLength + 4;
  std::array<const void*, kStackTaskTraceSnapshotSize> task_backtrace;

  // Store a marker to locate |task_backtrace| content easily on a memory
  // dump. The layout is as follows:
  //
  // +------------ +----+---------+-----+-----------+--------+-------------+
  // | Head Marker | PC | frame 0 | ... | frame N-1 | IPC PC | Tail Marker |
  // +------------ +----+---------+-----+-----------+--------+-------------+
  //
  // Markers glossary (compliments of wez):
  //      cool code,do it dude!
  //   0x c001 c0de d0 17 d00d
  //      o dude,i did it biig
  //   0x 0 d00d 1 d1d 17 8119
  task_backtrace.front() = reinterpret_cast<void*>(0xc001c0ded017d00d);
  task_backtrace.back() = reinterpret_cast<void*>(0x0d00d1d1d178119);

  task_backtrace[1] = pending_task->posted_from.program_counter();
  std::copy(pending_task->task_backtrace.begin(),
            pending_task->task_backtrace.end(), task_backtrace.begin() + 2);
  task_backtrace[kStackTaskTraceSnapshotSize - 2] =
      pending_task->ipc_program_counter;
  debug::Alias(&task_backtrace);

  auto* tls_info = GetTLSForCurrentPendingTask();
  const auto* previous_pending_task = tls_info->pending_task;
  tls_info->pending_task = pending_task;

  if (g_task_annotator_observer)
    g_task_annotator_observer->BeforeRunTask(pending_task);
  std::move(pending_task->task).Run();

  tls_info->pending_task = previous_pending_task;
}

uint64_t TaskAnnotator::GetTaskTraceID(const PendingTask& task) const {
  return (static_cast<uint64_t>(task.sequence_num) << 32) |
         ((static_cast<uint64_t>(reinterpret_cast<intptr_t>(this)) << 32) >>
          32);
}

// static
void TaskAnnotator::RegisterObserverForTesting(ObserverForTesting* observer) {
  DCHECK(!g_task_annotator_observer);
  g_task_annotator_observer = observer;
}

// static
void TaskAnnotator::ClearObserverForTesting() {
  g_task_annotator_observer = nullptr;
}

TaskAnnotator::ScopedSetIpcProgramCounter::ScopedSetIpcProgramCounter(
    const void* program_counter) {
  auto* tls_info = GetTLSForCurrentPendingTask();
  old_ipc_message_handler_task_ = tls_info->ipc_message_handler_task;
  old_ipc_program_counter_ = tls_info->ipc_program_counter;
  tls_info->ipc_message_handler_task = tls_info->pending_task;
  tls_info->ipc_program_counter = program_counter;
}

TaskAnnotator::ScopedSetIpcProgramCounter::~ScopedSetIpcProgramCounter() {
  auto* tls_info = GetTLSForCurrentPendingTask();
  tls_info->ipc_message_handler_task = old_ipc_message_handler_task_;
  tls_info->ipc_program_counter = old_ipc_program_counter_;
}

}  // namespace base
