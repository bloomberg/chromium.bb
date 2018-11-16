// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/debug/task_annotator.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_task_runner.h"
#include "base/message_loop/message_pump_default.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/sequenced_task_source.h"
#include "base/run_loop.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"

#if defined(OS_MACOSX)
#include "base/message_loop/message_pump_mac.h"
#elif defined(OS_ANDROID)
#include "base/message_loop/message_pump_android.h"
#endif

namespace base {

class MessageLoopImpl::Controller : public SequencedTaskSource::Observer {
 public:
  // Constructs a MessageLoopController which controls |message_loop|, notifying
  // |task_annotator_| when tasks are queued and scheduling work on
  // |message_loop| as fits. |message_loop| and |task_annotator_| will not be
  // used after DisconnectFromParent() returns.
  Controller(MessageLoopImpl* message_loop);

  ~Controller() override;

  // SequencedTaskSource::Observer:
  void WillQueueTask(PendingTask* task) final;
  void DidQueueTask(bool was_empty) final;

  // Informs this Controller that it can start invoking
  // |message_loop_->ScheduleWork()|. Must be invoked only once on the thread
  // |message_loop_| is bound to (when it is bound).
  void StartScheduling();

  // Disconnects |message_loop_| from this Controller instance (DidQueueTask()
  // will no-op from this point forward). Must be invoked only once on the
  // thread |message_loop_| is bound to (when the thread is shutting down).
  void DisconnectFromParent();

  // Shares this Controller's TaskAnnotator with MessageLoop as TaskAnnotator
  // requires DidQueueTask(x)/RunTask(x) to be invoked on the same TaskAnnotator
  // instance.
  debug::TaskAnnotator& task_annotator() { return task_annotator_; }

 private:
  // Helpers to be invoked before using |message_loop_| instead of operating
  // directly on |operations_state_| below. BeforeOperation() returns true iff
  // the operation is allowed to be performed (in which case, AfterOperation()
  // must be invoked when done).
  bool BeforeOperation();
  void AfterOperation();

  // A TaskAnnotator which is owned by this Controller to be able to use it
  // without locking |message_loop_lock_|. It cannot be owned by MessageLoop
  // because this Controller cannot access |message_loop_| safely without the
  // lock. Note: the TaskAnnotator API itself is thread-safe.
  debug::TaskAnnotator task_annotator_;

  // An atomic representation of the ongoing operations and shutdown state. The
  // lower bit (kDisconnectedBit) is used to indicate that
  // DisconnectFromParent() was initiated. The other bits are used to indicate
  // ongoing operations. As such this should be incremented by
  // kOperationInProgress before making any operation on |message_loop_|.
  // Conversely DisconnectFromParent() will wait on |safe_to_shutdown_| if this
  // was non-zero when it set the lower bit.
  static constexpr int kDisconnectedBit = 1;
  static constexpr int kOperationInProgress = 1 << kDisconnectedBit;
  std::atomic_int operations_state_{0};

  // DisconnectFromParent() will instantiate and wait on this event if
  // |operations_state_| wasn't zero when it set the lower bit. Whoever then
  // completes the last in-progress operation needs to signal this event to
  // resume the disconnect.
  Optional<WaitableEvent> safe_to_shutdown_;

  enum InitializationState {
    // Initial state : ScheduleWork() cannot be called yet.
    kNotReady,
    // ScheduleWork() cannot be called yet but should be when transitioning to
    // kReadyForScheduling.
    kPendingWork,
    // ScheduleWork() can be called now.
    kReadyForScheduling,
  };
  std::atomic_int initialization_state_{kNotReady};

