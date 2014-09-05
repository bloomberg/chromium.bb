// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/channel_endpoint.h"

#include "mojo/system/channel.h"
#include "mojo/system/message_in_transit_queue.h"

namespace mojo {
namespace system {

ChannelEndpoint::ChannelEndpoint(MessagePipe* message_pipe, unsigned port)
    : state_(STATE_NORMAL), message_pipe_(message_pipe), port_(port) {
}

ChannelEndpoint::~ChannelEndpoint() {
}

}  // namespace system
}  // namespace mojo
