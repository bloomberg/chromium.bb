// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "mojo/system/limits.h"
#include "mojo/system/memory.h"

namespace mojo {
namespace system {

namespace {

unsigned DestinationPortFromSourcePort(unsigned port) {
  DCHECK(port == 0 || port == 1);
  return port ^ 1;
}

}  // namespace

MessagePipe::MessagePipe() {
  is_open_[0] = is_open_[1] = true;
}

void MessagePipe::CancelAllWaiters(unsigned port) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(is_open_[port]);

  waiter_lists_[port].CancelAllWaiters();
}

void MessagePipe::Close(unsigned port) {
  DCHECK(port == 0 || port == 1);

  unsigned destination_port = DestinationPortFromSourcePort(port);

  base::AutoLock locker(lock_);
  DCHECK(is_open_[port]);

  // Record the old state of the other (destination) port, so we can tell if it
  // changes.
  // TODO(vtl): Maybe the |WaiterList| should track the old state, so that we
  // don't have to do this.
  MojoWaitFlags old_dest_satisfied_flags;
  MojoWaitFlags old_dest_satisfiable_flags;
  bool dest_is_open = is_open_[destination_port];
  if (dest_is_open) {
    old_dest_satisfied_flags = SatisfiedFlagsNoLock(destination_port);
    old_dest_satisfiable_flags = SatisfiableFlagsNoLock(destination_port);
  }

  is_open_[port] = false;
  STLDeleteElements(&message_queues_[port]);  // Clear incoming queue for port.

  // Notify the other (destination) port if its state has changed.
  if (dest_is_open) {
    MojoWaitFlags new_dest_satisfied_flags =
        SatisfiedFlagsNoLock(destination_port);
    MojoWaitFlags new_dest_satisfiable_flags =
        SatisfiableFlagsNoLock(destination_port);
    if (new_dest_satisfied_flags != old_dest_satisfied_flags ||
        new_dest_satisfiable_flags != old_dest_satisfiable_flags) {
      waiter_lists_[destination_port].AwakeWaitersForStateChange(
          new_dest_satisfied_flags, new_dest_satisfiable_flags);
    }
  }
}

MojoResult MessagePipe::WriteMessage(
    unsigned port,
    const void* bytes, uint32_t num_bytes,
    const MojoHandle* handles, uint32_t num_handles,
    MojoWriteMessageFlags /*flags*/) {
  DCHECK(port == 0 || port == 1);

  unsigned destination_port = DestinationPortFromSourcePort(port);

  if (!VerifyUserPointer(bytes, num_bytes, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_bytes > kMaxMessageNumBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  if (!VerifyUserPointer(handles, num_handles, sizeof(handles[0])))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_handles > kMaxMessageNumHandles)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  if (num_handles > 0) {
    // TODO(vtl): Verify each handle.
    NOTIMPLEMENTED();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  // TODO(vtl): Handle flags.

  base::AutoLock locker(lock_);
  DCHECK(is_open_[port]);

  // The destination port need not be open, unlike the source port.
  if (!is_open_[destination_port])
    return MOJO_RESULT_FAILED_PRECONDITION;

  bool dest_was_empty = message_queues_[destination_port].empty();

  // TODO(vtl): Eventually (with C++11), this should be an |emplace_back()|.
  message_queues_[destination_port].push_back(
      new MessageInTransit(bytes, num_bytes));
  // TODO(vtl): Support sending handles.

  // The other (destination) port was empty and now isn't, so it should now be
  // readable. Wake up anyone waiting on this.
  if (dest_was_empty) {
    waiter_lists_[destination_port].AwakeWaitersForStateChange(
        SatisfiedFlagsNoLock(destination_port),
        SatisfiableFlagsNoLock(destination_port));
  }

  return MOJO_RESULT_OK;
}

MojoResult MessagePipe::ReadMessage(unsigned port,
                                    void* bytes, uint32_t* num_bytes,
                                    MojoHandle* handles, uint32_t* num_handles,
                                    MojoReadMessageFlags flags) {
  DCHECK(port == 0 || port == 1);

  const size_t max_bytes = num_bytes ? *num_bytes : 0;
  if (!VerifyUserPointer(bytes, max_bytes, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;

  const size_t max_handles = num_handles ? *num_handles : 0;
  if (!VerifyUserPointer(handles, max_handles, sizeof(handles[0])))
    return MOJO_RESULT_INVALID_ARGUMENT;

  base::AutoLock locker(lock_);
  DCHECK(is_open_[port]);

  if (message_queues_[port].empty())
    return MOJO_RESULT_NOT_FOUND;

  // TODO(vtl): If |flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD|, we could pop
  // and release the lock immediately.
  bool not_enough_space = false;
  MessageInTransit* const message = message_queues_[port].front();
  const size_t message_size = message->data.size();
  if (num_bytes)
    *num_bytes = static_cast<uint32_t>(message_size);
  if (message_size <= max_bytes)
    memcpy(bytes, message->data.data(), message_size);
  else
    not_enough_space = true;

  // TODO(vtl): Support receiving handles.
  if (num_handles)
    *num_handles = 0;

  if (!not_enough_space || (flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD)) {
    message_queues_[port].pop_front();
    delete message;

    // Now it's empty, thus no longer readable.
    if (message_queues_[port].empty()) {
      // It's currently not possible to wait for non-readability, but we should
      // do the state change anyway.
      waiter_lists_[port].AwakeWaitersForStateChange(
          SatisfiedFlagsNoLock(port), SatisfiableFlagsNoLock(port));
    }
  }

  if (not_enough_space)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  return MOJO_RESULT_OK;
}

MojoResult MessagePipe::AddWaiter(unsigned port,
                                  Waiter* waiter,
                                  MojoWaitFlags flags,
                                  MojoResult wake_result) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(is_open_[port]);

  if ((flags & SatisfiedFlagsNoLock(port)))
    return MOJO_RESULT_ALREADY_EXISTS;
  if (!(flags & SatisfiableFlagsNoLock(port)))
    return MOJO_RESULT_FAILED_PRECONDITION;

  waiter_lists_[port].AddWaiter(waiter, flags, wake_result);
  return MOJO_RESULT_OK;
}

void MessagePipe::RemoveWaiter(unsigned port, Waiter* waiter) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(is_open_[port]);

  waiter_lists_[port].RemoveWaiter(waiter);
}

MessagePipe::~MessagePipe() {
  // Owned by the dispatchers. The owning dispatchers should only release us via
  // their |Close()| method, which should inform us of being closed via our
  // |Close()|. Thus these should already be null.
  DCHECK(!is_open_[0]);
  DCHECK(!is_open_[1]);
}

MojoWaitFlags MessagePipe::SatisfiedFlagsNoLock(unsigned port) {
  DCHECK(port == 0 || port == 1);

  unsigned destination_port = DestinationPortFromSourcePort(port);

  lock_.AssertAcquired();

  MojoWaitFlags satisfied_flags = 0;
  if (!message_queues_[port].empty())
    satisfied_flags |= MOJO_WAIT_FLAG_READABLE;
  if (is_open_[destination_port])
    satisfied_flags |= MOJO_WAIT_FLAG_WRITABLE;

  return satisfied_flags;
}

MojoWaitFlags MessagePipe::SatisfiableFlagsNoLock(unsigned port) {
  DCHECK(port == 0 || port == 1);

  unsigned destination_port = DestinationPortFromSourcePort(port);

  lock_.AssertAcquired();

  MojoWaitFlags satisfiable_flags = 0;
  if (!message_queues_[port].empty() || is_open_[destination_port])
    satisfiable_flags |= MOJO_WAIT_FLAG_READABLE;
  if (is_open_[destination_port])
    satisfiable_flags |= MOJO_WAIT_FLAG_WRITABLE;

  return satisfiable_flags;
}

}  // namespace system
}  // namespace mojo
