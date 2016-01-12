// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_driver.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/mus/gles2/gpu_memory_tracker.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/gles2/mojo_buffer_backing.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"

namespace mus {

CommandBufferDriver::Client::~Client() {}

CommandBufferDriver::CommandBufferDriver(
    gpu::CommandBufferNamespace command_buffer_namespace,
    uint64_t command_buffer_id,
    gfx::AcceleratedWidget widget,
    scoped_refptr<GpuState> gpu_state)
    : command_buffer_namespace_(command_buffer_namespace),
      command_buffer_id_(command_buffer_id),
      widget_(widget),
      client_(nullptr),
      gpu_state_(gpu_state),
      weak_factory_(this) {
  DCHECK_EQ(base::ThreadTaskRunnerHandle::Get(),
            gpu_state_->command_buffer_task_runner()->task_runner());
}

CommandBufferDriver::~CommandBufferDriver() {
  DCHECK(CalledOnValidThread());
  DestroyDecoder();
}

bool CommandBufferDriver::Initialize(
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs) {
  DCHECK(CalledOnValidThread());
  gpu::gles2::ContextCreationAttribHelper attrib_helper;
  if (!attrib_helper.Parse(attribs.storage()))
    return false;

  const bool offscreen = widget_ == gfx::kNullAcceleratedWidget;
  if (offscreen) {
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size(1, 1));
  } else {
    surface_ = gfx::GLSurface::CreateViewGLSurface(widget_);
    gfx::VSyncProvider* vsync_provider =
        surface_ ? surface_->GetVSyncProvider() : nullptr;
    if (vsync_provider) {
      vsync_provider->GetVSyncParameters(
          base::Bind(&CommandBufferDriver::OnUpdateVSyncParameters,
                     weak_factory_.GetWeakPtr()));
    }
  }

  if (!surface_.get())
    return false;

  // TODO(piman): virtual contexts, gpu preference.
  context_ = gfx::GLContext::CreateGLContext(
      gpu_state_->share_group(), surface_.get(), gfx::PreferIntegratedGpu);
  if (!context_.get())
    return false;

  if (!context_->MakeCurrent(surface_.get()))
    return false;

  // TODO(piman): ShaderTranslatorCache is currently per-ContextGroup but
  // only needs to be per-thread.
  const bool bind_generates_resource = attrib_helper.bind_generates_resource;
  scoped_refptr<gpu::gles2::ContextGroup> context_group =
      new gpu::gles2::ContextGroup(
          gpu_state_->mailbox_manager(), new GpuMemoryTracker,
          new gpu::gles2::ShaderTranslatorCache,
          new gpu::gles2::FramebufferCompletenessCache, nullptr, nullptr,
          nullptr, bind_generates_resource);

  command_buffer_.reset(
      new gpu::CommandBufferService(context_group->transfer_buffer_manager()));
  bool result = command_buffer_->Initialize();
  DCHECK(result);

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group.get()));
  scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(), decoder_.get(),
                                         decoder_.get()));
  sync_point_order_data_ = gpu::SyncPointOrderData::Create();
  sync_point_client_ = gpu_state_->sync_point_manager()->CreateSyncPointClient(
      sync_point_order_data_, GetNamespaceID(), command_buffer_id_);
  decoder_->set_engine(scheduler_.get());
  decoder_->SetWaitSyncPointCallback(base::Bind(
      &CommandBufferDriver::OnWaitSyncPoint, base::Unretained(this)));
  decoder_->SetFenceSyncReleaseCallback(base::Bind(
      &CommandBufferDriver::OnFenceSyncRelease, base::Unretained(this)));
  decoder_->SetWaitFenceSyncCallback(base::Bind(
      &CommandBufferDriver::OnWaitFenceSync, base::Unretained(this)));

  gpu::gles2::DisallowedFeatures disallowed_features;

  std::vector<int32_t> attrib_vector;
  attrib_helper.Serialize(&attrib_vector);
  if (!decoder_->Initialize(surface_, context_, offscreen, gfx::Size(1, 1),
                            disallowed_features, attrib_vector))
    return false;

  command_buffer_->SetPutOffsetChangeCallback(base::Bind(
      &gpu::GpuScheduler::PutChanged, base::Unretained(scheduler_.get())));
  command_buffer_->SetGetBufferChangeCallback(base::Bind(
      &gpu::GpuScheduler::SetGetBuffer, base::Unretained(scheduler_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&CommandBufferDriver::OnParseError, base::Unretained(this)));

  // TODO(piman): other callbacks

  const size_t kSize = sizeof(gpu::CommandBufferSharedState);
  scoped_ptr<gpu::BufferBacking> backing(
      MojoBufferBacking::Create(std::move(shared_state), kSize));
  if (!backing)
    return false;

  command_buffer_->SetSharedStateBuffer(std::move(backing));
  return true;
}

