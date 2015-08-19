// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_IMPL_H_
#define COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "components/view_manager/surfaces/surfaces_context_provider.h"
#include "components/view_manager/surfaces/surfaces_context_provider_delegate.h"
#include "components/view_manager/surfaces/surfaces_state.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class Display;
class SurfaceFactory;
}

namespace gles2 {
class GpuState;
}

namespace surfaces {

class DisplayDelegate;
class SurfacesScheduler;
class SurfacesState;

// A TopLevelDisplayClient manages the top level surface that is rendered into a
// provided AcceleratedWidget. Frames are submitted here. New frames are
// scheduled to be generated here based on VSync.
class TopLevelDisplayClient
    : public cc::DisplayClient,
      public cc::SurfaceFactoryClient,
      public surfaces::SurfacesContextProviderDelegate {
 public:
  TopLevelDisplayClient(gfx::AcceleratedWidget widget,
                        const scoped_refptr<gles2::GpuState>& gpu_state,
                        const scoped_refptr<SurfacesState>& surfaces_state);
  ~TopLevelDisplayClient() override;

  void SubmitFrame(scoped_ptr<cc::CompositorFrame> frame,
                   const base::Closure& callback);

 private:
  // DisplayClient implementation.
  // TODO(rjkroege, fsamuel): This won't work correctly with multiple displays.
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void OutputSurfaceLost() override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;

  // SurfacesContextProviderDelegate:
  void OnContextCreated();
  void OnVSyncParametersUpdated(int64_t timebase, int64_t interval) override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  void Draw();

  scoped_refptr<gles2::GpuState> gpu_state_;
  scoped_refptr<SurfacesState> surfaces_state_;
  cc::SurfaceFactory factory_;
  cc::SurfaceId cc_id_;

  gfx::Size last_submitted_frame_size_;
  scoped_ptr<cc::CompositorFrame> pending_frame_;
  base::Closure pending_callback_;

  scoped_ptr<cc::Display> display_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelDisplayClient);
};

}  // namespace surfaces

#endif  // COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_IMPL_H_
