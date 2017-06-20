// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_STREAM_H_
#define CHROME_COMMON_PROFILING_MEMLOG_STREAM_H_

#include <stdint.h>

#include "build/build_config.h"

namespace profiling {

static const uint32_t kStreamSignature = 0xF6103B71;

static const uint32_t kAllocPacketType = 0xA1A1A1A1;
static const uint32_t kFreePacketType = 0xFEFEFEFE;

#pragma pack(push, 1)
struct StreamHeader {
  uint32_t signature;  // kStreamSignature
};

struct AllocPacket {
  uint32_t op;  // = kAllocPacketType

  uint64_t time;
  uint64_t address;
  uint64_t size;

  uint32_t stack_len;
  // Immediately followed by |stack_len| more addresses.
};

struct FreePacket {
  uint32_t op;  // = kFreePacketType

  uint64_t time;
  uint64_t address;
};
#pragma pack(pop)

#if defined(OS_WIN)
// Prefix for pipe name for communicating between chrome processes and the
// memlog process. The pipe ID is appended to this to get the pipe name.
extern const wchar_t kWindowsPipePrefix[];
#endif  // OS_WIN

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_MEMLOG_STREAM_H_
