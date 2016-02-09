// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_DISPLAY_IMPL_H_
#define COMPONENTS_MUS_SURFACES_DISPLAY_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/surfaces/surfaces_context_provider.h"
#include "components/mus/surfaces/surfaces_context_provider_delegate.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class CopyOutputResult;
class Display;
class DisplayScheduler;
class SurfaceFactory;
class SyntheticBeginFrameSource;
}

namespace mus {

class DisplayDelegate;
class SurfacesState;

// A TopLevelDisplayClient manages the top level surface that is rendered into a
// provided AcceleratedWidget. Frames are submitted here. New frames are
// scheduled to be generated here based on VSync.
class TopLevelDisplayClient : public cc::DisplayClient,
                              public cc::SurfaceFactoryClient,
                              public SurfacesContextProviderDelegate {
 public:
  TopLevelDisplayClient(gfx::AcceleratedWidget widget,
                        const scoped_refptr<GpuState>& gpu_state,
                        const scoped_refptr<SurfacesState>& surfaces_state);
  ~TopLevelDisplayClient() override;

  void SubmitCompositorFrame(scoped_ptr<cc::CompositorFrame> frame,
                             const base::Closure& callback);
  const cc::SurfaceId& surface_id() const { return cc_id_; }

  void RequestCopyOfOutput(scoped_ptr<cc::CopyOutputRequest> output_request);

 private:
  // DisplayClient implementation.
  // TODO(rjkroege, fsamuel): This won't work correctly with multiple displays.
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void OutputSurfaceLost() override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;

  // SurfacesContextProviderDelegate:
  void OnVSyncParametersUpdated(int64_t timebase, int64_t interval) override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::SurfaceId surface_id,
                           cc::BeginFrameSource* begin_frame_source) override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<SurfacesState> surfaces_state_;
  cc::SurfaceFactory factory_;
  cc::SurfaceId cc_id_;

  gfx::Size last_submitted_frame_size_;
  scoped_ptr<cc::CompositorFrame> pending_frame_;

  scoped_ptr<cc::SyntheticBeginFrameSource> synthetic_frame_source_;
  scoped_ptr<cc::DisplayScheduler> scheduler_;
  scoped_ptr<cc::Display> display_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelDisplayClient);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_DISPLAY_IMPL_H_
