// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/message_pump/message_pump_mojo.h"

#include <algorithm>
#include <vector>

#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "mojo/message_pump/message_pump_mojo_handler.h"
#include "mojo/message_pump/time_helper.h"

namespace mojo {
namespace common {
namespace {

base::LazyInstance<base::ThreadLocalPointer<MessagePumpMojo> >::Leaky
    g_tls_current_pump = LAZY_INSTANCE_INITIALIZER;

MojoDeadline TimeTicksToMojoDeadline(base::TimeTicks time_ticks,
                                     base::TimeTicks now) {
  // The is_null() check matches that of HandleWatcher as well as how
  // |delayed_work_time| is used.
  if (time_ticks.is_null())
    return MOJO_DEADLINE_INDEFINITE;
  const int64_t delta = (time_ticks - now).InMicroseconds();
  return delta < 0 ? static_cast<MojoDeadline>(0) :
                     static_cast<MojoDeadline>(delta);
}

}  // namespace

// State needed for one iteration of WaitMany. The first handle and flags
// corresponds to that of the control pipe.
struct MessagePumpMojo::WaitState {
  std::vector<Handle> handles;
  std::vector<MojoHandleSignals> wait_signals;
};

struct MessagePumpMojo::RunState {
  RunState() : should_quit(false) {}

  base::TimeTicks delayed_work_time;

