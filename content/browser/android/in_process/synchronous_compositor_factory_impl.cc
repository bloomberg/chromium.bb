// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"

#include <stdint.h>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sys_info.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/android/in_process/context_provider_in_process.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/browser/android/in_process/synchronous_compositor_registry_in_proc.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/android/synchronous_compositor_external_begin_frame_source.h"
#include "content/renderer/android/synchronous_compositor_output_surface.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/blink/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"

using cc_blink::ContextProviderWebContext;
using gpu_blink::WebGraphicsContext3DImpl;
using gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl;

namespace content {

namespace {

struct ContextHolder {
  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> command_buffer;
  gpu::GLInProcessContext* gl_in_process_context;
};

blink::WebGraphicsContext3D::Attributes GetDefaultAttribs() {
  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.depth = false;
  attributes.stencil = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  return attributes;
}

ContextHolder CreateContextHolder(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
    const gpu::GLInProcessContextSharedMemoryLimits& mem_limits,
    bool is_offscreen) {
  gpu::gles2::ContextCreationAttribHelper in_process_attribs;
  WebGraphicsContext3DImpl::ConvertAttributes(attributes, &in_process_attribs);
  in_process_attribs.lose_context_when_out_of_memory = true;

  scoped_ptr<gpu::GLInProcessContext> context(gpu::GLInProcessContext::Create(
      service, NULL /* surface */, is_offscreen, gfx::kNullAcceleratedWidget,
      gfx::Size(1, 1), NULL /* share_context */, attributes.shareResources,
      in_process_attribs, gfx::PreferDiscreteGpu, mem_limits,
      BrowserGpuMemoryBufferManager::current(), nullptr));

  gpu::GLInProcessContext* context_ptr = context.get();

  ContextHolder holder;
  holder.command_buffer =
      scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>(
          WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
              std::move(context), attributes));
  holder.gl_in_process_context = context_ptr;

  return holder;
}

}  // namespace

SynchronousCompositorFactoryImpl::SynchronousCompositorFactoryImpl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    // TODO(boliu): Figure out how to deal with this more nicely.
    SynchronousCompositorFactory::SetInstance(this);
  }
}

SynchronousCompositorFactoryImpl::~SynchronousCompositorFactoryImpl() {}

scoped_refptr<base::SingleThreadTaskRunner>
SynchronousCompositorFactoryImpl::GetCompositorTaskRunner() {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
}

scoped_ptr<cc::OutputSurface>
SynchronousCompositorFactoryImpl::CreateOutputSurface(
    int routing_id,
    const scoped_refptr<FrameSwapMessageQueue>& frame_swap_message_queue,
    const scoped_refptr<cc::ContextProvider>& onscreen_context,
    const scoped_refptr<cc::ContextProvider>& worker_context) {
  return make_scoped_ptr(new SynchronousCompositorOutputSurface(
      onscreen_context, worker_context, routing_id,
      SynchronousCompositorRegistryInProc::GetInstance(),
      frame_swap_message_queue));
}

InputHandlerManagerClient*
SynchronousCompositorFactoryImpl::GetInputHandlerManagerClient() {
  return synchronous_input_event_filter();
}

scoped_ptr<cc::BeginFrameSource>
SynchronousCompositorFactoryImpl::CreateExternalBeginFrameSource(
    int routing_id) {
  return make_scoped_ptr(new SynchronousCompositorExternalBeginFrameSource(
      routing_id, SynchronousCompositorRegistryInProc::GetInstance()));
}

// SynchronousCompositorStreamTextureFactoryImpl.

