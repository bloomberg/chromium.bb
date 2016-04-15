// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_CONTEXT_PROVIDER_IN_PROCESS_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_CONTEXT_PROVIDER_IN_PROCESS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/blink/context_provider_web_context.h"

namespace blink { class WebGraphicsContext3D; }

namespace gpu_blink {
class WebGraphicsContext3DInProcessCommandBufferImpl;
}

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace content {

class ContextProviderInProcess
    : NON_EXPORTED_BASE(public cc_blink::ContextProviderWebContext) {
 public:
  static scoped_refptr<ContextProviderInProcess> Create(
      std::unique_ptr<gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl>
          context3d,
      const std::string& debug_name);

 private:
  ContextProviderInProcess(
      std::unique_ptr<gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl>
          context3d,
      const std::string& debug_name);
  ~ContextProviderInProcess() override;

  // cc_blink::ContextProviderWebContext:
  blink::WebGraphicsContext3D* WebContext3D() override;

  // cc::ContextProvider:
  bool BindToCurrentThread() override;
  void DetachFromThread() override;
  Capabilities ContextCapabilities() override;
  ::gpu::gles2::GLES2Interface* ContextGL() override;
  ::gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  void SetupLock() override;
  base::Lock* GetLock() override;
  void DeleteCachedResources() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;

  void OnLostContext();
  void InitializeCapabilities();

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  std::unique_ptr<gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl>
      context3d_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;

  LostContextCallback lost_context_callback_;

  base::Lock context_lock_;
  std::string debug_name_;
  class LostContextCallbackProxy;
  std::unique_ptr<LostContextCallbackProxy> lost_context_callback_proxy_;

  cc::ContextProvider::Capabilities capabilities_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderInProcess);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_CONTEXT_PROVIDER_IN_PROCESS_H_
