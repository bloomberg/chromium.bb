// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CC_OUTPUT_SURFACE_MOJO_H_
#define MOJO_CC_OUTPUT_SURFACE_MOJO_H_

#include "cc/output/output_surface.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"

namespace mojo {

class OutputSurfaceMojoClient {
 public:
  virtual ~OutputSurfaceMojoClient() {}

  virtual void DidCreateSurface(cc::SurfaceId id) = 0;
};

class OutputSurfaceMojo : public cc::OutputSurface, public SurfaceClient {
 public:
  OutputSurfaceMojo(OutputSurfaceMojoClient* client,
                    const scoped_refptr<cc::ContextProvider>& context_provider,
                    SurfacePtr surface,
                    uint32_t id_namespace);

  // SurfaceClient implementation.
  virtual void ReturnResources(Array<ReturnedResourcePtr> resources) OVERRIDE;

  // cc::OutputSurface implementation.
  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE;
  virtual bool BindToClient(cc::OutputSurfaceClient* client) OVERRIDE;

 protected:
  virtual ~OutputSurfaceMojo();

 private:
  OutputSurfaceMojoClient* output_surface_mojo_client_;
  SurfacePtr surface_;
  cc::SurfaceIdAllocator id_allocator_;
  cc::SurfaceId surface_id_;
  gfx::Size surface_size_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurfaceMojo);
};
}

#endif  // MOJO_CC_OUTPUT_SURFACE_MOJO_H_
