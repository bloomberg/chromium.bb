// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/context_provider_command_buffer.h"

#include <set>
#include <vector>

#include "base/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "cc/output/managed_memory_policy.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"

namespace content {

class ContextProviderCommandBuffer::LostContextCallbackProxy
    : public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderCommandBuffer* provider)
      : provider_(provider) {
    provider_->context3d_->setContextLostCallback(this);
  }

  virtual ~LostContextCallbackProxy() {
    provider_->context3d_->setContextLostCallback(NULL);
  }

  virtual void onContextLost() {
    provider_->OnLostContext();
  }

 private:
  ContextProviderCommandBuffer* provider_;
};

class ContextProviderCommandBuffer::SwapBuffersCompleteCallbackProxy
    : public WebKit::WebGraphicsContext3D::
          WebGraphicsSwapBuffersCompleteCallbackCHROMIUM {
 public:
  explicit SwapBuffersCompleteCallbackProxy(
      ContextProviderCommandBuffer* provider)
      : provider_(provider) {
    provider_->context3d_->setSwapBuffersCompleteCallbackCHROMIUM(this);
  }

  virtual ~SwapBuffersCompleteCallbackProxy() {
    provider_->context3d_->setSwapBuffersCompleteCallbackCHROMIUM(NULL);
  }

  virtual void onSwapBuffersComplete() {
    provider_->OnSwapBuffersComplete();
  }

 private:
  ContextProviderCommandBuffer* provider_;
};

scoped_refptr<ContextProviderCommandBuffer>
ContextProviderCommandBuffer::Create(
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
    const std::string& debug_name) {
  if (!context3d)
    return NULL;

  return new ContextProviderCommandBuffer(context3d.Pass(), debug_name);
}

ContextProviderCommandBuffer::ContextProviderCommandBuffer(
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
    const std::string& debug_name)
    : context3d_(context3d.Pass()),
      debug_name_(debug_name),
      leak_on_destroy_(false),
      destroyed_(false) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context3d_);
  context_thread_checker_.DetachFromThread();
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(main_thread_lock_);

  // Destroy references to the context3d_ before leaking it.
  if (context3d_->GetCommandBufferProxy()) {
    context3d_->GetCommandBufferProxy()->SetMemoryAllocationChangedCallback(
        CommandBufferProxyImpl::MemoryAllocationChangedCallback());
  }
  swap_buffers_complete_callback_proxy_.reset();
  lost_context_callback_proxy_.reset();

  if (leak_on_destroy_) {
    WebGraphicsContext3DCommandBufferImpl* context3d ALLOW_UNUSED =
        context3d_.release();
    webkit::gpu::GrContextForWebGraphicsContext3D* gr_context ALLOW_UNUSED =
        gr_context_.release();
  }
}

bool ContextProviderCommandBuffer::BindToCurrentThread() {
  DCHECK(context3d_);

  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (lost_context_callback_proxy_)
    return true;

  if (!context3d_->makeContextCurrent())
    return false;

  if (!InitializeCapabilities())
    return false;

  std::string unique_context_name =
      base::StringPrintf("%s-%p", debug_name_.c_str(), context3d_.get());
  context3d_->pushGroupMarkerEXT(unique_context_name.c_str());

  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  swap_buffers_complete_callback_proxy_.reset(
      new SwapBuffersCompleteCallbackProxy(this));
  context3d_->GetCommandBufferProxy()->SetMemoryAllocationChangedCallback(
      base::Bind(&ContextProviderCommandBuffer::OnMemoryAllocationChanged,
                 base::Unretained(this)));
  return true;
}

WebGraphicsContext3DCommandBufferImpl*
ContextProviderCommandBuffer::Context3d() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}

gpu::ContextSupport* ContextProviderCommandBuffer::ContextSupport() {
  return context3d_->GetContextSupport();
}

class GrContext* ContextProviderCommandBuffer::GrContext() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(
      new webkit::gpu::GrContextForWebGraphicsContext3D(context3d_.get()));
  return gr_context_->get();
}

cc::ContextProvider::Capabilities
ContextProviderCommandBuffer::ContextCapabilities() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return capabilities_;
}

bool ContextProviderCommandBuffer::IsContextLost() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_->isContextLost();
}

void ContextProviderCommandBuffer::VerifyContexts() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (context3d_->isContextLost())
    OnLostContext();
}

void ContextProviderCommandBuffer::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  {
    base::AutoLock lock(main_thread_lock_);
    if (destroyed_)
      return;
    destroyed_ = true;
  }
  if (!lost_context_callback_.is_null())
    base::ResetAndReturn(&lost_context_callback_).Run();
}

