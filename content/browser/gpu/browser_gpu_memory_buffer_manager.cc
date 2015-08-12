// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"
#include "content/common/gpu/gpu_memory_buffer_factory_shared_memory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_MACOSX)
#include "content/common/gpu/gpu_memory_buffer_factory_io_surface.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/gpu_memory_buffer_factory_surface_texture.h"
#endif

#if defined(USE_OZONE)
#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_pixmap.h"
#endif

namespace content {
namespace {

void GpuMemoryBufferDeleted(
    scoped_refptr<base::SingleThreadTaskRunner> destruction_task_runner,
    const GpuMemoryBufferImpl::DestructionCallback& destruction_callback,
    uint32 sync_point) {
  destruction_task_runner->PostTask(
      FROM_HERE, base::Bind(destruction_callback, sync_point));
}

bool IsGpuMemoryBufferFactoryConfigurationSupported(
    gfx::GpuMemoryBufferType type,
    const GpuMemoryBufferFactory::Configuration& configuration) {
  switch (type) {
    case gfx::SHARED_MEMORY_BUFFER:
      return GpuMemoryBufferFactorySharedMemory::
          IsGpuMemoryBufferConfigurationSupported(configuration.format,
                                                  configuration.usage);
#if defined(OS_MACOSX)
    case gfx::IO_SURFACE_BUFFER:
      return GpuMemoryBufferFactoryIOSurface::
          IsGpuMemoryBufferConfigurationSupported(configuration.format,
                                                  configuration.usage);
#endif
#if defined(OS_ANDROID)
    case gfx::SURFACE_TEXTURE_BUFFER:
      return GpuMemoryBufferFactorySurfaceTexture::
          IsGpuMemoryBufferConfigurationSupported(configuration.format,
                                                  configuration.usage);
#endif
#if defined(USE_OZONE)
    case gfx::OZONE_NATIVE_PIXMAP:
      return GpuMemoryBufferFactoryOzoneNativePixmap::
          IsGpuMemoryBufferConfigurationSupported(configuration.format,
                                                  configuration.usage);
#endif
    default:
      NOTREACHED();
      return false;
  }
}

gfx::GpuMemoryBufferType GetGpuMemoryBufferFactoryType() {
  std::vector<gfx::GpuMemoryBufferType> supported_types;
  GpuMemoryBufferFactory::GetSupportedTypes(&supported_types);
  DCHECK(!supported_types.empty());

  // The GPU service will always use the preferred type.
  return supported_types[0];
}

std::vector<GpuMemoryBufferFactory::Configuration>
GetSupportedGpuMemoryBufferConfigurations(gfx::GpuMemoryBufferType type) {
  std::vector<GpuMemoryBufferFactory::Configuration> configurations;
#if defined(OS_MACOSX)
  bool enable_native_gpu_memory_buffers =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNativeGpuMemoryBuffers);
#else
  bool enable_native_gpu_memory_buffers =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeGpuMemoryBuffers);
#endif

  // Disable native buffers when using Mesa.
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseGL) == gfx::kGLImplementationOSMesaName) {
    enable_native_gpu_memory_buffers = false;
  }

  if (enable_native_gpu_memory_buffers) {
    const GpuMemoryBufferFactory::Configuration kNativeConfigurations[] = {
        {gfx::BufferFormat::R_8, gfx::BufferUsage::MAP},
        {gfx::BufferFormat::R_8, gfx::BufferUsage::PERSISTENT_MAP},
        {gfx::BufferFormat::RGBA_4444, gfx::BufferUsage::MAP},
        {gfx::BufferFormat::RGBA_4444, gfx::BufferUsage::PERSISTENT_MAP},
        {gfx::BufferFormat::RGBA_8888, gfx::BufferUsage::MAP},
        {gfx::BufferFormat::RGBA_8888, gfx::BufferUsage::PERSISTENT_MAP},
        {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::MAP},
        {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::PERSISTENT_MAP},
        {gfx::BufferFormat::YUV_420_BIPLANAR, gfx::BufferUsage::MAP},
        {gfx::BufferFormat::YUV_420_BIPLANAR, gfx::BufferUsage::PERSISTENT_MAP},
    };
    for (auto& configuration : kNativeConfigurations) {
      if (IsGpuMemoryBufferFactoryConfigurationSupported(type, configuration))
        configurations.push_back(configuration);
    }
  }

