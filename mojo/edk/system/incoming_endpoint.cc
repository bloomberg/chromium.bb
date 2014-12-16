// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/incoming_endpoint.h"

#include "base/logging.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/message_in_transit.h"
#include "mojo/edk/system/message_pipe.h"

namespace mojo {
namespace system {

IncomingEndpoint::IncomingEndpoint() {
}

scoped_refptr<ChannelEndpoint> IncomingEndpoint::Init() {
  endpoint_ = new ChannelEndpoint(this, 0);
  return endpoint_;
}

scoped_refptr<MessagePipe> IncomingEndpoint::ConvertToMessagePipe() {
  base::AutoLock locker(lock_);
  scoped_refptr<MessagePipe> message_pipe(
      MessagePipe::CreateLocalProxyFromExisting(&message_queue_,
                                                endpoint_.get()));
  DCHECK(message_queue_.IsEmpty());
  endpoint_ = nullptr;
  return message_pipe;
}

void IncomingEndpoint::Close() {
  base::AutoLock locker(lock_);
  if (endpoint_) {
    endpoint_->DetachFromClient();
    endpoint_ = nullptr;
  }
}

bool IncomingEndpoint::OnReadMessage(unsigned /*port*/,
                                     MessageInTransit* message) {
  base::AutoLock locker(lock_);
  if (!endpoint_)
    return false;

  message_queue_.AddMessage(make_scoped_ptr(message));
  return true;
}

void IncomingEndpoint::OnDetachFromChannel(unsigned /*port*/) {
  Close();
}

IncomingEndpoint::~IncomingEndpoint() {
}

}  // namespace system
}  // namespace mojo
