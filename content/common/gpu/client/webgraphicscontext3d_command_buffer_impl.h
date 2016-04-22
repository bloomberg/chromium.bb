// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/command_buffer_metrics.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace gpu {

class ContextSupport;
class GpuChannelHost;
struct SharedMemoryLimits;
class TransferBuffer;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
class GLES2Interface;
}
}

namespace content {

class WebGraphicsContext3DCommandBufferImpl
    : public NON_EXPORTED_BASE(blink::WebGraphicsContext3D) {
 public:
  enum MappedMemoryReclaimLimit {
    kNoLimit = 0,
  };

  class ShareGroup : public base::RefCountedThreadSafe<ShareGroup> {
   public:
    ShareGroup();

    WebGraphicsContext3DCommandBufferImpl* GetAnyContextLocked() {
      // In order to ensure that the context returned is not removed while
      // in use, the share group's lock should be aquired before calling this
      // function.
      lock_.AssertAcquired();
      if (contexts_.empty())
        return NULL;
      return contexts_.front();
    }

    void AddContextLocked(WebGraphicsContext3DCommandBufferImpl* context) {
      lock_.AssertAcquired();
      contexts_.push_back(context);
    }

    void RemoveContext(WebGraphicsContext3DCommandBufferImpl* context) {
      base::AutoLock auto_lock(lock_);
      contexts_.erase(std::remove(contexts_.begin(), contexts_.end(), context),
          contexts_.end());
    }

    void RemoveAllContexts() {
      base::AutoLock auto_lock(lock_);
      contexts_.clear();
    }

    base::Lock& lock() {
      return lock_;
    }

   private:
    friend class base::RefCountedThreadSafe<ShareGroup>;
    virtual ~ShareGroup();

    std::vector<WebGraphicsContext3DCommandBufferImpl*> contexts_;
    base::Lock lock_;

    DISALLOW_COPY_AND_ASSIGN(ShareGroup);
  };

  class WebGraphicsContextLostCallback {
   public:
    virtual void onContextLost() = 0;

   protected:
    virtual ~WebGraphicsContextLostCallback() {}
  };

  // If surface_handle is not kNullSurfaceHandle, this creates a
  // CommandBufferProxy that renders directly to a view. The view and
  // the associated window must not be destroyed until the returned
  // CommandBufferProxy has been destroyed, otherwise the GPU process might
  // attempt to render to an invalid window handle.
  CONTENT_EXPORT WebGraphicsContext3DCommandBufferImpl(
      gpu::SurfaceHandle surface_handle,
      const GURL& active_url,
      gpu::GpuChannelHost* host,
      const gpu::gles2::ContextCreationAttribHelper& attributes,
      gfx::GpuPreference gpu_preference,
      bool share_resources,
      bool automatic_flushes,
      WebGraphicsContext3DCommandBufferImpl* share_context);

  ~WebGraphicsContext3DCommandBufferImpl() override;

  gpu::CommandBufferProxyImpl* GetCommandBufferProxy() {
    return command_buffer_.get();
  }

  CONTENT_EXPORT gpu::ContextSupport* GetContextSupport();

  gpu::gles2::GLES2Implementation* GetImplementation() {
    return real_gl_.get();
  }

  void SetContextLostCallback(WebGraphicsContextLostCallback* callback) {
    context_lost_callback_ = callback;
  }

  CONTENT_EXPORT bool InitializeOnCurrentThread(
      const gpu::SharedMemoryLimits& memory_limits);

  void SetContextType(CommandBufferContextType type) {
    context_type_ = type;
  }
 private:
  // These are the same error codes as used by EGL.
  enum Error {
    SUCCESS               = 0x3000,
    BAD_ATTRIBUTE         = 0x3004,
    CONTEXT_LOST          = 0x300E
  };

  // Initialize the underlying GL context. May be called multiple times; second
  // and subsequent calls are ignored. Must be called from the thread that is
  // going to use this object to issue GL commands (which might not be the main
  // thread).
  bool MaybeInitializeGL(const gpu::SharedMemoryLimits& memory_limits);

  bool InitializeCommandBuffer(
      WebGraphicsContext3DCommandBufferImpl* share_context);

  void Destroy();

  bool CreateContext(const gpu::SharedMemoryLimits& memory_limits);

  void OnContextLost();

  bool initialized_ = false;
  bool initialize_failed_ = false;
  WebGraphicsContextLostCallback* context_lost_callback_ = nullptr;

  bool automatic_flushes_;
  gpu::gles2::ContextCreationAttribHelper attributes_;

  // State needed by MaybeInitializeGL.
  scoped_refptr<gpu::GpuChannelHost> host_;
  gpu::SurfaceHandle surface_handle_;
  GURL active_url_;
  CommandBufferContextType context_type_;

  gfx::GpuPreference gpu_preference_;

  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_;
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> real_gl_;
  std::unique_ptr<gpu::gles2::GLES2Interface> trace_gl_;
  scoped_refptr<ShareGroup> share_group_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<WebGraphicsContext3DCommandBufferImpl> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_