class SynchronousCompositorStreamTextureFactoryImpl::VideoContextProvider
    : public StreamTextureFactorySynchronousImpl::ContextProvider {
 public:
  VideoContextProvider(scoped_refptr<cc::ContextProvider> context_provider,
                       gpu::GLInProcessContext* gl_in_process_context)
      : context_provider_(context_provider),
        gl_in_process_context_(gl_in_process_context) {
    context_provider_->BindToCurrentThread();
  }

  scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
      uint32_t stream_id) override {
    return gl_in_process_context_->GetSurfaceTexture(stream_id);
  }

  uint32_t CreateStreamTexture(uint32_t texture_id) override {
    return gl_in_process_context_->CreateStreamTexture(texture_id);
  }

  gpu::gles2::GLES2Interface* ContextGL() override {
    return context_provider_->ContextGL();
  }

  void AddObserver(StreamTextureFactoryContextObserver* obs) override {
    observer_list_.AddObserver(obs);
  }

  void RemoveObserver(StreamTextureFactoryContextObserver* obs) override {
    observer_list_.RemoveObserver(obs);
  }

  void RestoreContext() {
    FOR_EACH_OBSERVER(StreamTextureFactoryContextObserver, observer_list_,
                      ResetStreamTextureProxy());
  }

 private:
  friend class base::RefCountedThreadSafe<VideoContextProvider>;
  ~VideoContextProvider() override {}

  scoped_refptr<cc::ContextProvider> context_provider_;
  gpu::GLInProcessContext* gl_in_process_context_;
  base::ObserverList<StreamTextureFactoryContextObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(VideoContextProvider);
};

namespace {
base::LazyInstance<SynchronousCompositorStreamTextureFactoryImpl>::Leaky
    g_stream_texture_factory = LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
SynchronousCompositorStreamTextureFactoryImpl*
SynchronousCompositorStreamTextureFactoryImpl::GetInstance() {
  return g_stream_texture_factory.Pointer();
}

SynchronousCompositorStreamTextureFactoryImpl::
    SynchronousCompositorStreamTextureFactoryImpl()
    : num_hardware_compositors_(0) {}

scoped_refptr<StreamTextureFactory>
SynchronousCompositorStreamTextureFactoryImpl::CreateStreamTextureFactory() {
  return StreamTextureFactorySynchronousImpl::Create(
      base::Bind(&SynchronousCompositorStreamTextureFactoryImpl::
                     TryCreateStreamTextureFactory,
                 base::Unretained(this)));
}

void SynchronousCompositorStreamTextureFactoryImpl::
    CompositorInitializedHardwareDraw() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  num_hardware_compositors_++;
  if (num_hardware_compositors_ == 1 && main_thread_task_runner_.get()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&SynchronousCompositorStreamTextureFactoryImpl::
                                  RestoreContextOnMainThread,
                              base::Unretained(this)));
  }
}

void SynchronousCompositorStreamTextureFactoryImpl::
    CompositorReleasedHardwareDraw() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  DCHECK_GT(num_hardware_compositors_, 0u);
  num_hardware_compositors_--;
}

void SynchronousCompositorStreamTextureFactoryImpl::
    RestoreContextOnMainThread() {
  if (CanCreateMainThreadContext() && video_context_provider_.get())
    video_context_provider_->RestoreContext();
}

bool SynchronousCompositorStreamTextureFactoryImpl::
    CanCreateMainThreadContext() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  return num_hardware_compositors_ > 0;
}

scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
SynchronousCompositorStreamTextureFactoryImpl::TryCreateStreamTextureFactory() {
  {
    base::AutoLock lock(num_hardware_compositor_lock_);
    main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  // Always fail creation even if |video_context_provider_| is not NULL.
  // This is to avoid synchronous calls that may deadlock. Setting
  // |video_context_provider_| to null is also not safe since it makes
  // synchronous destruction uncontrolled and possibly deadlock.
  if (!CanCreateMainThreadContext()) {
    return scoped_refptr<
        StreamTextureFactorySynchronousImpl::ContextProvider>();
  }

  if (!video_context_provider_.get()) {
    DCHECK(android_view_service_.get());

    blink::WebGraphicsContext3D::Attributes attributes = GetDefaultAttribs();
    attributes.shareResources = false;
    // This needs to run in on-screen |android_view_service_| context due to
    // SurfaceTexture limitations.
    ContextHolder holder =
        CreateContextHolder(attributes, android_view_service_,
                            gpu::GLInProcessContextSharedMemoryLimits(), false);
    video_context_provider_ = new VideoContextProvider(
        ContextProviderInProcess::Create(std::move(holder.command_buffer),
                                         "Video-Offscreen-main-thread"),
        holder.gl_in_process_context);
  }
  return video_context_provider_;
}

void SynchronousCompositorStreamTextureFactoryImpl::SetDeferredGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  DCHECK(!android_view_service_.get());
  android_view_service_ = service;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    RenderThreadImpl::SetStreamTextureFactory(CreateStreamTextureFactory());
  }
}

}  // namespace content
