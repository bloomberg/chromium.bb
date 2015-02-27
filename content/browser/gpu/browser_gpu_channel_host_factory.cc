// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_channel_host_factory.h"

#include <set>

#include "base/bind.h"
#include "base/profiler/scoped_tracker.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_client.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_forwarding_message_filter.h"
#include "ipc/message_filter.h"

#if defined(OS_MACOSX)
#include "content/common/gpu/gpu_memory_buffer_factory_io_surface.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/gpu_memory_buffer_factory_surface_texture.h"
#endif

#if defined(USE_OZONE)
#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_buffer.h"
#endif

namespace content {
namespace {

base::LazyInstance<std::set<gfx::GpuMemoryBuffer::Usage>>
    g_enabled_gpu_memory_buffer_usages;
}

BrowserGpuChannelHostFactory* BrowserGpuChannelHostFactory::instance_ = NULL;

struct BrowserGpuChannelHostFactory::CreateRequest {
  CreateRequest(int32 route_id)
      : event(true, false),
        gpu_host_id(0),
        route_id(route_id),
        result(CREATE_COMMAND_BUFFER_FAILED) {}
  ~CreateRequest() {}
  base::WaitableEvent event;
  int gpu_host_id;
  int32 route_id;
  CreateCommandBufferResult result;
};

class BrowserGpuChannelHostFactory::EstablishRequest
    : public base::RefCountedThreadSafe<EstablishRequest> {
 public:
  static scoped_refptr<EstablishRequest> Create(CauseForGpuLaunch cause,
                                                int gpu_client_id,
                                                int gpu_host_id);
  void Wait();
  void Cancel();

  int gpu_host_id() { return gpu_host_id_; }
  IPC::ChannelHandle& channel_handle() { return channel_handle_; }
  gpu::GPUInfo gpu_info() { return gpu_info_; }

 private:
  friend class base::RefCountedThreadSafe<EstablishRequest>;
  explicit EstablishRequest(CauseForGpuLaunch cause,
                            int gpu_client_id,
                            int gpu_host_id);
  ~EstablishRequest() {}
  void EstablishOnIO();
  void OnEstablishedOnIO(const IPC::ChannelHandle& channel_handle,
                         const gpu::GPUInfo& gpu_info);
  void FinishOnIO();
  void FinishOnMain();

  base::WaitableEvent event_;
  CauseForGpuLaunch cause_for_gpu_launch_;
  const int gpu_client_id_;
  int gpu_host_id_;
  bool reused_gpu_process_;
  IPC::ChannelHandle channel_handle_;
  gpu::GPUInfo gpu_info_;
  bool finished_;
  scoped_refptr<base::MessageLoopProxy> main_loop_;
};

scoped_refptr<BrowserGpuChannelHostFactory::EstablishRequest>
BrowserGpuChannelHostFactory::EstablishRequest::Create(CauseForGpuLaunch cause,
                                                       int gpu_client_id,
                                                       int gpu_host_id) {
  scoped_refptr<EstablishRequest> establish_request =
      new EstablishRequest(cause, gpu_client_id, gpu_host_id);
  scoped_refptr<base::MessageLoopProxy> loop =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  // PostTask outside the constructor to ensure at least one reference exists.
  loop->PostTask(
      FROM_HERE,
      base::Bind(&BrowserGpuChannelHostFactory::EstablishRequest::EstablishOnIO,
                 establish_request));
  return establish_request;
}

BrowserGpuChannelHostFactory::EstablishRequest::EstablishRequest(
    CauseForGpuLaunch cause,
    int gpu_client_id,
    int gpu_host_id)
    : event_(false, false),
      cause_for_gpu_launch_(cause),
      gpu_client_id_(gpu_client_id),
      gpu_host_id_(gpu_host_id),
      reused_gpu_process_(false),
      finished_(false),
      main_loop_(base::MessageLoopProxy::current()) {
}

void BrowserGpuChannelHostFactory::EstablishRequest::EstablishOnIO() {
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                               cause_for_gpu_launch_);
    if (!host) {
      LOG(ERROR) << "Failed to launch GPU process.";
      FinishOnIO();
      return;
    }
    gpu_host_id_ = host->host_id();
    reused_gpu_process_ = false;
  } else {
    if (reused_gpu_process_) {
      // We come here if we retried to establish the channel because of a
      // failure in ChannelEstablishedOnIO, but we ended up with the same
      // process ID, meaning the failure was not because of a channel error,
      // but another reason. So fail now.
      LOG(ERROR) << "Failed to create channel.";
      FinishOnIO();
      return;
    }
    reused_gpu_process_ = true;
  }

  host->EstablishGpuChannel(
      gpu_client_id_,
      true,
      true,
      base::Bind(
          &BrowserGpuChannelHostFactory::EstablishRequest::OnEstablishedOnIO,
          this));
}

