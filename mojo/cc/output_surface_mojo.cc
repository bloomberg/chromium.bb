// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/cc/output_surface_mojo.h"

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/output_surface_client.h"
#include "components/mus/public/cpp/view_surface.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

namespace mojo {

OutputSurfaceMojo::OutputSurfaceMojo(
    const scoped_refptr<cc::ContextProvider>& context_provider,
    scoped_ptr<mus::ViewSurface> surface)
    : cc::OutputSurface(context_provider), surface_(surface.Pass()) {
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
}

OutputSurfaceMojo::~OutputSurfaceMojo() {
}

bool OutputSurfaceMojo::BindToClient(cc::OutputSurfaceClient* client) {
  surface_->BindToThread();
  surface_->set_client(this);
  return cc::OutputSurface::BindToClient(client);
}

void OutputSurfaceMojo::DetachFromClient() {
  surface_.reset();
  cc::OutputSurface::DetachFromClient();
}

void OutputSurfaceMojo::SwapBuffers(cc::CompositorFrame* frame) {
  // TODO(fsamuel, rjkroege): We should probably throttle compositor frames.
  client_->DidSwapBuffers();
  // OutputSurfaceMojo owns ViewSurface, and so if OutputSurfaceMojo is
  // destroyed then SubmitCompositorFrame's callback will never get called.
  // Thus, base::Unretained is safe here.
  surface_->SubmitCompositorFrame(
      mojo::CompositorFrame::From(*frame),
      base::Bind(&OutputSurfaceMojo::SwapBuffersComplete,
                 base::Unretained(this)));
}

void OutputSurfaceMojo::OnResourcesReturned(
    mus::ViewSurface* surface,
    mojo::Array<mojo::ReturnedResourcePtr> resources) {
  cc::CompositorFrameAck cfa;
  cfa.resources = resources.To<cc::ReturnedResourceArray>();
  ReclaimResources(&cfa);
}

void OutputSurfaceMojo::SwapBuffersComplete() {
  client_->DidSwapBuffersComplete();
}

}  // namespace mojo
