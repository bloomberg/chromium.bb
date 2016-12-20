// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "cc/output/output_surface_client.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "content/browser/compositor/reflector_impl.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    scoped_refptr<cc::ContextProvider> context_provider,
    const UpdateVSyncParametersCallback& update_vsync_parameters_callback,
    std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
        overlay_candidate_validator)
    : OutputSurface(std::move(context_provider)),
      update_vsync_parameters_callback_(update_vsync_parameters_callback),
      reflector_(nullptr) {
  overlay_candidate_validator_ = std::move(overlay_candidate_validator);
}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    std::unique_ptr<cc::SoftwareOutputDevice> software_device,
    const UpdateVSyncParametersCallback& update_vsync_parameters_callback)
    : OutputSurface(std::move(software_device)),
      update_vsync_parameters_callback_(update_vsync_parameters_callback),
      reflector_(nullptr) {}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    const scoped_refptr<cc::VulkanContextProvider>& vulkan_context_provider,
    const UpdateVSyncParametersCallback& update_vsync_parameters_callback)
    : OutputSurface(std::move(vulkan_context_provider)),
      update_vsync_parameters_callback_(update_vsync_parameters_callback),
      reflector_(nullptr) {}

BrowserCompositorOutputSurface::~BrowserCompositorOutputSurface() {
  if (reflector_)
    reflector_->DetachFromOutputSurface();
  DCHECK(!reflector_);
}

void BrowserCompositorOutputSurface::SetReflector(ReflectorImpl* reflector) {
  // Software mirroring is done by doing a GL copy out of the framebuffer - if
  // we have overlays then that data will be missing.
  if (overlay_candidate_validator_) {
    overlay_candidate_validator_->SetSoftwareMirrorMode(reflector != nullptr);
  }
  reflector_ = reflector;

  OnReflectorChanged();
}

void BrowserCompositorOutputSurface::OnReflectorChanged() {
}

cc::OverlayCandidateValidator*
BrowserCompositorOutputSurface::GetOverlayCandidateValidator() const {
  return overlay_candidate_validator_.get();
}

bool BrowserCompositorOutputSurface::HasExternalStencilTest() const {
  return false;
}

void BrowserCompositorOutputSurface::ApplyExternalStencil() {}

}  // namespace content
