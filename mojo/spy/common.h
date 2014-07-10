// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SPY_COMMON_H_
#define MOJO_SPY_COMMON_H_

#include <stdint.h>

namespace mojo {

#pragma pack(push, 1)

// Mojo message header structures. These are based off the Mojo spec.

enum {
  kMessageExpectsResponse = 1 << 0,
  kMessageIsResponse      = 1 << 1
};

struct MojoCommonHeader {
  uint32_t num_bytes;
  uint32_t num_fields;
};

struct MojoMessageHeader : public MojoCommonHeader {
  uint32_t name;
  uint32_t flags;
};

struct MojoRequestHeader : public MojoMessageHeader {
  uint64_t request_id;
};

struct MojoMessageData  {
  MojoRequestHeader header;
};

#pragma pack(pop)

}  // namespace mojo

#endif  // MOJO_SPY_COMMON_H_
