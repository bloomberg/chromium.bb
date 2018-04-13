// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop_current.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"

namespace base {

//------------------------------------------------------------------------------
// MessageLoopCurrent

void MessageLoopCurrent::AddDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->destruction_observers_.AddObserver(destruction_observer);
}

void MessageLoopCurrent::RemoveDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->destruction_observers_.RemoveObserver(destruction_observer);
}

// static
Closure MessageLoopCurrent::QuitWhenIdleClosure() {
  return Bind(&RunLoop::QuitCurrentWhenIdleDeprecated);
}

const scoped_refptr<SingleThreadTaskRunner>& MessageLoopCurrent::task_runner()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return current_->task_runner();
}

void MessageLoopCurrent::SetTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->SetTaskRunner(std::move(task_runner));
}

bool MessageLoopCurrent::IsIdleForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return current_->IsIdleForTesting();
}

void MessageLoopCurrent::AddTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->AddTaskObserver(task_observer);
}

void MessageLoopCurrent::RemoveTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->RemoveTaskObserver(task_observer);
}

void MessageLoopCurrent::SetNestableTasksAllowed(bool allowed) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  if (allowed) {
    // Kick the native pump just in case we enter a OS-driven nested message
    // loop that does not go through RunLoop::Run().
    current_->pump_->ScheduleWork();
  }
  current_->task_execution_allowed_ = allowed;
}

bool MessageLoopCurrent::NestableTasksAllowed() const {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return current_->task_execution_allowed_;
}

#if !defined(OS_NACL)

//------------------------------------------------------------------------------
// MessageLoopCurrentForUI

#if (defined(USE_OZONE) && !defined(OS_FUCHSIA)) || \
    (defined(USE_X11) && !defined(USE_GLIB))
bool MessageLoopCurrentForUI::WatchFileDescriptor(
    int fd,
    bool persistent,
    MessagePumpForUI::Mode mode,
    MessagePumpForUI::FdWatchController* controller,
    MessagePumpForUI::FdWatcher* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->WatchFileDescriptor(fd, persistent, mode, controller, delegate);
}
#endif

#if defined(OS_IOS)
void MessageLoopCurrentForUI::Attach() {
  static_cast<MessageLoopForUI*>(current_)->Attach();
}
#endif  // defined(OS_IOS)

#if defined(OS_ANDROID)
void MessageLoopCurrentForUI::Start() {
  static_cast<MessageLoopForUI*>(current_)->Start();
}

void MessageLoopCurrentForUI::Abort() {
  static_cast<MessageLoopForUI*>(current_)->Abort();
}
#endif  // defined(OS_ANDROID)

#endif  // !defined(OS_NACL)

//------------------------------------------------------------------------------
// MessageLoopCurrentForIO

#if !defined(OS_NACL_SFI)

#if defined(OS_WIN)
void MessageLoopCurrentForIO::RegisterIOHandler(
    HANDLE file,
    MessagePumpForIO::IOHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  pump_->RegisterIOHandler(file, handler);
}

bool MessageLoopCurrentForIO::RegisterJobObject(
    HANDLE job,
    MessagePumpForIO::IOHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->RegisterJobObject(job, handler);
}

bool MessageLoopCurrentForIO::WaitForIOCompletion(
    DWORD timeout,
    MessagePumpForIO::IOHandler* filter) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->WaitForIOCompletion(timeout, filter);
}
#elif defined(OS_POSIX)  // defined(OS_WIN)
bool MessageLoopCurrentForIO::WatchFileDescriptor(
    int fd,
    bool persistent,
    MessagePumpForIO::Mode mode,
    MessagePumpForIO::FdWatchController* controller,
    MessagePumpForIO::FdWatcher* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->WatchFileDescriptor(fd, persistent, mode, controller, delegate);
}
#endif                   // defined(OS_WIN)

#endif  // !defined(OS_NACL_SFI)

#if defined(OS_FUCHSIA)
// Additional watch API for native platform resources.
bool MessageLoopCurrentForIO::WatchZxHandle(
    zx_handle_t handle,
    bool persistent,
    zx_signals_t signals,
    MessagePumpForIO::ZxHandleWatchController* controller,
    MessagePumpForIO::ZxHandleWatcher* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->WatchZxHandle(handle, persistent, signals, controller,
                              delegate);
}
#endif

}  // namespace base