void BrowserGpuChannelHostFactory::EstablishRequest::OnEstablishedOnIO(
    const IPC::ChannelHandle& channel_handle,
    const gpu::GPUInfo& gpu_info) {
  if (channel_handle.name.empty() && reused_gpu_process_) {
    // We failed after re-using the GPU process, but it may have died in the
    // mean time. Retry to have a chance to create a fresh GPU process.
    DVLOG(1) << "Failed to create channel on existing GPU process. Trying to "
                "restart GPU process.";
    EstablishOnIO();
  } else {
    channel_handle_ = channel_handle;
    gpu_info_ = gpu_info;
    FinishOnIO();
  }
}

void BrowserGpuChannelHostFactory::EstablishRequest::FinishOnIO() {
  event_.Signal();
  main_loop_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserGpuChannelHostFactory::EstablishRequest::FinishOnMain,
                 this));
}

void BrowserGpuChannelHostFactory::EstablishRequest::FinishOnMain() {
  if (!finished_) {
    BrowserGpuChannelHostFactory* factory =
        BrowserGpuChannelHostFactory::instance();
    factory->GpuChannelEstablished();
    finished_ = true;
  }
}

void BrowserGpuChannelHostFactory::EstablishRequest::Wait() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/125248 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "125248 BrowserGpuChannelHostFactory::EstablishRequest::Wait"));

    // We're blocking the UI thread, which is generally undesirable.
    // In this case we need to wait for this before we can show any UI
    // /anyway/, so it won't cause additional jank.
    // TODO(piman): Make this asynchronous (http://crbug.com/125248).
    TRACE_EVENT0("browser",
                 "BrowserGpuChannelHostFactory::EstablishGpuChannelSync");
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    event_.Wait();
  }
  FinishOnMain();
}

void BrowserGpuChannelHostFactory::EstablishRequest::Cancel() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  finished_ = true;
}

bool BrowserGpuChannelHostFactory::CanUseForTesting() {
  return GpuDataManager::GetInstance()->GpuAccessAllowed(NULL);
}

void BrowserGpuChannelHostFactory::Initialize(bool establish_gpu_channel) {
  DCHECK(!instance_);
  instance_ = new BrowserGpuChannelHostFactory();
  if (establish_gpu_channel) {
    instance_->EstablishGpuChannel(CAUSE_FOR_GPU_LAUNCH_BROWSER_STARTUP,
                                   base::Closure());
  }
}

void BrowserGpuChannelHostFactory::Terminate() {
  DCHECK(instance_);
  delete instance_;
  instance_ = NULL;
}

// static
void BrowserGpuChannelHostFactory::EnableGpuMemoryBufferFactoryUsage(
    gfx::GpuMemoryBuffer::Usage usage) {
  g_enabled_gpu_memory_buffer_usages.Get().insert(usage);
}

// static
bool BrowserGpuChannelHostFactory::IsGpuMemoryBufferFactoryUsageEnabled(
    gfx::GpuMemoryBuffer::Usage usage) {
  return g_enabled_gpu_memory_buffer_usages.Get().count(usage) != 0;
}

// static
uint32 BrowserGpuChannelHostFactory::GetImageTextureTarget() {
  if (!IsGpuMemoryBufferFactoryUsageEnabled(gfx::GpuMemoryBuffer::MAP))
    return GL_TEXTURE_2D;

  std::vector<gfx::GpuMemoryBufferType> supported_types;
  GpuMemoryBufferFactory::GetSupportedTypes(&supported_types);
  DCHECK(!supported_types.empty());

  // The GPU service will always use the preferred type.
  gfx::GpuMemoryBufferType type = supported_types[0];

  switch (type) {
    case gfx::SURFACE_TEXTURE_BUFFER:
      // Surface texture backed GPU memory buffers require
      // TEXTURE_EXTERNAL_OES.
      return GL_TEXTURE_EXTERNAL_OES;
    case gfx::IO_SURFACE_BUFFER:
      // IOSurface backed images require GL_TEXTURE_RECTANGLE_ARB.
      return GL_TEXTURE_RECTANGLE_ARB;
    default:
      return GL_TEXTURE_2D;
  }
}

