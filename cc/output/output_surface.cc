// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"


namespace cc {

OutputSurface::OutputSurface(
    const scoped_refptr<ContextProvider>& context_provider,
    const scoped_refptr<ContextProvider>& worker_context_provider,
    scoped_ptr<SoftwareOutputDevice> software_device)
    : client_(NULL),
      context_provider_(context_provider),
      worker_context_provider_(worker_context_provider),
      software_device_(software_device.Pass()),
      device_scale_factor_(-1),
      external_stencil_test_enabled_(false),
      weak_ptr_factory_(this) {
}

OutputSurface::OutputSurface(
    const scoped_refptr<ContextProvider>& context_provider)
    : OutputSurface(context_provider, nullptr, nullptr) {
}

OutputSurface::OutputSurface(
    const scoped_refptr<ContextProvider>& context_provider,
    const scoped_refptr<ContextProvider>& worker_context_provider)
    : OutputSurface(context_provider, worker_context_provider, nullptr) {
}

OutputSurface::OutputSurface(scoped_ptr<SoftwareOutputDevice> software_device)
    : OutputSurface(nullptr, nullptr, software_device.Pass()) {
}

OutputSurface::OutputSurface(
    const scoped_refptr<ContextProvider>& context_provider,
    scoped_ptr<SoftwareOutputDevice> software_device)
    : OutputSurface(context_provider, nullptr, software_device.Pass()) {
}

void OutputSurface::CommitVSyncParameters(base::TimeTicks timebase,
                                          base::TimeDelta interval) {
  TRACE_EVENT2("cc",
               "OutputSurface::CommitVSyncParameters",
               "timebase",
               (timebase - base::TimeTicks()).InSecondsF(),
               "interval",
               interval.InSecondsF());
  client_->CommitVSyncParameters(timebase, interval);
}

// Forwarded to OutputSurfaceClient
void OutputSurface::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "OutputSurface::SetNeedsRedrawRect");
  client_->SetNeedsRedrawRect(damage_rect);
}

void OutputSurface::ReclaimResources(const CompositorFrameAck* ack) {
  client_->ReclaimResources(ack);
}

void OutputSurface::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "OutputSurface::DidLoseOutputSurface");
  client_->DidLoseOutputSurface();
}

void OutputSurface::SetExternalStencilTest(bool enabled) {
  external_stencil_test_enabled_ = enabled;
}

void OutputSurface::SetExternalDrawConstraints(
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority,
    bool resourceless_software_draw) {
  client_->SetExternalDrawConstraints(transform,
                                      viewport,
                                      clip,
                                      viewport_rect_for_tile_priority,
                                      transform_for_tile_priority,
                                      resourceless_software_draw);
}

OutputSurface::~OutputSurface() {
  ResetContext3d();
}

bool OutputSurface::HasExternalStencilTest() const {
  return external_stencil_test_enabled_;
}

bool OutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  client_ = client;
  bool success = true;

  if (context_provider_.get()) {
    success = context_provider_->BindToCurrentThread();
    if (success)
      SetUpContext3d();
  }

  if (success && worker_context_provider_.get()) {
    success = worker_context_provider_->BindToCurrentThread();
    if (success) {
      worker_context_provider_->SetupLock();
      // The destructor resets the context lost callback, so base::Unretained
      // is safe, as long as the worker threads stop using the context before
      // the output surface is destroyed.
      worker_context_provider_->SetLostContextCallback(base::Bind(
          &OutputSurface::DidLoseOutputSurface, base::Unretained(this)));
    }
  }

  if (!success)
    client_ = NULL;

  return success;
}

bool OutputSurface::InitializeAndSetContext3d(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider) {
  DCHECK(!context_provider_.get());
  DCHECK(context_provider.get());
  DCHECK(client_);

  bool success = context_provider->BindToCurrentThread();
  if (success) {
    context_provider_ = context_provider;
    SetUpContext3d();
  }
  if (success && worker_context_provider.get()) {
    success = worker_context_provider->BindToCurrentThread();
    if (success) {
      worker_context_provider_ = worker_context_provider;
      // The destructor resets the context lost callback, so base::Unretained
      // is safe, as long as the worker threads stop using the context before
      // the output surface is destroyed.
      worker_context_provider_->SetLostContextCallback(base::Bind(
          &OutputSurface::DidLoseOutputSurface, base::Unretained(this)));
    }
  }

  if (!success)
    ResetContext3d();
  else
    client_->DeferredInitialize();

  return success;
}

void OutputSurface::ReleaseGL() {
  DCHECK(client_);
  DCHECK(context_provider_.get());
  client_->ReleaseGL();
  DCHECK(!context_provider_.get());
}

void OutputSurface::SetUpContext3d() {
  DCHECK(context_provider_.get());
  DCHECK(client_);

  context_provider_->SetLostContextCallback(
      base::Bind(&OutputSurface::DidLoseOutputSurface,
                 base::Unretained(this)));
  context_provider_->SetMemoryPolicyChangedCallback(
      base::Bind(&OutputSurface::SetMemoryPolicy,
                 base::Unretained(this)));
}

void OutputSurface::ReleaseContextProvider() {
  DCHECK(client_);
  DCHECK(context_provider_.get());
  ResetContext3d();
}

void OutputSurface::ResetContext3d() {
  if (context_provider_.get()) {
    context_provider_->SetLostContextCallback(
        ContextProvider::LostContextCallback());
    context_provider_->SetMemoryPolicyChangedCallback(
        ContextProvider::MemoryPolicyChangedCallback());
  }
  if (worker_context_provider_.get()) {
    worker_context_provider_->SetLostContextCallback(
        ContextProvider::LostContextCallback());
  }
  context_provider_ = NULL;
  worker_context_provider_ = NULL;
}

void OutputSurface::EnsureBackbuffer() {
  if (software_device_)
    software_device_->EnsureBackbuffer();
}

void OutputSurface::DiscardBackbuffer() {
  if (context_provider_.get())
    context_provider_->ContextGL()->DiscardBackbufferCHROMIUM();
  if (software_device_)
    software_device_->DiscardBackbuffer();
}

void OutputSurface::Reshape(const gfx::Size& size, float scale_factor) {
  if (size == surface_size_ && scale_factor == device_scale_factor_)
    return;

  surface_size_ = size;
  device_scale_factor_ = scale_factor;
  if (context_provider_.get()) {
    context_provider_->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), scale_factor);
  }
  if (software_device_)
    software_device_->Resize(size, scale_factor);
}

gfx::Size OutputSurface::SurfaceSize() const {
  return surface_size_;
}

void OutputSurface::BindFramebuffer() {
  DCHECK(context_provider_.get());
  context_provider_->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OutputSurface::PostSwapBuffersComplete() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&OutputSurface::OnSwapBuffersComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

// We don't post tasks bound to the client directly since they might run
// after the OutputSurface has been destroyed.
void OutputSurface::OnSwapBuffersComplete() {
  client_->DidSwapBuffersComplete();
}

void OutputSurface::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  TRACE_EVENT1("cc", "OutputSurface::SetMemoryPolicy",
               "bytes_limit_when_visible", policy.bytes_limit_when_visible);
  // Just ignore the memory manager when it says to set the limit to zero
  // bytes. This will happen when the memory manager thinks that the renderer
  // is not visible (which the renderer knows better).
  if (policy.bytes_limit_when_visible)
    client_->SetMemoryPolicy(policy);
}

}  // namespace cc
