// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

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

bool GPUProcessor::Initialize(gfx::PluginWindowHandle handle) {
  DCHECK(handle);

  // Cannot reinitialize.
  if (decoder_->hwnd() != NULL)
    return false;

  // Map the ring buffer and create the parser.
  Buffer ring_buffer = command_buffer_->GetRingBuffer();
  if (ring_buffer.ptr) {
    parser_.reset(new CommandParser(ring_buffer.ptr,
                                    ring_buffer.size,
                                    0,
                                    ring_buffer.size,
                                    0,
                                    decoder_.get()));
  } else {
    parser_.reset(new CommandParser(NULL, 0, 0, 0, 0,
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
}  // namespace gpu
