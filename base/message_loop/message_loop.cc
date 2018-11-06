// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"

#include <algorithm>
#include <atomic>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/debug/task_annotator.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_impl.h"
#include "base/message_loop/message_loop_task_runner.h"
#include "base/message_loop/message_pump_default.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/sequenced_task_source.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"

#if defined(OS_MACOSX)
#include "base/message_loop/message_pump_mac.h"
#endif

namespace base {

namespace {

MessageLoop::MessagePumpFactory* message_pump_for_ui_factory_ = nullptr;

std::unique_ptr<MessagePump> ReturnPump(std::unique_ptr<MessagePump> pump) {
  return pump;
}

}  // namespace

MessageLoop::MessageLoop(Type type)
    : MessageLoop(type, MessagePumpFactoryCallback()) {
  BindToCurrentThread();
}

MessageLoop::MessageLoop(std::unique_ptr<MessagePump> pump)
    : MessageLoop(TYPE_CUSTOM, BindOnce(&ReturnPump, std::move(pump))) {
  BindToCurrentThread();
}

MessageLoop::~MessageLoop() {
  // If |pump_| is non-null, this message loop has been bound and should be the
  // current one on this thread. Otherwise, this loop is being destructed before
  // it was bound to a thread, so a different message loop (or no loop at all)
  // may be current.
  DCHECK((pump_ && IsBoundToCurrentThread()) ||
         (!pump_ && !IsBoundToCurrentThread()));

// iOS just attaches to the loop, it doesn't Run it.
// TODO(stuartmorgan): Consider wiring up a Detach().
#if !defined(OS_IOS)
  // There should be no active RunLoops on this thread, unless this MessageLoop
  // isn't bound to the current thread (see other condition at the top of this
  // method).
  DCHECK((!pump_ && !IsBoundToCurrentThread()) ||
         !RunLoop::IsRunningOnCurrentThread());
#endif  // !defined(OS_IOS)

  // Clean up any unprocessed tasks, but take care: deleting a task could
  // result in the addition of more tasks (e.g., via DeleteSoon).  We set a
  // limit on the number of times we will allow a deleted task to generate more
  // tasks.  Normally, we should only pass through this loop once or twice.  If
  // we end up hitting the loop limit, then it is probably due to one task that
  // is being stubborn.  Inspect the queues to see who is left.
  bool tasks_remain;
  for (int i = 0; i < 100; ++i) {
    DeletePendingTasks();
    // If we end up with empty queues, then break out of the loop.
    tasks_remain = HasTasks();
    if (!tasks_remain)
      break;
  }
  DCHECK(!tasks_remain);

  // Let interested parties have one last shot at accessing this.
  for (auto& observer : destruction_observers_)
    observer.WillDestroyCurrentMessageLoop();

  // OK, now make it so that no one can find us.
  if (IsBoundToCurrentThread())
    MessageLoopCurrent::UnbindFromCurrentThreadInternal(this);
}

// static
bool MessageLoop::InitMessagePumpForUIFactory(MessagePumpFactory* factory) {
  if (message_pump_for_ui_factory_)
    return false;

  message_pump_for_ui_factory_ = factory;
  return true;
}

// static
std::unique_ptr<MessagePump> MessageLoop::CreateMessagePumpForType(Type type) {
  if (type == MessageLoop::TYPE_UI) {
    if (message_pump_for_ui_factory_)
      return message_pump_for_ui_factory_();
#if defined(OS_IOS) || defined(OS_MACOSX)
    return MessagePumpMac::Create();
#elif defined(OS_NACL) || defined(OS_AIX)
    // Currently NaCl and AIX don't have a UI MessageLoop.
    // TODO(abarth): Figure out if we need this.
    NOTREACHED();
    return nullptr;
#else
    return std::make_unique<MessagePumpForUI>();
#endif
  }

  if (type == MessageLoop::TYPE_IO)
    return std::make_unique<MessagePumpForIO>();

#if defined(OS_ANDROID)
  if (type == MessageLoop::TYPE_JAVA)
    return std::make_unique<MessagePumpForUI>();
#endif

  DCHECK_EQ(MessageLoop::TYPE_DEFAULT, type);
#if defined(OS_IOS)
  // On iOS, a native runloop is always required to pump system work.
  return std::make_unique<MessagePumpCFRunLoop>();
#else
  return std::make_unique<MessagePumpDefault>();
#endif
}

bool MessageLoop::IsType(Type type) const {
  return type_ == type;
}

// TODO(gab): Migrate TaskObservers to RunLoop as part of separating concerns
// between MessageLoop and RunLoop and making MessageLoop a swappable
// implementation detail. http://crbug.com/703346
void MessageLoop::AddTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  message_loop_impl_->AddTaskObserver(task_observer);
}

void MessageLoop::RemoveTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  message_loop_impl_->RemoveTaskObserver(task_observer);
}

bool MessageLoop::IsBoundToCurrentThread() const {
  return MessageLoopCurrent::Get()->ToMessageLoopDeprecated() == this;
}

void MessageLoop::SetAddQueueTimeToTasks(bool enable) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  message_loop_impl_->SetAddQueueTimeToTasks(enable);
}

bool MessageLoop::IsIdleForTesting() {
  return message_loop_impl_->IsIdleForTesting();
}

