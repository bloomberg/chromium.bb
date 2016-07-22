// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_output_surface.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "cc/output/output_surface_client.h"
#include "cc/resources/returned_resource.h"
#include "cc/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

FakeOutputSurface::FakeOutputSurface(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    bool delegated_rendering)
    : FakeOutputSurface(std::move(context_provider),
                        std::move(worker_context_provider),
                        nullptr,
                        delegated_rendering) {}

FakeOutputSurface::FakeOutputSurface(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    std::unique_ptr<SoftwareOutputDevice> software_device,
    bool delegated_rendering)
    : OutputSurface(std::move(context_provider),
                    std::move(worker_context_provider),
                    std::move(software_device)) {
  capabilities_.delegated_rendering = delegated_rendering;
}

FakeOutputSurface::~FakeOutputSurface() = default;

void FakeOutputSurface::SwapBuffers(CompositorFrame frame) {
  ReturnResourcesHeldByParent();

  last_sent_frame_.reset(new CompositorFrame(std::move(frame)));
  ++num_sent_frames_;

  if (last_sent_frame_->delegated_frame_data) {
    auto* frame_data = last_sent_frame_->delegated_frame_data.get();
    last_swap_rect_ = frame_data->render_pass_list.back()->damage_rect;
    last_swap_rect_valid_ = true;
  } else if (context_provider()) {
    last_swap_rect_ = last_sent_frame_->gl_frame_data->sub_buffer_rect;
    last_swap_rect_valid_ = true;
  } else {
    // Unknown for direct software frames.
    last_swap_rect_ = gfx::Rect();
    last_swap_rect_valid_ = false;
  }

  if (last_sent_frame_->delegated_frame_data || !context_provider()) {
    if (last_sent_frame_->delegated_frame_data) {
      auto* frame_data = last_sent_frame_->delegated_frame_data.get();
      resources_held_by_parent_.insert(resources_held_by_parent_.end(),
                                       frame_data->resource_list.begin(),
                                       frame_data->resource_list.end());
    }
  }

  PostSwapBuffersComplete();
}

void FakeOutputSurface::BindFramebuffer() {
  if (framebuffer_) {
    context_provider_->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER,
                                                    framebuffer_);
  } else {
    OutputSurface::BindFramebuffer();
  }
}

uint32_t FakeOutputSurface::GetFramebufferCopyTextureFormat() {
  if (framebuffer_)
    return framebuffer_format_;
  else
    return GL_RGB;
}

bool FakeOutputSurface::BindToClient(OutputSurfaceClient* client) {
  if (OutputSurface::BindToClient(client)) {
    client_ = client;
    if (memory_policy_to_set_at_bind_) {
      client_->SetMemoryPolicy(*memory_policy_to_set_at_bind_.get());
      memory_policy_to_set_at_bind_ = nullptr;
    }
    return true;
  } else {
    return false;
  }
}

void FakeOutputSurface::DetachFromClient() {
  ReturnResourcesHeldByParent();
  OutputSurface::DetachFromClient();
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

void FakeOutputSurface::SetMemoryPolicyToSetAtBind(
    std::unique_ptr<ManagedMemoryPolicy> memory_policy_to_set_at_bind) {
  memory_policy_to_set_at_bind_.swap(memory_policy_to_set_at_bind);
}

void FakeOutputSurface::ReturnResourcesHeldByParent() {
  // Check |delegated_frame_data| because we shouldn't reclaim resources
  // for the Display which does not swap delegated frames.
  if (last_sent_frame_ && last_sent_frame_->delegated_frame_data) {
    // Return the last frame's resources immediately.
    ReturnedResourceArray resources;
    for (const auto& resource : resources_held_by_parent_)
      resources.push_back(resource.ToReturnedResource());
    resources_held_by_parent_.clear();
    client_->ReclaimResources(resources);
  }
}

}  // namespace cc
