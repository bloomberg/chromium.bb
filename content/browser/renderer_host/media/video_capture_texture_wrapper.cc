// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_texture_wrapper.h"

#include "base/bind.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv.h"

namespace content {

namespace {

// VideoCaptureController has at most 3 capture frames in flight.
const size_t kNumGpuMemoryBuffers = 3;

uint32 VideoPixelFormatToFourCC(media::VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    // I420 is needed by Fake and FileVideoCaptureDevice
    case media::PIXEL_FORMAT_I420:
      return libyuv::FOURCC_I420;
    case media::PIXEL_FORMAT_UYVY:
      return libyuv::FOURCC_UYVY;
    case media::PIXEL_FORMAT_YUY2:
      return libyuv::FOURCC_YUY2;
    case media::PIXEL_FORMAT_MJPEG:
      return libyuv::FOURCC_MJPG;
    default:
      NOTREACHED() << "Bad captured pixel format: "
          << media::VideoCaptureFormat::PixelFormatToString(pixel_format);
  }
  return libyuv::FOURCC_ANY;
}

// Modelled after GpuProcessTransportFactory::CreateContextCommon().
scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl> CreateContextCommon(
    scoped_refptr<content::GpuChannelHost> gpu_channel_host,
    int surface_id) {
  if (!content::GpuDataManagerImpl::GetInstance()->
        CanUseGpuBrowserCompositor()) {
    DLOG(ERROR) << "No accelerated graphics found. Check chrome://gpu";
    return scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl>();
  }
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.noAutomaticFlushes = true;

  if (!gpu_channel_host.get()) {
    DLOG(ERROR) << "Failed to establish GPU channel.";
    return scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl>();
  }
  GURL url("chrome://gpu/GpuProcessTransportFactory::CreateCaptureContext");
  return make_scoped_ptr(
      new WebGraphicsContext3DCommandBufferImpl(
          surface_id,
          url,
          gpu_channel_host.get(),
          attrs,
          true  /* lose_context_when_out_of_memory */,
          content::WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits(),
          NULL));
}

// Modelled after
// GpuProcessTransportFactory::CreateOffscreenCommandBufferContext().
scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl>
CreateOffscreenCommandBufferContext() {
  content::CauseForGpuLaunch cause = content::CAUSE_FOR_GPU_LAUNCH_CANVAS_2D;
  scoped_refptr<content::GpuChannelHost> gpu_channel_host(
      content::BrowserGpuChannelHostFactory::instance()->
          EstablishGpuChannelSync(cause));
  DCHECK(gpu_channel_host);
  return CreateContextCommon(gpu_channel_host, 0);
}

typedef base::Callback<void(scoped_refptr<ContextProviderCommandBuffer>)>
    ProcessContextCallback;

void CreateContextOnUIThread(ProcessContextCallback bottom_half) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bottom_half.Run(ContextProviderCommandBuffer::Create(
      CreateOffscreenCommandBufferContext(), "Offscreen-CaptureThread"));
  return;
}

void ResetLostContextCallback(
    const scoped_refptr<ContextProviderCommandBuffer>& capture_thread_context) {
  capture_thread_context->SetLostContextCallback(
      cc::ContextProvider::LostContextCallback());
}

}  // anonymous namespace

