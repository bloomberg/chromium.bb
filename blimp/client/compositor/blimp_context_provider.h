// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_COMPOSITOR_BLIMP_CONTEXT_PROVIDER_H_
#define BLIMP_CLIENT_COMPOSITOR_BLIMP_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "skia/ext/refptr.h"
#include "ui/gl/gl_surface.h"

namespace blimp {
namespace client {

// Helper class to provide a graphics context for the compositor.
class BlimpContextProvider : public cc::ContextProvider {
 public:
  static scoped_refptr<BlimpContextProvider> Create(
      gfx::AcceleratedWidget widget);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  void DetachFromThread() override;
  Capabilities ContextCapabilities() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  void SetupLock() override;
  base::Lock* GetLock() override;
  void DeleteCachedResources() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;

 protected:
  explicit BlimpContextProvider(gfx::AcceleratedWidget widget);
  ~BlimpContextProvider() override;

 private:
  void OnLostContext();

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  base::Lock context_lock_;
  scoped_ptr<gpu::GLInProcessContext> context_;
  skia::RefPtr<class GrContext> gr_context_;

  cc::ContextProvider::Capabilities capabilities_;

  LostContextCallback lost_context_callback_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContextProvider);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_COMPOSITOR_BLIMP_CONTEXT_PROVIDER_H_
