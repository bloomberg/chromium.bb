// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace gpu {

bool GPUProcessor::Initialize(gfx::PluginWindowHandle handle) {
  // At this level we do not need the PluginWindowHandle. It is only
  // needed at the CommandBufferStub level to identify which GPU
  // plugin instance is creating a new backing store in response to a
  // resize event.

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

  // Initialize GAPI.
  return decoder_->Initialize();
}

void GPUProcessor::Destroy() {
  decoder_->Destroy();
}
}  // namespace gpu

