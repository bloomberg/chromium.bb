// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_output_surface.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/output_surface_client.h"
#include "cc/resources/returned_resource.h"
#include "cc/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

FakeOutputSurface::FakeOutputSurface(
    scoped_refptr<ContextProvider> context_provider)
    : OutputSurface(std::move(context_provider)), weak_ptr_factory_(this) {
  DCHECK(OutputSurface::context_provider());
}

FakeOutputSurface::FakeOutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : OutputSurface(std::move(software_device)), weak_ptr_factory_(this) {
  DCHECK(OutputSurface::software_device());
}

FakeOutputSurface::~FakeOutputSurface() = default;

void FakeOutputSurface::Reshape(const gfx::Size& size,
                                float device_scale_factor,
                                const gfx::ColorSpace& color_space,
                                bool has_alpha,
                                bool use_stencil) {
  if (context_provider()) {
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor, has_alpha);
  } else {
    software_device()->Resize(size, device_scale_factor);
  }
  last_reshape_color_space_ = color_space;
}

void FakeOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  last_sent_frame_.reset(new OutputSurfaceFrame(std::move(frame)));
  ++num_sent_frames_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FakeOutputSurface::SwapBuffersAck,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FakeOutputSurface::SwapBuffersAck() {
  client_->DidReceiveSwapBuffersAck();
}

void FakeOutputSurface::BindFramebuffer() {
  context_provider_->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

void FakeOutputSurface::SetDrawRectangle(const gfx::Rect& rect) {
  last_set_draw_rectangle_ = rect;
}

uint32_t FakeOutputSurface::GetFramebufferCopyTextureFormat() {
  if (framebuffer_)
    return framebuffer_format_;
  else
    return GL_RGB;
}

void FakeOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

bool FakeOutputSurface::HasExternalStencilTest() const {
  return has_external_stencil_test_;
}

bool FakeOutputSurface::SurfaceIsSuspendForRecycle() const {
  return suspended_for_recycle_;
}

OverlayCandidateValidator* FakeOutputSurface::GetOverlayCandidateValidator()
    const {
  return overlay_candidate_validator_;
}

bool FakeOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned FakeOutputSurface::GetOverlayTextureId() const {
  return 0;
}

}  // namespace cc
