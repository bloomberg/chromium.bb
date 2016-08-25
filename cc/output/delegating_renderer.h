// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_DELEGATING_RENDERER_H_
#define CC_OUTPUT_DELEGATING_RENDERER_H_

#include <memory>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/output/compositor_frame.h"

namespace cc {
class OutputSurface;
class ResourceProvider;

class CC_EXPORT DelegatingRenderer {
 public:
  DelegatingRenderer(OutputSurface* output_surface,
                     ResourceProvider* resource_provider);
  ~DelegatingRenderer();

  void DrawFrame(RenderPassList* render_passes_in_draw_order);

  void SwapBuffers(CompositorFrameMetadata metadata);

 private:
  OutputSurface* const output_surface_;
  ResourceProvider* const resource_provider_;
  std::unique_ptr<DelegatedFrameData> delegated_frame_data_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_DELEGATING_RENDERER_H_
