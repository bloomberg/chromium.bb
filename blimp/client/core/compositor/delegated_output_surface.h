// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_COMPOSITOR_DELEGATED_OUTPUT_SURFACE_H_
#define BLIMP_CLIENT_CORE_COMPOSITOR_DELEGATED_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "blimp/client/core/compositor/blimp_output_surface.h"
#include "cc/output/output_surface.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace cc {
class ContextProvider;
}  // namespace cc

namespace blimp {
namespace client {

// This class is created on the main thread, but then becomes bound to the
// compositor thread and will be destroyed there (soon, crbug.com/640730).
class DelegatedOutputSurface : public cc::OutputSurface,
                               public BlimpOutputSurface {
 public:
  DelegatedOutputSurface(
      scoped_refptr<cc::ContextProvider> compositor_context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      base::WeakPtr<BlimpOutputSurfaceClient> client);

  ~DelegatedOutputSurface() override;

  // cc::OutputSurface implementation.
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  void SwapBuffers(cc::CompositorFrame frame) override;
  void DetachFromClient() override;

  // BlimpOutputSurface implementation.
  void ReclaimCompositorResources(
      const cc::ReturnedResourceArray& resources) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  base::WeakPtr<BlimpOutputSurfaceClient> client_;

  bool bound_to_client_;

  base::WeakPtrFactory<DelegatedOutputSurface> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedOutputSurface);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_COMPOSITOR_DELEGATED_OUTPUT_SURFACE_H_
