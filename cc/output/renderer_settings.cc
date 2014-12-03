// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/renderer_settings.h"

#include <limits>

#include "base/logging.h"

namespace cc {

RendererSettings::RendererSettings()
    : allow_antialiasing(true),
      force_antialiasing(false),
      force_blending_with_shaders(false),
      partial_swap_enabled(false),
      finish_rendering_on_resize(false),
      should_clear_root_render_pass(true),
      refresh_rate(60.0),
      highp_threshold_min(0),
      use_rgba_4444_textures(false),
      texture_id_allocation_chunk_size(64) {
}

RendererSettings::~RendererSettings() {
}

}  // namespace cc
