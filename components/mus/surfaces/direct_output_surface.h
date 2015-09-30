// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_H_
#define COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_H_

#include "cc/output/output_surface.h"

namespace mus {

// An OutputSurface implementation that directly draws and
// swaps to an actual GL surface.
class DirectOutputSurface : public cc::OutputSurface {
 public:
  explicit DirectOutputSurface(
      const scoped_refptr<cc::ContextProvider>& context_provider);
  ~DirectOutputSurface() override;

  // cc::OutputSurface implementation
  void SwapBuffers(cc::CompositorFrame* frame) override;

 private:
  base::WeakPtrFactory<DirectOutputSurface> weak_ptr_factory_;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_H_
