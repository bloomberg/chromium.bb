// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_output_surface.h"

#include "base/logging.h"
#include "cc/output/output_surface_client.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/public/renderer/android/synchronous_compositor_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace content {

SynchronousCompositorOutputSurface::SynchronousCompositorOutputSurface(
    int32 routing_id,
    WebGraphicsContext3DCommandBufferImpl* context)
    : cc::OutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D>(context)),
      compositor_client_(NULL),
      routing_id_(routing_id) {
  // WARNING: may be called on any thread.
}

SynchronousCompositorOutputSurface::~SynchronousCompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->DidDestroyCompositor(this);
}

bool SynchronousCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* surface_client) {
  DCHECK(CalledOnValidThread());
  if (!cc::OutputSurface::BindToClient(surface_client))
    return false;
  GetContentClient()->renderer()->DidCreateSynchronousCompositor(routing_id_,
                                                                 this);
  return true;
}

void SynchronousCompositorOutputSurface::SendFrameToParentCompositor(
    cc::CompositorFrame* frame) {
  // Intentional no-op: see http://crbug.com/237006
}

void SynchronousCompositorOutputSurface::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  compositor_client_ = compositor_client;
}

bool SynchronousCompositorOutputSurface::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();  // TODO(joth): call through to OutputSurfaceClient
  return false;
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorOutputSurface() must only be used by
// embedders that supply their own compositor loop via
// OverrideCompositorMessageLoop().
bool SynchronousCompositorOutputSurface::CalledOnValidThread() const {
  return base::MessageLoop::current() && (base::MessageLoop::current() ==
      GetContentClient()->renderer()->OverrideCompositorMessageLoop());
}

}  // namespace content
