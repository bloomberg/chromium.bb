// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe_endpoint.h"

namespace mojo {
namespace system {

namespace {

unsigned DestinationPortFromSourcePort(unsigned port) {
  DCHECK(port == 0 || port == 1);
  return port ^ 1;
}

}  // namespace

MessagePipe::MessagePipe(scoped_ptr<MessagePipeEndpoint> endpoint_0,
                         scoped_ptr<MessagePipeEndpoint> endpoint_1) {
  endpoints_[0].reset(endpoint_0.release());
  endpoints_[1].reset(endpoint_1.release());
}

MessagePipe::MessagePipe() {
  endpoints_[0].reset(new LocalMessagePipeEndpoint());
  endpoints_[1].reset(new LocalMessagePipeEndpoint());
}

void MessagePipe::CancelAllWaiters(unsigned port) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());
  endpoints_[port]->CancelAllWaiters();
}

void MessagePipe::Close(unsigned port) {
  DCHECK(port == 0 || port == 1);

  unsigned destination_port = DestinationPortFromSourcePort(port);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  endpoints_[port]->Close();
  if (endpoints_[destination_port].get())
    endpoints_[destination_port]->OnPeerClose();

  endpoints_[port].reset();
}

// TODO(vtl): Handle flags.
MojoResult MessagePipe::WriteMessage(
    unsigned port,
    const void* bytes, uint32_t num_bytes,
    const MojoHandle* handles, uint32_t num_handles,
    MojoWriteMessageFlags flags) {
  DCHECK(port == 0 || port == 1);

  unsigned destination_port = DestinationPortFromSourcePort(port);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  // The destination port need not be open, unlike the source port.
  if (!endpoints_[destination_port].get())
    return MOJO_RESULT_FAILED_PRECONDITION;

  return endpoints_[destination_port]->EnqueueMessage(bytes, num_bytes,
                                                      handles, num_handles,
                                                      flags);
}

MojoResult MessagePipe::ReadMessage(unsigned port,
                                    void* bytes, uint32_t* num_bytes,
                                    MojoHandle* handles, uint32_t* num_handles,
                                    MojoReadMessageFlags flags) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  return endpoints_[port]->ReadMessage(bytes, num_bytes,
                                       handles, num_handles,
                                       flags);
}

MojoResult MessagePipe::AddWaiter(unsigned port,
                                  Waiter* waiter,
                                  MojoWaitFlags flags,
                                  MojoResult wake_result) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  return endpoints_[port]->AddWaiter(waiter, flags, wake_result);
}

void MessagePipe::RemoveWaiter(unsigned port, Waiter* waiter) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  endpoints_[port]->RemoveWaiter(waiter);
}

MessagePipe::~MessagePipe() {
  // Owned by the dispatchers. The owning dispatchers should only release us via
  // their |Close()| method, which should inform us of being closed via our
  // |Close()|. Thus these should already be null.
  DCHECK(!endpoints_[0].get());
  DCHECK(!endpoints_[1].get());
}

}  // namespace system
}  // namespace mojo
