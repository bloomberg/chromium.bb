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
    gpu::parse_error::ParseError parse_error =
        parser_->ProcessCommand();
    switch (parse_error) {
      case gpu::parse_error::kParseUnknownCommand:
      case gpu::parse_error::kParseInvalidArguments:
        command_buffer_->SetParseError(parse_error);
        break;

      case gpu::parse_error::kParseInvalidSize:
      case gpu::parse_error::kParseOutOfBounds:
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

void *GPUProcessor::GetSharedMemoryAddress(int32 shm_id) {
  ::base::SharedMemory* shared_memory =
      command_buffer_->GetTransferBuffer(shm_id);
  if (!shared_memory)
    return NULL;

  if (!shared_memory->memory()) {
    if (!shared_memory->Map(shared_memory->max_size()))
      return NULL;
  }

  return shared_memory->memory();
}

// TODO(apatrick): Consolidate this with the above and return both the address
// and size.
size_t GPUProcessor::GetSharedMemorySize(int32 shm_id) {
  ::base::SharedMemory* shared_memory =
      command_buffer_->GetTransferBuffer(shm_id);
  if (!shared_memory)
    return 0;

  return shared_memory->max_size();
}

void GPUProcessor::set_token(int32 token) {
  command_buffer_->SetToken(token);
}

}  // namespace gpu
