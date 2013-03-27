// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_DECODER_FACTORIES_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_DECODER_FACTORIES_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/content_export.h"
#include "media/filters/gpu_video_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

namespace base {
class WaitableEvent;
}

namespace content {
class GpuChannelHost;
class WebGraphicsContext3DCommandBufferImpl;

// Glue code to expose functionality needed by media::GpuVideoDecoder to
// RenderViewImpl.  This class is entirely an implementation detail of
// RenderViewImpl and only has its own header to allow extraction of its
// implementation from render_view_impl.cc which is already far too large.
//
// The public methods of the class can be called from any thread, and are
// internally trampolined to the appropriate thread.  GPU/GL-related calls go to
// the constructor-argument loop (mostly that's the compositor thread, or the
// renderer thread if threaded compositing is disabled), and shmem-related calls
// go to the render thread.
class CONTENT_EXPORT RendererGpuVideoDecoderFactories
    : public media::GpuVideoDecoder::Factories {
 public:
  // Takes a ref on |gpu_channel_host| and tests |context| for loss before each
  // use.
  RendererGpuVideoDecoderFactories(
      GpuChannelHost* gpu_channel_host,
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      WebGraphicsContext3DCommandBufferImpl* wgc3dcbi);

  // media::GpuVideoDecoder::Factories implementation.
  virtual media::VideoDecodeAccelerator* CreateVideoDecodeAccelerator(
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client) OVERRIDE;
  virtual bool CreateTextures(int32 count, const gfx::Size& size,
                              std::vector<uint32>* texture_ids,
                              uint32 texture_target) OVERRIDE;
  virtual void DeleteTexture(uint32 texture_id) OVERRIDE;
  virtual void ReadPixels(uint32 texture_id,
                          uint32 texture_target,
                          const gfx::Size& size,
                          const SkBitmap& pixels) OVERRIDE;
  virtual base::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() OVERRIDE;
  virtual void Abort() OVERRIDE;
  virtual bool IsAborted() OVERRIDE;

 protected:
  friend class base::RefCountedThreadSafe<RendererGpuVideoDecoderFactories>;
  virtual ~RendererGpuVideoDecoderFactories();

 private:
  // Helper for the constructor to acquire the ContentGLContext on the
  // compositor thread (when it is enabled).
  void AsyncGetContext(WebGraphicsContext3DCommandBufferImpl* context);

  // Async versions of the public methods.  They use output parameters instead
  // of return values and each takes a WaitableEvent* param to signal completion
  // (except for DeleteTexture, which is fire-and-forget).
  // AsyncCreateSharedMemory runs on the renderer thread and the rest run on
  // |message_loop_|.
  // The AsyncCreateVideoDecodeAccelerator returns its output in the vda_
  // member.
  void AsyncCreateVideoDecodeAccelerator(
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client);
  void AsyncCreateTextures(int32 count, const gfx::Size& size,
                           uint32 texture_target);
  void AsyncDeleteTexture(uint32 texture_id);
  void AsyncReadPixels(uint32 texture_id, uint32 texture_target,
                       const gfx::Size& size);
  void AsyncCreateSharedMemory(size_t size);
  void AsyncDestroyVideoDecodeAccelerator();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_refptr<GpuChannelHost> gpu_channel_host_;
  base::WeakPtr<WebGraphicsContext3DCommandBufferImpl> context_;

  // This event is signaled if we have been asked to Abort().
  base::WaitableEvent aborted_waiter_;

  // This event is signaled by asynchronous tasks to indicate their completion.
  // e.g. AsyncCreateVideoDecodeAccelerator()/AsyncCreateTextures() etc.
  base::WaitableEvent async_waiter_;

  // The vda returned by the CreateVideoAcclelerator function.
  scoped_ptr<media::VideoDecodeAccelerator> vda_;

  // Shared memory segment which is returned by the CreateSharedMemory()
  // function.
  scoped_ptr<base::SharedMemory> shared_memory_segment_;

  // Bitmap returned by ReadPixels().
  SkBitmap read_pixels_bitmap_;

  // Textures returned by the CreateTexture() function.
  std::vector<uint32> created_textures_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RendererGpuVideoDecoderFactories);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_DECODER_FACTORIES_H_
