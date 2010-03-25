// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace gpu {

bool GPUProcessor::Initialize(gfx::PluginWindowHandle handle,
                              GPUProcessor* parent,
                              const gfx::Size& size,
                              uint32 parent_texture_id) {
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
  gles2::GLES2Decoder* parent_decoder = parent ? parent->decoder_.get() : NULL;
  if (!decoder_->Initialize(parent_decoder,
                            size,
                            parent_texture_id)) {
    Destroy();
    return false;
  }

  return true;
}

void GPUProcessor::Destroy() {
  // Destroy decoder if initialized.
  if (decoder_.get()) {
    decoder_->Destroy();
    decoder_.reset();
  }

  parser_.reset();
}
}  // namespace gpu

