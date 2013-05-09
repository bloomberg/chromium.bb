// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_output_surface.h"

#include "base/logging.h"
#include "base/time.h"
#include "cc/output/output_surface_client.h"
#include "content/public/renderer/android/synchronous_compositor_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/transform.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

namespace content {
namespace {

// TODO(boliu): RenderThreadImpl should create in process contexts as well.
scoped_ptr<WebKit::WebGraphicsContext3D> CreateWebGraphicsContext3D() {
  WebKit::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  return scoped_ptr<WebKit::WebGraphicsContext3D>(
      WebGraphicsContext3DInProcessCommandBufferImpl
          ::CreateViewContext(attributes, NULL));
}
} // namespace

SynchronousCompositorOutputSurface::SynchronousCompositorOutputSurface(
    int32 routing_id)
    : cc::OutputSurface(CreateWebGraphicsContext3D()),
      compositor_client_(NULL),
      routing_id_(routing_id),
      vsync_enabled_(false),
      did_swap_buffer_(false) {
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

void SynchronousCompositorOutputSurface::Reshape(gfx::Size size) {
  // Intentional no-op.
}

void SynchronousCompositorOutputSurface::SendFrameToParentCompositor(
    cc::CompositorFrame* frame) {
  // Intentional no-op: see http://crbug.com/237006
}

void SynchronousCompositorOutputSurface::EnableVSyncNotification(
    bool enable_vsync) {
  DCHECK(CalledOnValidThread());
  vsync_enabled_ = enable_vsync;
  UpdateCompositorClientSettings();
}

void SynchronousCompositorOutputSurface::SwapBuffers(
    const cc::LatencyInfo& info) {
  context3d()->finish();
  did_swap_buffer_ = true;
}

void SynchronousCompositorOutputSurface::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  compositor_client_ = compositor_client;
  UpdateCompositorClientSettings();
}

bool SynchronousCompositorOutputSurface::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();  // TODO(joth): call through to OutputSurfaceClient
  return false;
}

bool SynchronousCompositorOutputSurface::DemandDrawHw(
      gfx::Size view_size,
      const gfx::Transform& transform,
      gfx::Rect damage_area) {
  DCHECK(CalledOnValidThread());
  DCHECK(client_);

  did_swap_buffer_ = false;

  // TODO(boliu): This assumes |transform| is identity and |damage_area| is the
  // whole view. Tracking bug to implement this: crbug.com/230463.
  client_->SetNeedsRedrawRect(damage_area);
  if (vsync_enabled_)
    client_->DidVSync(base::TimeTicks::Now());

  return did_swap_buffer_;
}

void SynchronousCompositorOutputSurface::UpdateCompositorClientSettings() {
  if (compositor_client_) {
    compositor_client_->SetContinuousInvalidate(vsync_enabled_);
  }
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