void CommandBufferDriver::SetGetBuffer(int32_t buffer) {
  DCHECK(CalledOnValidThread());
  command_buffer_->SetGetBuffer(buffer);
}

void CommandBufferDriver::Flush(int32_t put_offset) {
  DCHECK(CalledOnValidThread());
  if (!MakeCurrent())
    return;

  command_buffer_->Flush(put_offset);
}

void CommandBufferDriver::RegisterTransferBuffer(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  DCHECK(CalledOnValidThread());
  // Take ownership of the memory and map it into this process.
  // This validates the size.
  scoped_ptr<gpu::BufferBacking> backing(
      MojoBufferBacking::Create(std::move(transfer_buffer), size));
  if (!backing) {
    DVLOG(0) << "Failed to map shared memory.";
    return;
  }
  command_buffer_->RegisterTransferBuffer(id, std::move(backing));
}

void CommandBufferDriver::DestroyTransferBuffer(int32_t id) {
  DCHECK(CalledOnValidThread());
  command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferDriver::CreateImage(int32_t id,
                                      mojo::ScopedHandle memory_handle,
                                      int32_t type,
                                      mojo::SizePtr size,
                                      int32_t format,
                                      int32_t internal_format) {
  DCHECK(CalledOnValidThread());
  if (!MakeCurrent())
    return;

  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  gfx::BufferFormat gpu_format = static_cast<gfx::BufferFormat>(format);
  if (!gpu::ImageFactory::IsGpuMemoryBufferFormatSupported(
          gpu_format, decoder_->GetCapabilities())) {
    LOG(ERROR) << "Format is not supported.";
    return;
  }

  gfx::Size gfx_size = size.To<gfx::Size>();
  if (!gpu::ImageFactory::IsImageSizeValidForGpuMemoryBufferFormat(
          gfx_size, gpu_format)) {
    LOG(ERROR) << "Invalid image size for format.";
    return;
  }

  if (!gpu::ImageFactory::IsImageFormatCompatibleWithGpuMemoryBufferFormat(
          internal_format, gpu_format)) {
    LOG(ERROR) << "Incompatible image format.";
    return;
  }

  if (type != gfx::SHARED_MEMORY_BUFFER) {
    NOTIMPLEMENTED();
    return;
  }

  MojoPlatformHandle platform_handle;
  MojoResult extract_result = MojoExtractPlatformHandle(
      memory_handle.release().value(), &platform_handle);
  if (extract_result != MOJO_RESULT_OK) {
    NOTREACHED();
    return;
  }

#if defined(OS_WIN)
  base::SharedMemoryHandle handle(platform_handle, base::GetCurrentProcId());
#else
  base::FileDescriptor handle(platform_handle, false);
#endif

  scoped_refptr<gl::GLImageSharedMemory> image =
      new gl::GLImageSharedMemory(gfx_size, internal_format);
  // TODO(jam): also need a mojo enum for this enum
  if (!image->Initialize(
          handle, gfx::GpuMemoryBufferId(id), gpu_format, 0,
          gfx::RowSizeForBufferFormat(gfx_size.width(), gpu_format, 0))) {
    NOTREACHED();
    return;
  }

  image_manager->AddImage(image.get(), id);
}

void CommandBufferDriver::DestroyImage(int32_t id) {
  DCHECK(CalledOnValidThread());
  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }
  if (!MakeCurrent())
    return;
  image_manager->RemoveImage(id);
}