  bool should_quit;
};

MessagePumpMojo::MessagePumpMojo() : run_state_(NULL), next_handler_id_(0) {
  DCHECK(!current())
      << "There is already a MessagePumpMojo instance on this thread.";
  g_tls_current_pump.Pointer()->Set(this);

  MojoResult result = CreateMessagePipe(nullptr, &read_handle_, &write_handle_);
  CHECK_EQ(result, MOJO_RESULT_OK);
  CHECK(read_handle_.is_valid());
  CHECK(write_handle_.is_valid());
}

MessagePumpMojo::~MessagePumpMojo() {
  DCHECK_EQ(this, current());
  g_tls_current_pump.Pointer()->Set(NULL);
}

// static
scoped_ptr<base::MessagePump> MessagePumpMojo::Create() {
  return scoped_ptr<MessagePump>(new MessagePumpMojo());
}

// static
MessagePumpMojo* MessagePumpMojo::current() {
  return g_tls_current_pump.Pointer()->Get();
}

void MessagePumpMojo::AddHandler(MessagePumpMojoHandler* handler,
                                 const Handle& handle,
                                 MojoHandleSignals wait_signals,
                                 base::TimeTicks deadline) {
  CHECK(handler);
  DCHECK(handle.is_valid());
  // Assume it's an error if someone tries to reregister an existing handle.
  CHECK_EQ(0u, handlers_.count(handle));
  Handler handler_data;
  handler_data.handler = handler;
  handler_data.wait_signals = wait_signals;
  handler_data.deadline = deadline;
  handler_data.id = next_handler_id_++;
  handlers_[handle] = handler_data;
  if (!deadline.is_null()) {
    bool inserted = deadline_handles_.insert(handle).second;
    DCHECK(inserted);
  }
}

void MessagePumpMojo::RemoveHandler(const Handle& handle) {
  handlers_.erase(handle);
  deadline_handles_.erase(handle);
}

void MessagePumpMojo::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MessagePumpMojo::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessagePumpMojo::Run(Delegate* delegate) {
  RunState run_state;
  RunState* old_state = NULL;
  {
    base::AutoLock auto_lock(run_state_lock_);
    old_state = run_state_;
    run_state_ = &run_state;
  }
  DoRunLoop(&run_state, delegate);
  {
    base::AutoLock auto_lock(run_state_lock_);
    run_state_ = old_state;
  }
}

void MessagePumpMojo::Quit() {
  base::AutoLock auto_lock(run_state_lock_);
  if (run_state_)
    run_state_->should_quit = true;
}

void MessagePumpMojo::ScheduleWork() {
  SignalControlPipe();
}

void MessagePumpMojo::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  base::AutoLock auto_lock(run_state_lock_);
  if (!run_state_)
    return;
  run_state_->delayed_work_time = delayed_work_time;
}

void MessagePumpMojo::DoRunLoop(RunState* run_state, Delegate* delegate) {
  bool more_work_is_plausible = true;
  for (;;) {
    const bool block = !more_work_is_plausible;
    more_work_is_plausible = DoInternalWork(*run_state, block);

    if (run_state->should_quit)
      break;

    more_work_is_plausible |= delegate->DoWork();
    if (run_state->should_quit)
      break;

    more_work_is_plausible |= delegate->DoDelayedWork(
        &run_state->delayed_work_time);
    if (run_state->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = delegate->DoIdleWork();
    if (run_state->should_quit)
      break;
  }
}

bool MessagePumpMojo::DoInternalWork(const RunState& run_state, bool block) {
  const MojoDeadline deadline = block ? GetDeadlineForWait(run_state) : 0;
  const WaitState wait_state = GetWaitState();

  std::vector<MojoHandleSignalsState> states(wait_state.handles.size());
  const WaitManyResult wait_many_result =
      WaitMany(wait_state.handles, wait_state.wait_signals, deadline, &states);
  const MojoResult result = wait_many_result.result;
  bool did_work = true;
  if (result == MOJO_RESULT_OK) {
    if (wait_many_result.index == 0) {
      if (states[0].satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED) {
        // The Mojo EDK is shutting down. The ThreadQuitHelper task in
        // base::Thread won't get run since the control pipe depends on the EDK
        // staying alive. So quit manually to avoid this thread hanging.
        Quit();
      } else {
        // Control pipe was written to.
        ReadMessageRaw(read_handle_.get(), NULL, NULL, NULL, NULL,
                       MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
      }
    } else {
      DCHECK(handlers_.find(wait_state.handles[wait_many_result.index]) !=
             handlers_.end());
      WillSignalHandler();
      handlers_[wait_state.handles[wait_many_result.index]]
          .handler->OnHandleReady(wait_state.handles[wait_many_result.index]);
      DidSignalHandler();
    }
  } else {
    switch (result) {
      case MOJO_RESULT_CANCELLED:
      case MOJO_RESULT_FAILED_PRECONDITION:
      case MOJO_RESULT_INVALID_ARGUMENT:
        RemoveInvalidHandle(wait_state, result, wait_many_result.index);
        break;
      case MOJO_RESULT_DEADLINE_EXCEEDED:
        did_work = false;
        break;
      default:
        base::debug::Alias(&result);
        // Unexpected result is likely fatal, crash so we can determine cause.
        CHECK(false);
    }
  }

  // Notify and remove any handlers whose time has expired. First, iterate over
  // the set of handles that have a deadline, and add the expired handles to a
  // map of <Handle, id>. Then, iterate over those expired handles and remove
  // them. The two-step process is because a handler can add/remove new
  // handlers.
  std::map<Handle, int> expired_handles;
  const base::TimeTicks now(internal::NowTicks());
  for (const Handle handle : deadline_handles_) {
    const auto it = handlers_.find(handle);
    // Expect any handle in |deadline_handles_| to also be in |handlers_| since
    // the two are modified in lock-step.
    DCHECK(it != handlers_.end());
    if (!it->second.deadline.is_null() && it->second.deadline < now)
      expired_handles[handle] = it->second.id;
  }
  for (auto& pair : expired_handles) {
    auto it = handlers_.find(pair.first);
    // Don't need to check deadline again since it can't change if id hasn't
    // changed.
    if (it != handlers_.end() && it->second.id == pair.second) {
      MessagePumpMojoHandler* handler = handlers_[pair.first].handler;
      RemoveHandler(pair.first);
      WillSignalHandler();
      handler->OnHandleError(pair.first, MOJO_RESULT_DEADLINE_EXCEEDED);
      DidSignalHandler();
      did_work = true;
    }
  }
  return did_work;
}

void MessagePumpMojo::RemoveInvalidHandle(const WaitState& wait_state,
                                          MojoResult result,
                                          uint32_t index) {
  // TODO(sky): deal with control pipe going bad.
  CHECK(result == MOJO_RESULT_INVALID_ARGUMENT ||
        result == MOJO_RESULT_FAILED_PRECONDITION ||
        result == MOJO_RESULT_CANCELLED);
  CHECK_NE(index, 0u);  // Indicates the control pipe went bad.

  // Remove the handle first, this way if OnHandleError() tries to remove the
  // handle our iterator isn't invalidated.
  Handle handle = wait_state.handles[index];
  CHECK(handlers_.find(handle) != handlers_.end());
  MessagePumpMojoHandler* handler = handlers_[handle].handler;
  RemoveHandler(handle);
  WillSignalHandler();
  handler->OnHandleError(handle, result);
  DidSignalHandler();
}

void MessagePumpMojo::SignalControlPipe() {
  const MojoResult result =
      WriteMessageRaw(write_handle_.get(), NULL, 0, NULL, 0,
                      MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Mojo EDK is shutting down.
    return;
  }

  // If we can't write we likely won't wake up the thread and there is a strong
  // chance we'll deadlock.
  CHECK_EQ(MOJO_RESULT_OK, result);
}

MessagePumpMojo::WaitState MessagePumpMojo::GetWaitState() const {
  WaitState wait_state;
  wait_state.handles.push_back(read_handle_.get());
  wait_state.wait_signals.push_back(
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  for (HandleToHandler::const_iterator i = handlers_.begin();
       i != handlers_.end(); ++i) {
    wait_state.handles.push_back(i->first);
    wait_state.wait_signals.push_back(i->second.wait_signals);
  }
  return wait_state;
}

MojoDeadline MessagePumpMojo::GetDeadlineForWait(
    const RunState& run_state) const {
  const base::TimeTicks now(internal::NowTicks());
  MojoDeadline deadline = TimeTicksToMojoDeadline(run_state.delayed_work_time,
                                                  now);
  for (const Handle handle : deadline_handles_) {
    auto it = handlers_.find(handle);
    DCHECK(it != handlers_.end());
    deadline = std::min(
        TimeTicksToMojoDeadline(it->second.deadline, now), deadline);
  }
  return deadline;
}

void MessagePumpMojo::WillSignalHandler() {
  FOR_EACH_OBSERVER(Observer, observers_, WillSignalHandler());
}

void MessagePumpMojo::DidSignalHandler() {
  FOR_EACH_OBSERVER(Observer, observers_, DidSignalHandler());
}

}  // namespace common
}  // namespace mojo
