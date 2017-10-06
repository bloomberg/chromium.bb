// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
#define CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3DProvider.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace ui {
class ContextProviderCommandBuffer;
}

namespace content {

class CONTENT_EXPORT WebGraphicsContext3DProviderImpl
    : public blink::WebGraphicsContext3DProvider {
 public:
  explicit WebGraphicsContext3DProviderImpl(
      scoped_refptr<ui::ContextProviderCommandBuffer> provider,
      bool software_rendering);
  ~WebGraphicsContext3DProviderImpl() override;

  // WebGraphicsContext3DProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  GrContext* GetGrContext() override;
  const gpu::Capabilities& GetCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  bool IsSoftwareRendering() const override;
  void SetLostContextCallback(const base::Closure&) override;
  void SetErrorMessageCallback(
      const base::Callback<void(const char*, int32_t)>&) override;
  void SignalQuery(uint32_t, const base::Closure&) override;

  ui::ContextProviderCommandBuffer* context_provider() const {
    return provider_.get();
  }

 private:
  scoped_refptr<ui::ContextProviderCommandBuffer> provider_;
  const bool software_rendering_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
