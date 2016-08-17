// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/renderer_capabilities.h"

namespace cc {

RendererCapabilities::RendererCapabilities(ResourceFormat best_texture_format,
                                           int max_texture_size,
                                           bool using_shared_memory_resources)
    : best_texture_format(best_texture_format),
      max_texture_size(max_texture_size),
      using_shared_memory_resources(using_shared_memory_resources) {
}

RendererCapabilities::RendererCapabilities() = default;
RendererCapabilities::~RendererCapabilities() = default;

}  // namespace cc
