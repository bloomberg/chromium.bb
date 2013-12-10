// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/context_provider.h"

#include <limits>

namespace cc {

ContextProvider::Capabilities::Capabilities()
    : egl_image_external(false),
      fast_npot_mo8_textures(false),
      iosurface(false),
      map_image(false),
      post_sub_buffer(false),
      texture_format_bgra8888(false),
      texture_format_etc1(false),
      texture_rectangle(false),
      texture_storage(false),
      texture_usage(false),
      discard_framebuffer(false),
      max_transfer_buffer_usage_bytes(std::numeric_limits<size_t>::max()) {}

ContextProvider::Capabilities::Capabilities(
    const gpu::Capabilities& gpu_capabilities)
    : egl_image_external(gpu_capabilities.egl_image_external),
      fast_npot_mo8_textures(gpu_capabilities.fast_npot_mo8_textures),
      iosurface(gpu_capabilities.iosurface),
      map_image(gpu_capabilities.map_image),
      post_sub_buffer(gpu_capabilities.post_sub_buffer),
      texture_format_bgra8888(gpu_capabilities.texture_format_bgra8888),
      texture_format_etc1(gpu_capabilities.texture_format_etc1),
      texture_rectangle(gpu_capabilities.texture_rectangle),
      texture_storage(gpu_capabilities.texture_storage),
      texture_usage(gpu_capabilities.texture_usage),
      discard_framebuffer(gpu_capabilities.discard_framebuffer),
      max_transfer_buffer_usage_bytes(std::numeric_limits<size_t>::max()) {}

}  // namespace cc
