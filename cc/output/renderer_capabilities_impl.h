// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_RENDERER_CAPABILITIES_IMPL_H_
#define CC_OUTPUT_RENDERER_CAPABILITIES_IMPL_H_

#include "cc/output/renderer_capabilities.h"

namespace cc {

struct RendererCapabilitiesImpl {
  RendererCapabilitiesImpl();
  ~RendererCapabilitiesImpl();

  // Capabilities copied to main thread.
  ResourceFormat best_texture_format = RGBA_8888;
  int max_texture_size = 0;

  RendererCapabilities MainThreadCapabilities() const {
    return RendererCapabilities(best_texture_format, max_texture_size);
  }
};

}  // namespace cc

#endif  // CC_OUTPUT_RENDERER_CAPABILITIES_IMPL_H_
