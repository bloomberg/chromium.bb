// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder.h"

#include "ui/gl/buildflags.h"

#if BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/webgpu_decoder_impl.h"
#endif

namespace gpu {
namespace webgpu {

// static
WebGPUDecoder* WebGPUDecoder::Create(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    gles2::Outputter* outputter) {
#if BUILDFLAG(USE_DAWN)
  return CreateWebGPUDecoderImpl(client, command_buffer_service, outputter);
#else
  NOTREACHED();
  return nullptr;
#endif
}

WebGPUDecoder::WebGPUDecoder(DecoderClient* client,
                             CommandBufferServiceBase* command_buffer_service,
                             gles2::Outputter* outputter)
    : CommonDecoder(command_buffer_service) {}

WebGPUDecoder::~WebGPUDecoder() {}

}  // namespace webgpu
}  // namespace gpu
