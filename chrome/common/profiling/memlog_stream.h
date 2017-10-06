// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_STREAM_H_
#define CHROME_COMMON_PROFILING_MEMLOG_STREAM_H_

#include <stdint.h>

#include "build/build_config.h"

namespace profiling {

constexpr uint32_t kStreamSignature = 0xF6103B71;

constexpr uint32_t kAllocPacketType = 0xA1A1A1A1;
constexpr uint32_t kFreePacketType = 0xFEFEFEFE;
constexpr uint32_t kBarrierPacketType = 0xBABABABA;

constexpr uint32_t kMaxStackEntries = 256;
constexpr uint32_t kMaxContextLen = 256;

// This should count up from 0 so it can be used to index into an array.
enum class AllocatorType : uint32_t {
  kMalloc = 0,
  kPartitionAlloc = 1,
  kOilpan = 2,
  kCount  // Number of allocator types.
};

#pragma pack(push, 1)
struct StreamHeader {
  uint32_t signature = kStreamSignature;
};

struct AllocPacket {
  uint32_t op = kAllocPacketType;

  AllocatorType allocator;

  uint64_t address;
  uint64_t size;

  // Number of stack entries following this header.
  uint32_t stack_len;

  // Number of context bytes followint the stack;
  uint32_t context_byte_len;

  // Immediately followed by |stack_len| uint64_t addresses and
  // |context_byte_len| bytes of context (not null terminated).
};

struct FreePacket {
  uint32_t op = kFreePacketType;

  uint64_t address;
};

// A barrier packet is a way to synchronize with the sender to make sure all
// events are received up to a certain point. The barrier ID is just a number
// that can be used to uniquely identify these events.
struct BarrierPacket {
  const uint32_t op = kBarrierPacketType;

  uint32_t barrier_id;
};
#pragma pack(pop)

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_MEMLOG_STREAM_H_