#if defined(USE_OZONE) || defined(OS_MACOSX)
  const GpuMemoryBufferFactory::Configuration kScanoutConfigurations[] = {
      {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::SCANOUT},
      {gfx::BufferFormat::RGBX_8888, gfx::BufferUsage::SCANOUT},
      {gfx::BufferFormat::YUV_420_BIPLANAR, gfx::BufferUsage::SCANOUT},
  };
  for (auto& configuration : kScanoutConfigurations) {
    if (IsGpuMemoryBufferFactoryConfigurationSupported(type, configuration))
      configurations.push_back(configuration);
  }
#endif

  return configurations;
}

BrowserGpuMemoryBufferManager* g_gpu_memory_buffer_manager = nullptr;

// Global atomic to generate gpu memory buffer unique IDs.
base::StaticAtomicSequenceNumber g_next_gpu_memory_buffer_id;

}  // namespace

struct BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferRequest {
  AllocateGpuMemoryBufferRequest(const gfx::Size& size,
                                 gfx::BufferFormat format,
                                 gfx::BufferUsage usage,
                                 int client_id,
                                 int surface_id)
      : event(true, false),
        size(size),
        format(format),
        usage(usage),
        client_id(client_id),
        surface_id(surface_id) {}
  ~AllocateGpuMemoryBufferRequest() {}
  base::WaitableEvent event;
  gfx::Size size;
  gfx::BufferFormat format;
  gfx::BufferUsage usage;
  int client_id;
  int surface_id;
  scoped_ptr<gfx::GpuMemoryBuffer> result;
};

BrowserGpuMemoryBufferManager::BrowserGpuMemoryBufferManager(
    int gpu_client_id,
    uint64_t gpu_client_tracing_id)
    : factory_type_(GetGpuMemoryBufferFactoryType()),
      supported_configurations_(
          GetSupportedGpuMemoryBufferConfigurations(factory_type_)),
      gpu_client_id_(gpu_client_id),
      gpu_client_tracing_id_(gpu_client_tracing_id),
      gpu_host_id_(0) {
  DCHECK(!g_gpu_memory_buffer_manager);
  g_gpu_memory_buffer_manager = this;
}

BrowserGpuMemoryBufferManager::~BrowserGpuMemoryBufferManager() {
  g_gpu_memory_buffer_manager = nullptr;
}

// static
BrowserGpuMemoryBufferManager* BrowserGpuMemoryBufferManager::current() {
  return g_gpu_memory_buffer_manager;
}

// static
uint32 BrowserGpuMemoryBufferManager::GetImageTextureTarget(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  gfx::GpuMemoryBufferType type = GetGpuMemoryBufferFactoryType();
  for (auto& configuration : GetSupportedGpuMemoryBufferConfigurations(type)) {
    if (configuration.format != format || configuration.usage != usage)
      continue;

    switch (type) {
      case gfx::SURFACE_TEXTURE_BUFFER:
      case gfx::OZONE_NATIVE_PIXMAP:
        // GPU memory buffers that are shared with the GL using EGLImages
        // require TEXTURE_EXTERNAL_OES.
        return GL_TEXTURE_EXTERNAL_OES;
      case gfx::IO_SURFACE_BUFFER:
        // IOSurface backed images require GL_TEXTURE_RECTANGLE_ARB.
        return GL_TEXTURE_RECTANGLE_ARB;
      default:
        return GL_TEXTURE_2D;
    }
  }

  return GL_TEXTURE_2D;
}

scoped_ptr<gfx::GpuMemoryBuffer>
BrowserGpuMemoryBufferManager::AllocateGpuMemoryBuffer(const gfx::Size& size,
                                                       gfx::BufferFormat format,
                                                       gfx::BufferUsage usage) {
  return AllocateGpuMemoryBufferForSurface(size, format, usage, 0);
}

scoped_ptr<gfx::GpuMemoryBuffer>
BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForScanout(
    const gfx::Size& size,
    gfx::BufferFormat format,
    int32 surface_id) {
  DCHECK_GT(surface_id, 0);
  return AllocateGpuMemoryBufferForSurface(
      size, format, gfx::BufferUsage::SCANOUT, surface_id);
}

void BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForChildProcess(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    base::ProcessHandle child_process_handle,
    int child_client_id,
    const AllocationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  gfx::GpuMemoryBufferId new_id = g_next_gpu_memory_buffer_id.GetNext();

  // Use service side allocation if this is a supported configuration.
  if (IsGpuMemoryBufferConfigurationSupported(format, usage)) {
    AllocateGpuMemoryBufferOnIO(new_id, size, format, usage, child_client_id, 0,
                                false, callback);
    return;
  }

  // Early out if we cannot fallback to shared memory buffer.
  if (!GpuMemoryBufferImplSharedMemory::IsFormatSupported(format) ||
      !GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage) ||
      !GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(size, format)) {
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  BufferMap& buffers = clients_[child_client_id];
  DCHECK(buffers.find(new_id) == buffers.end());

  // Allocate shared memory buffer as fallback.
  buffers[new_id] =
      BufferInfo(size, gfx::SHARED_MEMORY_BUFFER, format, usage, 0);
  callback.Run(GpuMemoryBufferImplSharedMemory::AllocateForChildProcess(
      new_id, size, format, child_process_handle));
}

gfx::GpuMemoryBuffer*
BrowserGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return GpuMemoryBufferImpl::FromClientBuffer(buffer);
}

void BrowserGpuMemoryBufferManager::SetDestructionSyncPoint(
    gfx::GpuMemoryBuffer* buffer,
    uint32 sync_point) {
  static_cast<GpuMemoryBufferImpl*>(buffer)
      ->set_destruction_sync_point(sync_point);
}

bool BrowserGpuMemoryBufferManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& client : clients_) {
    int client_id = client.first;

    for (const auto& buffer : client.second) {
      if (buffer.second.type == gfx::EMPTY_BUFFER)
        continue;

      gfx::GpuMemoryBufferId buffer_id = buffer.first;
      base::trace_event::MemoryAllocatorDump* dump =
          pmd->CreateAllocatorDump(base::StringPrintf(
              "gpumemorybuffer/client_%d/buffer_%d", client_id, buffer_id));
      if (!dump)
        return false;

      size_t buffer_size_in_bytes = 0;
      // Note: BufferSizeInBytes returns an approximated size for the buffer
      // but the factory can be made to return the exact size if this
      // approximation is not good enough.
      bool valid_size = GpuMemoryBufferImpl::BufferSizeInBytes(
          buffer.second.size, buffer.second.format, &buffer_size_in_bytes);
      DCHECK(valid_size);

      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      buffer_size_in_bytes);

      // Create the cross-process ownership edge. If the client creates a
      // corresponding dump for the same buffer, this will avoid to
      // double-count them in tracing. If, instead, no other process will emit a
      // dump with the same guid, the segment will be accounted to the browser.
      uint64 client_tracing_process_id = ClientIdToTracingProcessId(client_id);

      base::trace_event::MemoryAllocatorDumpGuid shared_buffer_guid =
          gfx::GetGpuMemoryBufferGUIDForTracing(client_tracing_process_id,
                                                buffer_id);
      pmd->CreateSharedGlobalAllocatorDump(shared_buffer_guid);
      pmd->AddOwnershipEdge(dump->guid(), shared_buffer_guid);
    }
  }

  return true;
}

void BrowserGpuMemoryBufferManager::ChildProcessDeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    base::ProcessHandle child_process_handle,
    int child_client_id,
    uint32 sync_point) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DestroyGpuMemoryBufferOnIO(id, child_client_id, sync_point);
}

void BrowserGpuMemoryBufferManager::ProcessRemoved(
    base::ProcessHandle process_handle,
    int client_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ClientMap::iterator client_it = clients_.find(client_id);
  if (client_it == clients_.end())
    return;

  for (const auto& buffer : client_it->second) {
    // This might happen if buffer is currenlty in the process of being
    // allocated. The buffer will in that case be cleaned up when allocation
    // completes.
    if (buffer.second.type == gfx::EMPTY_BUFFER)
      continue;

    GpuProcessHost* host = GpuProcessHost::FromID(buffer.second.gpu_host_id);
    if (host)
      host->DestroyGpuMemoryBuffer(buffer.first, client_id, 0);
  }

  clients_.erase(client_it);
}