void ContextProviderCommandBuffer::OnSwapBuffersComplete() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (!swap_buffers_complete_callback_.is_null())
    swap_buffers_complete_callback_.Run();
}

void ContextProviderCommandBuffer::OnMemoryAllocationChanged(
    const gpu::MemoryAllocation& allocation) {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_) {
    bool nonzero_allocation = !!allocation.bytes_limit_when_visible;
    gr_context_->SetMemoryLimit(nonzero_allocation);
  }

  if (memory_policy_changed_callback_.is_null())
    return;

  memory_policy_changed_callback_.Run(cc::ManagedMemoryPolicy(allocation));
}

bool ContextProviderCommandBuffer::InitializeCapabilities() {
  // The command buffer provides the following capabilities always.
  // TODO(jamesr): This information is duplicated with
  // gpu::gles2::FeatureInfo::AddFeatures().
  Capabilities caps;
  caps.discard_backbuffer = true;
  caps.set_visibility = true;

  // TODO(jamesr): These are also added in
  // gpu::gles2::GLES2Implementation::GetStringHelper() on the client side.
  caps.map_sub = true;
  caps.shallow_flush = true;

  // The swapbuffers complete callback is always supported by multi-process
  // command buffer implementations.
  caps.swapbuffers_complete_callback = true;

  const GLubyte* extensions_cstr =
      context3d_->GetImplementation()->GetString(0x1F03 /* GL_EXTENSIONS */);
  if (!extensions_cstr)
    return false;
  std::string extensions = reinterpret_cast<const char*>(extensions_cstr);
  std::vector<std::string> extension_list;
  base::SplitString(extensions, ' ', &extension_list);
  std::set<std::string> extension_set(extension_list.begin(),
                                      extension_list.end());


  // caps.map_image depends on GL_CHROMIUM_map_image, which is set client-side
  // based on the presence of GpuControl.
  caps.map_image = extension_set.count("GL_CHROMIUM_map_image") > 0;

  // caps.fast_npot_mo8_textures depends on
  //    workarounds_.enable_chromium_fast_npot_mo8_textures which controls
  //    GL_CHROMIUM_fast_NPOT_MO8_textures
  caps.fast_npot_mo8_textures =
      extension_set.count("GL_CHROMIUM_fast_NPOT_MO8_textures") > 0;

  caps.egl_image_external =
      extension_set.count("GL_OES_EGL_image_external") > 0;

  caps.texture_format_bgra8888 =
      extension_set.count("GL_EXT_texture_format_BGRA8888") > 0;

  caps.texture_format_etc1 =
      extension_set.count("GL_OES_compressed_ETC1_RGB8_texture") > 0;

  caps.texture_rectangle = extension_set.count("GL_ARB_texture_rectangle") > 0;

  caps.post_sub_buffer = extension_set.count("GL_CHROMIUM_post_sub_buffer") > 0;

  // TODO(jamesr): This is unconditionally true on mac, no need to test for it
  // at runtime.
  caps.iosurface = extension_set.count("GL_CHROMIUM_iosurface") > 0;

  caps.texture_usage = extension_set.count("GL_ANGLE_texture_usage") > 0;
  caps.texture_storage = extension_set.count("GL_EXT_texture_storage") > 0;

  caps.discard_framebuffer =
      extension_set.count("GL_EXT_discard_framebuffer") > 0;
  size_t mapped_memory_limit = context3d_->GetMappedMemoryLimit();
  caps.max_transfer_buffer_usage_bytes =
      mapped_memory_limit == WebGraphicsContext3DCommandBufferImpl::kNoLimit
      ? std::numeric_limits<size_t>::max() : mapped_memory_limit;

  capabilities_ = caps;
  return true;
}


bool ContextProviderCommandBuffer::DestroyedOnMainThread() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(main_thread_lock_);
  return destroyed_;
}

void ContextProviderCommandBuffer::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() ||
         lost_context_callback.is_null());
  lost_context_callback_ = lost_context_callback;
}

void ContextProviderCommandBuffer::SetSwapBuffersCompleteCallback(
    const SwapBuffersCompleteCallback& swap_buffers_complete_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(swap_buffers_complete_callback_.is_null() ||
         swap_buffers_complete_callback.is_null());
  swap_buffers_complete_callback_ = swap_buffers_complete_callback;
}

void ContextProviderCommandBuffer::SetMemoryPolicyChangedCallback(
    const MemoryPolicyChangedCallback& memory_policy_changed_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(memory_policy_changed_callback_.is_null() ||
         memory_policy_changed_callback.is_null());
  memory_policy_changed_callback_ = memory_policy_changed_callback;
}

}  // namespace content
