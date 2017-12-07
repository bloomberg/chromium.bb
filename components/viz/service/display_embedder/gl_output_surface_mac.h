// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_MAC_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_MAC_H_

#include "components/viz/service/display_embedder/gl_output_surface.h"

namespace viz {

// TODO(ccameron): This should share most of its implementation with
// GLOutputSurfaceOzone.
class GLOutputSurfaceMac : public GLOutputSurface {
 public:
  GLOutputSurfaceMac(scoped_refptr<InProcessContextProvider> context_provider,
                     SyntheticBeginFrameSource* synthetic_begin_frame_source);
  ~GLOutputSurfaceMac() override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_MAC_H_
