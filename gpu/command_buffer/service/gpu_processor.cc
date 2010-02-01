// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace gpu {

GPUProcessor::GPUProcessor(CommandBuffer* command_buffer)
    : command_buffer_(command_buffer),
      commands_per_update_(100) {
  DCHECK(command_buffer);
  decoder_.reset(gles2::GLES2Decoder::Create());
  decoder_->set_engine(this);
}

GPUProcessor::GPUProcessor(CommandBuffer* command_buffer,
                           gles2::GLES2Decoder* decoder,
                           CommandParser* parser,
                           int commands_per_update)
    : command_buffer_(command_buffer),
      commands_per_update_(commands_per_update) {
  DCHECK(command_buffer);
  decoder_.reset(decoder);
  parser_.reset(parser);
}

GPUProcessor::~GPUProcessor() {
}

void GPUProcessor::ProcessCommands() {
  CommandBuffer::State state = command_buffer_->GetState();
  if (state.error != error::kNoError)
    return;

  if (decoder_.get()) {
    // TODO(apatrick): need to do more than this on failure.
    if (!decoder_->MakeCurrent())
      return;
  }

  parser_->set_put(state.put_offset);

  int commands_processed = 0;
  while (commands_processed < commands_per_update_ && !parser_->IsEmpty()) {
    error::Error error = parser_->ProcessCommand();
    if (error != error::kNoError) {
      command_buffer_->SetParseError(error);
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

bool GPUProcessor::SetGetOffset(int32 offset) {
  if (parser_->set_get(offset)) {
    command_buffer_->SetGetOffset(static_cast<int32>(parser_->get()));
    return true;
  }
  return false;
}

int32 GPUProcessor::GetGetOffset() {
  return parser_->get();
}

}  // namespace gpu
