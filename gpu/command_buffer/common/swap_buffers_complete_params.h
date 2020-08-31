// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SWAP_BUFFERS_COMPLETE_PARAMS_H_
#define GPU_COMMAND_BUFFER_COMMON_SWAP_BUFFERS_COMPLETE_PARAMS_H_

#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/swap_result.h"

namespace gpu {

struct GPU_EXPORT SwapBuffersCompleteParams {
  SwapBuffersCompleteParams();
  SwapBuffersCompleteParams(SwapBuffersCompleteParams&& other);
  SwapBuffersCompleteParams(const SwapBuffersCompleteParams& other);
  SwapBuffersCompleteParams& operator=(SwapBuffersCompleteParams&& other);
  SwapBuffersCompleteParams& operator=(const SwapBuffersCompleteParams& other);
  ~SwapBuffersCompleteParams();

  gfx::SwapResponse swap_response;
  // Used only on macOS, for coordinating IOSurface reuse with the system
  // WindowServer.
  gpu::TextureInUseResponses texture_in_use_responses;
  // Used only on macOS, to allow the browser hosted NSWindow to display
  // content populated in the GPU process.
  gfx::CALayerParams ca_layer_params;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SWAP_BUFFERS_COMPLETE_PARAMS_H_