BrowserGpuChannelHostFactory::BrowserGpuChannelHostFactory()
    : gpu_client_id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      shutdown_event_(new base::WaitableEvent(true, false)),
      gpu_memory_buffer_manager_(
          new BrowserGpuMemoryBufferManager(this, gpu_client_id_)),
      gpu_host_id_(0),
      next_create_gpu_memory_buffer_request_id_(0) {
}

BrowserGpuChannelHostFactory::~BrowserGpuChannelHostFactory() {
  DCHECK(IsMainThread());
  if (pending_request_.get())
    pending_request_->Cancel();
  for (size_t n = 0; n < established_callbacks_.size(); n++)
    established_callbacks_[n].Run();
  shutdown_event_->Signal();
}

bool BrowserGpuChannelHostFactory::IsMainThread() {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

base::MessageLoop* BrowserGpuChannelHostFactory::GetMainLoop() {
  return BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::UI);
}

scoped_refptr<base::MessageLoopProxy>
BrowserGpuChannelHostFactory::GetIOLoopProxy() {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

scoped_ptr<base::SharedMemory>
BrowserGpuChannelHostFactory::AllocateSharedMemory(size_t size) {
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAnonymous(size))
    return scoped_ptr<base::SharedMemory>();
  return shm.Pass();
}

void BrowserGpuChannelHostFactory::CreateViewCommandBufferOnIO(
    CreateRequest* request,
    int32 surface_id,
    const GPUCreateCommandBufferConfig& init_params) {
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    request->event.Signal();
    return;
  }

  gfx::GLSurfaceHandle surface =
      GpuSurfaceTracker::Get()->GetSurfaceHandle(surface_id);

  host->CreateViewCommandBuffer(
      surface,
      surface_id,
      gpu_client_id_,
      init_params,
      request->route_id,
      base::Bind(&BrowserGpuChannelHostFactory::CommandBufferCreatedOnIO,
                 request));
}

// static
void BrowserGpuChannelHostFactory::CommandBufferCreatedOnIO(
    CreateRequest* request, CreateCommandBufferResult result) {
  request->result = result;
  request->event.Signal();
}

CreateCommandBufferResult BrowserGpuChannelHostFactory::CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id) {
  CreateRequest request(route_id);
  GetIOLoopProxy()->PostTask(FROM_HERE, base::Bind(
        &BrowserGpuChannelHostFactory::CreateViewCommandBufferOnIO,
        base::Unretained(this),
        &request,
        surface_id,
        init_params));
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/125248 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "125248 BrowserGpuChannelHostFactory::CreateViewCommandBuffer"));

  // We're blocking the UI thread, which is generally undesirable.
  // In this case we need to wait for this before we can show any UI /anyway/,
  // so it won't cause additional jank.
  // TODO(piman): Make this asynchronous (http://crbug.com/125248).
  TRACE_EVENT0("browser",
               "BrowserGpuChannelHostFactory::CreateViewCommandBuffer");
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  request.event.Wait();
  return request.result;
}

// Blocking the UI thread to open a GPU channel is not supported on Android.
// (Opening the initial channel to a child process involves handling a reply
// task on the UI thread first, so we cannot block here.)
#if !defined(OS_ANDROID)
GpuChannelHost* BrowserGpuChannelHostFactory::EstablishGpuChannelSync(
    CauseForGpuLaunch cause_for_gpu_launch) {
  EstablishGpuChannel(cause_for_gpu_launch, base::Closure());

  if (pending_request_.get())
    pending_request_->Wait();

  return gpu_channel_.get();
}
#endif

void BrowserGpuChannelHostFactory::EstablishGpuChannel(
    CauseForGpuLaunch cause_for_gpu_launch,
    const base::Closure& callback) {
  if (gpu_channel_.get() && gpu_channel_->IsLost()) {
    DCHECK(!pending_request_.get());
    // Recreate the channel if it has been lost.
    gpu_channel_ = NULL;
  }

  if (!gpu_channel_.get() && !pending_request_.get()) {
    // We should only get here if the context was lost.
    pending_request_ = EstablishRequest::Create(
        cause_for_gpu_launch, gpu_client_id_, gpu_host_id_);
  }

  if (!callback.is_null()) {
    if (gpu_channel_.get())
      callback.Run();
    else
      established_callbacks_.push_back(callback);
  }
}

