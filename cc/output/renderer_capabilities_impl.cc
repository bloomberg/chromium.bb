// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/renderer_capabilities_impl.h"

namespace cc {

RendererCapabilitiesImpl::RendererCapabilitiesImpl() = default;
RendererCapabilitiesImpl::~RendererCapabilitiesImpl() = default;

RendererCapabilities RendererCapabilitiesImpl::MainThreadCapabilities() const {
  return RendererCapabilities(best_texture_format, max_texture_size,
                              using_shared_memory_resources);
}

}  // namespace cc