//------------------------------------------------------------------------------

// static
std::unique_ptr<MessageLoop> MessageLoop::CreateUnbound(
    Type type,
    MessagePumpFactoryCallback pump_factory) {
  return WrapUnique(new MessageLoop(type, std::move(pump_factory)));
}

MessageLoop::MessageLoop(Type type, MessagePumpFactoryCallback pump_factory)
    : MessageLoopCurrent(this),
      pump_(nullptr),
      message_loop_impl_(std::make_unique<MessageLoopImpl>()),
      type_(type),
      pump_factory_(std::move(pump_factory)) {
  // If type is TYPE_CUSTOM non-null pump_factory must be given.
  DCHECK(type_ != TYPE_CUSTOM || !pump_factory_.is_null());

  // Bound in BindToCurrentThread();
  DETACH_FROM_THREAD(bound_thread_checker_);

  if (type_ == MessageLoop::TYPE_UI) {
    // Only track idle metrics in MessageLoopForUI to avoid too much contention
    // logging the histogram (https://crbug.com/860801) -- there's typically
    // only one UI thread per process and, for practical purposes, restricting
    // the MessageLoop diagnostic metrics to it yields similar information.
    message_loop_impl_->SetShouldRecordIdleMetrics(true);
  }
}

void MessageLoop::BindToCurrentThread() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_checker_);
  thread_id_ = PlatformThread::CurrentId();

  DCHECK(!pump_);

  std::unique_ptr<MessagePump> pump = CreateMessagePump();
  pump_ = pump.get();

  DCHECK(!MessageLoopCurrent::IsSet())
      << "should only have one message loop per thread";
  MessageLoopCurrent::BindToCurrentThreadInternal(this);

  message_loop_impl_->BindToCurrentThread(std::move(pump));

#if defined(OS_ANDROID)
  // On Android, attach to the native loop when there is one.
  if (type_ == TYPE_UI || type_ == TYPE_JAVA)
    static_cast<MessagePumpForUI*>(pump_)->Attach(message_loop_impl_.get());
#endif
}

std::unique_ptr<MessagePump> MessageLoop::CreateMessagePump() {
  if (!pump_factory_.is_null()) {
    return std::move(pump_factory_).Run();
  } else {
    return CreateMessagePumpForType(type_);
  }
}

std::string MessageLoop::GetThreadName() const {
  DCHECK_NE(kInvalidThreadId, thread_id_)
      << "GetThreadName() must only be called after BindToCurrentThread()'s "
      << "side-effects have been synchronized with this thread.";
  return ThreadIdNameManager::GetInstance()->GetName(thread_id_);
}

const scoped_refptr<SingleThreadTaskRunner>& MessageLoop::task_runner() const {
  return message_loop_impl_->task_runner();
}

void MessageLoop::SetTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  message_loop_impl_->SetTaskRunner(task_runner);
}

void MessageLoop::DeletePendingTasks() {
  message_loop_impl_->DeletePendingTasks();
}

bool MessageLoop::HasTasks() const {
  return message_loop_impl_->HasTasks();
}

void MessageLoop::SetTaskExecutionAllowed(bool allowed) {
  return message_loop_impl_->SetTaskExecutionAllowed(allowed);
}

bool MessageLoop::IsTaskExecutionAllowed() const {
  return message_loop_impl_->IsTaskExecutionAllowed();
}

#if !defined(OS_NACL)

//------------------------------------------------------------------------------
// MessageLoopForUI

MessageLoopForUI::MessageLoopForUI(Type type) : MessageLoop(type) {
#if defined(OS_ANDROID)
  DCHECK(type == TYPE_UI || type == TYPE_JAVA);
#else
  DCHECK_EQ(type, TYPE_UI);
#endif
}

// static
MessageLoopCurrentForUI MessageLoopForUI::current() {
  return MessageLoopCurrentForUI::Get();
}

// static
bool MessageLoopForUI::IsCurrent() {
  return MessageLoopCurrentForUI::IsSet();
}

#if defined(OS_IOS)
void MessageLoopForUI::Attach() {
  static_cast<MessagePumpUIApplication*>(pump_)->Attach(
      message_loop_impl_.get());
}
#endif  // defined(OS_IOS)

#if defined(OS_ANDROID)
void MessageLoopForUI::Abort() {
  static_cast<MessagePumpForUI*>(pump_)->Abort();
}

bool MessageLoopForUI::IsAborted() {
  return static_cast<MessagePumpForUI*>(pump_)->IsAborted();
}

void MessageLoopForUI::QuitWhenIdle(base::OnceClosure callback) {
  static_cast<MessagePumpForUI*>(pump_)->QuitWhenIdle(std::move(callback));
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
void MessageLoopForUI::EnableWmQuit() {
  static_cast<MessagePumpForUI*>(pump_)->EnableWmQuit();
}
#endif  // defined(OS_WIN)

#endif  // !defined(OS_NACL)

//------------------------------------------------------------------------------
// MessageLoopForIO

// static
MessageLoopCurrentForIO MessageLoopForIO::current() {
  return MessageLoopCurrentForIO::Get();
}

// static
bool MessageLoopForIO::IsCurrent() {
  return MessageLoopCurrentForIO::IsSet();
}

}  // namespace base