  // Points to this Controller's outer MessageLoop instance.
  // |initialization_state_| must be set to kReadyForScheduling before using
  // this. |operations_state_| must then be incremented per the above protocol
  // to use this.
  MessageLoopImpl* const message_loop_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

MessageLoopImpl::Controller::Controller(MessageLoopImpl* message_loop)
    : message_loop_(message_loop) {
  DCHECK(message_loop_);
}

MessageLoopImpl::Controller::~Controller() {
  DCHECK(safe_to_shutdown_)
      << "DisconnectFromParent() needs to be invoked before destruction.";
}

void MessageLoopImpl::Controller::WillQueueTask(PendingTask* task) {
  task_annotator_.WillQueueTask("MessageLoop::PostTask", task);
}

void MessageLoopImpl::Controller::DidQueueTask(bool was_empty) {
  if (!was_empty)
    return;

  // Perform a lock-less check that we are ready for scheduling. If not,
  // atomically inform StartScheduling() about the pending work.
  // std::memory_order_acquire as documented in StartScheduling().
  int previous_state = kNotReady;
  if (initialization_state_.compare_exchange_strong(
          previous_state, kPendingWork, std::memory_order_acquire) ||
      previous_state == kPendingWork) {
    return;
  }
  DCHECK_EQ(previous_state, kReadyForScheduling);

  if (!BeforeOperation())
    return;

  // Some scenarios can result in getting to this point on multiple threads at
  // once, e.g.:
  //
  // Two threads post a task and both make the queue non-empty because an
  // unrelated event (A) (e.g. timer or system event) woke up the MessageLoop
  // thread in between, allowing it to process the first task, before either
  // thread got to ScheduleWork() :
  //
  // T0(ML) ---[]↘------(A)--------------[]↘--------------------↗↔(racy SchedW)
  // T1     --↗---↘-------------------↗Post-→(was_empty=true)---↑--↑SchedW()
  // T2     -↗Post--→(was_empty=true)------(descheduled)--------↑SchedW()

  // MessageLoop/MessagePump::ScheduleWork() is thread-safe so this is fine.
  message_loop_->ScheduleWork();

  AfterOperation();
}

void MessageLoopImpl::Controller::StartScheduling() {
  DCHECK_CALLED_ON_VALID_THREAD(message_loop_->bound_thread_checker_);

  // std::memory_order_release because any thread that acquires this value and
  // sees kReadyForScheduling needs to also see the state initialized on this
  // thread before StartScheduling(). (this is also why matching reads use
  // std::memory_order_acquire)
  auto previous_state = initialization_state_.exchange(
      kReadyForScheduling, std::memory_order_release);
  DCHECK_NE(previous_state, kReadyForScheduling);

  if (previous_state == kPendingWork)
    message_loop_->ScheduleWork();
}

void MessageLoopImpl::Controller::DisconnectFromParent() {
  DCHECK_CALLED_ON_VALID_THREAD(message_loop_->bound_thread_checker_);
  DCHECK(!safe_to_shutdown_);

  safe_to_shutdown_.emplace();

  // Acquire semantics are required to guarantee that all memory side-effects
  // made to |message_loop_| by other threads are visible to this thread before
  // it returns from this method. Release semantics are required to guarantee
  // that threads seeing the disconnect bit will also see a fully initialized
  // |safe_to_shutdown_|.
  if (operations_state_.fetch_add(kDisconnectedBit,
                                  std::memory_order_acq_rel) != 0) {
    ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait_for_fast_ops;
    safe_to_shutdown_->Wait();
  }
}

bool MessageLoopImpl::Controller::BeforeOperation() {
  // Acquire semantics are required to ensure that no operation on the current
  // thread can be reordered before this one.
  const bool allowed = (operations_state_.fetch_add(kOperationInProgress,
                                                    std::memory_order_acquire) &
                        kDisconnectedBit) == 0;

  // Undo the increment if disallowed (and potentially signal if that racily
  // ended up being the last operation).
  if (!allowed)
    AfterOperation();

  return allowed;
}

void MessageLoopImpl::Controller::AfterOperation() {
  constexpr int kWasDisconnectedWithOnlyOneOperationLeft =
      kOperationInProgress | kDisconnectedBit;
  // Release semantics are required to ensure that no operation on the current
  // thread can be reordered after this one. Technically, acquire semantics are
  // only required if the conditional is true (to synchronize with
  // DisconnectFromParent() before using |safe_to_shutdown_|). As such, per
  // "Atomic-fence synchronization" semantics [1], it'd be sufficient to
  // fetch_sub(std::memory_order_release) and only have a
  // std::atomic_thread_fence(std::memory_order_acquire) inside the
  // conditional's body. However, as documented in atomic_ref_count.h TSAN
  // doesn't support fences at the moment. Hence, this uses acq_rel for now.
  // [1] https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence
  if (operations_state_.fetch_sub(kOperationInProgress,
                                  std::memory_order_acq_rel) ==
      kWasDisconnectedWithOnlyOneOperationLeft) {
    safe_to_shutdown_->Signal();
  }
}

//------------------------------------------------------------------------------

MessageLoopImpl::~MessageLoopImpl() {

#if defined(OS_WIN)
  if (in_high_res_mode_)
    Time::ActivateHighResolutionTimer(false);
#endif
  thread_task_runner_handle_.reset();

  // Detach this instance's Controller from |this|. After this point,
  // |underlying_task_runner_| may still receive tasks and notify the controller
  // but the controller will no-op (and not use this MessageLoop after free).
  // |underlying_task_runner_| being ref-counted and potentially kept alive by
  // many SingleThreadTaskRunner refs, the best we can do is tell it to shutdown
  // after which it will start returning false to PostTasks that happen-after
  // this point (note that invoking Shutdown() first would not remove the need
  // to DisconnectFromParent() since the controller is invoked *after* a task is
  // enqueued and the incoming queue's lock is released (see
  // MessageLoopTaskRunner::AddToIncomingQueue()).
  // Details : while an "in-progress post tasks" refcount in Controller in lieu
  // of |message_loop_lock_| would be an option to handle the "pending post
  // tasks on shutdown" case, |message_loop_lock_| would still be required to
  // serialize ScheduleWork() call and as such that optimization isn't worth it.
  message_loop_controller_->DisconnectFromParent();
  underlying_task_runner_->Shutdown();

  // Let interested parties have one last shot at accessing this.
  for (auto& observer : destruction_observers_)
    observer.WillDestroyCurrentMessageLoop();

  // OK, now make it so that no one can find us.
  if (IsBoundToCurrentThread())
    MessageLoopCurrent::UnbindFromCurrentThreadInternal(this);
}

// TODO(gab): Migrate TaskObservers to RunLoop as part of separating concerns
// between MessageLoop and RunLoop and making MessageLoop a swappable
// implementation detail. http://crbug.com/703346
void MessageLoopImpl::AddTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  task_observers_.push_back(task_observer);
}

void MessageLoopImpl::RemoveTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  auto it =
      std::find(task_observers_.begin(), task_observers_.end(), task_observer);
  DCHECK(it != task_observers_.end());
  task_observers_.erase(it);
}

