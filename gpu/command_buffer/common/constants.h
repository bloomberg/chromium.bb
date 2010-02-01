// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_CONSTANTS_H_
#define GPU_COMMAND_BUFFER_COMMON_CONSTANTS_H_

#include "base/basictypes.h"

namespace gpu {

typedef int32 CommandBufferOffset;
const CommandBufferOffset kInvalidCommandBufferOffset = -1;

// This enum must stay in sync with NPDeviceContext3DError.
namespace error {
  enum Error {
    kNoError,
    kInvalidSize,
    kOutOfBounds,
    kUnknownCommand,
    kInvalidArguments,
    kGenericError
  };
}

// Invalid shared memory Id, returned by RegisterSharedMemory in case of
// failure.
const int32 kInvalidSharedMemoryId = -1;

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CONSTANTS_H_
