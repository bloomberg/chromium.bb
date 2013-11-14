// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/message_pump_mojo.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "mojo/common/message_pump_mojo_handler.h"
#include "mojo/common/scoped_message_pipe.h"

namespace mojo {
namespace common {

// State needed for one iteration of MojoWaitMany. The first handle and flags
// corresponds to that of the control pipe.
struct MessagePumpMojo::WaitState {
  std::vector<MojoHandle> handles;
  std::vector<MojoWaitFlags> wait_flags;
};

struct MessagePumpMojo::RunState {
 public:
  RunState() : should_quit(false) {}

  MojoHandle read_handle() const { return control_pipe.handle_0(); }
  MojoHandle write_handle() const { return control_pipe.handle_1(); }

  base::TimeTicks delayed_work_time;

  // Used to wake up WaitForWork().
  ScopedMessagePipe control_pipe;

  bool should_quit;
};

MessagePumpMojo::MessagePumpMojo() : run_state_(NULL) {
}

MessagePumpMojo::~MessagePumpMojo() {
}

void MessagePumpMojo::AddHandler(MessagePumpMojoHandler* handler,
                                 MojoHandle handle,
                                 MojoWaitFlags wait_flags) {
  DCHECK(handler);
  DCHECK_NE(MOJO_HANDLE_INVALID, handle);
  handlers_[handle].handler = handler;
  handlers_[handle].wait_flags = wait_flags;

  SignalControlPipe();
}

void MessagePumpMojo::RemoveHandler(MojoHandle handle) {
  handlers_.erase(handle);
  SignalControlPipe();
}

void MessagePumpMojo::Run(Delegate* delegate) {
  RunState* old_state = run_state_;
  RunState run_state;
  // TODO: better deal with error handling.
  CHECK_NE(run_state.control_pipe.handle_0(), MOJO_HANDLE_INVALID);
  CHECK_NE(run_state.control_pipe.handle_1(), MOJO_HANDLE_INVALID);
  run_state_ = &run_state;
  bool more_work_is_plausible = true;
  for (;;) {
    const bool block = !more_work_is_plausible;
    DoInternalWork(block);

    // There isn't a good way to know if there are more handles ready, we assume
    // not.
    more_work_is_plausible = false;

    if (run_state.should_quit)
      break;

    more_work_is_plausible |= delegate->DoWork();
    if (run_state.should_quit)
      break;

    more_work_is_plausible |= delegate->DoDelayedWork(
        &run_state.delayed_work_time);
    if (run_state.should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = delegate->DoIdleWork();
    if (run_state.should_quit)
      break;
  }
  run_state_ = old_state;
}

void MessagePumpMojo::Quit() {
  if (run_state_)
    run_state_->should_quit = true;
}

void MessagePumpMojo::ScheduleWork() {
  SignalControlPipe();
}

void MessagePumpMojo::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  if (!run_state_)
    return;
  run_state_->delayed_work_time = delayed_work_time;
  SignalControlPipe();
}

void MessagePumpMojo::DoInternalWork(bool block) {
  MojoDeadline deadline;
  if (block && !run_state_->delayed_work_time.is_null()) {
    const base::TimeDelta delta = run_state_->delayed_work_time -
        base::TimeTicks::Now();
    deadline = std::max(static_cast<MojoDeadline>(0),
                        static_cast<MojoDeadline>(delta.InMicroseconds()));
  } else {
    deadline = 0;
  }
  const WaitState wait_state = GetWaitState();
  const MojoResult result = MojoWaitMany(
      &wait_state.handles.front(),
      &wait_state.wait_flags.front(),
      static_cast<uint32_t>(wait_state.handles.size()),
      deadline);
  if (result == 0) {
    // Control pipe was written to.
    uint32_t num_bytes = 0;
    MojoReadMessage(run_state_->read_handle(), NULL, &num_bytes, NULL, 0,
                    MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
  } else if (result > 0) {
    const size_t index = static_cast<size_t>(result);
    DCHECK(handlers_.find(wait_state.handles[index]) != handlers_.end());
    handlers_[wait_state.handles[index]].handler->OnHandleReady(
        wait_state.handles[index]);
  } else {
    switch (result) {
      case MOJO_RESULT_INVALID_ARGUMENT:
      case MOJO_RESULT_FAILED_PRECONDITION:
        RemoveFirstInvalidHandle(wait_state);
        break;
      case MOJO_RESULT_DEADLINE_EXCEEDED:
        break;
      default:
        NOTREACHED();
    }
  }
}

void MessagePumpMojo::RemoveFirstInvalidHandle(const WaitState& wait_state) {
  // TODO(sky): deal with control pipe going bad.
  for (size_t i = 1; i < wait_state.handles.size(); ++i) {
    const MojoResult result =
        MojoWait(wait_state.handles[i], wait_state.wait_flags[i], 0);
    if (result == MOJO_RESULT_INVALID_ARGUMENT ||
        result == MOJO_RESULT_FAILED_PRECONDITION) {
      DCHECK(handlers_.find(wait_state.handles[i]) != handlers_.end());
      MessagePumpMojoHandler* handler =
          handlers_[wait_state.handles[i]].handler;
      handlers_.erase(wait_state.handles[i]);
      handler->OnHandleError(wait_state.handles[i], result);
      return;
    } else {
      DCHECK_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, result);
    }
  }
}

void MessagePumpMojo::SignalControlPipe() {
  if (!run_state_)
    return;

  // TODO(sky): deal with error?
  MojoWriteMessage(run_state_->write_handle(), NULL, 0, NULL, 0,
                   MOJO_WRITE_MESSAGE_FLAG_NONE);
}

MessagePumpMojo::WaitState MessagePumpMojo::GetWaitState() const {
  WaitState wait_state;
  wait_state.handles.push_back(run_state_->read_handle());
  wait_state.wait_flags.push_back(MOJO_WAIT_FLAG_READABLE);

  for (HandleToHandler::const_iterator i = handlers_.begin();
       i != handlers_.end(); ++i) {
    wait_state.handles.push_back(i->first);
    wait_state.wait_flags.push_back(i->second.wait_flags);
  }
  return wait_state;
}

}  // namespace common
}  // namespace mojo