void MessageLoopImpl::SetAddQueueTimeToTasks(bool enable) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  underlying_task_runner_->SetAddQueueTimeToTasks(enable);
}

bool MessageLoopImpl::IsIdleForTesting() {
  // Have unprocessed tasks? (this reloads the work queue if necessary)
  if (sequenced_task_source_->HasTasks())
    return false;

  // Have unprocessed deferred tasks which can be processed at this run-level?
  if (pending_task_queue_.deferred_tasks().HasTasks() &&
      !RunLoop::IsNestedOnCurrentThread()) {
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------

MessageLoopImpl::MessageLoopImpl(MessageLoopBase::Type type)
    : type_(type),
      message_loop_controller_(
          new Controller(this)),  // Ownership transferred on the next line.
      underlying_task_runner_(MakeRefCounted<internal::MessageLoopTaskRunner>(
          WrapUnique(message_loop_controller_))),
      sequenced_task_source_(underlying_task_runner_.get()),
      task_runner_(underlying_task_runner_) {
  // Bound in BindToCurrentThread();
  DETACH_FROM_THREAD(bound_thread_checker_);
}

void MessageLoopImpl::BindToCurrentThread(std::unique_ptr<MessagePump> pump) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);

  DCHECK(!pump_);
  pump_ = std::move(pump);

  underlying_task_runner_->BindToCurrentThread();
  message_loop_controller_->StartScheduling();
  SetThreadTaskRunnerHandle();
  thread_id_ = PlatformThread::CurrentId();

  scoped_set_sequence_local_storage_map_for_current_thread_ = std::make_unique<
      internal::ScopedSetSequenceLocalStorageMapForCurrentThread>(
      &sequence_local_storage_map_);

  RunLoop::RegisterDelegateForCurrentThread(this);
  MessageLoopCurrent::BindToCurrentThreadInternal(this);
}

