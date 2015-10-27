// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_driver.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "components/mus/gles2/command_buffer_type_conversions.h"
#include "components/mus/gles2/gpu_memory_tracker.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/gles2/mojo_buffer_backing.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"

namespace mus {

CommandBufferDriver::Client::~Client() {}

CommandBufferDriver::CommandBufferDriver(scoped_refptr<GpuState> gpu_state)
    : client_(nullptr), gpu_state_(gpu_state), weak_factory_(this) {}

CommandBufferDriver::~CommandBufferDriver() {
  DestroyDecoder();
}

void CommandBufferDriver::Initialize(
    mojo::InterfacePtrInfo<mojom::CommandBufferSyncClient> sync_client,
    mojo::InterfacePtrInfo<mojom::CommandBufferLostContextObserver>
        loss_observer,
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs) {
  sync_client_ = mojo::MakeProxy(sync_client.Pass());
  loss_observer_ = mojo::MakeProxy(loss_observer.Pass());
  bool success = DoInitialize(shared_state.Pass(), attribs.Pass());
  mojom::GpuCapabilitiesPtr capabilities =
      success ? mojom::GpuCapabilities::From(decoder_->GetCapabilities())
              : nullptr;
  sync_client_->DidInitialize(success, capabilities.Pass());
}

bool CommandBufferDriver::MakeCurrent() {
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

bool CommandBufferDriver::DoInitialize(
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs) {
  gpu::gles2::ContextCreationAttribHelper attrib_helper;
  if (!attrib_helper.Parse(attribs.storage()))
    return false;

  surface_ = gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size(1, 1));

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
  decoder_->set_engine(scheduler_.get());
  decoder_->SetResizeCallback(
      base::Bind(&CommandBufferDriver::OnResize, base::Unretained(this)));
  decoder_->SetWaitSyncPointCallback(base::Bind(
      &CommandBufferDriver::OnWaitSyncPoint, base::Unretained(this)));
  decoder_->SetFenceSyncReleaseCallback(base::Bind(
      &CommandBufferDriver::OnFenceSyncRelease, base::Unretained(this)));
  decoder_->SetWaitFenceSyncCallback(base::Bind(
      &CommandBufferDriver::OnWaitFenceSync, base::Unretained(this)));

  gpu::gles2::DisallowedFeatures disallowed_features;

  const bool offscreen = true;
  std::vector<int32> attrib_vector;
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
      MojoBufferBacking::Create(shared_state.Pass(), kSize));
  if (!backing)
    return false;

  command_buffer_->SetSharedStateBuffer(backing.Pass());
  return true;
}

void CommandBufferDriver::SetGetBuffer(int32_t buffer) {
  command_buffer_->SetGetBuffer(buffer);
}

void CommandBufferDriver::Flush(int32_t put_offset) {
  if (!context_)
    return;
  if (!context_->MakeCurrent(surface_.get())) {
    DLOG(WARNING) << "Context lost";
    OnContextLost(gpu::error::kUnknown);
    return;
  }
  command_buffer_->Flush(put_offset);
}

void CommandBufferDriver::MakeProgress(int32_t last_get_offset) {
  // TODO(piman): handle out-of-order.
  sync_client_->DidMakeProgress(
      mojom::CommandBufferState::From(command_buffer_->GetLastState()));
}

void CommandBufferDriver::RegisterTransferBuffer(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  // Take ownership of the memory and map it into this process.
  // This validates the size.
  scoped_ptr<gpu::BufferBacking> backing(
      MojoBufferBacking::Create(transfer_buffer.Pass(), size));
  if (!backing) {
    DVLOG(0) << "Failed to map shared memory.";
    return;
  }
  command_buffer_->RegisterTransferBuffer(id, backing.Pass());
}

void CommandBufferDriver::DestroyTransferBuffer(int32_t id) {
  command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferDriver::Echo(const mojo::Callback<void()>& callback) {
  callback.Run();
}

void CommandBufferDriver::CreateImage(int32_t id,
                                      mojo::ScopedHandle memory_handle,
                                      int32 type,
                                      mojo::SizePtr size,
                                      int32_t format,
                                      int32_t internal_format) {
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

  base::SharedMemoryHandle handle;
#if defined(OS_WIN)
  handle = base::SharedMemoryHandle(platform_handle, base::GetCurrentProcId());
#else
  handle = base::FileDescriptor(platform_handle, false);
#endif

  scoped_refptr<gfx::GLImageSharedMemory> image =
      new gfx::GLImageSharedMemory(gfx_size, internal_format);
  // TODO(jam): also need a mojo enum for this enum
  if (!image->Initialize(handle, gfx::GpuMemoryBufferId(id), gpu_format, 0)) {
    NOTREACHED();
    return;
  }

  image_manager->AddImage(image.get(), id);
}

void CommandBufferDriver::DestroyImage(int32_t id) {
  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }
  if (!MakeCurrent())
    return;
  image_manager->RemoveImage(id);
}

void CommandBufferDriver::OnParseError() {
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  OnContextLost(state.context_lost_reason);
}

void CommandBufferDriver::OnResize(gfx::Size size, float scale_factor) {
  surface_->Resize(size);
}

bool CommandBufferDriver::OnWaitSyncPoint(uint32_t sync_point) {
  if (!sync_point)
    return true;
  if (gpu_state_->sync_point_manager()->IsSyncPointRetired(sync_point))
    return true;
  scheduler_->SetScheduled(false);
  gpu_state_->sync_point_manager()->AddSyncPointCallback(
      sync_point, base::Bind(&CommandBufferDriver::OnSyncPointRetired,
                             weak_factory_.GetWeakPtr()));
  return scheduler_->scheduled();
}

void CommandBufferDriver::OnFenceSyncRelease(uint64_t release) {
  // TODO(dyen): Implement once CommandBufferID has been figured out and
  // we have a SyncPointClient. It would probably look like what is commented
  // out below:
  // if (!sync_point_client_->client_state()->IsFenceSyncReleased(release))
  //   sync_point_client_->ReleaseFenceSync(release);
  NOTIMPLEMENTED();
}

bool CommandBufferDriver::OnWaitFenceSync(
    gpu::CommandBufferNamespace namespace_id,
    uint64_t command_buffer_id,
    uint64_t release) {
  gpu::SyncPointManager* sync_point_manager = gpu_state_->sync_point_manager();
  DCHECK(sync_point_manager);

  scoped_refptr<gpu::SyncPointClientState> release_state =
      sync_point_manager->GetSyncPointClientState(namespace_id,
                                                  command_buffer_id);

  if (!release_state)
    return true;

  if (release_state->IsFenceSyncReleased(release))
    return true;

  // TODO(dyen): Implement once CommandBufferID has been figured out and
  // we have a SyncPointClient. It would probably look like what is commented
  // out below:
  // scheduler_->SetScheduled(false);
  // sync_point_client_->Wait(
  //     release_state.get(),
  //     release,
  //     base::Bind(&CommandBufferDriver::OnSyncPointRetired,
  //                weak_factory_.GetWeakPtr()));
  NOTIMPLEMENTED();
  return scheduler_->scheduled();
}

void CommandBufferDriver::OnSyncPointRetired() {
  scheduler_->SetScheduled(true);
}

void CommandBufferDriver::OnContextLost(uint32_t reason) {
  loss_observer_->DidLoseContext(reason);
  client_->DidLoseContext();
}

void CommandBufferDriver::DestroyDecoder() {
  if (decoder_) {
    bool have_context = decoder_->MakeCurrent();
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
}

}  // namespace mus
