// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_COMPOSITOR_DISPLAY_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_COMPOSITOR_DISPLAY_PROVIDER_H_

#include <memory>

#include "gpu/ipc/common/surface_handle.h"

namespace cc {
class BeginFrameSource;
class Display;
class FrameSinkId;
}

namespace viz {

// Handles creating new cc::Displays and related classes for
// MojoFrameSinkManager.
class DisplayProvider {
 public:
  virtual ~DisplayProvider() {}

  // Creates a new cc::Display for |surface_handle| with |frame_sink_id|. Will
  // also create cc::BeginFrameSource and return it in |begin_frame_source|.
  virtual std::unique_ptr<cc::Display> CreateDisplay(
      const cc::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      std::unique_ptr<cc::BeginFrameSource>* begin_frame_source) = 0;
};

}  // namespace viz

#endif  //  COMPONENTS_VIZ_SERVICE_DISPLAY_COMPOSITOR_DISPLAY_PROVIDER_H_
