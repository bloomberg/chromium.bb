// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_H_
#define COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_H_

#include <memory>

#include "cc/output/output_surface.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/mus/surfaces/surfaces_context_provider.h"
#include "components/mus/surfaces/surfaces_context_provider_delegate.h"

namespace mus {

// An OutputSurface implementation that directly draws and
// swaps to an actual GL surface.
class DirectOutputSurface : public cc::OutputSurface,
                            public SurfacesContextProviderDelegate {
 public:
  explicit DirectOutputSurface(
      scoped_refptr<SurfacesContextProvider> context_provider,
      base::SingleThreadTaskRunner* task_runner);
  ~DirectOutputSurface() override;

  // cc::OutputSurface implementation
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  void SwapBuffers(cc::CompositorFrame* frame) override;

  // SurfacesContextProviderDelegate implementation
  void OnVSyncParametersUpdated(int64_t timebase, int64_t interval) override;

 private:
  std::unique_ptr<cc::SyntheticBeginFrameSource> synthetic_begin_frame_source_;
  base::WeakPtrFactory<DirectOutputSurface> weak_ptr_factory_;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_H_
