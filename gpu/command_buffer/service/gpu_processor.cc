// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace gpu {

GPUProcessor::~GPUProcessor() {
}

void GPUProcessor::ProcessCommands() {
  if (command_buffer_->GetErrorStatus())
    return;

  parser_->set_put(command_buffer_->GetPutOffset());

  int commands_processed = 0;
  while (commands_processed < commands_per_update_ && !parser_->IsEmpty()) {
    parse_error::ParseError parse_error = parser_->ProcessCommand();
    switch (parse_error) {
      case parse_error::kParseUnknownCommand:
      case parse_error::kParseInvalidArguments:
        command_buffer_->SetParseError(parse_error);
        break;

      case parse_error::kParseInvalidSize:
      case parse_error::kParseOutOfBounds:
        command_buffer_->SetParseError(parse_error);
        command_buffer_->RaiseErrorStatus();
        return;
    }

    ++commands_processed;
  }

  command_buffer_->SetGetOffset(static_cast<int32>(parser_->get()));

  if (!parser_->IsEmpty()) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &GPUProcessor::ProcessCommands));
  }
}

Buffer GPUProcessor::GetSharedMemoryBuffer(int32 shm_id) {
  return command_buffer_->GetTransferBuffer(shm_id);
}

void GPUProcessor::set_token(int32 token) {
  command_buffer_->SetToken(token);
}

}  // namespace gpu
