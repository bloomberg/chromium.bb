// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/context_provider_command_buffer.h"

#include <stddef.h>
#include <set>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "cc/output/managed_memory_policy.h"
#include "content/common/gpu/client/grcontext_for_webgraphicscontext3d.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace content {

class ContextProviderCommandBuffer::LostContextCallbackProxy
    : public blink::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderCommandBuffer* provider)
      : provider_(provider) {
    provider_->WebContext3DNoChecks()->setContextLostCallback(this);
  }

  ~LostContextCallbackProxy() override {
    provider_->WebContext3DNoChecks()->setContextLostCallback(NULL);
  }

  void onContextLost() override { provider_->OnLostContext(); }

 private:
  ContextProviderCommandBuffer* provider_;
};

scoped_refptr<ContextProviderCommandBuffer>
ContextProviderCommandBuffer::Create(
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
    CommandBufferContextType type) {
  if (!context3d)
    return NULL;

  return new ContextProviderCommandBuffer(std::move(context3d), type);
}

ContextProviderCommandBuffer::ContextProviderCommandBuffer(
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
    CommandBufferContextType type)
    : context_type_(type),
      debug_name_(CommandBufferContextTypeToString(type)) {
  gr_interface_ = skia::AdoptRef(
      new GrGLInterfaceForWebGraphicsContext3D(std::move(context3d)));
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(gr_interface_->WebContext3D());
  context_thread_checker_.DetachFromThread();
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());

  // Destroy references to the context3d_ before leaking it.
  if (WebContext3DNoChecks()->GetCommandBufferProxy())
    WebContext3DNoChecks()->GetCommandBufferProxy()->SetLock(nullptr);
  lost_context_callback_proxy_.reset();
}


CommandBufferProxyImpl* ContextProviderCommandBuffer::GetCommandBufferProxy() {
  return WebContext3DNoChecks()->GetCommandBufferProxy();
}

WebGraphicsContext3DCommandBufferImpl*
ContextProviderCommandBuffer::WebContext3D() {
  DCHECK(gr_interface_);
  DCHECK(gr_interface_->WebContext3D());
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return WebContext3DNoChecks();
}

WebGraphicsContext3DCommandBufferImpl*
    ContextProviderCommandBuffer::WebContext3DNoChecks() {
  DCHECK(gr_interface_);
  return static_cast<WebGraphicsContext3DCommandBufferImpl*>(
      gr_interface_->WebContext3D());
}

bool ContextProviderCommandBuffer::BindToCurrentThread() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(gr_interface_ && gr_interface_->WebContext3D());

  if (lost_context_callback_proxy_)
    return true;

  WebContext3DNoChecks()->SetContextType(context_type_);
  if (!WebContext3DNoChecks()->InitializeOnCurrentThread())
    return false;

  gr_interface_->BindToCurrentThread();
  InitializeCapabilities();

  std::string unique_context_name =
      base::StringPrintf("%s-%p", debug_name_.c_str(), WebContext3DNoChecks());
  WebContext3DNoChecks()->traceBeginCHROMIUM("gpu_toplevel",
                                             unique_context_name.c_str());

  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  return true;
}

void ContextProviderCommandBuffer::DetachFromThread() {
  context_thread_checker_.DetachFromThread();
}

gpu::gles2::GLES2Interface* ContextProviderCommandBuffer::ContextGL() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.

  return WebContext3D()->GetImplementation();
}

gpu::ContextSupport* ContextProviderCommandBuffer::ContextSupport() {
  return WebContext3DNoChecks()->GetContextSupport();
}

class GrContext* ContextProviderCommandBuffer::GrContext() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(new GrContextForWebGraphicsContext3D(gr_interface_));

  // If GlContext is already lost, also abandon the new GrContext.
  if (gr_context_->get() &&
      ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    gr_context_->get()->abandonContext();

  return gr_context_->get();
}

void ContextProviderCommandBuffer::InvalidateGrContext(uint32_t state) {
  if (gr_context_) {
    DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
    DCHECK(context_thread_checker_.CalledOnValidThread());
    gr_context_->get()->resetContext(state);
  }
}

void ContextProviderCommandBuffer::SetupLock() {
  WebContext3D()->GetCommandBufferProxy()->SetLock(&context_lock_);
}

base::Lock* ContextProviderCommandBuffer::GetLock() {
  return &context_lock_;
}

cc::ContextProvider::Capabilities
ContextProviderCommandBuffer::ContextCapabilities() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return capabilities_;
}

void ContextProviderCommandBuffer::DeleteCachedResources() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->FreeGpuResources();
}

void ContextProviderCommandBuffer::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (!lost_context_callback_.is_null())
    base::ResetAndReturn(&lost_context_callback_).Run();
  if (gr_context_)
    gr_context_->OnLostContext();
}

void ContextProviderCommandBuffer::InitializeCapabilities() {
  Capabilities caps;
  caps.gpu = WebContext3DNoChecks()->GetImplementation()->capabilities();

  size_t mapped_memory_limit = WebContext3DNoChecks()->GetMappedMemoryLimit();
  caps.max_transfer_buffer_usage_bytes =
      mapped_memory_limit == WebGraphicsContext3DCommandBufferImpl::kNoLimit
      ? std::numeric_limits<size_t>::max() : mapped_memory_limit;

  capabilities_ = caps;
}

void ContextProviderCommandBuffer::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() ||
         lost_context_callback.is_null());
  lost_context_callback_ = lost_context_callback;
}

}  // namespace content
