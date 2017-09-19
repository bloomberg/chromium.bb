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

namespace viz {

class VIZ_SERVICE_EXPORT OutputSurfaceClient {
 public:
  // A notification that the swap of the backbuffer to the hardware is complete
  // and is now visible to the user.
  virtual void DidReceiveSwapBuffersAck() = 0;

  // For surfaceless/ozone implementations to create damage for the next frame.
  virtual void SetNeedsRedrawRect(const gfx::Rect& damage_rect) = 0;

  // For overlays.
  virtual void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) = 0;

 protected:
  virtual ~OutputSurfaceClient() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_CLIENT_H_
