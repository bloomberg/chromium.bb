// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of the command parser.

#include "gpu/command_buffer/service/async_api_interface.h"

#include <stddef.h>

#include "base/logging.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"

namespace gpu {

// Decode multiple commands, and call the corresponding GL functions.
// NOTE: buffer is a pointer to the command buffer. As such, it could be
// changed by a (malicious) client at any time, so if validation has to happen,
// it should operate on a copy of them.
error::Error AsyncAPIInterface::DoCommands(unsigned int num_commands,
                                           const volatile void* buffer,
                                           int num_entries,
                                           int* entries_processed) {
  int commands_to_process = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process--) {
    CommandHeader header = CommandHeader::FromVolatile(cmd_data->value_header);
    if (header.size == 0) {
      DVLOG(1) << "Error: zero sized command in command buffer";
      return error::kInvalidSize;
    }

    if (static_cast<int>(header.size) + process_pos > num_entries) {
      DVLOG(1) << "Error: get offset out of bounds";
      return error::kOutOfBounds;
    }

    const unsigned int command = header.command;
    const unsigned int arg_count = header.size - 1;

    result = DoCommand(command, arg_count, cmd_data);

    if (result != error::kDeferCommandUntilLater) {
      process_pos += header.size;
      cmd_data += header.size;
    }
  }

  if (entries_processed)
    *entries_processed = process_pos;

  return result;
}

}  // namespace gpu