scoped_ptr<gfx::GpuMemoryBuffer>
BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForSurface(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int32 surface_id) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  AllocateGpuMemoryBufferRequest request(size, format, usage, gpu_client_id_,
                                         surface_id);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForSurfaceOnIO,
          base::Unretained(this),  // Safe as we wait for result below.
          base::Unretained(&request)));

  // We're blocking the UI thread, which is generally undesirable.
  TRACE_EVENT0(
      "browser",
      "BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForSurface");
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  request.event.Wait();
  return request.result.Pass();
}

bool BrowserGpuMemoryBufferManager::IsGpuMemoryBufferConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) const {
  for (auto& configuration : supported_configurations_) {
    if (configuration.format == format && configuration.usage == usage)
      return true;
  }
  return false;
}

void BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForSurfaceOnIO(
    AllocateGpuMemoryBufferRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  gfx::GpuMemoryBufferId new_id = g_next_gpu_memory_buffer_id.GetNext();

  // Use service side allocation if this is a supported configuration.
  if (IsGpuMemoryBufferConfigurationSupported(request->format,
                                              request->usage)) {
    // Note: Unretained is safe as this is only used for synchronous allocation
    // from a non-IO thread.
    AllocateGpuMemoryBufferOnIO(
        new_id, request->size, request->format, request->usage,
        request->client_id, request->surface_id, false,
        base::Bind(&BrowserGpuMemoryBufferManager::
                       GpuMemoryBufferAllocatedForSurfaceOnIO,
                   base::Unretained(this), base::Unretained(request)));
    return;
  }

  DCHECK(GpuMemoryBufferImplSharedMemory::IsFormatSupported(request->format))
      << static_cast<int>(request->format);
  DCHECK(GpuMemoryBufferImplSharedMemory::IsUsageSupported(request->usage))
      << static_cast<int>(request->usage);

  BufferMap& buffers = clients_[request->client_id];
  DCHECK(buffers.find(new_id) == buffers.end());

  // Allocate shared memory buffer as fallback.
  buffers[new_id] = BufferInfo(request->size, gfx::SHARED_MEMORY_BUFFER,
                               request->format, request->usage, 0);
  // Note: Unretained is safe as IO thread is stopped before manager is
  // destroyed.
  request->result = GpuMemoryBufferImplSharedMemory::Create(
      new_id, request->size, request->format,
      base::Bind(
          &GpuMemoryBufferDeleted,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          base::Bind(&BrowserGpuMemoryBufferManager::DestroyGpuMemoryBufferOnIO,
                     base::Unretained(this), new_id, request->client_id)));
  request->event.Signal();
}

void BrowserGpuMemoryBufferManager::GpuMemoryBufferAllocatedForSurfaceOnIO(
    AllocateGpuMemoryBufferRequest* request,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Early out if factory failed to allocate the buffer.
  if (handle.is_null()) {
    request->event.Signal();
    return;
  }

  // Note: Unretained is safe as IO thread is stopped before manager is
  // destroyed.
  request->result = GpuMemoryBufferImpl::CreateFromHandle(
      handle, request->size, request->format, request->usage,
      base::Bind(
          &GpuMemoryBufferDeleted,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          base::Bind(&BrowserGpuMemoryBufferManager::DestroyGpuMemoryBufferOnIO,
                     base::Unretained(this), handle.id, request->client_id)));
  request->event.Signal();
}

void BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferOnIO(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    int surface_id,
    bool reused_gpu_process,
    const AllocationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BufferMap& buffers = clients_[client_id];
  DCHECK(buffers.find(id) == buffers.end());

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                               CAUSE_FOR_GPU_LAUNCH_GPU_MEMORY_BUFFER_ALLOCATE);
    if (!host) {
      LOG(ERROR) << "Failed to launch GPU process.";
      callback.Run(gfx::GpuMemoryBufferHandle());
      return;
    }
    gpu_host_id_ = host->host_id();
    reused_gpu_process = false;
  } else {
    if (reused_gpu_process) {
      // We come here if we retried to allocate the buffer because of a
      // failure in GpuMemoryBufferAllocatedOnIO, but we ended up with the
      // same process ID, meaning the failure was not because of a channel
      // error, but another reason. So fail now.
      LOG(ERROR) << "Failed to allocate GpuMemoryBuffer.";
      callback.Run(gfx::GpuMemoryBufferHandle());
      return;
    }
    reused_gpu_process = true;
  }

  // Note: Handling of cases where the client is removed before the allocation
  // completes is less subtle if we set the buffer type to EMPTY_BUFFER here
  // and verify that this has not changed when allocation completes.
  buffers[id] = BufferInfo(size, gfx::EMPTY_BUFFER, format, usage, 0);

  // Note: Unretained is safe as IO thread is stopped before manager is
  // destroyed.
  host->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id, surface_id,
      base::Bind(&BrowserGpuMemoryBufferManager::GpuMemoryBufferAllocatedOnIO,
                 base::Unretained(this), id, client_id, surface_id,
                 gpu_host_id_, reused_gpu_process, callback));
}