bool MessageLoopImpl::IsType(MessageLoopBase::Type type) const {
  return type_ == type;
}

std::string MessageLoopImpl::GetThreadName() const {
  DCHECK_NE(kInvalidThreadId, thread_id_)
      << "GetThreadName() must only be called after BindToCurrentThread()'s "
      << "side-effects have been synchronized with this thread.";
  return ThreadIdNameManager::GetInstance()->GetName(thread_id_);
}

void MessageLoopImpl::SetTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  if (thread_id_ == kInvalidThreadId) {
    // ThreadTaskRunnerHandle will be set during BindToCurrentThread().
    task_runner_ = std::move(task_runner);
  } else {
    // Once MessageLoop is bound, |task_runner_| may only be altered on the
    // bound thread.
    DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
    DCHECK(task_runner->BelongsToCurrentThread());
    task_runner_ = std::move(task_runner);
    SetThreadTaskRunnerHandle();
  }
}

scoped_refptr<SingleThreadTaskRunner> MessageLoopImpl::GetTaskRunner() {
  return task_runner_;
}

void MessageLoopImpl::AddDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  destruction_observers_.AddObserver(destruction_observer);
}

void MessageLoopImpl::RemoveDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  destruction_observers_.RemoveObserver(destruction_observer);
}

bool MessageLoopImpl::IsBoundToCurrentThread() const {
  return MessageLoopCurrent::Get()->ToMessageLoopBaseDeprecated() == this;
}

MessagePump* MessageLoopImpl::GetMessagePump() const {
  return pump_.get();
}

#if defined(OS_IOS)
void MessageLoopImpl::AttachToMessagePump() {
  DCHECK_EQ(type_, MessageLoopBase::TYPE_UI);
  static_cast<MessagePumpUIApplication*>(pump_.get())->Attach(this);
}
#elif defined(OS_ANDROID)
void MessageLoopImpl::AttachToMessagePump() {
  DCHECK(type_ == MessageLoopBase::TYPE_UI ||
         type_ == MessageLoopBase::TYPE_JAVA);
  static_cast<MessagePumpForUI*>(pump_.get())->Attach(this);
}
#endif  // defined(OS_IOS)

void MessageLoopImpl::SetTimerSlack(TimerSlack timer_slack) {
  pump_->SetTimerSlack(timer_slack);
}

void MessageLoopImpl::Run(bool application_tasks_allowed) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  if (application_tasks_allowed && !task_execution_allowed_) {
    // Allow nested task execution as explicitly requested.
    DCHECK(RunLoop::IsNestedOnCurrentThread());
    task_execution_allowed_ = true;
    pump_->Run(this);
    task_execution_allowed_ = false;
  } else {
    pump_->Run(this);
  }
}

void MessageLoopImpl::Quit() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  pump_->Quit();
}

void MessageLoopImpl::EnsureWorkScheduled() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  if (sequenced_task_source_->HasTasks())
    pump_->ScheduleWork();
}

