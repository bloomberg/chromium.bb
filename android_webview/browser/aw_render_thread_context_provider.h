// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_RENDER_THREAD_CONTEXT_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_RENDER_THREAD_CONTEXT_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"
#include "skia/ext/refptr.h"

namespace gfx {
class GLSurface;
}

namespace gpu {
class GLInProcessContext;
}

namespace android_webview {

class AwRenderThreadContextProvider : public cc::ContextProvider {
 public:
  static scoped_refptr<AwRenderThreadContextProvider> Create(
      scoped_refptr<gfx::GLSurface> surface,
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service);

 private:
  AwRenderThreadContextProvider(
      scoped_refptr<gfx::GLSurface> surface,
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service);
  ~AwRenderThreadContextProvider() override;

  // cc::ContextProvider:
  bool BindToCurrentThread() override;
  Capabilities ContextCapabilities() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  void SetupLock() override;
  base::Lock* GetLock() override;
  void VerifyContexts() override;
  void DeleteCachedResources() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;

  void OnLostContext();

  base::ThreadChecker main_thread_checker_;

  scoped_ptr<gpu::GLInProcessContext> context_;
  skia::RefPtr<class GrContext> gr_context_;

  cc::ContextProvider::Capabilities capabilities_;

  LostContextCallback lost_context_callback_;

  base::Lock context_lock_;

  DISALLOW_COPY_AND_ASSIGN(AwRenderThreadContextProvider);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_RENDER_THREAD_CONTEXT_PROVIDER_H_