void BrowserGpuMemoryBufferManager::GpuMemoryBufferAllocatedOnIO(
    gfx::GpuMemoryBufferId id,
    int client_id,
    int surface_id,
    int gpu_host_id,
    bool reused_gpu_process,
    const AllocationCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ClientMap::iterator client_it = clients_.find(client_id);

  // This can happen if client is removed while the buffer is being allocated.
  if (client_it == clients_.end()) {
    if (!handle.is_null()) {
      GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id);
      if (host)
        host->DestroyGpuMemoryBuffer(handle.id, client_id, 0);
    }
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  BufferMap& buffers = client_it->second;

  BufferMap::iterator buffer_it = buffers.find(id);
  DCHECK(buffer_it != buffers.end());
  DCHECK_EQ(buffer_it->second.type, gfx::EMPTY_BUFFER);

  // If the handle isn't valid, that means that the GPU process crashed or is
  // misbehaving.
  bool valid_handle = !handle.is_null() && handle.id == id;
  if (!valid_handle) {
    // If we failed after re-using the GPU process, it may have died in the
    // mean time. Retry to have a chance to create a fresh GPU process.
    if (handle.is_null() && reused_gpu_process) {
      DVLOG(1) << "Failed to create buffer through existing GPU process. "
                  "Trying to restart GPU process.";
      // If the GPU process has already been restarted, retry without failure
      // when GPU process host ID already exists.
      if (gpu_host_id != gpu_host_id_)
        reused_gpu_process = false;
      gfx::Size size = buffer_it->second.size;
      gfx::BufferFormat format = buffer_it->second.format;
      gfx::BufferUsage usage = buffer_it->second.usage;
      // Remove the buffer entry and call AllocateGpuMemoryBufferOnIO again.
      buffers.erase(buffer_it);
      AllocateGpuMemoryBufferOnIO(id, size, format, usage, client_id,
                                  surface_id, reused_gpu_process, callback);
    } else {
      // Remove the buffer entry and run the allocation callback with an empty
      // handle to indicate failure.
      buffers.erase(buffer_it);
      callback.Run(gfx::GpuMemoryBufferHandle());
    }
    return;
  }

  // Store the type and host id of this buffer so it can be cleaned up if the
  // client is removed.
  buffer_it->second.type = handle.type;
  buffer_it->second.gpu_host_id = gpu_host_id;

  callback.Run(handle);
}

void BrowserGpuMemoryBufferManager::DestroyGpuMemoryBufferOnIO(
    gfx::GpuMemoryBufferId id,
    int client_id,
    uint32 sync_point) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(clients_.find(client_id) != clients_.end());

  BufferMap& buffers = clients_[client_id];

  BufferMap::iterator buffer_it = buffers.find(id);
  if (buffer_it == buffers.end()) {
    LOG(ERROR) << "Invalid GpuMemoryBuffer ID for client.";
    return;
  }

  // This can happen if a client managed to call this while a buffer is in the
  // process of being allocated.
  if (buffer_it->second.type == gfx::EMPTY_BUFFER) {
    LOG(ERROR) << "Invalid GpuMemoryBuffer type.";
    return;
  }

  GpuProcessHost* host = GpuProcessHost::FromID(buffer_it->second.gpu_host_id);
  if (host)
    host->DestroyGpuMemoryBuffer(id, client_id, sync_point);

  buffers.erase(buffer_it);
}

uint64_t BrowserGpuMemoryBufferManager::ClientIdToTracingProcessId(
    int client_id) const {
  if (client_id == gpu_client_id_) {
    // The gpu_client uses a fixed tracing ID.
    return gpu_client_tracing_id_;
  }

  // In normal cases, |client_id| is a child process id, so we can perform
  // the standard conversion.
  return ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
      client_id);
}

}  // namespace content
