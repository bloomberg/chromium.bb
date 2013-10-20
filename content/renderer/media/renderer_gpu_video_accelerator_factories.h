// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/content_export.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}

namespace content {
class ContextProviderCommandBuffer;
class GpuChannelHost;
class WebGraphicsContext3DCommandBufferImpl;

// Glue code to expose functionality needed by media::GpuVideoAccelerator to
// RenderViewImpl.  This class is entirely an implementation detail of
// RenderViewImpl and only has its own header to allow extraction of its
// implementation from render_view_impl.cc which is already far too large.
//
// The RendererGpuVideoAcceleratorFactories can be constructed on any thread.
// Most public methods of the class must be called from the media thread.  The
// exceptions (which can be called from any thread, as they are internally
// trampolined) are:
// * CreateVideoDecodeAccelerator()
// * ReadPixels()
class CONTENT_EXPORT RendererGpuVideoAcceleratorFactories
    : public media::GpuVideoAcceleratorFactories {
 public:
  // Takes a ref on |gpu_channel_host| and tests |context| for loss before each
  // use.  Safe to call from any thread.
  RendererGpuVideoAcceleratorFactories(
      GpuChannelHost* gpu_channel_host,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider);

  // media::GpuVideoAcceleratorFactories implementation.
  // CreateVideoDecodeAccelerator() is safe to call from any thread.
  virtual scoped_ptr<media::VideoDecodeAccelerator>
      CreateVideoDecodeAccelerator(
          media::VideoCodecProfile profile,
          media::VideoDecodeAccelerator::Client* client) OVERRIDE;
  virtual scoped_ptr<media::VideoEncodeAccelerator>
      CreateVideoEncodeAccelerator(
          media::VideoEncodeAccelerator::Client* client) OVERRIDE;
  // Creates textures and produces them into mailboxes. Returns a sync point to
  // wait on before using the mailboxes, or 0 on failure.
  virtual uint32 CreateTextures(int32 count,
                                const gfx::Size& size,
                                std::vector<uint32>* texture_ids,
                                std::vector<gpu::Mailbox>* texture_mailboxes,
                                uint32 texture_target) OVERRIDE;
  virtual void DeleteTexture(uint32 texture_id) OVERRIDE;
  virtual void WaitSyncPoint(uint32 sync_point) OVERRIDE;
  // ReadPixels() is safe to call from any thread.
  virtual void ReadPixels(uint32 texture_id,
                          const gfx::Size& size,
                          const SkBitmap& pixels) OVERRIDE;
  virtual base::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() OVERRIDE;
  virtual void Abort() OVERRIDE;
  virtual bool IsAborted() OVERRIDE;
  scoped_refptr<RendererGpuVideoAcceleratorFactories> Clone();

 protected:
  friend class base::RefCountedThreadSafe<RendererGpuVideoAcceleratorFactories>;
  virtual ~RendererGpuVideoAcceleratorFactories();

 private:
  RendererGpuVideoAcceleratorFactories();

  // Helper to get a pointer to the WebGraphicsContext3DCommandBufferImpl,
  // if it has not been lost yet.
  WebGraphicsContext3DCommandBufferImpl* GetContext3d();

  // Helper for the constructor to acquire the ContentGLContext on
  // |message_loop_|.
  void AsyncBindContext();

  // Async versions of the public methods, run on |message_loop_|.
  // They use output parameters instead of return values and each takes
  // a WaitableEvent* param to signal completion (except for DeleteTexture,
  // which is fire-and-forget).
  // AsyncCreateVideoDecodeAccelerator returns its output in the |vda_| member.
  void AsyncCreateVideoDecodeAccelerator(
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client);
  void AsyncReadPixels(uint32 texture_id, const gfx::Size& size);
  void AsyncDestroyVideoDecodeAccelerator();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_refptr<GpuChannelHost> gpu_channel_host_;
  scoped_refptr<ContextProviderCommandBuffer> context_provider_;

  // For sending requests to allocate shared memory in the Browser process.
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // This event is signaled if we have been asked to Abort().
  base::WaitableEvent aborted_waiter_;

  // This event is signaled by asynchronous tasks posted to |message_loop_| to
  // indicate their completion.
  // e.g. AsyncCreateVideoDecodeAccelerator()/AsyncCreateTextures() etc.
  base::WaitableEvent message_loop_async_waiter_;

  // The vda returned by the CreateVideoDecodeAccelerator function.
  scoped_ptr<media::VideoDecodeAccelerator> vda_;

  // Bitmap returned by ReadPixels().
  SkBitmap read_pixels_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(RendererGpuVideoAcceleratorFactories);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