// Internal ref-counted class to manage a pool of GpuMemoryBuffers. The contents
// of an incoming captured frame are_copied_ into the first available buffer
// from the pool and sent to our client ultimately wrapped into a VideoFrame.
// This VideoFrame creation is balanced by a waiting on the associated
// |sync_point|. After VideoFrame consumption the inserted ReleaseCallback()
// will be called, where the GpuMemoryBuffer is recycled.
//
// This class jumps between threads due to GPU-related thread limitations, i.e.
// some objects cannot be accessed from IO Thread, where we are constructed,
// others need to be constructed on UI Thread. For this reason most of the
// operations are carried out on Capture Thread (|capture_task_runner_|).
//
// TODO(mcasas): ctor |frame_format| is used for early GpuMemoryBuffer pool
// allocation, but VideoCaptureDevices might provide a different resolution when
// calling OnIncomingCapturedData(), be that due to driver preferences or to
// its ResolutionChangePolicy. Make the GpuMemoryBuffer allocated on demand.
class VideoCaptureTextureWrapper::TextureWrapperDelegate final
    : public base::RefCountedThreadSafe<TextureWrapperDelegate> {
 public:
  TextureWrapperDelegate(
      const base::WeakPtr<VideoCaptureController>& controller,
      const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner,
      const media::VideoCaptureFormat& capture_format);

  // Copy-converts the incoming data into a GpuMemoruBuffer backed Texture, and
  // sends it to |controller_| wrapped in a VideoFrame, with |buffer| as storage
  // backend.
  void OnIncomingCapturedData(
      const scoped_refptr<media::VideoCaptureDevice::Client::Buffer>&
          texture_buffer,
      const scoped_refptr<media::VideoCaptureDevice::Client::Buffer>&
          argb_buffer,
      const gfx::Size& frame_size,
      const base::TimeTicks& timestamp);

 private:
  friend class base::RefCountedThreadSafe<TextureWrapperDelegate>;
  ~TextureWrapperDelegate();

  // Creates some necessary members in |capture_task_runner_|.
  void Init(const media::VideoCaptureFormat& capture_format);
  // Runs the bottom half of the GlHelper creation.
  void CreateGlHelper(
      scoped_refptr<ContextProviderCommandBuffer> capture_thread_context);

  // Recycles |memory_buffer|, deletes Image and Texture on VideoFrame release.
  void ReleaseCallback(GLuint image_id,
                       GLuint texture_id,
                       linked_ptr<gfx::GpuMemoryBuffer> memory_buffer,
                       uint32 sync_point);

  // The Command Buffer lost the GL context, f.i. GPU process crashed. Signal
  // error to our owner so the capture can be torn down.
  void LostContextCallback();

  // Prints the error |message| and notifies |controller_| of an error.
  void OnError(const std::string& message);

  const base::WeakPtr<VideoCaptureController> controller_;
  const scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;

  // Command buffer reference, needs to be destroyed when unused. It is created
  // on UI Thread and bound to Capture Thread. In particular, it cannot be used
  // from IO Thread.
  scoped_refptr<ContextProviderCommandBuffer> capture_thread_context_;
  // Created and used from Capture Thread. Cannot be used from IO Thread.
  scoped_ptr<GLHelper> gl_helper_;

  // A pool of GpuMemoryBuffers that are used to wrap incoming captured frames;
  // recycled via ReleaseCallback().
  std::queue<linked_ptr<gfx::GpuMemoryBuffer>> gpu_memory_buffers_;

  DISALLOW_COPY_AND_ASSIGN(TextureWrapperDelegate);
};

VideoCaptureTextureWrapper::VideoCaptureTextureWrapper(
    const base::WeakPtr<VideoCaptureController>& controller,
    const scoped_refptr<VideoCaptureBufferPool>& buffer_pool,
    const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner,
    const media::VideoCaptureFormat& capture_format)
    : VideoCaptureDeviceClient(controller, buffer_pool),
      wrapper_delegate_(new TextureWrapperDelegate(controller,
                                                   capture_task_runner,
                                                   capture_format)),
      capture_task_runner_(capture_task_runner)  {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

VideoCaptureTextureWrapper::~VideoCaptureTextureWrapper() {
}

void VideoCaptureTextureWrapper::OnIncomingCapturedData(
    const uint8* data,
    int length,
    const media::VideoCaptureFormat& frame_format,
    int clockwise_rotation,
    const base::TimeTicks& timestamp) {

  // Reserve a temporary Buffer for conversion to ARGB.
  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> argb_buffer =
      ReserveOutputBuffer(media::VideoFrame::ARGB, frame_format.frame_size);
  DVLOG_IF(1, !argb_buffer) << "Couldn't allocate ARGB Buffer";
  if (!argb_buffer)
    return;

  DCHECK(argb_buffer->data());
  // TODO(mcasas): Take |rotation| into acount.
  int ret = libyuv::ConvertToARGB(data,
                                  length,
                                  reinterpret_cast<uint8*>(argb_buffer->data()),
                                  frame_format.frame_size.width() * 4,
                                  0,
                                  0,
                                  frame_format.frame_size.width(),
                                  frame_format.frame_size.height(),
                                  frame_format.frame_size.width(),
                                  frame_format.frame_size.height(),
                                  libyuv::kRotate0,
                                  VideoPixelFormatToFourCC(
                                      frame_format.pixel_format));
  DLOG_IF(ERROR, ret != 0) << "Error converting incoming frame";
  if (ret != 0)
    return;

  // Reserve output buffer for the texture on the IPC borderlands.
  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> texture_buffer =
      ReserveOutputBuffer(media::VideoFrame::NATIVE_TEXTURE, gfx::Size());
  DVLOG_IF(1, !texture_buffer) << "Couldn't allocate Texture Buffer";
  if (!texture_buffer)
    return;

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &TextureWrapperDelegate::OnIncomingCapturedData,
          wrapper_delegate_,
          texture_buffer,
          argb_buffer,
          frame_format.frame_size,
          timestamp));
}

