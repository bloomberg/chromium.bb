// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"

#include "base/command_line.h"
#include "base/observer_list.h"
#include "base/sys_info.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/android/in_process/context_provider_in_process.h"
#include "content/browser/android/in_process/synchronous_compositor_context_provider.h"
#include "content/browser/android/in_process/synchronous_compositor_external_begin_frame_source.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/browser/android/in_process/synchronous_compositor_output_surface.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_switches.h"
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
              context.Pass(), attributes));
  holder.gl_in_process_context = context_ptr;

  return holder;
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateContext3D(
    int surface_id,
    const blink::WebGraphicsContext3D::Attributes& attributes,
    const content::WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits&
        mem_limits) {
  DCHECK(RenderThreadImpl::current());
  CauseForGpuLaunch cause =
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
  scoped_refptr<GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync(cause));
  CHECK(gpu_channel_host.get());

  bool lose_context_when_out_of_memory = true;
  return make_scoped_ptr(new WebGraphicsContext3DCommandBufferImpl(
      surface_id, GURL(), gpu_channel_host.get(), attributes,
      lose_context_when_out_of_memory, mem_limits, NULL));
}

}  // namespace

class SynchronousCompositorFactoryImpl::VideoContextProvider
    : public StreamTextureFactorySynchronousImpl::ContextProvider {
 public:
  VideoContextProvider(
      scoped_refptr<cc::ContextProvider> context_provider,
      gpu::GLInProcessContext* gl_in_process_context)
      : context_provider_(context_provider),
        gl_in_process_context_(gl_in_process_context) {
    context_provider_->BindToCurrentThread();
  }

  scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
      uint32 stream_id) override {
    return gl_in_process_context_->GetSurfaceTexture(stream_id);
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
    FOR_EACH_OBSERVER(StreamTextureFactoryContextObserver,
                      observer_list_,
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

SynchronousCompositorFactoryImpl::SynchronousCompositorFactoryImpl()
    : use_ipc_command_buffer_(false),
      num_hardware_compositors_(0) {
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
    int surface_id,
    scoped_refptr<content::FrameSwapMessageQueue> frame_swap_message_queue) {
  scoped_refptr<cc::ContextProvider> onscreen_context =
      CreateContextProviderForCompositor(surface_id, RENDER_COMPOSITOR_CONTEXT);
  scoped_refptr<cc::ContextProvider> worker_context =
      GetSharedWorkerContextProvider();
  return make_scoped_ptr(new SynchronousCompositorOutputSurface(
      onscreen_context, worker_context, routing_id, frame_swap_message_queue));
}

InputHandlerManagerClient*
SynchronousCompositorFactoryImpl::GetInputHandlerManagerClient() {
  return synchronous_input_event_filter();
}

scoped_ptr<cc::BeginFrameSource>
SynchronousCompositorFactoryImpl::CreateExternalBeginFrameSource(
    int routing_id) {
  return make_scoped_ptr(
             new SynchronousCompositorExternalBeginFrameSource(routing_id));
}

bool SynchronousCompositorFactoryImpl::OverrideWithFactory() {
  return !use_ipc_command_buffer_;
}

scoped_refptr<ContextProviderWebContext>
SynchronousCompositorFactoryImpl::CreateOffscreenContextProvider(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    const std::string& debug_name) {
  DCHECK(!use_ipc_command_buffer_);
  ContextHolder holder =
      CreateContextHolder(attributes, GpuThreadService(),
                          gpu::GLInProcessContextSharedMemoryLimits(), true);
  return ContextProviderInProcess::Create(holder.command_buffer.Pass(),
                                          debug_name);
}

scoped_refptr<cc::ContextProvider>
SynchronousCompositorFactoryImpl::CreateContextProviderForCompositor(
    int surface_id,
    CommandBufferContextType type) {
  // This is half of what RenderWidget uses because synchronous compositor
  // pipeline is only one frame deep. But twice of half for low end here
  // because 16bit texture is not supported.
  // TODO(reveman): This limit is based on the usage required by async
  // uploads. Determine what a good limit is now that async uploads are
  // no longer used.
  unsigned int mapped_memory_reclaim_limit =
      (base::SysInfo::IsLowEndDevice() ? 2 : 6) * 1024 * 1024;
  blink::WebGraphicsContext3D::Attributes attributes = GetDefaultAttribs();

  if (use_ipc_command_buffer_) {
    WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits mem_limits;
    mem_limits.mapped_memory_reclaim_limit = mapped_memory_reclaim_limit;
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context =
        CreateContext3D(surface_id, GetDefaultAttribs(), mem_limits);
    return make_scoped_refptr(
        new SynchronousCompositorContextProvider(context.Pass(), type));
  }

  gpu::GLInProcessContextSharedMemoryLimits mem_limits;
  mem_limits.mapped_memory_reclaim_limit = mapped_memory_reclaim_limit;
  ContextHolder holder =
      CreateContextHolder(attributes, GpuThreadService(), mem_limits, true);
  return ContextProviderInProcess::Create(holder.command_buffer.Pass(),
                                          "Child-Compositor");
}

scoped_refptr<cc::ContextProvider>
SynchronousCompositorFactoryImpl::GetSharedWorkerContextProvider() {
  // TODO(reveman): This limit is based on the usage required by async
  // uploads. Determine what a good limit is now that async uploads are
  // no longer used.
  unsigned int mapped_memory_reclaim_limit =
      (base::SysInfo::IsLowEndDevice() ? 2 : 6) * 1024 * 1024;

  if (use_ipc_command_buffer_) {
    bool shared_worker_context_lost = false;
    if (shared_worker_context_) {
      // Note: If context is lost, we delete reference after releasing the lock.
      base::AutoLock lock(*shared_worker_context_->GetLock());
      if (shared_worker_context_->ContextGL()->GetGraphicsResetStatusKHR() !=
          GL_NO_ERROR) {
        shared_worker_context_lost = true;
      }
    }
    if (!shared_worker_context_ || shared_worker_context_lost) {
      WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits mem_limits;
      mem_limits.mapped_memory_reclaim_limit = mapped_memory_reclaim_limit;
      scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context =
          CreateContext3D(0, GetDefaultAttribs(), mem_limits);
      shared_worker_context_ =
          make_scoped_refptr(new SynchronousCompositorContextProvider(
              context.Pass(), RENDER_WORKER_CONTEXT));
      if (!shared_worker_context_->BindToCurrentThread())
        shared_worker_context_ = nullptr;
      if (shared_worker_context_)
        shared_worker_context_->SetupLock();
    }

    return shared_worker_context_;
  }

  bool in_process_shared_worker_context_lost = false;
  if (in_process_shared_worker_context_) {
    // Note: If context is lost, we delete reference after releasing the lock.
    base::AutoLock lock(*in_process_shared_worker_context_->GetLock());
    if (in_process_shared_worker_context_->ContextGL()
            ->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
      in_process_shared_worker_context_lost = true;
    }
  }
  if (!in_process_shared_worker_context_ ||
      in_process_shared_worker_context_lost) {
    gpu::GLInProcessContextSharedMemoryLimits mem_limits;
    mem_limits.mapped_memory_reclaim_limit = mapped_memory_reclaim_limit;
    ContextHolder holder = CreateContextHolder(
        GetDefaultAttribs(), GpuThreadService(), mem_limits, true);
    in_process_shared_worker_context_ = ContextProviderInProcess::Create(
        holder.command_buffer.Pass(), "Child-Worker");
    if (!in_process_shared_worker_context_->BindToCurrentThread())
      in_process_shared_worker_context_ = nullptr;
    if (in_process_shared_worker_context_)
      in_process_shared_worker_context_->SetupLock();
  }

  return in_process_shared_worker_context_;
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

WebGraphicsContext3DInProcessCommandBufferImpl*
SynchronousCompositorFactoryImpl::CreateOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  DCHECK(!use_ipc_command_buffer_);
  ContextHolder holder =
      CreateContextHolder(attributes, GpuThreadService(),
                          gpu::GLInProcessContextSharedMemoryLimits(), true);
  return holder.command_buffer.release();
}

gpu::GPUInfo SynchronousCompositorFactoryImpl::GetGPUInfo() const {
  DCHECK(!use_ipc_command_buffer_);
  return content::GpuDataManager::GetInstance()->GetGPUInfo();
}

void SynchronousCompositorFactoryImpl::CompositorInitializedHardwareDraw() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  num_hardware_compositors_++;
  if (num_hardware_compositors_ == 1 && main_thread_task_runner_.get()) {
    main_thread_task_runner_->PostTask(
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
  if (CanCreateMainThreadContext() && video_context_provider_.get())
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
    main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  // Always fail creation even if |video_context_provider_| is not NULL.
  // This is to avoid synchronous calls that may deadlock. Setting
  // |video_context_provider_| to null is also not safe since it makes
  // synchronous destruction uncontrolled and possibly deadlock.
  if (!CanCreateMainThreadContext()) {
    return
        scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>();
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
        ContextProviderInProcess::Create(holder.command_buffer.Pass(),
                                         "Video-Offscreen-main-thread"),
        holder.gl_in_process_context);
  }
  return video_context_provider_;
}

void SynchronousCompositorFactoryImpl::SetDeferredGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  DCHECK(!android_view_service_.get());
  android_view_service_ = service;
}

base::Thread* SynchronousCompositorFactoryImpl::CreateInProcessGpuThread(
    const InProcessChildThreadParams& params) {
  DCHECK(android_view_service_.get());
  return new InProcessGpuThread(params,
                                android_view_service_->sync_point_manager());
}

scoped_refptr<gpu::InProcessCommandBuffer::Service>
SynchronousCompositorFactoryImpl::GpuThreadService() {
  DCHECK(android_view_service_.get());
  // Create thread lazily on first use.
  if (!gpu_thread_service_.get()) {
    gpu_thread_service_ = new gpu::GpuInProcessThread(
        android_view_service_->sync_point_manager());
  }
  return gpu_thread_service_;
}

void SynchronousCompositorFactoryImpl::SetUseIpcCommandBuffer() {
  use_ipc_command_buffer_ = true;
}

}  // namespace content
