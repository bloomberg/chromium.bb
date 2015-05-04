// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_output_surface.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/compositor/browser_compositor_overlay_candidate_validator.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"

namespace content {

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    const scoped_refptr<cc::ContextProvider>& context_provider,
    scoped_ptr<BrowserCompositorOverlayCandidateValidator>
        overlay_candidate_validator)
    : OutputSurface(context_provider),
      reflector_(nullptr) {
  overlay_candidate_validator_ = overlay_candidate_validator.Pass();
  Initialize();
}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    scoped_ptr<cc::SoftwareOutputDevice> software_device)
    : OutputSurface(software_device.Pass()),
      reflector_(nullptr) {
  Initialize();
}

BrowserCompositorOutputSurface::~BrowserCompositorOutputSurface() {
  if (reflector_)
    reflector_->DetachFromOutputSurface();
  DCHECK(!reflector_);
}

void BrowserCompositorOutputSurface::Initialize() {
  capabilities_.max_frames_pending = 1;
  capabilities_.adjust_deadline_for_parent = false;
}

void BrowserCompositorOutputSurface::OnUpdateVSyncParametersFromGpu(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(HasClient());
  CommitVSyncParameters(timebase, interval);
}

void BrowserCompositorOutputSurface::SetReflector(ReflectorImpl* reflector) {
  // Software mirroring is done by doing a GL copy out of the framebuffer - if
  // we have overlays then that data will be missing.
  if (overlay_candidate_validator_) {
    overlay_candidate_validator_->SetSoftwareMirrorMode(reflector != nullptr);
  }
  reflector_ = reflector;
}

cc::OverlayCandidateValidator*
BrowserCompositorOutputSurface::GetOverlayCandidateValidator() const {
  return overlay_candidate_validator_.get();
}

}  // namespace content