VideoCaptureTextureWrapper::TextureWrapperDelegate::TextureWrapperDelegate(
    const base::WeakPtr<VideoCaptureController>& controller,
    const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner,
    const media::VideoCaptureFormat& capture_format)
  : controller_(controller),
    capture_task_runner_(capture_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  capture_task_runner_->PostTask(FROM_HERE,
      base::Bind(&TextureWrapperDelegate::Init, this, capture_format));
}

void VideoCaptureTextureWrapper::TextureWrapperDelegate::OnIncomingCapturedData(
    const scoped_refptr<media::VideoCaptureDevice::Client::Buffer>&
        texture_buffer,
    const scoped_refptr<media::VideoCaptureDevice::Client::Buffer>& argb_buffer,
    const gfx::Size& frame_size,
    const base::TimeTicks& timestamp) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  DVLOG_IF(1, !gl_helper_) << " Skipping ingress frame, no GL context.";
  if (!gl_helper_)
    return;

  DVLOG_IF(1, gpu_memory_buffers_.empty()) << " Skipping ingress frame, 0 GMBs";
  if (gpu_memory_buffers_.empty())
    return;

  linked_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      gpu_memory_buffers_.front();
  gpu_memory_buffers_.pop();
  DCHECK(gpu_memory_buffer.get());

  uint8* mapped_buffer = static_cast<uint8*>(gpu_memory_buffer->Map());
  DCHECK(mapped_buffer);
  libyuv::ARGBCopy(
      reinterpret_cast<uint8*>(argb_buffer->data()), frame_size.width() * 4,
      mapped_buffer, frame_size.width() * 4,
      frame_size.width(), frame_size.height());
  gpu_memory_buffer->Unmap();

  gpu::gles2::GLES2Interface* gl = capture_thread_context_->ContextGL();
  GLuint image_id = gl->CreateImageCHROMIUM(gpu_memory_buffer->AsClientBuffer(),
                                            frame_size.width(),
                                            frame_size.height(),
                                            GL_RGBA);
  DCHECK(image_id);

  GLuint texture_id = gl_helper_->CreateTexture();
  DCHECK(texture_id);
  {
    content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl, texture_id);
    gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);
  }

  scoped_ptr<gpu::MailboxHolder> mailbox_holder(new gpu::MailboxHolder(
      gl_helper_->ProduceMailboxHolderFromTexture(texture_id)));
  DCHECK(!mailbox_holder->mailbox.IsZero());
  DCHECK(mailbox_holder->mailbox.Verify());
  DCHECK(mailbox_holder->texture_target);
  DCHECK(mailbox_holder->sync_point);

  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapNativeTexture(
          mailbox_holder.Pass(),
          media::BindToCurrentLoop(
              base::Bind(&VideoCaptureTextureWrapper::TextureWrapperDelegate::
                             ReleaseCallback,
                         this, image_id, texture_id, gpu_memory_buffer)),
          frame_size,
          gfx::Rect(frame_size),
          frame_size,
          base::TimeDelta(),
          true /* allow_overlay */);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_, texture_buffer, video_frame, timestamp));
}

