// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_endpoint_id.h"

#include "base/compiler_specific.h"

namespace mojo {
namespace system {

STATIC_CONST_MEMBER_DEFINITION const uint32_t
    ChannelEndpointId::kRemotelyAllocatedFlag;
STATIC_CONST_MEMBER_DEFINITION const uint32_t
    ChannelEndpointId::kLocallyAllocatedMask;

ChannelEndpointId LocalChannelEndpointIdGenerator::GetNext() {
  ChannelEndpointId rv = next_channel_endpoint_id_;
  next_channel_endpoint_id_.value_ = (next_channel_endpoint_id_.value_ + 1) &
                                     ChannelEndpointId::kLocallyAllocatedMask;
  // Skip over the invalid value, in case we wrap.
  if (!next_channel_endpoint_id_.is_valid())
    next_channel_endpoint_id_.value_++;
  return rv;
}

}  // namespace system
}  // namespace mojo