GpuChannelHost* BrowserGpuChannelHostFactory::GetGpuChannel() {
  if (gpu_channel_.get() && !gpu_channel_->IsLost())
    return gpu_channel_.get();

  return NULL;
}

void BrowserGpuChannelHostFactory::GpuChannelEstablished() {
  DCHECK(IsMainThread());
  DCHECK(pending_request_.get());
  if (pending_request_->channel_handle().name.empty()) {
    DCHECK(!gpu_channel_.get());
  } else {
    GetContentClient()->SetGpuInfo(pending_request_->gpu_info());
    gpu_channel_ =
        GpuChannelHost::Create(this,
                               pending_request_->gpu_info(),
                               pending_request_->channel_handle(),
                               shutdown_event_.get(),
                               BrowserGpuMemoryBufferManager::current());
  }
  gpu_host_id_ = pending_request_->gpu_host_id();
  pending_request_ = NULL;

  for (size_t n = 0; n < established_callbacks_.size(); n++)
    established_callbacks_[n].Run();

  established_callbacks_.clear();
}

// static
void BrowserGpuChannelHostFactory::AddFilterOnIO(
    int host_id,
    scoped_refptr<IPC::MessageFilter> filter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  GpuProcessHost* host = GpuProcessHost::FromID(host_id);
  if (host)
    host->AddFilter(filter.get());
}

bool BrowserGpuChannelHostFactory::IsGpuMemoryBufferConfigurationSupported(
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage) {
  // Return early if usage is not enabled.
  if (!IsGpuMemoryBufferFactoryUsageEnabled(usage))
    return false;

  // Preferred type is always used by factory.
  std::vector<gfx::GpuMemoryBufferType> supported_types;
  GpuMemoryBufferFactory::GetSupportedTypes(&supported_types);
  DCHECK(!supported_types.empty());
  switch (supported_types[0]) {
    case gfx::SHARED_MEMORY_BUFFER:
      // Shared memory buffers must be created in-process.
      return false;
#if defined(OS_MACOSX)
    case gfx::IO_SURFACE_BUFFER:
      return GpuMemoryBufferFactoryIOSurface::
          IsGpuMemoryBufferConfigurationSupported(format, usage);
#endif
#if defined(OS_ANDROID)
    case gfx::SURFACE_TEXTURE_BUFFER:
      return GpuMemoryBufferFactorySurfaceTexture::
          IsGpuMemoryBufferConfigurationSupported(format, usage);
#endif
#if defined(USE_OZONE)
    case gfx::OZONE_NATIVE_BUFFER:
      return GpuMemoryBufferFactoryOzoneNativeBuffer::
          IsGpuMemoryBufferConfigurationSupported(format, usage);
#endif
    default:
      NOTREACHED();
      return false;
  }
}

void BrowserGpuChannelHostFactory::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int client_id,
    int32 surface_id,
    const CreateGpuMemoryBufferCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  uint32 request_id = next_create_gpu_memory_buffer_request_id_++;
  create_gpu_memory_buffer_requests_[request_id] = callback;

  host->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id, surface_id,
      base::Bind(&BrowserGpuChannelHostFactory::OnGpuMemoryBufferCreated,
                 base::Unretained(this), request_id));
}

void BrowserGpuChannelHostFactory::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    int32 sync_point) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&BrowserGpuChannelHostFactory::DestroyGpuMemoryBufferOnIO,
                 base::Unretained(this),
                 id,
                 client_id,
                 sync_point));
}

void BrowserGpuChannelHostFactory::DestroyGpuMemoryBufferOnIO(
    gfx::GpuMemoryBufferId id,
    int client_id,
    int32 sync_point) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host)
    return;

  host->DestroyGpuMemoryBuffer(id, client_id, sync_point);
}

void BrowserGpuChannelHostFactory::OnGpuMemoryBufferCreated(
    uint32 request_id,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CreateGpuMemoryBufferCallbackMap::iterator iter =
      create_gpu_memory_buffer_requests_.find(request_id);
  DCHECK(iter != create_gpu_memory_buffer_requests_.end());
  iter->second.Run(handle);
  create_gpu_memory_buffer_requests_.erase(iter);
}

}  // namespace content
