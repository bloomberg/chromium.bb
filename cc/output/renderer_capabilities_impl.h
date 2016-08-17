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
  bool allow_partial_texture_updates = false;
  int max_texture_size = 0;
  bool using_shared_memory_resources = false;

  // Capabilities used on compositor thread only.
  bool using_partial_swap = false;
  // Whether it's valid to SwapBuffers with an empty rect. Trivially true when
  // |using_partial_swap|.
  bool allow_empty_swap = false;
  bool using_egl_image = false;
  bool using_image = false;
  bool using_discard_framebuffer = false;
  bool allow_rasterize_on_demand = false;
  int max_msaa_samples = 0;

  RendererCapabilities MainThreadCapabilities() const;
};

}  // namespace cc

#endif  // CC_OUTPUT_RENDERER_CAPABILITIES_IMPL_H_
