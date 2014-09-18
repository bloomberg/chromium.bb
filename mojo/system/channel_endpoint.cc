// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/channel_endpoint.h"

#include "base/logging.h"
#include "mojo/system/channel.h"
#include "mojo/system/message_pipe.h"

namespace mojo {
namespace system {

ChannelEndpoint::ChannelEndpoint(MessagePipe* message_pipe,
                                 unsigned port,
                                 Channel* channel,
                                 MessageInTransit::EndpointId local_id)
    : state_(STATE_NORMAL),
      message_pipe_(message_pipe),
      port_(port),
      channel_(channel),
      local_id_(local_id) {
  DCHECK(message_pipe_.get());
  DCHECK(port_ == 0 || port_ == 1);
  DCHECK(channel_);
  DCHECK_NE(local_id_, MessageInTransit::kInvalidEndpointId);
}

void ChannelEndpoint::DetachFromChannel() {
  base::AutoLock locker(lock_);
  DCHECK(channel_);
  channel_ = NULL;
}

ChannelEndpoint::~ChannelEndpoint() {
  DCHECK(!channel_);
}

}  // namespace system
}  // namespace mojo