VideoCaptureTextureWrapper::TextureWrapperDelegate::~TextureWrapperDelegate() {
  // Might not be running on capture_task_runner_'s thread. Ensure owned objects
  // are destroyed on the correct threads.
  while (!gpu_memory_buffers_.empty()) {
    capture_task_runner_->DeleteSoon(FROM_HERE,
                                     gpu_memory_buffers_.front().release());
    gpu_memory_buffers_.pop();
  }
  if (gl_helper_)
    capture_task_runner_->DeleteSoon(FROM_HERE, gl_helper_.release());

  if (capture_thread_context_) {
    capture_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ResetLostContextCallback, capture_thread_context_));
    capture_thread_context_->AddRef();
    ContextProviderCommandBuffer* raw_capture_thread_context =
        capture_thread_context_.get();
    capture_thread_context_ = nullptr;
    capture_task_runner_->ReleaseSoon(FROM_HERE, raw_capture_thread_context);
  }
}

void VideoCaptureTextureWrapper::TextureWrapperDelegate::Init(
    const media::VideoCaptureFormat& capture_format) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  // BrowserGpuMemoryBufferManager::current() may not be accessed on IO Thread.
  // TODO(mcasas): At this point |frame_format| represents the format we want to
  // get from the VCDevice, but this might send another format.
  for (size_t i = 0; i < kNumGpuMemoryBuffers; ++i) {
    linked_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer(
        BrowserGpuMemoryBufferManager::current()->AllocateGpuMemoryBuffer(
            capture_format.frame_size,
            gfx::GpuMemoryBuffer::BGRA_8888,
            gfx::GpuMemoryBuffer::MAP).release());
    if (!gpu_memory_buffer.get()) {
      OnError("Could not allocate GpuMemoryBuffer");
      while(!gpu_memory_buffers_.empty())
        gpu_memory_buffers_.pop();
      return;
    }
    gpu_memory_buffers_.push(gpu_memory_buffer);
  }

  // In threaded compositing mode, we have to create our own context for Capture
  // to avoid using the GPU command queue from multiple threads. Context
  // creation must happen on UI thread; then the context needs to be bound to
  // the appropriate thread, which is done in CreateGlHelper().
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CreateContextOnUIThread,
                 media::BindToCurrentLoop(
                     base::Bind(&VideoCaptureTextureWrapper::
                                    TextureWrapperDelegate::CreateGlHelper,
                                this))));
}

void VideoCaptureTextureWrapper::TextureWrapperDelegate::CreateGlHelper(
    scoped_refptr<ContextProviderCommandBuffer> capture_thread_context) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!capture_thread_context.get()) {
    DLOG(ERROR) << "No offscreen GL Context!";
    return;
  }
  // This may not happen in IO Thread. The destructor resets the context lost
  // callback, so base::Unretained is safe; otherwise it'd be a circular ref
  // counted dependency.
  capture_thread_context->SetLostContextCallback(media::BindToCurrentLoop(
      base::Bind(
          &VideoCaptureTextureWrapper::TextureWrapperDelegate::
              LostContextCallback,
          base::Unretained(this))));
  if (!capture_thread_context->BindToCurrentThread()) {
    capture_thread_context = NULL;
    DLOG(ERROR) << "Couldn't bind the Capture Context to the Capture Thread.";
    return;
  }
  DCHECK(capture_thread_context);
  capture_thread_context_ = capture_thread_context;

  // At this point, |capture_thread_context| is a cc::ContextProvider. Creation
  // of our GLHelper should happen on Capture Thread.
  gl_helper_.reset(new GLHelper(capture_thread_context->ContextGL(),
                                capture_thread_context->ContextSupport()));
  DCHECK(gl_helper_);
}

void VideoCaptureTextureWrapper::TextureWrapperDelegate::ReleaseCallback(
    GLuint image_id,
    GLuint texture_id,
    linked_ptr<gfx::GpuMemoryBuffer> memory_buffer,
    uint32 sync_point) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  // TODO(mcasas): Before recycling |memory_buffer| we have to make sure it has
  // been consumed and fully used.
  gpu_memory_buffers_.push(memory_buffer);

  if (gl_helper_) {
    gl_helper_->DeleteTexture(texture_id);
    capture_thread_context_->ContextGL()->DestroyImageCHROMIUM(image_id);
  }
}

void VideoCaptureTextureWrapper::TextureWrapperDelegate::LostContextCallback() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  // Prevent incoming frames from being processed while OnError gets groked.
  gl_helper_.reset();
  OnError("GLContext lost");
}

void VideoCaptureTextureWrapper::TextureWrapperDelegate::OnError(
    const std::string& message) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DLOG(ERROR) << message;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, controller_));
}

}  // namespace content
