// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_hardware.h"

#include "base/logging.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"

static double output_sample_rate = 0.0;
static double input_sample_rate = 0.0;
static size_t output_buffer_size = 0;

namespace audio_hardware {

double GetOutputSampleRate() {
  DCHECK(RenderThreadImpl::current() != NULL);

  if (!output_sample_rate) {
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_GetHardwareSampleRate(&output_sample_rate));
  }
  return output_sample_rate;
}

double GetInputSampleRate() {
  DCHECK(RenderThreadImpl::current() != NULL);

  if (!input_sample_rate) {
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_GetHardwareInputSampleRate(&input_sample_rate));
  }
  return input_sample_rate;
}

size_t GetOutputBufferSize() {
  DCHECK(RenderThreadImpl::current() != NULL);

  if (!output_buffer_size) {
    uint32 buffer_size = 0;
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_GetHardwareBufferSize(&buffer_size));
    output_buffer_size = buffer_size;
  }

  return output_buffer_size;
}

void ResetCache() {
  DCHECK(RenderThreadImpl::current() != NULL);

  output_sample_rate = 0.0;
  input_sample_rate = 0.0;
  output_buffer_size = 0;
}

}  // namespace audio_hardware
