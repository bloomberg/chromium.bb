// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_CLIENT_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
struct CALayerParams;
struct PresentationFeedback;
}  // namespace gfx

namespace viz {

class VIZ_SERVICE_EXPORT OutputSurfaceClient {
 public:
  // A notification that the swap of the backbuffer to the hardware is complete
  // and is now visible to the user.
  virtual void DidReceiveSwapBuffersAck(uint64_t swap_id) = 0;

  // For surfaceless/ozone implementations to create damage for the next frame.
  virtual void SetNeedsRedrawRect(const gfx::Rect& damage_rect) = 0;

  // For synchronizing IOSurface use with the macOS WindowServer.
  virtual void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) = 0;

  // For displaying a swapped frame's contents on macOS.
  virtual void DidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) = 0;

  // A notification that the presentation feedback for a CompositorFrame with
  // given |swap_id|. See |gfx::PresentationFeedback| for detail.
  virtual void DidReceivePresentationFeedback(
      uint64_t swap_id,
      const gfx::PresentationFeedback& feedback) {}

 protected:
  virtual ~OutputSurfaceClient() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_CLIENT_H_
