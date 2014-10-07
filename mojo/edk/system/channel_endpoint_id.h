// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHANNEL_ENDPOINT_ID_H_
#define MOJO_EDK_SYSTEM_CHANNEL_ENDPOINT_ID_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>

// TODO(vtl): Remove these once we can use |std::unordered_{map,set}|.
#include "base/containers/hash_tables.h"
#include "build/build_config.h"

namespace mojo {
namespace system {

struct ChannelEndpointId {
  ChannelEndpointId() : value(0) {}
  ChannelEndpointId(const ChannelEndpointId& other) : value(other.value) {}

  // Returns the local ID to use for the first message pipe endpoint on a
  // channel.
  static ChannelEndpointId GetBootstrap() {
    ChannelEndpointId rv;
    rv.value = 1;
    return rv;
  }

  bool operator==(const ChannelEndpointId& other) const {
    return value == other.value;
  }
  bool operator!=(const ChannelEndpointId& other) const {
    return !operator==(other);
  }
  // So that we can be used in |std::map|, etc.
  bool operator<(const ChannelEndpointId& other) const {
    return value < other.value;
  }

  bool is_valid() const { return !!value; }

  uint32_t value;

  // Copying and assignment allowed.
};
// This wrapper should add no overhead.
// TODO(vtl): Rewrite |sizeof(uint32_t)| as |sizeof(ChannelEndpointId::value)|
// once we have sufficient C++11 support.
static_assert(sizeof(ChannelEndpointId) == sizeof(uint32_t),
              "ChannelEndpointId has incorrect size");

// So logging macros and |DCHECK_EQ()|, etc. work.
inline std::ostream& operator<<(std::ostream& out,
                                const ChannelEndpointId& channel_endpoint_id) {
  return out << channel_endpoint_id.value;
}

}  // namespace system
}  // namespace mojo

namespace BASE_HASH_NAMESPACE {

#if defined(COMPILER_GCC)

template <>
struct hash<mojo::system::ChannelEndpointId> {
  size_t operator()(mojo::system::ChannelEndpointId channel_endpoint_id) const {
    return static_cast<size_t>(channel_endpoint_id.value);
  }
};

#elif defined(COMPILER_MSVC)

inline size_t hash_value(mojo::system::ChannelEndpointId channel_endpoint_id) {
  return static_cast<size_t>(channel_endpoint_id.value);
}
#endif

}  // namespace BASE_HASH_NAMESPACE

#endif  // MOJO_EDK_SYSTEM_CHANNEL_ENDPOINT_ID_H_
