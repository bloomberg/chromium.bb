// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"

#include "base/observer_list.h"
#include "content/browser/android/in_process/synchronous_compositor_output_surface.h"
#include "content/public/browser/browser_thread.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

using webkit::gpu::ContextProviderWebContext;

namespace content {

namespace {

blink::WebGraphicsContext3D::Attributes GetDefaultAttribs() {
  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.depth = false;
  attributes.stencil = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  return attributes;
}

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;
using webkit::gpu::WebGraphicsContext3DImpl;

scoped_ptr<gpu::GLInProcessContext> CreateOffscreenContext(
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  const gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

  gpu::gles2::ContextCreationAttribHelper in_process_attribs;
  WebGraphicsContext3DImpl::ConvertAttributes(
      attributes, &in_process_attribs);
  in_process_attribs.lose_context_when_out_of_memory = true;

  scoped_ptr<gpu::GLInProcessContext> context(gpu::GLInProcessContext::Create(
      NULL /* service */,
      NULL /* surface */,
      true /* is_offscreen */,
      gfx::kNullAcceleratedWidget,
      gfx::Size(1, 1),
      NULL /* share_context */,
      false /* share_resources */,
      in_process_attribs,
      gpu_preference,
      gpu::GLInProcessContextSharedMemoryLimits()));
  return context.Pass();
}

scoped_ptr<gpu::GLInProcessContext> CreateContext(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
    const gpu::GLInProcessContextSharedMemoryLimits& mem_limits) {
  const gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;
  gpu::gles2::ContextCreationAttribHelper in_process_attribs;
  WebGraphicsContext3DImpl::ConvertAttributes(
      GetDefaultAttribs(), &in_process_attribs);
  in_process_attribs.lose_context_when_out_of_memory = true;

  scoped_ptr<gpu::GLInProcessContext> context(gpu::GLInProcessContext::Create(
      service,
      NULL /* surface */,
      false /* is_offscreen */,
      gfx::kNullAcceleratedWidget,
      gfx::Size(1, 1),
      NULL /* share_context */,
      false /* share_resources */,
      in_process_attribs,
      gpu_preference,
      mem_limits));
  return context.Pass();
}

scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> WrapContext(
    scoped_ptr<gpu::GLInProcessContext> context) {
  if (!context.get())
    return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>();

  return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>(
      WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
          context.Pass(), GetDefaultAttribs()));
}

scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WrapContextWithAttributes(
    scoped_ptr<gpu::GLInProcessContext> context,
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  if (!context.get())
    return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>();

  return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>(
      WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
          context.Pass(), attributes));
}

}  // namespace

class SynchronousCompositorFactoryImpl::VideoContextProvider
    : public StreamTextureFactorySynchronousImpl::ContextProvider {
 public:
  VideoContextProvider(
      scoped_ptr<gpu::GLInProcessContext> gl_in_process_context)
      : gl_in_process_context_(gl_in_process_context.get()) {

    context_provider_ = webkit::gpu::ContextProviderInProcess::Create(
        WrapContext(gl_in_process_context.Pass()),
        "Video-Offscreen-main-thread");
    context_provider_->BindToCurrentThread();
  }

  virtual scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
      uint32 stream_id) OVERRIDE {
    return gl_in_process_context_->GetSurfaceTexture(stream_id);
  }

  virtual gpu::gles2::GLES2Interface* ContextGL() OVERRIDE {
    return context_provider_->ContextGL();
  }

  virtual void AddObserver(StreamTextureFactoryContextObserver* obs) OVERRIDE {
    observer_list_.AddObserver(obs);
  }

  virtual void RemoveObserver(
      StreamTextureFactoryContextObserver* obs) OVERRIDE {
    observer_list_.RemoveObserver(obs);
  }

  void RestoreContext() {
    FOR_EACH_OBSERVER(StreamTextureFactoryContextObserver,
                      observer_list_,
                      ResetStreamTextureProxy());
  }

 private:
  friend class base::RefCountedThreadSafe<VideoContextProvider>;
  virtual ~VideoContextProvider() {}

  scoped_refptr<cc::ContextProvider> context_provider_;
  gpu::GLInProcessContext* gl_in_process_context_;
  ObserverList<StreamTextureFactoryContextObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(VideoContextProvider);
};

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

SynchronousCompositorFactoryImpl::SynchronousCompositorFactoryImpl()
    : record_full_layer_(true),
      num_hardware_compositors_(0) {
  SynchronousCompositorFactory::SetInstance(this);
}

SynchronousCompositorFactoryImpl::~SynchronousCompositorFactoryImpl() {}