void MessageLoopImpl::SetThreadTaskRunnerHandle() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  // Clear the previous thread task runner first, because only one can exist at
  // a time.
  thread_task_runner_handle_.reset();
  thread_task_runner_handle_.reset(new ThreadTaskRunnerHandle(task_runner_));
}

bool MessageLoopImpl::ProcessNextDelayedNonNestableTask() {
  if (RunLoop::IsNestedOnCurrentThread())
    return false;

  while (pending_task_queue_.deferred_tasks().HasTasks()) {
    PendingTask pending_task = pending_task_queue_.deferred_tasks().Pop();
    if (!pending_task.task.IsCancelled()) {
      RunTask(&pending_task);
      return true;
    }
  }

  return false;
}

void MessageLoopImpl::RunTask(PendingTask* pending_task) {
  DCHECK(task_execution_allowed_);

  // Execute the task and assume the worst: It is probably not reentrant.
  task_execution_allowed_ = false;

  TRACE_TASK_EXECUTION("MessageLoop::RunTask", *pending_task);

  for (TaskObserver* task_observer : task_observers_)
    task_observer->WillProcessTask(*pending_task);
  message_loop_controller_->task_annotator().RunTask("MessageLoop::PostTask",
                                                     pending_task);
  for (TaskObserver* task_observer : task_observers_)
    task_observer->DidProcessTask(*pending_task);

  task_execution_allowed_ = true;
}

bool MessageLoopImpl::DeferOrRunPendingTask(PendingTask pending_task) {
  if (pending_task.nestable == Nestable::kNestable ||
      !RunLoop::IsNestedOnCurrentThread()) {
    RunTask(&pending_task);
    // Show that we ran a task (Note: a new one might arrive as a
    // consequence!).
    return true;
  }

  // We couldn't run the task now because we're in a nested run loop
  // and the task isn't nestable.
  pending_task_queue_.deferred_tasks().Push(std::move(pending_task));
  return false;
}

void MessageLoopImpl::DeletePendingTasks() {
  // Delete all currently pending tasks but not tasks potentially posted from
  // their destructors. See ~MessageLoop() for the full logic mitigating against
  // infite loops when clearing pending tasks. The ScopedClosureRunner below
  // will be bound to a task posted at the end of the queue. After it is posted,
  // tasks will be deleted one by one, when the bound ScopedClosureRunner is
  // deleted and sets |deleted_all_originally_pending|, we know we've deleted
  // all originally pending tasks.
  bool deleted_all_originally_pending = false;
  ScopedClosureRunner capture_deleted_all_originally_pending(BindOnce(
      [](bool* deleted_all_originally_pending) {
        *deleted_all_originally_pending = true;
      },
      Unretained(&deleted_all_originally_pending)));
  sequenced_task_source_->InjectTask(
      BindOnce([](ScopedClosureRunner) {},
               std::move(capture_deleted_all_originally_pending)));

  while (!deleted_all_originally_pending) {
    PendingTask pending_task = sequenced_task_source_->TakeTask();

    // New delayed tasks should be deleted after older ones.
    if (!pending_task.delayed_run_time.is_null())
      pending_task_queue_.delayed_tasks().Push(std::move(pending_task));
  }

  pending_task_queue_.deferred_tasks().Clear();
  // TODO(robliao): Determine if we can move delayed task destruction before
  // deferred tasks to maintain the MessagePump DoWork, DoDelayedWork, and
  // DoIdleWork processing order.
  pending_task_queue_.delayed_tasks().Clear();
}

bool MessageLoopImpl::HasTasks() {
  return sequenced_task_source_->HasTasks();
}

void MessageLoopImpl::SetTaskExecutionAllowed(bool allowed) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  if (allowed)
    pump_->ScheduleWork();
  task_execution_allowed_ = allowed;
}

bool MessageLoopImpl::IsTaskExecutionAllowed() const {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  return task_execution_allowed_;
}

