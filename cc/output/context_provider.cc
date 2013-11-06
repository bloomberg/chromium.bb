// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/context_provider.h"

#include <limits>

namespace cc {

ContextProvider::Capabilities::Capabilities()
    : discard_backbuffer(false),
      egl_image_external(false),
      fast_npot_mo8_textures(false),
      iosurface(false),
      map_image(false),
      map_sub(false),
      post_sub_buffer(false),
      set_visibility(false),
      shallow_flush(false),
      swapbuffers_complete_callback(false),
      texture_format_bgra8888(false),
      texture_format_etc1(false),
      texture_rectangle(false),
      texture_storage(false),
      texture_usage(false),
      discard_framebuffer(false),
      max_transfer_buffer_usage_bytes(std::numeric_limits<size_t>::max()) {}

}  // namespace cc
