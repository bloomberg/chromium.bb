// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/cc/output_surface_mojo.h"

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface_client.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

namespace mojo {

OutputSurfaceMojo::OutputSurfaceMojo(
    OutputSurfaceMojoClient* client,
    const scoped_refptr<cc::ContextProvider>& context_provider,
    SurfacePtr surface)
    : cc::OutputSurface(context_provider),
      output_surface_mojo_client_(client),
      surface_(surface.Pass()),
      id_namespace_(0u),
      local_id_(0u) {
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
}

OutputSurfaceMojo::~OutputSurfaceMojo() {
}

void OutputSurfaceMojo::SetIdNamespace(uint32_t id_namespace) {
  id_namespace_ = id_namespace;
  if (local_id_) {
    cc::SurfaceId qualified_id(static_cast<uint64_t>(id_namespace_) << 32 |
                               local_id_);
    output_surface_mojo_client_->DidCreateSurface(qualified_id);
  }
}

bool OutputSurfaceMojo::BindToClient(cc::OutputSurfaceClient* client) {
  surface_->GetIdNamespace(
      base::Bind(&OutputSurfaceMojo::SetIdNamespace, base::Unretained(this)));
  return cc::OutputSurface::BindToClient(client);
}

void OutputSurfaceMojo::SwapBuffers(cc::CompositorFrame* frame) {
  gfx::Size frame_size =
      frame->delegated_frame_data->render_pass_list.back()->output_rect.size();
  if (frame_size != surface_size_) {
    if (local_id_ != 0u) {
      surface_->DestroySurface(local_id_);
    }
    local_id_++;
    surface_->CreateSurface(local_id_);
    if (id_namespace_) {
      cc::SurfaceId qualified_id(static_cast<uint64_t>(id_namespace_) << 32 |
                                 local_id_);
      output_surface_mojo_client_->DidCreateSurface(qualified_id);
    }
    surface_size_ = frame_size;
  }

  surface_->SubmitFrame(local_id_, Frame::From(*frame), mojo::Closure());

  client_->DidSwapBuffers();
  client_->DidSwapBuffersComplete();
}

}  // namespace mojo
