// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdkx.h>
#include "gpu/command_buffer/service/gpu_processor.h"
#include "gpu/command_buffer/service/x_utils.h"

using ::base::SharedMemory;

namespace gpu {

bool GPUProcessor::Initialize(gfx::PluginWindowHandle handle) {
  DCHECK(handle);

  // Cannot reinitialize.
  if (decoder_->window() != NULL)
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
  XWindowWrapper *window = new XWindowWrapper(GDK_DISPLAY(), handle);
  decoder_->set_window_wrapper(window);
  return decoder_->Initialize();
}

void GPUProcessor::Destroy() {
  // Destroy GAPI if window handle has not already become invalid.
  XWindowWrapper *window = decoder_->window();
  if (window) {
    decoder_->Destroy();
    decoder_->set_window_wrapper(NULL);
    delete window;
  }
}

}  // namespace gpu