scoped_refptr<base::MessageLoopProxy>
SynchronousCompositorFactoryImpl::GetCompositorMessageLoop() {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
}

bool
SynchronousCompositorFactoryImpl::RecordFullLayer() {
  return record_full_layer_;
}

scoped_ptr<cc::OutputSurface>
SynchronousCompositorFactoryImpl::CreateOutputSurface(
    int routing_id,
    scoped_refptr<content::FrameSwapMessageQueue> frame_swap_message_queue) {
  scoped_ptr<SynchronousCompositorOutputSurface> output_surface(
      new SynchronousCompositorOutputSurface(routing_id,
                                             frame_swap_message_queue));
  return output_surface.PassAs<cc::OutputSurface>();
}

InputHandlerManagerClient*
SynchronousCompositorFactoryImpl::GetInputHandlerManagerClient() {
  return synchronous_input_event_filter();
}

scoped_refptr<ContextProviderWebContext>
SynchronousCompositorFactoryImpl::CreateOffscreenContextProvider(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    const std::string& debug_name) {
  scoped_ptr<gpu::GLInProcessContext> context =
      CreateOffscreenContext(attributes);
  return webkit::gpu::ContextProviderInProcess::Create(
      WrapContext(context.Pass()), debug_name);
}

scoped_refptr<cc::ContextProvider> SynchronousCompositorFactoryImpl::
    CreateOnscreenContextProviderForCompositorThread() {
  DCHECK(service_);

  gpu::GLInProcessContextSharedMemoryLimits mem_limits;
  // This is half of what RenderWidget uses because synchronous compositor
  // pipeline is only one frame deep.
  mem_limits.mapped_memory_reclaim_limit = 6 * 1024 * 1024;
  return webkit::gpu::ContextProviderInProcess::Create(
      WrapContext(CreateContext(service_, mem_limits)),
      "Child-Compositor");
}

scoped_refptr<StreamTextureFactory>
SynchronousCompositorFactoryImpl::CreateStreamTextureFactory(int frame_id) {
  scoped_refptr<StreamTextureFactorySynchronousImpl> factory(
      StreamTextureFactorySynchronousImpl::Create(
          base::Bind(
              &SynchronousCompositorFactoryImpl::TryCreateStreamTextureFactory,
              base::Unretained(this)),
          frame_id));
  return factory;
}

webkit::gpu::WebGraphicsContext3DImpl*
SynchronousCompositorFactoryImpl::CreateOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  return WrapContextWithAttributes(CreateOffscreenContext(attributes),
                                   attributes).release();
}

void SynchronousCompositorFactoryImpl::CompositorInitializedHardwareDraw() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  num_hardware_compositors_++;
  if (num_hardware_compositors_ == 1 && main_thread_proxy_) {
    main_thread_proxy_->PostTask(
        FROM_HERE,
        base::Bind(
            &SynchronousCompositorFactoryImpl::RestoreContextOnMainThread,
            base::Unretained(this)));
  }
}

void SynchronousCompositorFactoryImpl::CompositorReleasedHardwareDraw() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  DCHECK_GT(num_hardware_compositors_, 0u);
  num_hardware_compositors_--;
}

void SynchronousCompositorFactoryImpl::RestoreContextOnMainThread() {
  if (CanCreateMainThreadContext() && video_context_provider_ )
    video_context_provider_->RestoreContext();
}

bool SynchronousCompositorFactoryImpl::CanCreateMainThreadContext() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  return num_hardware_compositors_ > 0;
}

scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
SynchronousCompositorFactoryImpl::TryCreateStreamTextureFactory() {
  {
    base::AutoLock lock(num_hardware_compositor_lock_);
    main_thread_proxy_ = base::MessageLoopProxy::current();
  }

  // Always fail creation even if |video_context_provider_| is not NULL.
  // This is to avoid synchronous calls that may deadlock. Setting
  // |video_context_provider_| to null is also not safe since it makes
  // synchronous destruction uncontrolled and possibly deadlock.
  if (!CanCreateMainThreadContext()) {
    return
        scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>();
  }

  if (!video_context_provider_) {
    DCHECK(service_);

    video_context_provider_ = new VideoContextProvider(
        CreateContext(service_,
                      gpu::GLInProcessContextSharedMemoryLimits()));
  }
  return video_context_provider_;
}

void SynchronousCompositorFactoryImpl::SetDeferredGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  DCHECK(!service_);
  service_ = service;
}

void SynchronousCompositorFactoryImpl::SetRecordFullDocument(
    bool record_full_document) {
  record_full_layer_ = record_full_document;
}

}  // namespace content
