// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_COMPOSITOR_BLIMP_OUTPUT_SURFACE_H_
#define BLIMP_CLIENT_COMPOSITOR_BLIMP_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/output/output_surface.h"

namespace blimp {
namespace client {

// Minimal implementation of cc::OutputSurface.
class BlimpOutputSurface : public cc::OutputSurface {
 public:
  explicit BlimpOutputSurface(
      const scoped_refptr<cc::ContextProvider>& context_provider);
  ~BlimpOutputSurface() override;

  // OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpOutputSurface);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_COMPOSITOR_BLIMP_OUTPUT_SURFACE_H_