void MessageLoopImpl::ScheduleWork() {
  pump_->ScheduleWork();
}

TimeTicks MessageLoopImpl::CapAtOneDay(TimeTicks next_run_time) {
  return std::min(next_run_time, recent_time_ + TimeDelta::FromDays(1));
}

bool MessageLoopImpl::DoWork() {
  if (!task_execution_allowed_)
    return false;

  // Execute oldest task.
  while (sequenced_task_source_->HasTasks()) {
    PendingTask pending_task = sequenced_task_source_->TakeTask();
    if (pending_task.task.IsCancelled())
      continue;

    if (!pending_task.delayed_run_time.is_null()) {
      int sequence_num = pending_task.sequence_num;
      TimeTicks delayed_run_time = pending_task.delayed_run_time;
      pending_task_queue_.delayed_tasks().Push(std::move(pending_task));
      // If we changed the topmost task, then it is time to reschedule.
      if (pending_task_queue_.delayed_tasks().Peek().sequence_num ==
          sequence_num) {
        pump_->ScheduleDelayedWork(delayed_run_time);
      }
    } else if (DeferOrRunPendingTask(std::move(pending_task))) {
      return true;
    }
  }

  // Nothing happened.
  return false;
}

bool MessageLoopImpl::DoDelayedWork(TimeTicks* next_delayed_work_time) {
  if (!task_execution_allowed_ ||
      !pending_task_queue_.delayed_tasks().HasTasks()) {
    *next_delayed_work_time = TimeTicks();
    return false;
  }

  // When we "fall behind", there will be a lot of tasks in the delayed work
  // queue that are ready to run.  To increase efficiency when we fall behind,
  // we will only call Time::Now() intermittently, and then process all tasks
  // that are ready to run before calling it again.  As a result, the more we
  // fall behind (and have a lot of ready-to-run delayed tasks), the more
  // efficient we'll be at handling the tasks.

  TimeTicks next_run_time =
      pending_task_queue_.delayed_tasks().Peek().delayed_run_time;

  if (next_run_time > recent_time_) {
    recent_time_ = TimeTicks::Now();  // Get a better view of Now();
    if (next_run_time > recent_time_) {
      *next_delayed_work_time = CapAtOneDay(next_run_time);
      return false;
    }
  }

  PendingTask pending_task = pending_task_queue_.delayed_tasks().Pop();

  if (pending_task_queue_.delayed_tasks().HasTasks()) {
    *next_delayed_work_time = CapAtOneDay(
        pending_task_queue_.delayed_tasks().Peek().delayed_run_time);
  }

  return DeferOrRunPendingTask(std::move(pending_task));
}

bool MessageLoopImpl::DoIdleWork() {
  if (ProcessNextDelayedNonNestableTask())
    return true;

#if defined(OS_WIN)
  bool need_high_res_timers = false;
#endif

  // Do not report idle metrics if about to quit the loop and/or in a nested
  // loop where |!task_execution_allowed_|. In the former case, the loop isn't
  // going to sleep and in the latter case DoDelayedWork() will not actually do
  // the work this is prepping for.
  if (ShouldQuitWhenIdle()) {
    pump_->Quit();
  } else if (task_execution_allowed_) {
#if defined(OS_WIN)
    // On Windows we activate the high resolution timer so that the wait
    // _if_ triggered by the timer happens with good resolution. If we don't
    // do this the default resolution is 15ms which might not be acceptable
    // for some tasks.
    need_high_res_timers = pending_task_queue_.HasPendingHighResolutionTasks();
#endif
  }

#if defined(OS_WIN)
  if (in_high_res_mode_ != need_high_res_timers) {
    in_high_res_mode_ = need_high_res_timers;
    Time::ActivateHighResolutionTimer(in_high_res_mode_);
  }
#endif

  // When we return we will do a kernel wait for more tasks.
  return false;
}

}  // namespace base
