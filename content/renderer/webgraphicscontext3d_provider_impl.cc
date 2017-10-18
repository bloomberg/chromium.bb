// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webgraphicscontext3d_provider_impl.h"

#include "gpu/command_buffer/client/context_support.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

WebGraphicsContext3DProviderImpl::WebGraphicsContext3DProviderImpl(
    scoped_refptr<ui::ContextProviderCommandBuffer> provider,
    bool software_rendering)
    : provider_(std::move(provider)), software_rendering_(software_rendering) {}

WebGraphicsContext3DProviderImpl::~WebGraphicsContext3DProviderImpl() {}

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

const gpu::Capabilities& WebGraphicsContext3DProviderImpl::GetCapabilities()
    const {
  return provider_->ContextCapabilities();
}

const gpu::GpuFeatureInfo& WebGraphicsContext3DProviderImpl::GetGpuFeatureInfo()
    const {
  return provider_->GetGpuFeatureInfo();
}

bool WebGraphicsContext3DProviderImpl::IsSoftwareRendering() const {
  return software_rendering_;
}

void WebGraphicsContext3DProviderImpl::SetLostContextCallback(
    const base::Closure& c) {
  provider_->SetLostContextCallback(c);
}

void WebGraphicsContext3DProviderImpl::SetErrorMessageCallback(
    const base::Callback<void(const char*, int32_t)>& c) {
  provider_->ContextSupport()->SetErrorMessageCallback(c);
}

void WebGraphicsContext3DProviderImpl::SignalQuery(
    uint32_t query,
    const base::Closure& callback) {
  provider_->ContextSupport()->SignalQuery(query, callback);
}

}  // namespace content
