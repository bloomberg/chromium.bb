// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_CONSTANTS_H_
#define GPU_COMMAND_BUFFER_COMMON_CONSTANTS_H_

#include "base/basictypes.h"

namespace gpu {

typedef int32 CommandBufferOffset;
const CommandBufferOffset kInvalidCommandBufferOffset = -1;

// Status of the command buffer service. It does not process commands
// (meaning: get will not change) unless in kParsing state.
namespace parser_status {
  enum ParserStatus {
    kNotConnected,  // The service is not connected - initial state.
    kNoBuffer,      // The service is connected but no buffer was set.
    kParsing,       // The service is connected, and parsing commands from the
                    // buffer.
    kParseError,    // Parsing stopped because a parse error was found.
  };
}

namespace parse_error {
  enum ParseError {
    kParseNoError,
    kParseInvalidSize,
    kParseOutOfBounds,
    kParseUnknownCommand,
    kParseInvalidArguments,
  };
}

// Invalid shared memory Id, returned by RegisterSharedMemory in case of
// failure.
const int32 kInvalidSharedMemoryId = -1;

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CONSTANTS_H_
