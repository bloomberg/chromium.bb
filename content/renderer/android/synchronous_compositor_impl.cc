// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_impl.h"

#include "base/message_loop.h"
#include "cc/input/input_handler.h"
#include "content/public/renderer/android/synchronous_compositor_client.h"
#include "content/public/renderer/content_renderer_client.h"

namespace content {

SynchronousCompositorImpl::SynchronousCompositorImpl(int32 routing_id)
    : routing_id_(routing_id), compositor_client_(NULL) {}

SynchronousCompositorImpl::~SynchronousCompositorImpl() {
}

scoped_ptr<cc::OutputSurface>
SynchronousCompositorImpl::CreateOutputSurface() {
  scoped_ptr<SynchronousCompositorOutputSurface> output_surface(
      new SynchronousCompositorOutputSurface(this));
  output_surface_ = output_surface.get();
  return output_surface.PassAs<cc::OutputSurface>();
}

bool SynchronousCompositorImpl::IsHwReady() {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->IsHwReady();
}

void SynchronousCompositorImpl::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  compositor_client_ = compositor_client;
}

bool SynchronousCompositorImpl::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->DemandDrawSw(canvas);
}

bool SynchronousCompositorImpl::DemandDrawHw(
      gfx::Size view_size,
      const gfx::Transform& transform,
      gfx::Rect damage_area) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->DemandDrawHw(view_size, transform, damage_area);
}

void SynchronousCompositorImpl::SetContinuousInvalidate(bool enable) {
  DCHECK(CalledOnValidThread());
  // compositor_client_ can be NULL during WebContents teardown.
  if (compositor_client_)
    compositor_client_->SetContinuousInvalidate(enable);
}

void SynchronousCompositorImpl::DidCreateSynchronousOutputSurface() {
  DCHECK(CalledOnValidThread());
  GetContentClient()->renderer()->DidCreateSynchronousCompositor(routing_id_,
                                                                 this);
}

void SynchronousCompositorImpl::DidDestroySynchronousOutputSurface() {
  DCHECK(CalledOnValidThread());
  output_surface_ = NULL;
  // compositor_client_ can be NULL during WebContents teardown.
  if (compositor_client_)
    compositor_client_->DidDestroyCompositor(this);
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorImpl() must only be used by
// embedders that supply their own compositor loop via
// OverrideCompositorMessageLoop().
bool SynchronousCompositorImpl::CalledOnValidThread() const {
  return base::MessageLoop::current() && (base::MessageLoop::current() ==
      GetContentClient()->renderer()->OverrideCompositorMessageLoop());
}

}  // namespace content
