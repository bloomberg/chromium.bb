// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_RENDER_THREAD_CONTEXT_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_RENDER_THREAD_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrContext.h"

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
  gpu::Capabilities ContextCapabilities() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  void DeleteCachedResources() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;

  void OnLostContext();

  base::ThreadChecker main_thread_checker_;

  std::unique_ptr<gpu::GLInProcessContext> context_;
  sk_sp<class GrContext> gr_context_;

  LostContextCallback lost_context_callback_;

  DISALLOW_COPY_AND_ASSIGN(AwRenderThreadContextProvider);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_RENDER_THREAD_CONTEXT_PROVIDER_H_
