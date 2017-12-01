// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webgraphicscontext3d_provider_impl.h"

#include "components/viz/common/gl_helper.h"
#include "gpu/command_buffer/client/context_support.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

WebGraphicsContext3DProviderImpl::WebGraphicsContext3DProviderImpl(
    scoped_refptr<ui::ContextProviderCommandBuffer> provider,
    bool software_rendering)
    : provider_(std::move(provider)), software_rendering_(software_rendering) {
  provider_->AddObserver(this);
}

WebGraphicsContext3DProviderImpl::~WebGraphicsContext3DProviderImpl() {
  provider_->RemoveObserver(this);
}

bool WebGraphicsContext3DProviderImpl::BindToCurrentThread() {
  // TODO(danakj): Could plumb this result out to the caller so they know to
  // retry or not, if any client cared to know if it should retry or not.
  return provider_->BindToCurrentThread() == gpu::ContextResult::kSuccess;
}

gpu::gles2::GLES2Interface* WebGraphicsContext3DProviderImpl::ContextGL() {
  return provider_->ContextGL();
}

GrContext* WebGraphicsContext3DProviderImpl::GetGrContext() {
  return provider_->GrContext();
}

void WebGraphicsContext3DProviderImpl::InvalidateGrContext(uint32_t state) {
  return provider_->InvalidateGrContext(state);
}

const gpu::Capabilities& WebGraphicsContext3DProviderImpl::GetCapabilities()
    const {
  return provider_->ContextCapabilities();
}

const gpu::GpuFeatureInfo& WebGraphicsContext3DProviderImpl::GetGpuFeatureInfo()
    const {
  return provider_->GetGpuFeatureInfo();
}

viz::GLHelper* WebGraphicsContext3DProviderImpl::GetGLHelper() {
  if (!gl_helper_) {
    gl_helper_ = std::make_unique<viz::GLHelper>(provider_->ContextGL(),
                                                 provider_->ContextSupport());
  }
  return gl_helper_.get();
}

bool WebGraphicsContext3DProviderImpl::IsSoftwareRendering() const {
  return software_rendering_;
}

void WebGraphicsContext3DProviderImpl::SetLostContextCallback(
    base::RepeatingClosure c) {
  context_lost_callback_ = std::move(c);
}

void WebGraphicsContext3DProviderImpl::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> c) {
  provider_->ContextSupport()->SetErrorMessageCallback(std::move(c));
}

void WebGraphicsContext3DProviderImpl::SignalQuery(uint32_t query,
                                                   base::OnceClosure callback) {
  provider_->ContextSupport()->SignalQuery(query, std::move(callback));
}

void WebGraphicsContext3DProviderImpl::OnContextLost() {
  if (!context_lost_callback_.is_null())
    context_lost_callback_.Run();
}

}  // namespace content
