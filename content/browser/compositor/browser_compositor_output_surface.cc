// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_output_surface.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"

namespace content {

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    const scoped_refptr<cc::ContextProvider>& context_provider,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager)
    : OutputSurface(context_provider),
      vsync_manager_(vsync_manager),
      reflector_(nullptr) {
  Initialize();
}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    scoped_ptr<cc::SoftwareOutputDevice> software_device,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager)
    : OutputSurface(software_device.Pass()),
      vsync_manager_(vsync_manager),
      reflector_(nullptr) {
  Initialize();
}

BrowserCompositorOutputSurface::~BrowserCompositorOutputSurface() {
  if (reflector_)
    reflector_->DetachFromOutputSurface();
  DCHECK(!reflector_);
  if (!HasClient())
    return;
  vsync_manager_->RemoveObserver(this);
}

void BrowserCompositorOutputSurface::Initialize() {
  capabilities_.max_frames_pending = 1;
  capabilities_.adjust_deadline_for_parent = false;
}

bool BrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  if (!OutputSurface::BindToClient(client))
    return false;
  // Don't want vsync notifications until there is a client.
  vsync_manager_->AddObserver(this);
  return true;
}

void BrowserCompositorOutputSurface::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(HasClient());
  CommitVSyncParameters(timebase, interval);
}

void BrowserCompositorOutputSurface::OnUpdateVSyncParametersFromGpu(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(HasClient());
  vsync_manager_->UpdateVSyncParameters(timebase, interval);
}

void BrowserCompositorOutputSurface::SetReflector(ReflectorImpl* reflector) {
  reflector_ = reflector;
}

}  // namespace content
