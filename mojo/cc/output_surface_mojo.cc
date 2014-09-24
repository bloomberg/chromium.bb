// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/cc/output_surface_mojo.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface_client.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"

namespace mojo {

OutputSurfaceMojo::OutputSurfaceMojo(
    OutputSurfaceMojoClient* client,
    const scoped_refptr<cc::ContextProvider>& context_provider,
    SurfacePtr surface,
    uint32_t id_namespace)
    : cc::OutputSurface(context_provider),
      output_surface_mojo_client_(client),
      surface_(surface.Pass()),
      id_allocator_(id_namespace) {
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
}

OutputSurfaceMojo::~OutputSurfaceMojo() {
}

void OutputSurfaceMojo::ReturnResources(Array<ReturnedResourcePtr> resources) {
}

bool OutputSurfaceMojo::BindToClient(cc::OutputSurfaceClient* client) {
  surface_.set_client(this);
  return cc::OutputSurface::BindToClient(client);
}

void OutputSurfaceMojo::SwapBuffers(cc::CompositorFrame* frame) {
  gfx::Size frame_size =
      frame->delegated_frame_data->render_pass_list.back()->output_rect.size();
  if (frame_size != surface_size_) {
    if (!surface_id_.is_null()) {
      surface_->DestroySurface(SurfaceId::From(surface_id_));
    }
    surface_id_ = id_allocator_.GenerateId();
    surface_->CreateSurface(SurfaceId::From(surface_id_),
                            Size::From(frame_size));
    output_surface_mojo_client_->DidCreateSurface(surface_id_);
    surface_size_ = frame_size;
  }

  surface_->SubmitFrame(SurfaceId::From(surface_id_), Frame::From(*frame));

  client_->DidSwapBuffers();
  client_->DidSwapBuffersComplete();
}

}  // namespace mojo