bool CommandBufferDriver::IsScheduled() const {
  DCHECK(CalledOnValidThread());
  return scheduler_->scheduled();
}

bool CommandBufferDriver::HasUnprocessedCommands() const {
  DCHECK(CalledOnValidThread());
  if (command_buffer_) {
    gpu::CommandBuffer::State state = GetLastState();
    return command_buffer_->GetPutOffset() != state.get_offset &&
        !gpu::error::IsError(state.error);
  }
  return false;
}

gpu::Capabilities CommandBufferDriver::GetCapabilities() const {
  DCHECK(CalledOnValidThread());
  return decoder_->GetCapabilities();
}

gpu::CommandBuffer::State CommandBufferDriver::GetLastState() const {
  DCHECK(CalledOnValidThread());
  return command_buffer_->GetLastState();
}

bool CommandBufferDriver::MakeCurrent() {
  DCHECK(CalledOnValidThread());
  if (!decoder_)
    return false;
  if (decoder_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  gpu::error::ContextLostReason reason =
      static_cast<gpu::error::ContextLostReason>(
          decoder_->GetContextLostReason());
  command_buffer_->SetContextLostReason(reason);
  command_buffer_->SetParseError(gpu::error::kLostContext);
  OnContextLost(reason);
  return false;
}

void CommandBufferDriver::OnUpdateVSyncParameters(
    const base::TimeTicks timebase,
    const base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  if (client_) {
    client_->UpdateVSyncParameters(timebase.ToInternalValue(),
                                   interval.ToInternalValue());
  }
}

bool CommandBufferDriver::OnWaitSyncPoint(uint32_t sync_point) {
  DCHECK(CalledOnValidThread());
  DCHECK(scheduler_->scheduled());
  if (!sync_point)
    return true;

  scheduler_->SetScheduled(false);
  gpu_state_->sync_point_manager()->AddSyncPointCallback(
      sync_point, base::Bind(&gpu::GpuScheduler::SetScheduled,
                             scheduler_->AsWeakPtr(), true));
  return scheduler_->scheduled();
}

void CommandBufferDriver::OnFenceSyncRelease(uint64_t release) {
  DCHECK(CalledOnValidThread());
  if (!sync_point_client_->client_state()->IsFenceSyncReleased(release))
    sync_point_client_->ReleaseFenceSync(release);
}

bool CommandBufferDriver::OnWaitFenceSync(
    gpu::CommandBufferNamespace namespace_id,
    uint64_t command_buffer_id,
    uint64_t release) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsScheduled());
  gpu::SyncPointManager* sync_point_manager = gpu_state_->sync_point_manager();
  DCHECK(sync_point_manager);

  scoped_refptr<gpu::SyncPointClientState> release_state =
      sync_point_manager->GetSyncPointClientState(namespace_id,
                                                  command_buffer_id);

  if (!release_state)
    return true;

  scheduler_->SetScheduled(false);
  sync_point_client_->Wait(
      release_state.get(),
      release,
      base::Bind(&gpu::GpuScheduler::SetScheduled,
                 scheduler_->AsWeakPtr(), true));
  return scheduler_->scheduled();
}

void CommandBufferDriver::OnParseError() {
  DCHECK(CalledOnValidThread());
  gpu::CommandBuffer::State state = GetLastState();
  OnContextLost(state.context_lost_reason);
}

void CommandBufferDriver::OnContextLost(uint32_t reason) {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->DidLoseContext(reason);
}

void CommandBufferDriver::DestroyDecoder() {
  DCHECK(CalledOnValidThread());
  if (decoder_) {
    bool have_context = decoder_->MakeCurrent();
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
}

}  // namespace mus
