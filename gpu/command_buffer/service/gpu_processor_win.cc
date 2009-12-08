// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace command_buffer {

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

bool GPUProcessor::Initialize(HWND handle) {
  DCHECK(handle);

  // Cannot reinitialize.
  if (decoder_->hwnd() != NULL)
    return false;

  // Map the ring buffer and create the parser.
  ::base::SharedMemory* ring_buffer = command_buffer_->GetRingBuffer();
  if (ring_buffer) {
    size_t size = ring_buffer->max_size();
    if (!ring_buffer->Map(size)) {
      return false;
    }

    void* ptr = ring_buffer->memory();
    parser_.reset(new command_buffer::CommandParser(ptr, size, 0, size, 0,
                                                    decoder_.get()));
  } else {
    parser_.reset(new command_buffer::CommandParser(NULL, 0, 0, 0, 0,
                                                    decoder_.get()));
  }

  // Initialize GAPI immediately if the window handle is valid.
  decoder_->set_hwnd(handle);
  return decoder_->Initialize();
}

void GPUProcessor::Destroy() {
  // Destroy GAPI if window handle has not already become invalid.
  if (decoder_->hwnd()) {
    decoder_->Destroy();
    decoder_->set_hwnd(NULL);
  }
}

bool GPUProcessor::SetWindow(HWND handle, int width, int height) {
  if (handle == NULL) {
    // Destroy GAPI when the window handle becomes invalid.
    Destroy();
    return true;
  } else {
    return Initialize(handle);
  }
}

}  // namespace command_buffer
