// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHANNEL_ENDPOINT_ID_H_
#define MOJO_EDK_SYSTEM_CHANNEL_ENDPOINT_ID_H_

#include <stdint.h>

namespace mojo {
namespace system {

typedef uint32_t ChannelEndpointId;

// Never a valid channel endpoint ID.
const ChannelEndpointId kInvalidChannelEndpointId = 0;

// The first message pipe endpoint on a channel will have this as its local ID.
const ChannelEndpointId kBootstrapChannelEndpointId = 1;

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHANNEL_ENDPOINT_ID_H_
